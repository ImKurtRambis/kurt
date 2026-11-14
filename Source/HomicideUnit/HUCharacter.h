// The detective you play: first person. WASD walk, mouse look, SHIFT run, SPACE jump, C / CTRL crouch,
// ESC back to the desk. Reads mouse sensitivity and field of view from your saved settings.
// Carries the interaction component (E / Q, flashlight, UV, scanner) from the start.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HULook.h"
#include "HUCharacter.generated.h"

UCLASS()
class HOMICIDEUNIT_API AHUCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AHUCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) class UCameraComponent* Eyes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) class UHUInteractComponent* Interact;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detective") float WalkSpeed = 330.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detective") float RunSpeed = 560.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detective") FName MenuLevel = TEXT("L_Menu");
	// leave the case (and the squad) for the desk menu - from the ESC menu
	void BackToDesk();

	// ---- how you look (from your name plate): a body everyone else sees; you only see your hand ----
	UPROPERTY(ReplicatedUsing = OnRep_Look) FHULook Look;
	UFUNCTION(Server, Reliable) void ServerSetLook(const FHULook& NewLook);
	UFUNCTION() void OnRep_Look() { BuildBody(); }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

	// ---- sitting: E on a chair sits you down facing the way it faces; WASD, SPACE or E again stands you up ----
	UPROPERTY(BlueprintReadOnly, Category = "Detective") bool bSeated = false;
	// SeatTop: the world point on the seat cushion; Yaw: the way the chair faces
	UFUNCTION(BlueprintCallable, Category = "Detective") void SitDown(FVector SeatTop, float Yaw);
	UFUNCTION(BlueprintCallable, Category = "Detective") void StandUp();
	// the host seating you (in a squad): sit on your own screen too
	UFUNCTION(Client, Reliable) void ClientSitDown(FVector SeatTop, float Yaw);

	// ---- coffee: for a while, careful work goes quicker and you're quicker on your feet ----
	UFUNCTION(BlueprintCallable, Category = "Detective") void DrinkCoffee(float Seconds = 150.f);
	UFUNCTION(BlueprintPure, Category = "Detective") float CoffeeLeft() const;
	UFUNCTION(BlueprintPure, Category = "Detective") bool IsCaffeinated() const { return CoffeeLeft() > 0.f; }
	// how much faster timed tasks go (1 normally, 1.6 on coffee)
	UFUNCTION(BlueprintPure, Category = "Detective") float TaskSpeed() const { return IsCaffeinated() ? 1.6f : 1.f; }
	float CoffeeLength = 150.f;

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* Input) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	class USkeletalMeshComponent* VisualBody() const;

protected:
	virtual void BeginPlay() override;

private:
	void ToggleCrouch();
	void OpenMenu();
	UPROPERTY() USceneComponent* Body = nullptr;
	UPROPERTY() AActor* Model = nullptr;
	void BuildBody();
	bool bLookSent = false;
	FVector StandSpot;
	double CoffeeUntil = -1.0;
	void StartJump() { if (bSeated) StandUp(); else Jump(); }
	void EndJump() { StopJumping(); }
};
