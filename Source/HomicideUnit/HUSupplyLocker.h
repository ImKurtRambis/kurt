// The supply locker in the motor pool: buy your kit before you roll. Q flips through the shelf, E buys.
// What you own is saved, and so is your cash (earned from evidence that holds up and from closed cases).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUTools.h"
#include "HUSupplyLocker.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUSupplyLocker : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUSupplyLocker();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* Sign;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* Shelf;

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	int32 Selected = 0;
	void Refresh();
	class UHUCaseSubsystem* Cases() const;
};
