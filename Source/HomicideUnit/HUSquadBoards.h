// Two boards in the precinct (see HUProgress):
//   AHUUpgradeBoard - buy precinct upgrades with your own cash (Q next, E buy). What you own works for the whole squad
//                     while you're in the session; the board shows who brought what, and everyone's rank and specialty.
//   AHUCaseWall     - the cases you've closed, framed, with the detectives who worked them.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUSquadBoards.generated.h"

UCLASS()
class HOMICIDEUNIT_API AHUUpgradeBoard : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUUpgradeBoard();
	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* Title;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* Body;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* Squad;

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	int32 Selected = 0;
	float Refresh = 0.f;
	class UHUCaseSubsystem* Cases() const;
};

UCLASS()
class HOMICIDEUNIT_API AHUCaseWall : public AActor
{
	GENERATED_BODY()
public:
	AHUCaseWall();
	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* Title;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() TArray<class UTextRenderComponent*> Plaques;
	void Fill();
};
