// The game's sound: one-shot effects, footsteps, room ambience and the music bed.
// The files are synthesized by Tools/MakeSounds.py and imported to /Game/Audio by Tools/ImportSounds.py.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GameFramework/Actor.h"
#include "HUSound.generated.h"

namespace HUSound
{
	// Plays /Game/Audio/<Name> at a spot for every player. Call it where the action happens (usually the server);
	// in a networked game it spawns a short-lived replicated cue so squadmates hear it too.
	HOMICIDEUNIT_API void Play(const UObject* WorldContext, FName Name, const FVector& At, float Volume = 1.f);
	// Plays /Game/Audio/<Name> on this machine only, not positioned (menu clicks).
	HOMICIDEUNIT_API void PlayLocal2D(const UObject* WorldContext, FName Name, float Volume = 1.f);
}

// A sound event that replicates: spawned on the server, it plays where it lands on every machine and then goes away.
UCLASS(NotPlaceable)
class HOMICIDEUNIT_API AHUSoundCue : public AActor
{
	GENERATED_BODY()
public:
	AHUSoundCue();
	UPROPERTY(Replicated) FName Sound;
	UPROPERTY(Replicated) float Volume = 1.f;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};

// Starts each map's ambience and music, and plays footsteps for every walking character.
UCLASS()
class HOMICIDEUNIT_API UHUSoundWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHUSoundWorld, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }
private:
	TMap<TWeakObjectPtr<AActor>, float> StepDist;
	TArray<TObjectPtr<class UAudioComponent>> Loops;
	TSharedPtr<class IInputProcessor> Clicks;
	int32 LastStep = 0;
};
