// A piece of evidence. No bright markers: it's small and easy to walk past, and how it's concealed decides
// what you need to find or collect it (see EHUConceal): a flashlight for the dark, UV light for traces,
// the print kit for fingerprints, the crowbar for strongboxes. E examines and bags it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUTools.h"
#include "HUEvidence.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUEvidence : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUEvidence();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clue") FString ClueId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clue") int32 MarkerNumber = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clue") EHUConceal Conceal = EHUConceal::Plain;
	// optional: a real prop from Fab for the evidence itself (used for plain and dark evidence)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clue") UStaticMesh* Mesh = nullptr;

	// under furniture: invisible and untouchable until the furniture's moved
	UFUNCTION(BlueprintCallable, Category = "Clue") void Hide();
	UFUNCTION(BlueprintCallable, Category = "Clue") void Reveal();

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual bool Highlights() const override { return false; }
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() UStaticMeshComponent* Lid = nullptr;
	bool bCovered = false;  // under furniture
	bool bSpotted = false;  // dark / trace evidence caught in a beam (stays found)
	bool bOpened = false;   // strongbox pried open
	void Show(bool bVisible);
	class UHUCaseSubsystem* Cases() const;
};
