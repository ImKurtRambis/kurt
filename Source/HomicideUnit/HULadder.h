// A climbable ladder. Walk into it and you grab on: W climbs (looking down, W climbs down; S reverses),
// step off the top onto the ledge, or walk away at the bottom to let go.
// The ladder's +X points out from the wall, toward the climber.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HULadder.generated.h"

class ACharacter;

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHULadder : public AActor
{
	GENERATED_BODY()
public:
	AHULadder();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UBoxComponent* Grab;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float Height = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float ClimbSpeed = 260.f;
	// optional: a real ladder mesh from Fab, stretched to Height
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") UStaticMesh* LadderMesh = nullptr;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	TArray<TWeakObjectPtr<ACharacter>> Climbers;
	TMap<TWeakObjectPtr<ACharacter>, float> SavedBraking, SavedSpeed;

	UFUNCTION() void OnEnter(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Body, bool bSweep, const FHitResult& Hit);
	UFUNCTION() void OnLeave(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Body);
};
