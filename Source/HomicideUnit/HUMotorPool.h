// Three working sectional shutters, parked cruisers and the precinct maintenance bay.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUMotorPool.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUMotorPool : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUMotorPool();
	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* BaySign;
	UPROPERTY(VisibleAnywhere) TArray<class UChildActorComponent*> Cars;
	UPROPERTY(VisibleAnywhere) class UChildActorComponent* Locker;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual bool Highlights() const override { return false; }
	void SetBayOpen(int32 Bay, bool bOpen);
	bool IsBayClear(int32 Bay) const;
protected:
	virtual void BeginPlay() override;
private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() TArray<UStaticMeshComponent*> Slats;
	UPROPERTY() TArray<UTextRenderComponent*> Labels;
	UPROPERTY() TMap<UPrimitiveComponent*, int32> Controls;
	float Lift[3] = {};
	bool Open[3] = {};
	float TestClock = 0.f;
	int32 TestStage = 0;
	int32 FocusBay(APawn* User) const;
	void PlaceSlats();
};
