// A cardboard carrier of coffee that patrol brought to the scene. E drinks one: for a couple of minutes,
// careful work (bagging, prints, prying, examining, booking) goes quicker and you're quicker on your feet.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUCoffee.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUCoffee : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUCoffee();
	UPROPERTY(VisibleAnywhere) USceneComponent* Root;

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual float UseTime(APawn* User, bool bSecondary) const override { return bSecondary ? 0.f : 1.f; }
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
};
