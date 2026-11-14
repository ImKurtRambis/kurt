// THE FILM CAMERA: every view in the game - the desk menu, on foot, the cruiser - is shot through it. One post-process
// volume covering the whole level (spawned on every machine; nothing to place in a map) gives it:
//   exposure  an eye that adapts, but only a little, around the fixed exposure the lights were set up with: a dark
//             street opens up about 3/4 of a stop, a bright precinct closes down as much - the night stays night
//   focus     the lens focuses on what's in the middle of the view (at the desk menu: the thing you point at),
//             eased like a focus puller does it: f/4 on foot, f/2.8 at the desk and in the cruiser, where the
//             rain-streaked window and the city beyond fall soft
//   film      grain, a vignette, a little bloom and colour fringing toward the edges, motion blur
// The colour grade - the noir look itself - is EA-08's: leave colour alone here.
// The renderer under it (Lumen, Nanite, virtual shadow maps, TSR) is set in Config/DefaultEngine.ini.
// Tune live with the console: HU.Film.Fstop <n> (0 = automatic), HU.Film.Grain <0..1>, HU.Film.Off 1.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HUFilmCamera.generated.h"

UCLASS()
class HOMICIDEUNIT_API UHUFilmCamera : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHUFilmCamera, STATGROUP_Tickables); }

	// a view that knows what matters in it (the desk menu: the thing you're pointing at) says where to focus;
	// call it every frame you want it - it lapses after a moment and the centre of the frame takes over again
	void FocusOn(float DistanceCm) { FocusOverride = DistanceCm; OverrideLeft = 0.25f; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

private:
	UPROPERTY() TObjectPtr<class APostProcessVolume> Volume;
	float Focus = 300.f;   // cm, eased toward what the view is looking at
	float FocusOverride = 0.f, OverrideLeft = 0.f;
};
