// A patrol officer holding the scene outside. Stands at the tape; E gets a word out of them.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUOfficer.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUOfficer : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUOfficer();
	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Officer") int32 Variant = 0;

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	int32 Line = 0;
};
