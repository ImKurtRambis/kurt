#include "HUVoiceSubsystem.h"
#include "HUCaseSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Sound/SoundWaveProcedural.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "Misc/App.h"

static const int32 Rate = 24000;
// OpenAI voices, and ElevenLabs premade voices (Rachel, Adam, Antoni, Bella, Arnold, Josh, Elli, Sam)
static const TCHAR* OpenAIVoices[] = { TEXT("coral"), TEXT("ash"), TEXT("ballad"), TEXT("sage"), TEXT("verse"), TEXT("echo"), TEXT("shimmer"), TEXT("onyx") };
static const TCHAR* ElevenVoices[] = { TEXT("21m00Tcm4TlvDq8ikWAM"), TEXT("pNInz6obpgDQGcFmaJgB"), TEXT("ErXwobaYiN019PkySvjV"), TEXT("EXAVITQu4vr4xnSDxaCY"),
	TEXT("VR6AewLTigWG4xSOukaG"), TEXT("TxGEqnHWrfWFTfGW9XjX"), TEXT("MF3mGyEYCl7XYWbV9V6O"), TEXT("yoZ06aMxZJJ28mfd3POQ") };

FString UHUVoiceSubsystem::ProviderName(EHUVoiceProvider P)
{
	switch (P) { case EHUVoiceProvider::OpenAI: return TEXT("OpenAI (natural)"); case EHUVoiceProvider::ElevenLabs: return TEXT("ElevenLabs (natural)"); default: return TEXT("Off (captions only)"); }
}

// The key, from (in order): the desk computer (pasted in game), the ELEVENLABS_API_KEY environment variable,
// or ElevenLabsKey.txt in the project folder (first line that isn't a # comment).
FString UHUVoiceSubsystem::Key() const
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (C && C->VoiceKey.Len() > 10) return C->VoiceKey;
	const FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("ELEVENLABS_API_KEY")).TrimStartAndEnd();
	if (Env.Len() > 10) return Env;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToStringArray(Lines, *(FPaths::ProjectDir() / TEXT("ElevenLabsKey.txt"))))
		for (FString L : Lines) { L.TrimStartAndEndInline(); if (L.Len() > 10 && !L.StartsWith(TEXT("#")) && !L.Contains(TEXT(" "))) return L; }
	return FString();
}

EHUVoiceProvider UHUVoiceSubsystem::Provider() const
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	return C ? (EHUVoiceProvider)C->VoiceProvider : EHUVoiceProvider::ElevenLabs;
}

bool UHUVoiceSubsystem::HasNeuralVoice() const
{
	return HasLocalVoice() || (Provider() != EHUVoiceProvider::Windows && Key().Len() > 10);
}

