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
#include "HUProcess.h"

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

// A phone call played off a tape (the 911 call). Clean speech goes through what such a recording goes through:
//   the line     only 300-3400 Hz gets down a phone line: the bass and the top are each cut twice over
//                (24 dB an octave), leaving the middle - where the words are - as it was
//   the handset  turned up into a little grit
//   the deck     its speed wavers (a slow wow, a faster flutter), and there is hiss under everything
void UHUVoiceSubsystem::PhoneOnTape(int16* S, int32 N)
{
	if (!S || N <= 0) return;
	const float Dt = 1.f / Rate;
	// a standard second-order filter (the "audio EQ cookbook" one), flat up to its corner
	struct FCut
	{
		float B0, B1, B2, A1, A2, X1 = 0.f, X2 = 0.f, Y1 = 0.f, Y2 = 0.f;
		FCut(bool bBassOut, float Hz, float SampleRate)
		{
			const float W = 2.f * PI * Hz / SampleRate, C = FMath::Cos(W), Alpha = FMath::Sin(W) / (2.f * 0.7071f), A0 = 1.f + Alpha;
			B0 = (bBassOut ? (1.f + C) : (1.f - C)) / 2.f / A0; B2 = B0;
			B1 = (bBassOut ? -(1.f + C) : (1.f - C)) / A0;
			A1 = -2.f * C / A0; A2 = (1.f - Alpha) / A0;
		}
		float Run(float X) { const float Y = B0 * X + B1 * X1 + B2 * X2 - A1 * Y1 - A2 * Y2; X2 = X1; X1 = X; Y2 = Y1; Y1 = Y; return Y; }
	};
	FCut Cuts[] = { FCut(true, 300.f, Rate), FCut(true, 300.f, Rate), FCut(false, 3400.f, Rate), FCut(false, 3400.f, Rate) };
	TArray<float> Line;
	Line.SetNumUninitialized(N);
	for (int32 i = 0; i < N; i++)
	{
		float X = S[i] / 32768.f;
		for (FCut& C : Cuts) X = C.Run(X);
		const float Out = FMath::Clamp(X * 1.8f, -1.f, 1.f); // louder than it went in, as a handset is
		Line[i] = Out - (Out * Out * Out) / 3.f;             // and clipped softly
	}
	FRandomStream Hiss(N); // the same line always hisses the same way
	const int32 Fade = FMath::Max(1, FMath::Min(N / 2, Rate / 30)); // the hiss comes in and goes out over 1/30 s, not with a click
	for (int32 i = 0; i < N; i++)
	{
		// the tape runs a touch fast, then slow: read the line a wavering few samples back
		const float T = i * Dt;
		const float Back = 14.f + 10.f * FMath::Sin(2.f * PI * 0.55f * T) + 2.f * FMath::Sin(2.f * PI * 6.1f * T);
		const float At = i - Back;
		const int32 A = FMath::FloorToInt(At);
		const float Out = A < 0 || A + 1 >= N ? 0.f : FMath::Lerp(Line[A], Line[A + 1], At - A);
		const float Edge = FMath::Min(1.f, FMath::Min(i, N - 1 - i) / (float)Fade);
		S[i] = (int16)FMath::Clamp((Out + Hiss.FRandRange(-1.f, 1.f) * 0.006f * Edge) * 32767.f, -32768.f, 32767.f);
	}
}

USoundWave* UHUVoiceSubsystem::MakeSound(TArray<uint8>& Pcm16, bool bPhone, float& OutSeconds)
{
	const int32 N = Pcm16.Num() / 2;
	int16* S = reinterpret_cast<int16*>(Pcm16.GetData());
	if (bPhone) PhoneOnTape(S, N);
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
//   On an NVIDIA card with 12 GB: Orpheus 3B - the 911 call and tape audio (phone lines); Sesame CSM-1B - suspects
//   and everyone else. On any other PC: Supertonic, on the processor, for both. The server picks (see its top).
static FString ProjectFile(const TCHAR* Rel) { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Rel); }

bool UHUVoiceSubsystem::HasLocalVoice() const
{
	if (FParse::Param(FCommandLine::Get(), TEXT("HUNoVoice"))) return false; // measuring the game without the voices
	IPlatformFile& F = FPlatformFileManager::Get().GetPlatformFile();
	return F.FileExists(*ProjectFile(TEXT("AI/python/python.exe"))) && F.FileExists(*ProjectFile(TEXT("Tools/VoiceServer.bat")))
		&& (F.FileExists(*ProjectFile(TEXT("AI/orpheus/orpheus-3b-q4_k_m.gguf"))) || F.FileExists(*ProjectFile(TEXT("AI/csm/model/model.safetensors")))
			|| F.FileExists(*ProjectFile(TEXT("AI/voice/supertonic/tts.json"))));
}

void UHUVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// load the voices as the game starts, so they're warm by the first 911 call
	if (!IsRunningCommandlet() && !FApp::IsUnattended()) WarmUp();
