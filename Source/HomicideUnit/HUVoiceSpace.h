// WHERE A VOICE IS HEARD FROM. A spoken line is said at a spot in the world - a suspect's mouth, the tape on the
// briefing lectern - and what reaches each player depends on where they stand:
//   how far      full across a table, quieter across the room, faint down the corridor (and a little duller far off)
//   which side   it comes from where the speaker is
//   what's between  glass dulls it some; a wall or anything else solid dulls it a lot. The speaker's own body, other
//                people and furniture don't count: the straight line and three beside it are tried and the
//                clearest one wins, because sound gets round a chair but not round a wall.
// Every player's PC speaks the lines itself (UHUVoiceSubsystem), so each hears them from where they are standing.
//
// -HUVoiceSpaceTest (not in Shipping): on any level, checks the falloff by distance, builds a wall, a pane of glass
// and a monitor to check what counts as in the way, checks which pitches the phone-on-tape sound keeps, finds the
// mouths of the people in the level and, when the run has sound, says a line and plays a tape too quiet to hear.
// The log has a line for each check ("HUVoiceSpaceTest: ok ..."), then "all checks passed", and the game quits.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HUVoiceSpace.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;

UCLASS()
class HOMICIDEUNIT_API UHUVoiceSpace : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	// what is speaking: a person (carries across a room), or a recording played to a room (carries further)
	enum class EFrom : uint8 { Person, Device };
	// what stands between an ear and a voice
	enum class EBetween : uint8 { Nothing, Glass, Solid };

	// Says Sound at WorldAt, carried by On (and its Bone, if it has one) from then on. Through: actors the sound passes
	// straight through (the speaker's own body and collision). Returns the playing sound - Stop() it to cut them off -
	// or null when this PC has no sound.
	UAudioComponent* Say(USoundBase* Sound, USceneComponent* On, FName Bone, const FVector& WorldAt, const TArray<AActor*>& Through, EFrom From = EFrom::Person);

	// A person's mouth (HUPeople): their body's head bone carries it, sitting or standing, and where it is now. False if
	// there's no head to speak from.
	static bool Mouth(AActor* Look, USceneComponent*& OutOn, FName& OutBone, FVector& OutAt);

	// what's between an ear and a voice right now (the clearest of four lines: straight, and 30 cm above and to each side)
	EBetween Between(const FVector& Ear, const FVector& Voice, const TArray<TWeakObjectPtr<AActor>>& Through) const;
	// how loud (0..1) a voice is from distance alone, and how much of it is left behind glass or something solid
	float LoudnessAt(float Centimetres, EFrom From = EFrom::Person);
	static float Dulled(EBetween B, float& OutLowPassHz);

	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return Voices.Num() > 0; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHUVoiceSpace, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }

private:
	struct FVoice
	{
		TWeakObjectPtr<UAudioComponent> Audio;
		TArray<TWeakObjectPtr<AActor>> Through;
		float Loud = 1.f, LowPass = 20000.f; // what's applied now; it eases toward what Between() says
	};
	TArray<FVoice> Voices;
	UPROPERTY() TObjectPtr<USoundAttenuation> PersonFalloff;
	UPROPERTY() TObjectPtr<USoundAttenuation> DeviceFalloff;
	USoundAttenuation* Falloff(EFrom From);
	bool Ear(FVector& OutEar) const; // where this PC's player is listening from (their camera)
#if !UE_BUILD_SHIPPING
	void RunTest();
#endif
};
