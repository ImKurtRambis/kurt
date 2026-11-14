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
#include "WebSocketsModule.h"
#include "HUProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString UHUListenSubsystem::Dir() const { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI")); }

static FString FindModel(const FString& Folder, const TCHAR* Prefix)
{
	TArray<FString> Found;
	IFileManager::Get().FindFiles(Found, *(Folder / (FString(Prefix) + TEXT("*.onnx"))), true, false);
	Found.Sort(); // prefer *.int8.onnx when both exist (sorts after plain? no matter - either works)
	return Found.Num() ? Folder / Found.Last() : FString();
}

bool UHUListenSubsystem::HasAccurate() const
{
	const FString D = Dir() / TEXT("stt-accurate");
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*(D / TEXT("tokens.txt")))
		&& !FindModel(D, TEXT("encoder")).IsEmpty() && !FindModel(D, TEXT("decoder")).IsEmpty() && !FindModel(D, TEXT("joiner")).IsEmpty();
}

bool UHUListenSubsystem::HasFast() const
{
	const FString D = Dir() / TEXT("stt");
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*(D / TEXT("tokens.txt"))) && !FindModel(D, TEXT("encode")).IsEmpty();
}

bool UHUListenSubsystem::IsInstalled() const
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*(Dir() / TEXT("voice/bin/sherpa-onnx-offline.exe"))) && (HasAccurate() || HasFast());
}

FString UHUListenSubsystem::ModelName() const
{
	return HasAccurate() ? TEXT("Parakeet TDT 0.6B v2") : HasFast() ? TEXT("Moonshine tiny") : TEXT("none");
}

// the same flags work for the one-off exe and the always-on server
FString UHUListenSubsystem::ModelArgs() const
{
	if (HasAccurate())
	{
		const FString D = Dir() / TEXT("stt-accurate");
		return FString::Printf(TEXT("--encoder=\"%s\" --decoder=\"%s\" --joiner=\"%s\" --tokens=\"%s/tokens.txt\" --model-type=nemo_transducer"),
			*FindModel(D, TEXT("encoder")), *FindModel(D, TEXT("decoder")), *FindModel(D, TEXT("joiner")), *D);
	}
	const FString Stt = Dir() / TEXT("stt");
	return FString::Printf(TEXT("--tokens=\"%s/tokens.txt\" --moonshine-preprocessor=\"%s\" --moonshine-encoder=\"%s\" --moonshine-uncached-decoder=\"%s\" --moonshine-cached-decoder=\"%s\""),
		*Stt, *FindModel(Stt, TEXT("preprocess")), *FindModel(Stt, TEXT("encode")), *FindModel(Stt, TEXT("uncached_decode")), *FindModel(Stt, TEXT("cached_decode")));
}

// drop stray quote marks some models put around the sentence
static FString CleanHeard(FString Text)
{
	Text = Text.Replace(TEXT("\""), TEXT("")).TrimStartAndEnd();
	Text.TrimCharInline('\'', nullptr);
	return Text.TrimStartAndEnd();
}

// sherpa-onnx answers with {"lang": "", ..., "text": " where were you", ...}
static FString TextFromJson(const FString& Json)
{
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), J) || !J.IsValid()) return FString();
	FString Text;
	J->TryGetStringField(TEXT("text"), Text);
	return CleanHeard(Text);
}

bool UHUListenSubsystem::IsEnabled() const
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	return C && C->bVoiceQuestions && IsInstalled();
}

void UHUListenSubsystem::Deinitialize()
{
	if (Capture) { Capture->StopStream(); Capture->CloseStream(); Capture.Reset(); }
	Pending.Unbind();
	DropSocket();
	if (Server.IsValid()) { FPlatformProcess::TerminateProc(Server, true); FPlatformProcess::CloseProc(Server); }
	Super::Deinitialize();
}

