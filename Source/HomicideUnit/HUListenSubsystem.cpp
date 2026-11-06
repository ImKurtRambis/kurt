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

bool UHUListenSubsystem::StartListening()
{
	if (bListening || bWorking || !IsEnabled()) return false;
	if (!Capture) Capture = MakeUnique<Audio::FAudioCapture>();
	{ FScopeLock L(&Lock); Samples.Reset(); }
	Audio::FAudioCaptureDeviceParams Params; // the default microphone
	TWeakObjectPtr<UHUListenSubsystem> Self(this);
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
			Samples.Add(S);
			Peak = FMath::Max(Peak, FMath::Abs(S));
		}
		MicLevel = FMath::Clamp(Peak * 3.f, 0.f, 1.f);
		if (Samples.Num() > Rate * 15) Samples.RemoveAt(0, Frames); // 15 seconds at most
	}, 1024);
	if (!bOk || !Capture->StartStream()) { UE_LOG(LogTemp, Warning, TEXT("HomicideUnit listen: no microphone")); Capture->CloseStream(); return false; }
	bListening = true;
	return true;
}

void UHUListenSubsystem::StopListening(FHUHeard Done)
{
	if (!bListening) { Done.ExecuteIfBound(FString()); return; }
	bListening = false; MicLevel = 0.f;
	Capture->StopStream(); Capture->CloseStream();
	TArray<float> Mono; int32 Rate;
	{ FScopeLock L(&Lock); Mono = MoveTemp(Samples); Rate = CaptureRate; }
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
		FRegexMatcher M(FRegexPattern(TEXT("\"text\"\\s*:\\s*\"([^\"]*)\"")), StdOut + TEXT("\n") + StdErr);
		if (M.FindNext()) Text = M.GetCaptureGroup(1).TrimStartAndEnd();
		if (Code != 0) UE_LOG(LogTemp, Warning, TEXT("HomicideUnit listen: speech-to-text failed (%d) %s"), Code, *StdErr.Right(200));
		AsyncTask(ENamedThreads::GameThread, [Self, Text, Done]()
		{
			if (Self.IsValid()) Self->bWorking = false;
			Done.ExecuteIfBound(Text);
		});
	});
}
