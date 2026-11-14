// A patrol officer holding the scene outside. Stands at the tape; E gets a word out of them. Played by a MetaHuman in
// uniform through HUPeople, with a separate capsule for conversation and movement collision.
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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY() class UCapsuleComponent* InteractionBody = nullptr;
	UPROPERTY() AActor* Look = nullptr; // the MetaHuman
	int32 Line = 0;
};
