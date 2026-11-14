// A squad car. Once the Captain's briefing is done, the whole squad gathers at a cruiser and presses E:
// the screen fades out and everyone travels to the crime scene level together.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUCruiser.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUCruiser : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUCruiser();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UPointLightComponent* RedLight;
	UPROPERTY(VisibleAnywhere) class UPointLightComponent* BlueLight;

	// the level every player travels to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cruiser") FName SceneLevel = TEXT("L_Scene");
	// how close to the car each detective has to be before it can leave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cruiser") float GatherRadius = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cruiser") float DriveSeconds = 2.5f;
	// false for the patrol cars at a scene: they're not yours to drive
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cruiser") bool bPlayerCar = true;
	// where it goes, for the prompt (empty: the case's location)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cruiser") FString DestinationLabel;
	// a trip BACK to an open case from the precinct (the motor pool door): no briefing needed, just an open case
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cruiser") bool bReturnTrip = false;

	// how many detectives are in the game, and how many are at the car
	UFUNCTION(BlueprintPure, Category = "Cruiser") void CountSquad(int32& AtCar, int32& Total) const;
	// fade everyone out and travel to SceneLevel
	UFUNCTION(BlueprintCallable, Category = "Cruiser") void Depart();

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	bool bLeaving = false;
private:
	float Flash = 0.f;
	FTimerHandle TravelTimer;
	void Travel();
	bool Ready(FString* Why = nullptr) const;
};