void UHUVoiceSubsystem::Synthesize(const FString& Text, int32 Voice, const FString& Direction, bool bPhone, FHUVoiceReady Done, float Speed, float Emotion, bool bBackground)
{
	if (HasLocalVoice()) { SynthesizeLocal(Text, Voice, bPhone, bBackground, Done, 0); return; }
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!HasNeuralVoice() || !C) { Done.ExecuteIfBound(nullptr, 0.f); return; }
	const EHUVoiceProvider P = Provider();
	const FString K = Key();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetTimeout(30.f);
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (P == EHUVoiceProvider::OpenAI)
	{
		Req->SetURL(TEXT("https://api.openai.com/v1/audio/speech"));
		Req->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + K);
		Body->SetStringField(TEXT("model"), TEXT("gpt-4o-mini-tts"));
		Body->SetStringField(TEXT("voice"), OpenAIVoices[FMath::Abs(Voice) % UE_ARRAY_COUNT(OpenAIVoices)]);
		Body->SetStringField(TEXT("input"), Text);
		if (!Direction.IsEmpty()) Body->SetStringField(TEXT("instructions"), Direction);
		Body->SetStringField(TEXT("response_format"), TEXT("pcm")); // raw 24 kHz 16-bit mono
	}
	else
	{
		Req->SetURL(FString::Printf(TEXT("https://api.elevenlabs.io/v1/text-to-speech/%s?output_format=pcm_24000"), ElevenVoices[FMath::Abs(Voice) % UE_ARRAY_COUNT(ElevenVoices)]));
		Req->SetHeader(TEXT("xi-api-key"), K);
		// Eleven v3: the most expressive model, and it acts on inline audio tags - so the direction becomes tags
		const bool bPanicked = Direction.Contains(TEXT("terrified")) || Direction.Contains(TEXT("panic"));
		const bool bCalm = Direction.Contains(TEXT("Calm")) || Direction.Contains(TEXT("calm"));
		const FString Tags = bPanicked ? TEXT("[panicked] [out of breath] ") : bCalm ? TEXT("[calm] [professional] ") : TEXT("");
		Body->SetStringField(TEXT("text"), Tags + Text);
		Body->SetStringField(TEXT("model_id"), TEXT("eleven_v3"));
		TSharedRef<FJsonObject> Settings = MakeShared<FJsonObject>();
		Settings->SetNumberField(TEXT("stability"), bPanicked ? 0.0 : 0.5); // v3: 0 Creative (most emotion), 0.5 Natural
		Settings->SetNumberField(TEXT("similarity_boost"), 0.8);
		Body->SetObjectField(TEXT("voice_settings"), Settings);
	}
	FString Json;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Body, W);
	Req->SetContentAsString(Json);
	TWeakObjectPtr<UHUVoiceSubsystem> Self(this);
	Req->OnProcessRequestComplete().BindLambda([Self, Done, bPhone](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (!Self.IsValid()) return;
		if (!bOk || !Resp.IsValid() || Resp->GetResponseCode() != 200 || Resp->GetContent().Num() < 1000)
		{
			UE_LOG(LogTemp, Warning, TEXT("HomicideUnit voice: request failed (%d) %s"), Resp.IsValid() ? Resp->GetResponseCode() : -1, Resp.IsValid() ? *Resp->GetContentAsString().Left(200) : TEXT(""));
			Done.ExecuteIfBound(nullptr, 0.f);
			return;
		}
		TArray<uint8> Pcm = Resp->GetContent();
		float Seconds = 0.f;
		USoundWave* S = Self->MakeSound(Pcm, bPhone, Seconds);
		Done.ExecuteIfBound(S, Seconds);
	});
	Req->ProcessRequest();
}

USoundWave* UHUVoiceSubsystem::MakeSound(TArray<uint8>& Pcm16, bool bPhone, float& OutSeconds)
{
	const int32 N = Pcm16.Num() / 2;
	int16* S = reinterpret_cast<int16*>(Pcm16.GetData());
	if (bPhone)
	{
		// a telephone line: roughly 300 Hz - 3.4 kHz, a little gain and grit
		const float Dt = 1.f / Rate;
		const float RcHp = 1.f / (2.f * PI * 300.f), RcLp = 1.f / (2.f * PI * 3400.f);
		const float AHp = RcHp / (RcHp + Dt), ALp = Dt / (RcLp + Dt);
		float PrevIn = 0.f, Hp = 0.f, Lp = 0.f;
		for (int32 i = 0; i < N; i++)
		{
			const float In = S[i] / 32768.f;
			Hp = AHp * (Hp + In - PrevIn); PrevIn = In;
			Lp = Lp + ALp * (Hp - Lp);
			float Out = FMath::Clamp(Lp * 1.6f, -1.f, 1.f);
			Out = Out - (Out * Out * Out) / 3.f; // soft clip
			S[i] = (int16)FMath::Clamp(Out * 32767.f, -32768.f, 32767.f);
		}
	}
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(Rate);
	W->NumChannels = 1;
	W->Duration = (float)N / Rate;
	W->SoundGroup = SOUNDGROUP_Voice;
	W->bLooping = false;
	W->QueueAudio(Pcm16.GetData(), Pcm16.Num());
	Keep.Add(W);
	if (Keep.Num() > 40) Keep.RemoveAt(0);
	OutSeconds = W->Duration;
	return W;
}

