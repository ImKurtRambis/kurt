// A detective's DESK at the precinct: the case file, in folders you pick up, and the desk computer's case tools.
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
// THE COMPUTER (Q at the desk computer; E there plays the tapes once they're pulled) - each player's own screen:
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
	TWeakObjectPtr<APawn> Holder[FolderCount];                    // who has each folder (null: on the desk)
	TWeakObjectPtr<APawn> Viewer;                                 // this machine's player, while reading
	int32 FolderOf(const UPrimitiveComponent* Part) const;
	void SetOnDesk(int32 K, bool bOn);
	void Open(APawn* User, bool bTerm, int32 K);
	void Close();
	void Rebuild();       // fill Pages from the case
	void NextPage(); void PrevPage(); void ScrollDown(); void ScrollUp(); void CloseKey();
	class UHUCaseSubsystem* Cases() const;
};
