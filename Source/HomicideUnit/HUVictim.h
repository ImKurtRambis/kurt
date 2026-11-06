// The victim at the scene. What's there depends on what happened:
//   Body      (homicide)          - the body where it fell, a pool of blood, evidence markers around it
//   Outline   (assault, robbery)  - the victim's gone to County General: tape outline and blood where they lay
//   Belongings (missing person)   - their coat and bag, left behind
//   Scorch    (arson)             - the burnt patch where the fire started
// E examines it. A body is examined part by part - look at the head, neck, hands, torso or legs and press E:
// the wound (matching the weapon), signs of the kind of offender (they count toward the profile), time of death. Q searches it: the body's pockets,
// the bag - sometimes there's a piece of evidence on it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUProfiles.h"
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
	virtual void HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const override;
	virtual float UseTime(APawn* User, bool bSecondary) const override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	bool bSearched = false;
	// the body's parts: 0 head, 1 neck, 2 hands & arms, 3 torso, 4 legs & shoes
	UPROPERTY() TMap<UPrimitiveComponent*, int32> Spots;
	TSet<int32> Examined;
	static const TCHAR* SpotName(int32 Spot);
	FString Finding(int32 Spot, EHUProfile& OutSign) const;
	void ExamineSpot(int32 Spot, class UHUInteractComponent* I);
	FString Notes() const;
	class UHUCaseSubsystem* Cases() const;
};
