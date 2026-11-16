// Human-sounding voices. With a key pasted on the desk computer (Settings), lines are spoken by a neural voice:
//   OpenAI      - gpt-4o-mini-tts, which takes an acting direction ("panicked, breathless...")
//   ElevenLabs  - eleven_v3, the most expressive model available: emotion tags, a voice per speaker  (recommended)
// LOCAL (free, offline, no keys - preferred when installed): Tools\VoiceServer.py on port 8005, started as the game starts.
//   On an NVIDIA card with 12 GB or more: Orpheus 3B speaks phone lines (the 911 call, tape audio), Sesame CSM-1B
//   speaks suspects and everyone else (models in AI\orpheus, AI\snac, AI\csm).
//   On any other PC (AMD, Intel, a smaller NVIDIA card): Supertonic speaks them all, on the processor
//   (AI\voice\supertonic, run by sherpa-onnx). The server picks by itself; -HUVoices=cuda|cpu forces one.
//   Its Python is in AI\python (see AI\README.txt).
// Without a local voice or a key (or if a request fails) the line shows as a caption only - no robotic voice.
// Audio comes back as raw 24 kHz PCM and plays as a procedural sound. "Phone" lines (the 911 call) are a phone call
// on tape: the line's narrow band, then the recorder's waver and hiss (PhoneOnTape).
// This only makes the sound. Saying it somewhere - a suspect's mouth, the tape on the lectern - is UHUVoiceSpace.
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
	// bBackground: not needed right now (premade lines) - kept on disk once made, and waits behind real answers.
	void Synthesize(const FString& Text, int32 Voice, const FString& Direction, bool bPhone, FHUVoiceReady Done, float Speed = 1.f, float Emotion = 0.5f, bool bBackground = false);
	// start loading the local voices (done as the game starts; about a minute)
	void WarmUp();
	UFUNCTION(BlueprintPure, Category = "Voice") bool IsLocalVoiceReady() const { return bOrpheusReady || bCsmReady; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// the free offline voices are installed
	UFUNCTION(BlueprintPure, Category = "Voice") bool HasLocalVoice() const;

	// 24 kHz 16-bit mono samples -> a sound that can be played once (bPhone: through PhoneOnTape first)
	class USoundWave* MakeSound(TArray<uint8>& Pcm16, bool bPhone, float& OutSeconds);
	// Turns clean speech (24 kHz, in place) into a phone call played off a tape: only the 300-3400 Hz a phone line
	// carries, the handset's grit, then the deck - its speed wavering a little, hiss under everything.
	static void PhoneOnTape(int16* Samples, int32 Num);

	// LINES KEPT, AND LINES MADE AHEAD (the local voices). Every line a local voice says is kept (the oldest go once
	// there's about 20 minutes of them), so the same words in the same voice asked for again (Synthesize) play at
	// once: written lines come round again. MakeAhead has a line said before anyone asks for it - one at a time, and
	// only while nobody is waiting on a line.
	void MakeAhead(const FString& Text, int32 Voice);
	bool HasAhead(const FString& Text, int32 Voice) const;
	// how many lines were asked for and found already made (played with no wait)
	int32 ServedFromKept() const { return ServedKept; }
	// a kept line as a new sound, right now (null if it isn't kept). A sound plays once: ask again to play it again.
	class USoundWave* FromKept(const FString& Text, int32 Voice, float& OutSeconds);

private:
	UPROPERTY() TArray<class USoundWaveProcedural*> Keep; // stop the GC taking sounds while they play
	void SynthesizeLocal(const FString& Text, int32 Voice, bool bPhone, bool bBackground, FHUVoiceReady Done, int32 Tries, bool bAhead = false);
	// kept sound (24 kHz 16-bit mono) by voice and words
	TMap<FString, TArray<uint8>> Kept;
	TArray<FString> KeptOrder; // oldest first
	int64 KeptBytes = 0;
	static FString KeptKey(const FString& Text, int32 Voice, bool bPhone);
	void KeepSound(const FString& Key, const TArray<uint8>& Pcm);
	TArray<TPair<FString, int32>> AheadQueue; // (words, voice) waiting their turn
	bool bAheadBusy = false;
	int32 Waiting = 0;         // lines someone is waiting on right now: they go first
	int32 ServedKept = 0;
	FTimerHandle AheadTimer;
	void PumpAhead();
	FProcHandle ServerProc; bool bStarting = false, bOrpheusReady = false, bCsmReady = false; double StartedAt = 0.0;
	bool bLoaded = false; // the server has finished loading whatever this PC can run
	void Ping(bool bFirst);
};