// ---------- the always-on speech server (the accurate model) ----------
// Parakeet takes ~2 s to load and ~0.3 s to hear a question, so it's loaded once and kept: the game starts
// sherpa-onnx-offline-websocket-server on 127.0.0.1:8096 when an interview opens and talks to it over a websocket.
// A second copy of the game on this PC (squad testing) can't bind the port, and simply shares the first one's.
void UHUListenSubsystem::WarmUp()
{
	if (!HasAccurate() || bSocketOpen || bConnecting) return;
	const FString Exe = Dir() / TEXT("voice/bin/sherpa-onnx-offline-websocket-server.exe");
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*Exe)) return;
	if (!Server.IsValid() || !FPlatformProcess::IsProcRunning(Server))
	{
		if (Server.IsValid()) FPlatformProcess::CloseProc(Server);
		const FString Args = ModelArgs() + FString::Printf(TEXT(" --port=%d --num-work-threads=1 --num-threads=2"), ServerPort);
		Server = HUProcess::Launch(Exe, Args, FPaths::GetPath(Exe)); // ends with the game, even after a crash
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit listen: loading %s (%s)"), *ModelName(), Server.IsValid() ? TEXT("ok") : TEXT("failed to start"));
	}
	ServerStartedAt = FPlatformTime::Seconds();
	bConnecting = true;
	Connect();
}

void UHUListenSubsystem::DropSocket()
{
	if (Socket.IsValid())
	{
		Socket->OnConnected().Clear(); Socket->OnConnectionError().Clear(); Socket->OnClosed().Clear(); Socket->OnMessage().Clear();
		if (Socket->IsConnected()) Socket->Close();
		Socket.Reset();
	}
	bSocketOpen = false;
}

void UHUListenSubsystem::Connect()
{
	DropSocket();
	Socket = FWebSocketsModule::Get().CreateWebSocket(FString::Printf(TEXT("ws://127.0.0.1:%d"), ServerPort), TEXT(""));
	TWeakObjectPtr<UHUListenSubsystem> Self(this);
	Socket->OnConnected().AddLambda([Self]()
	{
		if (!Self.IsValid()) return;
		Self->bSocketOpen = true; Self->bConnecting = false;
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit listen: %s ready after %.1f s"), *Self->ModelName(), FPlatformTime::Seconds() - Self->ServerStartedAt);
	});
	Socket->OnConnectionError().AddLambda([Self](const FString& Error)
	{
		if (!Self.IsValid()) return;
		Self->bSocketOpen = false;
		// still loading the model: knock again shortly, for up to 30 s (the old socket is replaced from the timer, not from inside its own callback)
		if (FPlatformTime::Seconds() - Self->ServerStartedAt < 30.0)
		{
			if (UGameInstance* GI = Self->GetGameInstance()) GI->GetTimerManager().SetTimer(Self->RetryTimer, FTimerDelegate::CreateUObject(Self.Get(), &UHUListenSubsystem::Connect), 0.5f, false);
			return;
		}
		Self->bConnecting = false;
		UE_LOG(LogTemp, Warning, TEXT("HomicideUnit listen: the speech server never answered (%s) - questions are heard the slow way"), *Error);
	});
	Socket->OnClosed().AddLambda([Self](int32, const FString&, bool)
	{
		if (!Self.IsValid()) return;
		Self->bSocketOpen = false; Self->bConnecting = false;
		Self->OnReply(FString()); // a question in flight is heard the slow way instead
	});
	Socket->OnMessage().AddLambda([Self](const FString& Msg) { if (Self.IsValid()) Self->OnReply(Msg); });
	Socket->Connect();
}

// the server's answer (or, with an empty message, its silence): finish the question in flight
void UHUListenSubsystem::OnReply(const FString& Msg)
{
	if (!Pending.IsBound()) return;
	if (UGameInstance* GI = GetGameInstance()) GI->GetTimerManager().ClearTimer(PendingTimer);
	FHUHeard Done = Pending;
	Pending.Unbind();
	if (Msg.IsEmpty()) { RunExe(PendingFile, Done); return; }
	Finish(TextFromJson(Msg), Done);
}

