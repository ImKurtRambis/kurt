// The precinct motor pool: a concrete garage with three cruisers nose-out toward the roll-up doors.
// The briefing room builds one out its back door; you can also drop one in a level on its own.
// Its doorway is in the +X wall, at the middle.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUMotorPool.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUMotorPool : public AActor
{
	GENERATED_BODY()
public:
	AHUMotorPool();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* BaySign;
	UPROPERTY(VisibleAnywhere) TArray<class UChildActorComponent*> Cars;
	// the supply locker, by the door in from the corridor
	UPROPERTY(VisibleAnywhere) class UChildActorComponent* Locker;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
};
