// A detective's DESK at the precinct: the case file, in folders you pick up, and the desk computer's case tools.
// Each player in the squad gets their OWN desk (claimed as they arrive, name plate on the front): only they can
// use its folders and computer; a desk nobody has is free for anyone.
// One per squad desk (spawned by the precinct; every machine spawns the same ones with the same names, and replays
// the same uses, so the squad sees one desk).
//   THE FOLDERS (E picks one up, Q / E puts it back down):
//     SCENE REPORT  - what happened, where, when, the victim
//     EVIDENCE LOG  - everything bagged at the scene, and where it's pinned on the case board
//     SUSPECT FILES - one page per suspect: who they are, their record, every statement and whether it holds
//     AUTOPSY       - the M.E.'s findings: weapon, time, what the body says about who did it
//     TAPE NOTES    - what the squad flagged on the security tapes
//   While a detective has a folder it's gone from the desk for everyone ("Det. Reyes has it").
//   Reading: A / D turn the page, W / S scroll.
// THE COMPUTER runs PRECINCTOS, a desktop with a shortcut per case tool (the list is DeskApps in HUCaseDesk.cpp):
//   W / S pick an icon, E opens it in a window; in a window W / S scroll, A / D or Tab switch windows,
//   M minimises to the taskbar, Q closes it (Q on the desktop logs off).
//   CO-OP: F sends the window in front to the squad (it turns up on each partner's desktop with a NEW badge), and a
//   partner at your computer while you're on it can look over your shoulder (they see your screen, live, read-only).
// The tools (E at the desk computer before the tapes are pulled; Q after, when E plays the tapes):
//     RECORDS     - each suspect's criminal record, and what kind of offender it fits
//     ALIBI CHECK - every statement against the evidence you've bagged: what contradicts what
//     PROFILE     - which kind of offender the evidence points at
//     TAPE FLAGS  - the flagged tape moments
//     CASE STATUS - lies on the record, what's needed to charge, who's in holding
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUCaseDesk.generated.h"