void UHUListenSubsystem::Finish(const FString& Text, FHUHeard Done)
{
	bWorking = false;
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit listen: heard \"%s\""), *Text);
	Done.ExecuteIfBound(Text);
}

// one question, one sherpa-onnx process (the model loads each time: ~0.6 s for Moonshine, ~1.7 s for Parakeet)
void UHUListenSubsystem::RunExe(const FString& File, FHUHeard Done)
{
	const FString Exe = Dir() / TEXT("voice/bin/sherpa-onnx-offline.exe");
	const FString Args = ModelArgs() + FString::Printf(TEXT(" --num-threads=2 \"%s\""), *File);
	bWorking = true;
	TWeakObjectPtr<UHUListenSubsystem> Self(this);
	Async(EAsyncExecution::Thread, [Self, Exe, Args, Done]()
	{
		int32 Code = -1; FString StdOut, StdErr;
		FPlatformProcess::ExecProcess(*Exe, *Args, &Code, &StdOut, &StdErr, *FPaths::GetPath(Exe));
		FString Text;
		TArray<FString> Lines; StdOut.ParseIntoArrayLines(Lines);
		for (const FString& L : Lines) if (L.TrimStart().StartsWith(TEXT("{")) && L.Contains(TEXT("\"text\""))) { Text = TextFromJson(L); break; }
		if (Code != 0) UE_LOG(LogTemp, Warning, TEXT("HomicideUnit listen: speech-to-text failed (%d) %s"), Code, *StdErr.Right(200));
		AsyncTask(ENamedThreads::GameThread, [Self, Text, Done]()
		{
			if (Self.IsValid()) Self->Finish(Text, Done);
			else Done.ExecuteIfBound(Text);
		});
	});
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
	if (!bAfterSilence) WarmUp(); // an interview is opening: load the speech model while they say hello
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
	WarmUp(); // usually loaded already; if not, it's ready by the time they let go of V
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
	Hear(Mono, Rate, Done);
}

void UHUListenSubsystem::Hear(const TArray<float>& Mono, int32 Rate, FHUHeard Done)
{
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

	// the accurate model, already loaded: send the samples, the answer comes back in ~0.3 s (OnReply)
	if (bSocketOpen && Socket.IsValid() && Socket->IsConnected())
	{
		// <int32 sample rate><int32 byte count><float32 samples>, in 10 KB pieces
		TArray<uint8> Msg; Msg.SetNumUninitialized(8 + Out * 4);
		const int32 Rate16 = 16000, Bytes = Out * 4;
		FMemory::Memcpy(Msg.GetData(), &Rate16, 4); FMemory::Memcpy(Msg.GetData() + 4, &Bytes, 4);
		for (int32 i = 0; i < Out; i++) { const float F = Pcm[i] / 32768.f; FMemory::Memcpy(Msg.GetData() + 8 + i * 4, &F, 4); }
		bWorking = true;
		Pending = Done; PendingFile = File;
		for (int32 At = 0; At < Msg.Num(); At += 10240) Socket->Send(Msg.GetData() + At, FMath::Min(10240, Msg.Num() - At), true);
		// no answer in 6 s: hear it the slow way
		TWeakObjectPtr<UHUListenSubsystem> Self(this);
		GetGameInstance()->GetTimerManager().SetTimer(PendingTimer, FTimerDelegate::CreateLambda([Self]()
		{
			if (!Self.IsValid() || !Self->Pending.IsBound()) return;
			UE_LOG(LogTemp, Warning, TEXT("HomicideUnit listen: the speech server didn't answer - asking the slow way"));
			Self->OnReply(FString());
		}), 6.f, false);
		return;
	}
	RunExe(File, Done);
}

