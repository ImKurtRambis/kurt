// Human-sounding voices.
//
// ONE LOCAL VOICE FOR EVERY PLAYER (free, offline, no keys - HUVoiceLocal.cpp): Supertonic 3 (AI\voice\supertonic,
// 139 MB), spoken on the processor by sherpa-onnx's own program (AI\voice\bin\sherpa-onnx-offline-tts.exe). It takes
// no graphics memory - that is the brain's and the game's - and needs no Python and no server: the game runs the
// program once for each line. Ten voices: 0-4 women, 5-9 men. The game sounds the same on every PC.
// (It used to pick bigger voices on a 12 GB NVIDIA card. They are no longer used or shipped: 11 GB of download and
// 6 GB of graphics memory for a game that then sounded different from one PC to the next.)
//
// Without the local voice, a key pasted on the desk computer (Settings) gets a paid voice instead:
//   OpenAI      - gpt-4o-mini-tts, which takes an acting direction ("panicked, breathless...")
//   ElevenLabs  - eleven_v3: emotion tags, a voice per speaker
// With neither (or if a line fails) the line shows as a caption only - no robotic voice.
//
// Audio is raw 24 kHz PCM, played as a procedural sound. "Phone" lines (the 911 call) are a phone call on tape: the
// line's narrow band, then the recorder's waver and hiss (PhoneOnTape).
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
	// the rate every voice is played at
	static constexpr int32 SampleRate = 24000;

	// true when lines can be spoken: the local voice is installed, or a paid provider and its key are set
	UFUNCTION(BlueprintPure, Category = "Voice") bool HasNeuralVoice() const;
	UFUNCTION(BlueprintPure, Category = "Voice") static FString ProviderName(EHUVoiceProvider P);
	// the key in use (pasted in game, ELEVENLABS_API_KEY, or ElevenLabsKey.txt in the project folder)
	FString Key() const;
	EHUVoiceProvider Provider() const;

	// Ask for a line. Voice: which voice (the same number is the same voice; the local voice has ten: 0-4 women,
	// 5-9 men). Direction: how to say it (OpenAI only). bPhone: a phone call on tape. Speed, Emotion: paid voices only.
	// bBackground: not needed right now (premade lines) - made while nothing is wanted, and kept on disk.
	void Synthesize(const FString& Text, int32 Voice, const FString& Direction, bool bPhone, FHUVoiceReady Done, float Speed = 1.f, float Emotion = 0.5f, bool bBackground = false);
	// gets the first line's one-off cost (Windows reading the voice off the disk) out of the way; done as the game starts
	void WarmUp();
	// the local voice is installed (AI\voice) - and, there being nothing to load, ready
	UFUNCTION(BlueprintPure, Category = "Voice") bool HasLocalVoice() const;
	UFUNCTION(BlueprintPure, Category = "Voice") bool IsLocalVoiceReady() const { return HasLocalVoice(); }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 24 kHz 16-bit mono samples -> a sound that can be played once (bPhone: through PhoneOnTape first)
	class USoundWave* MakeSound(TArray<uint8>& Pcm16, bool bPhone, float& OutSeconds);
	// Turns clean speech (24 kHz, in place) into a phone call played off a tape: only the 300-3400 Hz a phone line
	// carries, the handset's grit, then the deck - its speed wavering a little, hiss under everything.
	static void PhoneOnTape(int16* Samples, int32 Num);

	// LINES KEPT, AND LINES MADE AHEAD (the local voice). Every line it says is kept in memory (the oldest go once
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

	// ---- the local voice (HUVoiceLocal.cpp) ----
	// What a line is for. WantedNow: someone is waiting to hear it. Premade: for later, and kept on disk (a suspect's
	// stalling lines - the same every session). Ahead: for later, kept in memory only (MakeAhead).
	enum : uint8 { WantedNow, Premade, Ahead };
	struct FAsk { uint8 Need = WantedNow; FHUVoiceReady Done; };
	// one line to say; everyone who asked for the same words in the same voice shares the one saying of it
	struct FLine { FString Text, Key; int32 Voice = 0; bool bPhone = false, bSpeaking = false; double AskedAt = 0.0; TArray<FAsk> Asks; bool Wanted() const; };
	void SynthesizeLocal(const FString& Text, int32 Voice, bool bPhone, uint8 Need, FHUVoiceReady Done);
	TArray<FLine> Queue;     // being spoken or waiting to be, in the order asked
	int32 LineCount = 0;     // (names the sound files)
	bool bWarm = false;
	int32 Failures = 0;      // different lines in a row the voice program couldn't say
	FString LastFailed;
	bool bBroken = false;    // it doesn't run on this PC: no more tries this session, lines are captions
	void SpeakNext();
	void Spoke(const FString& Key, TArray<uint8>& Pcm, int32 Code, float Took);
	void VoiceTest();        // -HUVoiceTest
	// kept sound (24 kHz 16-bit mono) by voice and words
	TMap<FString, TArray<uint8>> Kept;
	TArray<FString> KeptOrder; // oldest first
	int64 KeptBytes = 0;
	int32 ServedKept = 0;
	static FString Plain(const FString& Text); // the words as the voice program can be given them: plain letters, no tags
	static FString KeptKey(const FString& Text, int32 Voice, bool bPhone);
	void KeepSound(const FString& Key, const TArray<uint8>& Pcm);
	static FString DiskFile(const FString& Key); // where a premade line is kept between sessions
};