#if !UE_BUILD_SHIPPING
	// -HUVoiceTest: load the local voices, say a 911 call and a few suspect lines the way the game does - twice over,
	// as the first pass pays any one-off costs - log what each line took, and quit. Add -HUVoices=cpu for the small voice.
	if (FParse::Param(FCommandLine::Get(), TEXT("HUVoiceTest")))
	{
		WarmUp();
		struct FLine { const TCHAR* Text; int32 Voice; bool bPhone; };
		static const FLine Lines[] = {
			{ TEXT("Nine one one, what is the address of your emergency?"), 0, true },
			{ TEXT("There's a man on the floor, he's not moving, please, you have to send someone right now!"), 3, true },
			{ TEXT("I was by the coat check most of the night."), 1, false },
			{ TEXT("Fine. Yes. But that's got nothing to do with this."), 2, false },
			{ TEXT("Look, I've told you everything I know. I never went near the office."), 3, false },
			{ TEXT("I don't know what you want me to say. I heard a door slam around ten, and that's all I heard."), 5, false },
		};
		TSharedRef<int32> At = MakeShared<int32>(0);
		TSharedRef<bool> bBusy = MakeShared<bool>(false);
		TSharedRef<FTimerHandle> Tick = MakeShared<FTimerHandle>();
		TWeakObjectPtr<UHUVoiceSubsystem> Self(this);
		GetGameInstance()->GetTimerManager().SetTimer(*Tick, FTimerDelegate::CreateLambda([Self, At, bBusy, Tick]()
		{
			if (!Self.IsValid() || *bBusy) return;
			const bool bGaveUp = !Self->bLoaded && !Self->bStarting, bDone = *At >= 2 * (int32)UE_ARRAY_COUNT(Lines);
			if (!Self->bLoaded && !bGaveUp) return; // still loading
			if (bGaveUp || bDone || !Self->IsLocalVoiceReady())
			{
				if (!bDone) UE_LOG(LogTemp, Error, TEXT("HUVoiceTest: no local voice came up on this PC"));
				Self->GetGameInstance()->GetTimerManager().ClearTimer(*Tick);
				FPlatformMisc::RequestExit(false);
				return;
			}
			const FLine& L = Lines[*At % UE_ARRAY_COUNT(Lines)];
			const int32 Pass = *At / UE_ARRAY_COUNT(Lines) + 1;
			(*At)++;
			*bBusy = true;
			const double Asked = FPlatformTime::Seconds();
			Self->Synthesize(L.Text, L.Voice, FString(), L.bPhone, FHUVoiceReady::CreateLambda([bBusy, Asked, Pass, &L](USoundWave* S, float Seconds)
			{
				UE_LOG(LogTemp, Log, TEXT("HUVoiceTest: pass %d, %s line, voice %d: %.1f s of speech in %.2f s%s"), Pass, L.bPhone ? TEXT("phone") : TEXT("suspect"), L.Voice,
					Seconds, FPlatformTime::Seconds() - Asked, S ? TEXT("") : TEXT("  - FAILED"));
				*bBusy = false;
			}));
		}), 0.25f, true);
	}
#endif
}

void UHUVoiceSubsystem::Deinitialize()
{
	if (ServerProc.IsValid()) { FPlatformProcess::TerminateProc(ServerProc, true); FPlatformProcess::CloseProc(ServerProc); }
	Super::Deinitialize();
}

void UHUVoiceSubsystem::WarmUp()
{
	if (bStarting || bLoaded || !HasLocalVoice()) return;
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
			// "orpheus": phone lines can be spoken; "csm": everyone else can. Which voice does it is the server's
			// choice ("names"): the big ones on an NVIDIA card, the small one on the processor anywhere else.
			const bool bWasO = Self->bOrpheusReady, bWasC = Self->bCsmReady;
			Self->bOrpheusReady = J->GetBoolField(TEXT("orpheus"));
			Self->bCsmReady = J->GetBoolField(TEXT("csm"));
			FString Phone = TEXT("Orpheus"), People = TEXT("CSM");
			if (const TSharedPtr<FJsonObject>* N; J->TryGetObjectField(TEXT("names"), N)) { (*N)->TryGetStringField(TEXT("orpheus"), Phone); (*N)->TryGetStringField(TEXT("csm"), People); }
			if (Self->bOrpheusReady && !bWasO) UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: phone lines ready (%s)"), *Phone);
			if (Self->bCsmReady && !bWasC) UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: suspects' voices ready (%s)"), *People);
			bool bLoading = !(Self->bOrpheusReady && Self->bCsmReady); // (a server from before it said so itself)
			J->TryGetBoolField(TEXT("loading"), bLoading);
			if (!bLoading)
			{
				Self->bStarting = false; Self->bLoaded = true;
				if (!Self->bOrpheusReady && !Self->bCsmReady) UE_LOG(LogTemp, Warning, TEXT("HomicideUnit voice: no local voice runs on this PC - lines show as captions"));
				return;
			}
		}
		else if (bFirst && !Self->ServerProc.IsValid())
		{
			const FString Bat = ProjectFile(TEXT("Tools/VoiceServer.bat"));
			// -HUVoices=cuda|cpu picks the voices instead of the server (testing, or a launch option for a player in trouble)
			FString Force;
			if (FParse::Value(FCommandLine::Get(), TEXT("HUVoices="), Force)) FPlatformMisc::SetEnvironmentVar(TEXT("HU_VOICES"), *Force);
			// the server quits when this process ends, even after a crash (it watches HU_PARENT_PID, and the job ends it too)
			FPlatformMisc::SetEnvironmentVar(TEXT("HU_PARENT_PID"), *FString::Printf(TEXT("%u"), FPlatformProcess::GetCurrentProcessId()));
			FString Shell = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
			if (Shell.IsEmpty()) Shell = TEXT("cmd.exe");
			Self->ServerProc = HUProcess::Launch(Shell, FString::Printf(TEXT("/c \"\"%s\"\""), *Bat), FPaths::GetPath(Bat));
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
	// the sound arrives all at once at the end; Unreal otherwise gives up after 30 s without a byte, which lost the
	// first CSM line of every session (it compiles for longer than that)
	Req->SetActivityTimeout(bBackground ? 600.f : 120.f);
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
