// The victim at the scene. What's there depends on what happened:
//   Body      (homicide)          - the body where it fell, a pool of blood, evidence markers around it
//   Outline   (assault, robbery)  - the victim's gone to County General: tape outline and blood where they lay
//   Belongings (missing person)   - their coat and bag, left behind
//   Scorch    (arson)             - the burnt patch where the fire started
// E examines it (the medical examiner's notes / what the scene tells you). Q searches it: the body's pockets,
// the bag - sometimes there's a piece of evidence on it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUVictim.generated.h"

UENUM(BlueprintType)
enum class EHUVictimKind : uint8 { Body, Outline, Belongings, Scorch };

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUVictim : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUVictim();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Victim") EHUVictimKind Kind = EHUVictimKind::Body;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Victim") FLinearColor Coat = FLinearColor(0.15f, 0.12f, 0.1f);
	// evidence found by searching (Q); hidden until then
	UPROPERTY(BlueprintReadWrite, Category = "Victim") class AHUEvidence* Pocket = nullptr;

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	bool bSearched = false;
	FString Notes() const;
	class UHUCaseSubsystem* Cases() const;
};
