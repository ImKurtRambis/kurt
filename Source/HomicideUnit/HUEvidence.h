// A piece of evidence. No bright markers: it's small and easy to walk past, and how it's concealed decides
// what you need to find or collect it (see EHUConceal): a flashlight for the dark, UV light for traces,
// the print kit for fingerprints, the crowbar for strongboxes. E examines and bags it.
// Dark and trace pieces are only there while a beam is on them: one detective holds the light, another bags it.
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
	// whose light is on it right now (dark / trace only), or null
	APawn* LitByPawn() const;

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual bool Highlights() const override { return false; }
	virtual float UseTime(APawn* User, bool bSecondary) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() UStaticMeshComponent* Lid = nullptr;
	bool bCovered = false;  // under furniture
	// dark / trace evidence only shows WHILE a light is on it (anyone's - you can see a partner's beam).
	// Bagging it with the light in your own other hand is slow; with a partner holding the light it's quick.
	TWeakObjectPtr<APawn> LitBy;
	float LitUntil = -1.f;
	bool bOpened = false;   // strongbox pried open
	void Show(bool bVisible);
	class UHUCaseSubsystem* Cases() const;
};