// a .wav file -> 16-bit mono samples (the local voices write 24 kHz, the rate we play at)
static bool WavToPcm16(const TArray<uint8>& Wav, TArray<uint8>& Out)
{
	if (Wav.Num() < 44 || FMemory::Memcmp(Wav.GetData(), "RIFF", 4) != 0) return false;
	int32 Channels = 1, Bits = 16, Format = 1;
	int32 P = 12;
	while (P + 8 <= Wav.Num())
	{
		const int32 Size = *reinterpret_cast<const int32*>(Wav.GetData() + P + 4);
		if (FMemory::Memcmp(Wav.GetData() + P, "fmt ", 4) == 0)
		{
			Format = *reinterpret_cast<const int16*>(Wav.GetData() + P + 8);
			Channels = *reinterpret_cast<const int16*>(Wav.GetData() + P + 10);
			Bits = *reinterpret_cast<const int16*>(Wav.GetData() + P + 22);
		}
		else if (FMemory::Memcmp(Wav.GetData() + P, "data", 4) == 0)
		{
			const int32 Len = FMath::Min(Size, Wav.Num() - P - 8);
			const uint8* D = Wav.GetData() + P + 8;
			const int32 Frames = Len / (Bits / 8) / FMath::Max(1, Channels);
			Out.SetNumUninitialized(Frames * 2);
			int16* O = reinterpret_cast<int16*>(Out.GetData());
			for (int32 i = 0; i < Frames; i++)
			{
				const int32 Idx = i * Channels;
				O[i] = (Format == 3 && Bits == 32) ? (int16)FMath::Clamp(reinterpret_cast<const float*>(D)[Idx] * 32767.f, -32768.f, 32767.f)
					: reinterpret_cast<const int16*>(D)[Idx];
			}
			return Frames > 0;
		}
		P += 8 + Size + (Size & 1);
	}
	return false;
}

// ---------- the local voices (free, offline, no keys): Tools\VoiceServer.py on port 8005 ----------
//   Orpheus 3B - the 911 call and tape audio (phone lines); Sesame CSM-1B - suspects and everyone else.
static FString ProjectFile(const TCHAR* Rel) { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Rel); }

bool UHUVoiceSubsystem::HasLocalVoice() const
{
	IPlatformFile& F = FPlatformFileManager::Get().GetPlatformFile();
	return F.FileExists(*ProjectFile(TEXT("AI/python/python.exe"))) && F.FileExists(*ProjectFile(TEXT("Tools/VoiceServer.bat")))
		&& (F.FileExists(*ProjectFile(TEXT("AI/orpheus/orpheus-3b-q4_k_m.gguf"))) || F.FileExists(*ProjectFile(TEXT("AI/csm/model/model.safetensors"))));
}

void UHUVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// load the voices as the game starts, so they're warm by the first 911 call
	if (!IsRunningCommandlet() && !FApp::IsUnattended()) WarmUp();
}

void UHUVoiceSubsystem::Deinitialize()
{
	if (ServerProc.IsValid()) { FPlatformProcess::TerminateProc(ServerProc, true); FPlatformProcess::CloseProc(ServerProc); }
	Super::Deinitialize();
}

void UHUVoiceSubsystem::WarmUp()
{
	if (bStarting || (bOrpheusReady && bCsmReady) || !HasLocalVoice()) return;
	bStarting = true; StartedAt = FPlatformTime::Seconds();
	Ping(true);
}

