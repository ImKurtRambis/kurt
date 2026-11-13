#include "HUListenSubsystem.h"
#include "HUCaseSubsystem.h"
#include "AudioCaptureCore.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Internationalization/Regex.h"
#include "HUInteractComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

FString UHUListenSubsystem::Dir() const { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI")); }

static FString FindModel(const FString& Folder, const TCHAR* Prefix)
{
	TArray<FString> Found;
	IFileManager::Get().FindFiles(Found, *(Folder / (FString(Prefix) + TEXT("*.onnx"))), true, false);
	Found.Sort(); // prefer *.int8.onnx when both exist (sorts after plain? no matter - either works)
	return Found.Num() ? Folder / Found.Last() : FString();
}

bool UHUListenSubsystem::IsInstalled() const
{
	const FString Stt = Dir() / TEXT("stt");
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*(Dir() / TEXT("voice/bin/sherpa-onnx-offline.exe")))
		&& FPlatformFileManager::Get().GetPlatformFile().FileExists(*(Stt / TEXT("tokens.txt"))) && !FindModel(Stt, TEXT("encode")).IsEmpty();
}

bool UHUListenSubsystem::IsEnabled() const
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	return C && C->bVoiceQuestions && IsInstalled();
}

void UHUListenSubsystem::Deinitialize()
{
	if (Capture) { Capture->StopStream(); Capture->CloseStream(); Capture.Reset(); }
	Super::Deinitialize();
}

// ---------- the microphone ----------
TArray<FString> UHUListenSubsystem::MicNames()
{
	TArray<FString> Out;
	if (!Capture) Capture = MakeUnique<Audio::FAudioCapture>();
	TArray<Audio::FCaptureDeviceInfo> Devs;
	Capture->GetCaptureDevicesAvailable(Devs);
	for (const Audio::FCaptureDeviceInfo& D : Devs) Out.Add(D.DeviceName);
	return Out;
}

FString UHUListenSubsystem::MicLabel()
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!C || C->MicDevice.IsEmpty()) return TEXT("Windows default");
	return MicNames().Contains(C->MicDevice) ? C->MicDevice : C->MicDevice + TEXT(" (not plugged in - using Windows default)");
}

// Windows default, then each device in turn
void UHUListenSubsystem::CycleMic(int32 Dir)
{
	UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!C) return;
	TArray<FString> Opts = MicNames();
	Opts.Insert(FString(), 0);
	const int32 I = FMath::Max(0, Opts.IndexOfByKey(C->MicDevice));
	C->MicDevice = Opts[(I + Dir + Opts.Num()) % Opts.Num()];
	C->SaveProgress();
	if (bMonitoring) { StopMonitor(); StartMonitor(); } // the meter follows the new mic
}

int32 UHUListenSubsystem::PickedDeviceIndex()
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!C || C->MicDevice.IsEmpty()) return Audio::DefaultDeviceIndex;
	const int32 I = MicNames().IndexOfByKey(C->MicDevice);
	return I == INDEX_NONE ? Audio::DefaultDeviceIndex : I;
}

// open the picked microphone; samples are kept only while listening (the meter just watches the level)
bool UHUListenSubsystem::OpenMic()
{
	if (!Capture) Capture = MakeUnique<Audio::FAudioCapture>();
	Audio::FAudioCaptureDeviceParams Params;
	Params.DeviceIndex = PickedDeviceIndex();
	const bool bOk = Capture->OpenAudioCaptureStream(Params, [this](const void* In, int32 Frames, int32 Channels, int32 Rate, double, bool)
	{
		// float samples, interleaved -> mono
		const float* F = static_cast<const float*>(In);
		float Peak = 0.f;
		FScopeLock L(&Lock);
		CaptureRate = Rate;
		for (int32 i = 0; i < Frames; i++)
		{
			float S = 0.f;
			for (int32 c = 0; c < Channels; c++) S += F[i * Channels + c];
			S /= FMath::Max(1, Channels);
			if (bListening) Samples.Add(S);
			Peak = FMath::Max(Peak, FMath::Abs(S));
		}
		MicLevel = FMath::Clamp(Peak * 3.f, 0.f, 1.f);
		if (Samples.Num() > Rate * 15) Samples.RemoveAt(0, Frames); // 15 seconds at most
	}, 1024);
	if (!bOk || !Capture->StartStream()) { UE_LOG(LogTemp, Warning, TEXT("HomicideUnit listen: no microphone")); Capture->CloseStream(); return false; }
	return true;
}