UCLASS()
class HOMICIDEUNIT_API AHUCaseDesk : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUCaseDesk();

	static constexpr int32 FolderCount = 5;
	// which desk this is (0..3): each player is given one as they arrive (AHUPlayerState::Desk)
	int32 DeskIndex = 0;
	// the detective whose desk it is (null: nobody's - anyone can use it)
	class AHUPlayerState* OwnerState() const;
	// may this player use the folders and computer here? (their own desk, or one nobody has)
	bool CanUse(const APawn* User) const;
	FString OwnerName() const;
	static const TCHAR* FolderName(int32 K);

	// one page of what's on screen
	struct FPage { FString Title; TArray<FString> Lines; };

	// ---- what the reader shows (this player only) ----
	bool bReading = false;
	bool bTerminal = false;   // the computer (green screen) rather than a folder (paper)
	int32 Folder = -1;        // the folder in hand
	int32 Page = 0;
	int32 Scroll = 0;
	TArray<FPage> Pages;
	FString ReaderTitle() const { return bTerminal ? FString(TEXT("PRECINCT RECORDS TERMINAL")) : FString(FolderName(Folder)); }

	// the desk computer's case tools (called by the precinct when Q is pressed at this desk's computer)
	void OpenTerminal(APawn* User);

	// ---- PrecinctOS ----
	static const TCHAR* OSName;
	static int32 AppCount();
	static FString AppName(int32 A);
	static FString AppGlyph(int32 A);
	static FLinearColor AppColor(int32 A);
	int32 Sel = 0;                     // the highlighted desktop icon
	TArray<int32> OpenWins;            // open windows (apps), in taskbar order
	TSet<int32> Minimised;
	int32 Active = -1;                 // the window in front (-1: the desktop)
	bool bPeek = false;                // looking over the owner's shoulder (read-only, follows their screen)
	TMap<int32, FString> SharedFrom;   // app -> the partner who sent it to you (a NEW badge on the icon)
	bool IsOnDesktop() const { return Active < 0 || Minimised.Contains(Active); }
	// what the owner's screen shows, known on every machine (sent by the owner as it changes)
	bool bOwnerOn = false;
	int32 OwnerApp = -1;
	void SetOwnerView(bool bOn, int32 App) { bOwnerOn = bOn; OwnerApp = App; }
	void ReceiveShare(int32 App, const FString& From, const FString& InPhone = FString()) { SharedFrom.Add(App, From); if (!InPhone.IsEmpty()) SharedPhone = InPhone; }

	// ---- THE SUSPECT'S PHONE: a seized phone on the cable at this desk, used through the Phone app ----
	FString PhoneOf;                   // the suspect whose phone is plugged in here (the same on every machine)
	FString SharedPhone;               // a partner sent you the phone on their desk
	FString ShownPhone() const { return !PhoneOf.IsEmpty() ? PhoneOf : SharedPhone; }
	static bool IsPhoneApp(int32 A);
	enum class EPhone : uint8 { Home, Calls, Chats, Thread, Photos, Photo, Web, Cell };
	EPhone PhoneScreen = EPhone::Home;
	int32 PhoneSel = 0;                // the open conversation / photo
	struct FPhoneRow { FString Title, Sub, Right; bool bMe = false; bool bNote = false; };
	struct FPhoneView
	{
		FString Owner, Number, Clock;
		TArray<FPhoneRow> Calls, Photos, Web, Cell, Notes;   // Notes: what the lab could recover of deleted messages
		TArray<FString> Threads;                            // conversations, in order
		TArray<TArray<FPhoneRow>> Msgs;                     // the messages in each
	};
	FPhoneView Phone;                  // filled from the phone being shown (Rebuild)
	void PhoneTap(int32 Kind, int32 Arg); // the phone's screen was clicked: 0 an app icon, 1 a row, 2 back, 3 home
	void DevShowPhone(APawn* User, int32 Screen, int32 InSel); // development: seize every phone, plug in the culprit's, show a screen
	void OpenPeek(APawn* User);
	// THE MOUSE: the screen records where it drew each clickable thing (absolute screen coords) as it paints
	enum class EHit : uint8 { Icon, Close, Minimise, Task, Start, ScrollUp, ScrollDown, Share, Swallow, PhoneIcon, PhoneRow, PhoneBack, PhoneHome };
	struct FHit { FVector2D Min, Max; EHit Kind; int32 Arg; };
	mutable TArray<FHit> Hits;
	mutable FVector2D HitSpace = FVector2D(1, 1); // the screen's size in the units the hits are in (it fills the viewport)

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY() USceneComponent* Root;
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() TArray<UStaticMeshComponent*> FolderParts;        // one per folder
	UPROPERTY() TArray<class UTextRenderComponent*> FolderLabels; // the tab on each
	UPROPERTY() class UTextRenderComponent* NamePlate = nullptr;   // DET. NAME on the desk's front edge
	UPROPERTY() TArray<UStaticMeshComponent*> DockParts;           // the phone cable and its cradle
	UPROPERTY() UStaticMeshComponent* DockPhone = nullptr;         // a phone sitting in it (when one's plugged in)
	bool IsDock(const UPrimitiveComponent* Part) const;
	void BuildPhone();                                             // Phone from the phone being shown
	void AssignDesks(); // the host: everyone without a desk gets the lowest free one
	TWeakObjectPtr<APawn> Holder[FolderCount];                    // who has each folder (null: on the desk)
	TWeakObjectPtr<APawn> Viewer;                                 // this machine's player, while reading
	int32 FolderOf(const UPrimitiveComponent* Part) const;
	void SetOnDesk(int32 K, bool bOn);
	void Open(APawn* User, bool bTerm, int32 K);
	void Close();
	void Rebuild();       // fill Pages from the case
	void NextPage(); void PrevPage(); void ScrollDown(); void ScrollUp(); void CloseKey();
	void MouseClick(); void WheelUp(); void WheelDown();
	double LastClickT = 0.0; int32 LastClickIcon = -1;
	void KeyLeft(); void KeyRight(); void KeyUp(); void KeyDown(); void KeyE(); void KeyQ(); void KeyM(); void KeyTab(); void KeyF();
	void OpenApp(int32 A);
	void CloseWindow();
	void Broadcast();   // tell the squad what this screen shows (for looking over your shoulder)
	bool bSentOn = false; int32 SentApp = -2;
	class UHUCaseSubsystem* Cases() const;
};
