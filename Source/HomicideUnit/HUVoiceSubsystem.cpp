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

static const int32 Rate = UHUVoiceSubsystem::SampleRate;
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
	if (HasLocalVoice()) { SynthesizeLocal(Text, Voice, bPhone, bBackground ? Premade : WantedNow, Done); return; }
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
		FCut(bool bBassOut, float Hz, float PerSecond)
		{
			const float W = 2.f * PI * Hz / PerSecond, C = FMath::Cos(W), Alpha = FMath::Sin(W) / (2.f * 0.7071f), A0 = 1.f + Alpha;
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