// ---------- auto-detect: which microphone actually hears the room? ----------
// Each input device is opened for 0.4 s on a background thread and its noise level measured. A dead or
// disconnected input reads as pure silence; a real microphone always picks up a little room noise.
void UHUListenSubsystem::AutoPickMic(bool bAfterSilence)
{
	if (bProbing || (bProbedOnce && !bAfterSilence)) return;
	bProbing = true; bProbedOnce = true;
	TWeakObjectPtr<UHUListenSubsystem> Self(this);
	Async(EAsyncExecution::Thread, [Self, bAfterSilence]()
	{
		Audio::FAudioCapture Probe;
		TArray<Audio::FCaptureDeviceInfo> Devs;
		Probe.GetCaptureDevicesAvailable(Devs);
		FString DefaultName;
		{ Audio::FCaptureDeviceInfo Def; if (Probe.GetCaptureDeviceInfo(Def, Audio::DefaultDeviceIndex)) DefaultName = Def.DeviceName; }
		TArray<FString> Names; TArray<float> Rms;
		for (int32 i = 0; i < Devs.Num(); i++)
		{
			double Sum = 0.0; int64 N = 0; FCriticalSection CS;
			Audio::FAudioCaptureDeviceParams P; P.DeviceIndex = i;
			const bool bOk = Probe.OpenAudioCaptureStream(P, [&Sum, &N, &CS](const void* In, int32 Frames, int32 Channels, int32, double, bool)
			{
				const float* F = static_cast<const float*>(In);
				FScopeLock L(&CS);
				for (int32 k = 0; k < Frames * Channels; k++) { Sum += (double)F[k] * F[k]; N++; }
			}, 1024);
			if (bOk && Probe.StartStream()) { FPlatformProcess::Sleep(0.4f); Probe.StopStream(); }
			Probe.CloseStream();
			FScopeLock L(&CS);
			Names.Add(Devs[i].DeviceName);
			Rms.Add(N > 0 ? (float)FMath::Sqrt(Sum / N) : 0.f);
		}
		AsyncTask(ENamedThreads::GameThread, [Self, Names, Rms, DefaultName, bAfterSilence]()
		{
			if (Self.IsValid()) Self->FinishProbe(Names, Rms, DefaultName, bAfterSilence);
		});
	});
}

