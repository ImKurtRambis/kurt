// The police van (the paddy wagon) parked outside every scene. Walk a cuffed suspect up to it and press E:
// they go in the back. Whoever's inside rides to the precinct when the squad drives back.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUPoliceVan.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUPoliceVan : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUPoliceVan();
	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* SideText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Van") float LoadRange = 500.f;

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	class AHUSuspectActor* EscortedBy(const APawn* User) const;
	int32 Inside() const;
};
