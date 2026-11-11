// On the player: traces what you're looking at, shows its prompt and any subtitle on screen, sends E / Q to it,
// and carries the HOTBAR - what's in your hand:
//   1 hands  ·  2 flashlight  ·  3 UV light  ·  4 print kit  ·  5 crowbar  ·  6 scanner   (mouse wheel cycles)
// and the UTILITY BELT - look down (with nothing to pick up there): cuffs, pepper spray, taser, radio, pager.
//   Your body holds still while you look it over: put the crosshair on a pouch (or scroll), LEFT CLICK uses it. Your hand keeps what it's holding.
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
	static constexpr int32 SlotCount = 1 + (int32)EHUTool::Handcuffs; // the cuffs live on the belt
	// 0 = empty hands, 1.. = EHUTool (slot - 1)
	UPROPERTY(BlueprintReadOnly, Category = "Hotbar") int32 Held = 0;
	UFUNCTION(BlueprintPure, Category = "Hotbar") bool IsHolding(EHUTool Tool) const { return Held == (int32)Tool + 1; }
	UFUNCTION(BlueprintPure, Category = "Hotbar") bool SlotUsable(int32 Slot) const;
	UFUNCTION(BlueprintCallable, Category = "Hotbar") void SelectSlot(int32 Slot);
	UFUNCTION(BlueprintCallable, Category = "Hotbar") void UseHeld();

	// ---- the utility belt (HUBelt in HUTools.h) ----
	UPROPERTY(BlueprintReadOnly, Category = "Belt") bool bBeltOpen = false;
	UPROPERTY(BlueprintReadOnly, Category = "Belt") int32 BeltSel = 0;
	// seconds until that pouch can be used again (0 = ready)
	float BeltWait(int32 Item) const;
	void UseBelt();          // click a pouch: take it in hand / put it back (the pager opens the menu)
	void BeltAction(int32 Item); // use the belt item in your hand
	// what's in your hand: 0..SlotCount-1 the hotbar, SlotCount + HUBelt item = something off the belt
	int32 BeltInHand() const { return Held >= SlotCount ? Held - SlotCount : -1; }

	// ---- the menu (ESC, or the pager on your belt): settings, your pager messages, leave the case ----
	UPROPERTY(BlueprintReadOnly, Category = "Menu") bool bMenuOpen = false;
	UPROPERTY(BlueprintReadOnly, Category = "Menu") int32 MenuRow = 0;
	static constexpr int32 MenuRows = 8;
	void OpenMenu();
	void CloseMenu();
	void ToggleMenu() { if (bMenuOpen) CloseMenu(); else OpenMenu(); }
	// a row's text, for the HUD
	FString MenuLine(int32 Row) const;
	// every card that's popped up this case, oldest first (shown on the pager in the menu)
	const TArray<FString>& Pages() const { return PagerLog; }
	// a belt use goes to the host and every player's game applies it (same as Use)
	UFUNCTION(Server, Reliable) void ServerBelt(uint8 Item, const FString& TargetPath);
	UFUNCTION(NetMulticast, Reliable) void MulticastBelt(uint8 Item, const FString& TargetPath);

	// ---- squad markers: G drops one where you're looking (evidence, a suspect, or just "here"), for everyone ----
	struct FMarker { TWeakObjectPtr<APawn> By; FString Who; FVector At; uint8 Kind = 0; TWeakObjectPtr<AActor> Target; bool bHadTarget = false; float Born = 0.f; };
	enum : uint8 { MarkPlace, MarkEvidence, MarkSuspect };
	TArray<FMarker> Markers; // on the local player's component: every marker on the map, one per detective
	void Ping();
	UFUNCTION(Server, Reliable) void ServerPing(FVector At, uint8 Kind, const FString& TargetPath);
	UFUNCTION(NetMulticast, Reliable) void MulticastPing(FVector At, uint8 Kind, const FString& TargetPath);
	// your light on / off, so partners see your beam and what it lights up
	UFUNCTION(Server, Reliable) void ServerLights(bool bF, bool bUV);
	UFUNCTION(NetMulticast, Reliable) void MulticastLights(bool bF, bool bUV);

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

	// holding E/Q on a timed task: how far along (0..1), and what it is
	UPROPERTY(BlueprintReadOnly, Category = "Interact") float HoldProgress = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Interact") bool bHolding = false;

	// the suspect you're interviewing (the HUD shows the interview)
	TWeakObjectPtr<class AHUSuspectActor> Interviewing;
	// an interview move goes to the host, and every player's game applies it (same as Use)
	void SendInterrogation(const FString& SuspectPath, uint8 Action, int32 Statement, const FString& ClueId);
	UFUNCTION(Server, Reliable) void ServerInterrogate(const FString& SuspectPath, uint8 Action, int32 Statement, const FString& ClueId);
	UFUNCTION(NetMulticast, Reliable) void MulticastInterrogate(const FString& SuspectPath, uint8 Action, int32 Statement, const FString& ClueId);
	// a typed question and the suspect's answer (from the asker's local brain), shown to the squad
	void SendAnswer(const FString& SuspectPath, const FString& Question, const FString& Answer, uint8 Emotion = 255, uint8 Action = 0);
	UFUNCTION(Server, Reliable) void ServerAnswer(const FString& SuspectPath, const FString& Question, const FString& Answer, uint8 Emotion, uint8 Action);
	UFUNCTION(NetMulticast, Reliable) void MulticastAnswer(const FString& SuspectPath, const FString& Question, const FString& Answer, uint8 Emotion, uint8 Action);

	// a moment flagged on the security tapes: the note goes to the whole squad; bCatch = it's the clip that
	// catches the culprit's lie (every machine puts the lie on its record)
	void SendTapeNote(const FString& Note, bool bCatch);
	UFUNCTION(Server, Reliable) void ServerTapeNote(const FString& Note, bool bCatch);
	UFUNCTION(NetMulticast, Reliable) void MulticastTapeNote(const FString& Note, bool bCatch);

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
	void UsePrimary() { BeginUse(false); }
	void UseSecondary() { BeginUse(true); }
	void ReleasePrimary() { if (bHolding && !bHoldSecondary) CancelHold(); }
	void ReleaseSecondary() { if (bHolding && bHoldSecondary) CancelHold(); }
	void BeginUse(bool bSecondary);
	void CancelHold() { bHolding = false; HoldProgress = 0.f; }
	bool bHoldSecondary = false;
	float HoldTotal = 0.f;
	TWeakObjectPtr<AActor> HoldTarget;
	TWeakObjectPtr<UPrimitiveComponent> HoldPart;
	void Slot1() { SelectSlot(0); } void Slot2() { SelectSlot(1); } void Slot3() { SelectSlot(2); }
	void Slot4() { SelectSlot(3); } void Slot5() { SelectSlot(4); } void Slot6() { SelectSlot(5); }
	// the belt at your waist (only you see it), and how you're picking from it
	UPROPERTY() USceneComponent* Belt = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> BeltParts;
	float BeltYaw = 0.f;
	double BeltUsed[HUBelt::Count] = { -100.0, -100.0, -100.0, -100.0, -100.0 };
	TArray<FString> PagerLog; // every card that's popped up, oldest first
	int32 PagerRead = 0;
	void TickMarkers();
	UPROPERTY() class UInputComponent* MenuKeys = nullptr;
	void MenuUp(); void MenuDown(); void MenuLeft(); void MenuRight(); void MenuEnter();
	void ChangeMenu(int32 Dir);
	void BuildBelt();
	static FVector PouchSpot(int32 i);
	void SetBeltSel(int32 Sel, float LookYaw);
	void SendBelt(uint8 Item, const FString& TargetPath);
	void WheelUp(); void WheelDown();
	void Setup(class APlayerController* PC);
};
