// Human-sounding voices. With a key pasted on the desk computer (Settings), lines are spoken by a neural voice:
//   OpenAI      - gpt-4o-mini-tts, which takes an acting direction ("panicked, breathless...")
//   ElevenLabs  - eleven_v3, the most expressive model available: emotion tags, a voice per speaker  (recommended)
// Without a key (or if a request fails) the line shows as a caption only - no robotic voice.
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
	void Synthesize(const FString& Text, int32 Voice, const FString& Direction, bool bPhone, FHUVoiceReady Done);

private:
	UPROPERTY() TArray<class USoundWaveProcedural*> Keep; // stop the GC taking sounds while they play
	class USoundWave* MakeSound(TArray<uint8>& Pcm16, bool bPhone, float& OutSeconds);
};