// ask the server what's loaded; the first time nobody answers, start it
void UHUVoiceSubsystem::Ping(bool bFirst)
{
	if (FPlatformTime::Seconds() - StartedAt > 300.0) { bStarting = false; UE_LOG(LogTemp, Warning, TEXT("HomicideUnit voice: the local voices never loaded")); return; }
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(TEXT("http://127.0.0.1:8005/health"));
	Req->SetVerb(TEXT("GET"));
	Req->SetTimeout(2.f);
	TWeakObjectPtr<UHUVoiceSubsystem> Self(this);
	Req->OnProcessRequestComplete().BindLambda([Self, bFirst](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (!Self.IsValid()) return;
		TSharedPtr<FJsonObject> J;
		if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), J) && J.IsValid())
		{
			const bool bWasO = Self->bOrpheusReady, bWasC = Self->bCsmReady;
			Self->bOrpheusReady = J->GetBoolField(TEXT("orpheus"));
			Self->bCsmReady = J->GetBoolField(TEXT("csm"));
			if (Self->bOrpheusReady && !bWasO) UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: Orpheus ready"));
			if (Self->bCsmReady && !bWasC) UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: CSM ready"));
			if (Self->bOrpheusReady && Self->bCsmReady) { Self->bStarting = false; return; }
		}
		else if (bFirst && !Self->ServerProc.IsValid())
		{
			const FString Bat = ProjectFile(TEXT("Tools/VoiceServer.bat"));
			// the server quits when this process ends, even after a crash
			FPlatformMisc::SetEnvironmentVar(TEXT("HU_PARENT_PID"), *FString::Printf(TEXT("%u"), FPlatformProcess::GetCurrentProcessId()));
			Self->ServerProc = FPlatformProcess::CreateProc(TEXT("cmd.exe"), *FString::Printf(TEXT("/c \"\"%s\"\""), *Bat), false, true, true, nullptr, 0, *FPaths::GetPath(Bat), nullptr);
			UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: starting the local voices (%s)"), Self->ServerProc.IsValid() ? TEXT("ok") : TEXT("failed"));
		}
		FTimerHandle H;
		if (UGameInstance* GI = Self->GetGameInstance())
			GI->GetTimerManager().SetTimer(H, FTimerDelegate::CreateUObject(Self.Get(), &UHUVoiceSubsystem::Ping, false), 1.f, false);
	});
	Req->ProcessRequest();
}

void UHUVoiceSubsystem::SynthesizeLocal(const FString& Text, int32 Voice, bool bPhone, bool bBackground, FHUVoiceReady Done, int32 Tries)
{
	// phone lines (the 911 call, tape audio) are Orpheus; everyone else is CSM. Either stands in if the other isn't loaded.
	const bool bOrpheus = bPhone ? (bOrpheusReady || !bCsmReady) : (!bCsmReady && bOrpheusReady);
	if (!bOrpheusReady && !bCsmReady)
	{
		// still loading (about a minute after the game starts): wait for it rather than go silent
		WarmUp();
		if (Tries > 120 || !bStarting) { Done.ExecuteIfBound(nullptr, 0.f); return; }
		TWeakObjectPtr<UHUVoiceSubsystem> Self(this);
		FTimerHandle H;
		GetGameInstance()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self, Text, Voice, bPhone, bBackground, Done, Tries]()
		{
			if (Self.IsValid()) Self->SynthesizeLocal(Text, Voice, bPhone, bBackground, Done, Tries + 1);
		}), 1.f, false);
		return;
	}
	FString Clean = Text.Replace(TEXT("*"), TEXT("")).Replace(TEXT("\n"), TEXT(" "));
	Clean.TrimStartAndEndInline();
	if (Clean.IsEmpty()) { Done.ExecuteIfBound(nullptr, 0.f); return; }
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("engine"), bOrpheus ? TEXT("orpheus") : TEXT("csm"));
	Body->SetStringField(TEXT("text"), Clean);
	Body->SetNumberField(TEXT("voice"), FMath::Abs(Voice));
	Body->SetBoolField(TEXT("background"), bBackground);
	FString Json;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(TEXT("http://127.0.0.1:8005/tts"));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(Json);
	Req->SetTimeout(bBackground ? 600.f : 120.f);
	TWeakObjectPtr<UHUVoiceSubsystem> Self(this);
	Req->OnProcessRequestComplete().BindLambda([Self, Done, bPhone](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (!Self.IsValid()) return;
		TArray<uint8> Pcm;
		if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200 && WavToPcm16(Resp->GetContent(), Pcm))
		{
			float Seconds = 0.f;
			USoundWave* S = Self->MakeSound(Pcm, bPhone, Seconds);
			Done.ExecuteIfBound(S, Seconds);
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("HomicideUnit voice: local voice failed (%d) %s"), Resp.IsValid() ? Resp->GetResponseCode() : -1, Resp.IsValid() ? *Resp->GetContentAsString().Left(200) : TEXT(""));
		Done.ExecuteIfBound(nullptr, 0.f);
	});
	Req->ProcessRequest();
}