void UHUListenSubsystem::FinishProbe(const TArray<FString>& Names, const TArray<float>& Rms, const FString& DefaultName, bool bAfterSilence)
{
	bProbing = false;
	UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!C) return;
	const FString Using = !C->MicDevice.IsEmpty() && Names.Contains(C->MicDevice) ? C->MicDevice : DefaultName;
	// what-you-hear / loopback inputs carry the game's own sound, not the player: never pick those
	auto IsLoopback = [](const FString& N) { return N.Contains(TEXT("Stereo Mix")) || N.Contains(TEXT("What U Hear")) || N.Contains(TEXT("Loopback")) || N.Contains(TEXT("Wave Out")); };
	const float Alive = 2e-5f;
	int32 UsingAt = Names.IndexOfByKey(Using), Best = INDEX_NONE;
	for (int32 i = 0; i < Names.Num(); i++)
	{
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit listen: mic %d \"%s\" noise %.6f%s"), i, *Names[i], Rms[i], i == UsingAt ? TEXT("  (in use)") : TEXT(""));
		if (IsLoopback(Names[i]) || Rms[i] < Alive || (bAfterSilence && i == UsingAt)) continue;
		if (Best == INDEX_NONE || Rms[i] > Rms[Best]) Best = i;
	}
	const bool bUsingAlive = UsingAt != INDEX_NONE && Rms[UsingAt] >= Alive;
	if (!bAfterSilence && bUsingAlive) return; // the one in use hears fine: leave it
	if (Best == INDEX_NONE)
	{
		Tell(bUsingAlive ? FString::Printf(TEXT("%s is the only microphone that hears anything - speak up, or check it isn't muted."), *Using)
			: FString(TEXT("No microphone is picking up any sound. Check it's plugged in and not muted in Windows' sound settings - or press T to type.")));
		return;
	}
	C->MicDevice = Names[Best];
	C->SaveProgress();
	Tell(FString::Printf(TEXT("Now listening on %s%s. Hold V and talk."), *Names[Best], Using.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s heard nothing)"), *Using)));
}

void UHUListenSubsystem::Tell(const FString& Body)
{
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit listen: %s"), *Body);
	APawn* Me = UGameplayStatics::GetPlayerPawn(GetGameInstance()->GetWorld(), 0);
	if (UHUInteractComponent* I = Me ? Me->FindComponentByClass<UHUInteractComponent>() : nullptr) I->Notify(TEXT("MICROPHONE"), Body, FLinearColor(0.4f, 0.8f, 0.95f));
}

bool UHUListenSubsystem::StartMonitor()
{
	if (bMonitoring || bListening) return bMonitoring;
	bMonitoring = OpenMic();
	return bMonitoring;
}

void UHUListenSubsystem::StopMonitor()
{
	if (!bMonitoring) return;
	bMonitoring = false; MicLevel = 0.f;
	if (Capture) { Capture->StopStream(); Capture->CloseStream(); }
}

