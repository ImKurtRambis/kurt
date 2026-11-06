// On the player: traces what you're looking at, shows its prompt and any subtitle on screen, sends E / Q to it,
// and carries the HOTBAR - what's in your hand:
//   1 hands  ·  2 flashlight  ·  3 UV light  ·  4 print kit  ·  5 crowbar  ·  6 scanner  ·  7 handcuffs   (mouse wheel cycles)
//   LEFT CLICK uses what you're holding: lights switch on/off, the scanner pings, the print kit and crowbar work
//   on the evidence you're looking at. F / V / R grab the flashlight / UV light / scanner and use it in one go.
// Tools only work in your hand: prints need the print kit held, strongboxes need the crowbar held.
// What you hold is drawn in first person, in your hand.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HUTools.h"
#include "HUInteractComponent.generated.h"

UCLASS(ClassGroup = (HomicideUnit), meta = (BlueprintSpawnableComponent))
class HOMICIDEUNIT_API UHUInteractComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHUInteractComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact") float Reach = 250.f;
	UPROPERTY(BlueprintReadOnly, Category = "Interact") AActor* Focus = nullptr;
	// the exact part of it you're looking at (e.g. which chair in a room full of them)
	UPROPERTY(BlueprintReadOnly, Category = "Interact") UPrimitiveComponent* FocusPart = nullptr;
	// the "[E] ..." prompt for what you're looking at
	UPROPERTY(BlueprintReadOnly, Category = "Interact") FText Prompt;
	// a line of dialogue at the bottom of the screen (the Captain, a suspect...)
	UPROPERTY(BlueprintReadWrite, Category = "Interact") FText Subtitle;
	// a radio / phone line playing (the 911 call): shown above the subtitle
	UPROPERTY(BlueprintReadWrite, Category = "Interact") FText CallLine;

	UFUNCTION(BlueprintCallable, Category = "Interact") void Use(bool bSecondary);

	// Squad play: every use goes to the host, and the host has every player's game replay it on their own copy
	// of the scene (the scenes are identical - generated from the squad code). Things are found by their path,
	// parts by their name, so each machine acts on its own matching object.
	UFUNCTION(Server, Reliable) void ServerUse(const FString& TargetPath, FName Part, bool bSecondary, int32 HeldSlot);
	UFUNCTION(NetMulticast, Reliable) void MulticastUse(const FString& TargetPath, FName Part, bool bSecondary, int32 HeldSlot);

	// ---- the hotbar ----
	static constexpr int32 SlotCount = 1 + (int32)EHUTool::Count;
	// 0 = empty hands, 1.. = EHUTool (slot - 1)
	UPROPERTY(BlueprintReadOnly, Category = "Hotbar") int32 Held = 0;
	UFUNCTION(BlueprintPure, Category = "Hotbar") bool IsHolding(EHUTool Tool) const { return Held == (int32)Tool + 1; }
	UFUNCTION(BlueprintPure, Category = "Hotbar") bool SlotUsable(int32 Slot) const;
	UFUNCTION(BlueprintCallable, Category = "Hotbar") void SelectSlot(int32 Slot);
	UFUNCTION(BlueprintCallable, Category = "Hotbar") void UseHeld();

	// lights: on only while held
	UPROPERTY(BlueprintReadOnly, Category = "Kit") bool bLightOn = false;
	UPROPERTY(BlueprintReadOnly, Category = "Kit") bool bFlashlightOn = false;
	UPROPERTY(BlueprintReadOnly, Category = "Kit") bool bUVOn = false;
	UFUNCTION(BlueprintCallable, Category = "Kit") void ToggleFlashlight();
	UFUNCTION(BlueprintCallable, Category = "Kit") void ToggleUV();
	UFUNCTION(BlueprintCallable, Category = "Kit") void Scan();
	// is this point lit by the beam of the given light (held, on, in range, inside the cone)?
	UFUNCTION(BlueprintPure, Category = "Kit") bool InBeam(FVector Point, bool bUV) const;
	// how long you've been looking at the current thing (the prompt animates in with it)
	UPROPERTY(BlueprintReadOnly, Category = "Interact") float PromptAge = 0.f;

	// animations: both hands on the furniture you're dragging; a quick reach when you pick something up
	void SetGrabbing(AActor* Thing);
	void PlayReach() { ReachT = 0.f; }
	bool IsGrabbing() const { return Grabbed.IsValid(); }

	// the security desk you're watching the tapes at (the HUD shows the feed)
	TWeakObjectPtr<class AHUSecurityDesk> Watching;

	// pop-up cards in the corner (evidence bagged, kit bought...)
	struct FNote { FString Title; FString Body; FLinearColor Color; float Born = 0.f; };
	TArray<FNote> Notes;
	void Notify(const FString& Title, const FString& Body, FLinearColor Color);

	// a quick message in the subtitle line
	void Say(const FString& Line) { Subtitle = FText::FromString(Line); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY() class UInputComponent* Keys = nullptr;
	UPROPERTY() class UHUPromptWidget* Widget = nullptr;
	UPROPERTY() class USpotLightComponent* Beam = nullptr;
	UPROPERTY() USceneComponent* Hand = nullptr;           // the first-person hand, on the camera
	UPROPERTY() TArray<UStaticMeshComponent*> HandParts;
	UPROPERTY() UStaticMeshComponent* Lens = nullptr;      // the light's lens, lit when on
	// hover feedback: the parts that are glowing, their real colors, and a light that drifts over to them
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Lit;
	TArray<FLinearColor> LitColor;
	UPROPERTY() class USpotLightComponent* Glint = nullptr;
	void SetHover(const TArray<UPrimitiveComponent*>& Parts);
	void AnimateHover(float DeltaTime);
	double LastScan = -100.0;
	float Bob = 0.f;
	float ReachT = -1.f;
	TWeakObjectPtr<AActor> LastFocus;
	TWeakObjectPtr<UPrimitiveComponent> LastFocusPart;
	float GrabBlend = 0.f;
	TWeakObjectPtr<AActor> Grabbed;
	class UHUCaseSubsystem* Cases() const;
	void UpdateBeam();
	void BuildHand();
	void UsePrimary() { Use(false); }
	void UseSecondary() { Use(true); }
	void Slot1() { SelectSlot(0); } void Slot2() { SelectSlot(1); } void Slot3() { SelectSlot(2); }
	void Slot4() { SelectSlot(3); } void Slot5() { SelectSlot(4); } void Slot6() { SelectSlot(5); } void Slot7() { SelectSlot(6); }
	void WheelUp(); void WheelDown();
	void Setup(class APlayerController* PC);
};
