// A SCREENSHOT TOUR of any level, for checking art: -HUShots=X,Y,Yaw,Pitch|X,Y,Yaw,Pitch|...  [-HUShotsName=Prefix]
// The player is stood at each stop in turn (in the precinct / roll call room's own frame when there is one, else in
// world space), looks along Yaw/Pitch, and a screenshot is saved (Saved/Screenshots/<platform>/<Prefix>_<n>.png).
// The game quits after the last one. Pair a run with -HUNoProps to see the same views with the old blocks.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "HUShotTour.generated.h"

UCLASS()
class HOMICIDEUNIT_API UHUShotTour : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHUShotTour, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return Stops.Num() > 0 && !IsTemplate(); }
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional; }

private:
	TArray<FVector4> Stops; // X, Y, Yaw, Pitch
	FString Prefix = TEXT("Tour");
	float Clock = 0.f;
	int32 Done = 0;
	bool bPlaced = false;
};