bool UHUListenSubsystem::StartListening()
{
	if (bListening || bWorking || !IsInstalled()) return false; // interviews are spoken: no setting needed
	StopMonitor();
	{ FScopeLock L(&Lock); Samples.Reset(); }
	bListening = true;
	if (!OpenMic()) { bListening = false; return false; }
	return true;
}
void UHUListenSubsystem::StopListening(FHUHeard Done)
{
	if (!bListening) { Done.ExecuteIfBound(FString()); return; }
	bListening = false; MicLevel = 0.f;
	Capture->StopStream(); Capture->CloseStream();
	TArray<float> Mono; int32 Rate;
	{ FScopeLock L(&Lock); Mono = MoveTemp(Samples); Rate = CaptureRate; }
	float Peak = 0.f; for (float S : Mono) Peak = FMath::Max(Peak, FMath::Abs(S));
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit listen: %.1f s from %s, loudest %.4f"), Mono.Num() / (float)FMath::Max(1, Rate), *MicLabel(), Peak);
	bLastSilent = Mono.Num() >= Rate / 4 && Peak < 0.01f;
	if (bLastSilent) { AutoPickMic(true); Done.ExecuteIfBound(FString()); return; } // that mic heard nothing: try the others
	if (Mono.Num() < Rate / 4) { Done.ExecuteIfBound(FString()); return; } // under a quarter second: nothing said

	// 16 kHz, 16-bit mono .wav - what the model wants
	const int32 Out = (int32)((int64)Mono.Num() * 16000 / FMath::Max(1, Rate));
	TArray<uint8> Wav; Wav.SetNumZeroed(44 + Out * 2);
	int16* Pcm = reinterpret_cast<int16*>(Wav.GetData() + 44);
	for (int32 i = 0; i < Out; i++)
	{
		const double Src = (double)i * Rate / 16000.0;
		const int32 A = FMath::Min((int32)Src, Mono.Num() - 1), B = FMath::Min(A + 1, Mono.Num() - 1);
		const float S = FMath::Lerp(Mono[A], Mono[B], (float)(Src - A));
		Pcm[i] = (int16)FMath::Clamp(S * 32767.f, -32768.f, 32767.f);
	}
	auto Put32 = [&Wav](int32 At, uint32 V) { FMemory::Memcpy(Wav.GetData() + At, &V, 4); };
	auto Put16 = [&Wav](int32 At, uint16 V) { FMemory::Memcpy(Wav.GetData() + At, &V, 2); };
	FMemory::Memcpy(Wav.GetData(), "RIFF", 4); Put32(4, 36 + Out * 2); FMemory::Memcpy(Wav.GetData() + 8, "WAVEfmt ", 8);
	Put32(16, 16); Put16(20, 1); Put16(22, 1); Put32(24, 16000); Put32(28, 32000); Put16(32, 2); Put16(34, 16);
	FMemory::Memcpy(Wav.GetData() + 36, "data", 4); Put32(40, Out * 2);
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Voice/heard_%d.wav"), Counter++ % 4));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	FFileHelper::SaveArrayToFile(Wav, *File);

	const FString Stt = Dir() / TEXT("stt");
	const FString Exe = Dir() / TEXT("voice/bin/sherpa-onnx-offline.exe");
	const FString Args = FString::Printf(TEXT("--tokens=\"%s/tokens.txt\" --moonshine-preprocessor=\"%s\" --moonshine-encoder=\"%s\" --moonshine-uncached-decoder=\"%s\" --moonshine-cached-decoder=\"%s\" --num-threads=2 \"%s\""),
		*Stt, *FindModel(Stt, TEXT("preprocess")), *FindModel(Stt, TEXT("encode")), *FindModel(Stt, TEXT("uncached_decode")), *FindModel(Stt, TEXT("cached_decode")), *File);
	bWorking = true;
	TWeakObjectPtr<UHUListenSubsystem> Self(this);
	Async(EAsyncExecution::Thread, [Self, Exe, Args, Done]()
	{
		int32 Code = -1; FString StdOut, StdErr;
		FPlatformProcess::ExecProcess(*Exe, *Args, &Code, &StdOut, &StdErr, *FPaths::GetPath(Exe));
		// the result line looks like {"lang": "", ..., "text": " where were you", ...}
		FString Text;
		FRegexMatcher M(FRegexPattern(TEXT("\"text\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"")), StdOut + TEXT("\n") + StdErr);
		if (M.FindNext())
		{
			// unescape, then drop stray quote marks the model sometimes adds around the sentence
			Text = M.GetCaptureGroup(1).Replace(TEXT("\\\""), TEXT("\"")).Replace(TEXT("\\\\"), TEXT("\\"));
			Text = Text.Replace(TEXT("\""), TEXT("")).TrimStartAndEnd();
			Text.TrimCharInline('\'', nullptr);
			Text.TrimStartAndEndInline();
		}
		if (Code != 0) UE_LOG(LogTemp, Warning, TEXT("HomicideUnit listen: speech-to-text failed (%d) %s"), Code, *StdErr.Right(200));
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit listen: heard \"%s\""), *Text);
		AsyncTask(ENamedThreads::GameThread, [Self, Text, Done]()
		{
			if (Self.IsValid()) Self->bWorking = false;
			Done.ExecuteIfBound(Text);
		});
	});
}

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
// HU.MicCheck: measure every microphone now and log what each one hears (and switch if the one in use is dead)
static FAutoConsoleCommandWithWorld GHUMicCheck(TEXT("HU.MicCheck"), TEXT("Probe every microphone and log their noise levels"), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
	if (UGameInstance* GI = World ? World->GetGameInstance() : nullptr) if (UHUListenSubsystem* L = GI->GetSubsystem<UHUListenSubsystem>()) L->RecheckMic();
}));
#endif