#if !UE_BUILD_SHIPPING
// -HUListenTest=<16-bit mono .wav>: hear a recording through the always-on model and the slow way, log both, quit
void UHUListenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Wav;
	if (!FParse::Value(FCommandLine::Get(), TEXT("HUListenTest="), Wav)) return;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Wav) || Bytes.Num() < 44) { UE_LOG(LogTemp, Error, TEXT("HUListenTest: can't read %s"), *Wav); return; }
	// find "fmt " and "data"
	int32 Rate = 16000, Channels = 1, Bits = 16, DataAt = -1, DataLen = 0;
	for (int32 At = 12; At + 8 <= Bytes.Num();)
	{
		int32 Len; FMemory::Memcpy(&Len, Bytes.GetData() + At + 4, 4);
		if (!FMemory::Memcmp(Bytes.GetData() + At, "fmt ", 4)) { uint16 C, B; FMemory::Memcpy(&C, Bytes.GetData() + At + 10, 2); FMemory::Memcpy(&Rate, Bytes.GetData() + At + 12, 4); FMemory::Memcpy(&B, Bytes.GetData() + At + 22, 2); Channels = C; Bits = B; }
		if (!FMemory::Memcmp(Bytes.GetData() + At, "data", 4)) { DataAt = At + 8; DataLen = FMath::Min(Len, Bytes.Num() - DataAt); break; }
		At += 8 + Len + (Len & 1);
	}
	if (DataAt < 0 || Bits != 16) { UE_LOG(LogTemp, Error, TEXT("HUListenTest: %s isn't 16-bit PCM"), *Wav); return; }
	TArray<float> Mono;
	for (int32 i = 0; (i + 1) * Channels * 2 <= DataLen; i++) { int16 S; FMemory::Memcpy(&S, Bytes.GetData() + DataAt + i * Channels * 2, 2); Mono.Add(S / 32768.f); }
	UE_LOG(LogTemp, Log, TEXT("HUListenTest: %s, %.1f s, model %s"), *FPaths::GetCleanFilename(Wav), Mono.Num() / (float)Rate, *ModelName());
	WarmUp();
	// wait for the model to load, then ask it three times (the first warms caches), then the slow way once
	TSharedRef<int32> Step = MakeShared<int32>(0);
	TSharedRef<double> T0 = MakeShared<double>(FPlatformTime::Seconds());
	TWeakObjectPtr<UHUListenSubsystem> Self(this);
	TSharedRef<FTimerHandle> Tick = MakeShared<FTimerHandle>();
	GetGameInstance()->GetTimerManager().SetTimer(*Tick, FTimerDelegate::CreateLambda([Self, Mono, Rate, Step, T0, Tick]()
	{
		if (!Self.IsValid() || Self->bWorking) return;
		const bool bLoaded = Self->bSocketOpen;
		if (*Step < 3 && !bLoaded && FPlatformTime::Seconds() - *T0 < 30.0) return; // still loading
		if (*Step >= 4) { Self->GetGameInstance()->GetTimerManager().ClearTimer(*Tick); FPlatformMisc::RequestExit(false); return; }
		const bool bSlow = *Step == 3 || !bLoaded;
		const double Asked = FPlatformTime::Seconds();
		const int32 N = ++*Step;
		if (bSlow) *Step = 4;
		auto Report = FHUHeard::CreateLambda([Asked, N, bSlow](const FString& Text)
		{
			UE_LOG(LogTemp, Log, TEXT("HUListenTest: %s #%d heard \"%s\" in %.2f s"), bSlow ? TEXT("slow way") : TEXT("always-on model"), N, *Text, FPlatformTime::Seconds() - Asked);
		});
		if (bSlow) { Self->bSocketOpen = false; } // force the one-process path for the comparison
		Self->Hear(Mono, Rate, Report);
	}), 0.25f, true);
}
#endif

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
// HU.MicCheck: measure every microphone now and log what each one hears (and switch if the one in use is dead)
static FAutoConsoleCommandWithWorld GHUMicCheck(TEXT("HU.MicCheck"), TEXT("Probe every microphone and log their noise levels"), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
	if (UGameInstance* GI = World ? World->GetGameInstance() : nullptr) if (UHUListenSubsystem* L = GI->GetSubsystem<UHUListenSubsystem>()) L->RecheckMic();
}));
#endif
