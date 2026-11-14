// Cosmetic station life. No pawns, navigation, interaction interface or case state.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "HUPrecinctLife.generated.h"

USTRUCT(BlueprintType)
struct FHUBackgroundWorker
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) int32 Face=0; // EA-07 uniform variant; -1 is the waiting civilian
	UPROPERTY(EditAnywhere) FVector Feet=FVector::ZeroVector;
	UPROPERTY(EditAnywhere) float Yaw=0;
	UPROPERTY(EditAnywhere) bool bSeated=false;
	UPROPERTY(EditAnywhere) FVector WalkTo=FVector::ZeroVector;
};

// Authored in L_Precinct: positions survive map saves and can be edited without changing C++.
UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUPrecinctLifeLayout : public AActor
{
	GENERATED_BODY()
public:
	AHUPrecinctLifeLayout();
	UPROPERTY(EditAnywhere,Category="Station life") bool bEnabled=true;
	UPROPERTY(EditAnywhere,Category="Station life") TArray<FHUBackgroundWorker> Staff;
	UPROPERTY(EditAnywhere,Category="Station life") FVector FanPosition=FVector(-850,-940,283);
	UPROPERTY(EditAnywhere,Category="Station life") TArray<FVector> PhonePositions;
};

UCLASS()
class HOMICIDEUNIT_API UHUPrecinctLife : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bActive && !IsTemplate(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHUPrecinctLife, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type==EWorldType::Game || Type==EWorldType::PIE; }
private:
	struct FWorker
	{
		TWeakObjectPtr<AActor> Model;
		FVector From, To;
		float Phase=0, Yaw=0, Progress=0;
		bool bSeated=false, bWalk=false;
	};
	TArray<FWorker> Workers;
	TWeakObjectPtr<AActor> Station;
	TWeakObjectPtr<AHUPrecinctLifeLayout> Layout;
	UPROPERTY() AActor* Decor=nullptr;
	UPROPERTY() TArray<AActor*> People;
	UPROPERTY() TArray<class UStaticMeshComponent*> FanParts;
	UPROPERTY() class USceneComponent* Rotor=nullptr;
	UPROPERTY() class UAudioComponent* FanAudio=nullptr;
	UPROPERTY() TArray<class UAudioComponent*> Phones;
	bool bActive=false, bChecked=false;
	float Clock=0, NextRing=8, FanAngle=0;
	int32 Rings=0;
	void Build();
	void AddPerson(int32 Face, FVector At, float Yaw, bool bSeated, FVector WalkTo=FVector::ZeroVector);
	class UAudioComponent* Audio(const TCHAR* Name, FVector At, float Volume, float Radius);
	void Check();
};
