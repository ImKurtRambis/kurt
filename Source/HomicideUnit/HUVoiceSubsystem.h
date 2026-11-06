// Human-sounding voices. With a key pasted on the desk computer (Settings), lines are spoken by a neural voice:
//   OpenAI      - gpt-4o-mini-tts, which takes an acting direction ("panicked, breathless...")
//   ElevenLabs  - eleven_v3, the most expressive model available: emotion tags, a voice per speaker  (recommended)
// LOCAL EMOTIONAL (free, strong GPUs): Chatterbox, via Chatterbox-TTS-Server in AI\chatterbox (port 8004). Started on
//   first use; while it isn't up, Kokoro speaks. Emotion (0..1) sets Chatterbox's exaggeration.
// LOCAL (free, offline - preferred when installed): Kokoro, run by sherpa-onnx on this PC. Files in AI\voice\ (see AI\README.txt).
// Without a local voice or a key (or if a request fails) the line shows as a caption only - no robotic voice.
// Audio comes back as raw 24 kHz PCM and plays as a procedural sound; "phone" lines go through a telephone
// band-pass so a 911 call sounds like a recording.
// The key is stored in the local save file on this PC. A shipped game should proxy these calls through a server.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HUVoiceSubsystem.generated.h"

UENUM(BlueprintType)
enum class EHUVoiceProvider : uint8 { Windows, OpenAI, ElevenLabs };

DECLARE_DELEGATE_TwoParams(FHUVoiceReady, class USoundWave* /*Sound or null on failure*/, float /*Seconds*/);

UCLASS()
class HOMICIDEUNIT_API UHUVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	// true when a neural provider and a key are set
	UFUNCTION(BlueprintPure, Category = "Voice") bool HasNeuralVoice() const;
	UFUNCTION(BlueprintPure, Category = "Voice") static FString ProviderName(EHUVoiceProvider P);
	// the key in use (pasted in game, ELEVENLABS_API_KEY, or ElevenLabsKey.txt in the project folder)
	FString Key() const;
	EHUVoiceProvider Provider() const;

	// Ask for a line. Voice: 0..n picks one of the provider's voices (same number = same voice).
	// Direction: how to say it (OpenAI only). bPhone: telephone band-pass.
	// Speed: 1 normal, >1 faster (nerves), <1 slower (worn out) - the local voice only.
	void Synthesize(const FString& Text, int32 Voice, const FString& Direction, bool bPhone, FHUVoiceReady Done, float Speed = 1.f, float Emotion = 0.5f);
	UFUNCTION(BlueprintPure, Category = "Voice") bool HasChatterbox() const;
	UFUNCTION(BlueprintPure, Category = "Voice") bool IsChatterboxReady() const { return bCbReady; }
	void WarmUpChatterbox();
	virtual void Deinitialize() override;
	// the free offline voice is installed
	UFUNCTION(BlueprintPure, Category = "Voice") bool HasLocalVoice() const;

private:
	UPROPERTY() TArray<class USoundWaveProcedural*> Keep; // stop the GC taking sounds while they play
	class USoundWave* MakeSound(TArray<uint8>& Pcm16, bool bPhone, float& OutSeconds);
	void SynthesizeLocal(const FString& Text, int32 Voice, float Speed, bool bPhone, FHUVoiceReady Done);
	int32 LineCounter = 0;
	FProcHandle CbProc; bool bCbStarting = false, bCbReady = false; double CbStartedAt = 0.0;
	void PingChatterbox();
	void SynthesizeChatterbox(const FString& Text, int32 Voice, float Speed, float Emotion, bool bPhone, FHUVoiceReady Done, int32 Fallback);
};
