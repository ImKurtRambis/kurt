// A piece of evidence. No bright markers: it's small and easy to walk past, and how it's concealed decides
// what you need to find or collect it (see EHUConceal): a flashlight for the dark, UV light for traces,
// the print kit for fingerprints, the crowbar for strongboxes.
// Dark and trace pieces are only there while a beam is on them: one detective holds the light, another picks it up.
//
// PICKED UP AND LOOKED OVER. E picks a thing up (plain, dark, or what was inside a strongbox) and you hold it in front
// of you (UHUInteractComponent::StartInspect, HUInspect.cpp): the mouse turns it over, and it has a mark on its
// underside - turn that to face you and you see what it is (its detail, and the offender profile it points to).
// Then E bags it, or Q puts it back where it was. A dead end put back doesn't count toward the profile leads; one
// bagged does. Prints (the print kit) and traces (a swab) are tool work, and are bagged as they're lifted.
// While someone holds it, it's gone from the floor on every squad player's screen.
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
	// picked up and looked over (plain, dark, or a strongbox's contents once it's open), not bagged as it's touched
	bool IsHandled() const { return Conceal == EHUConceal::Plain || Conceal == EHUConceal::Dark || (Conceal == EHUConceal::Locked && bOpened); }
	// who is holding it up to look at it right now, or null
	APawn* HeldByPawn() const { return HeldBy.Get(); }
	// the parts to draw in the hand (empty for a strongbox: what's inside is a small packet, drawn by the hand)
	void HandLook(TArray<UStaticMeshComponent*>& OutParts) const;
	// what the mark shows once it's turned to face you: the detail, and the profile it points to (if any)
	FString MarkWords() const;
	void MarkLead(FString& OutTitle, FString& OutBody) const;

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
	// in someone's hand (bHeld stays set if they vanish - a partner who left - so it's put back by itself)
	TWeakObjectPtr<APawn> HeldBy;
	bool bHeld = false;
	void PickUp(APawn* User);
	void PutBack(APawn* User);
	void Bag(APawn* User);
	void Show(bool bVisible);
	class UHUCaseSubsystem* Cases() const;
};
