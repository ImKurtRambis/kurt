// THE SQUAD'S NOTEBOOK: everything a detective bags, hears or looks up this case, with who it was - the same on every
// squad player's machine, as it happens.
//
// Nothing is sent for it: every machine already replays each detective's actions (UHUInteractComponent::MulticastUse,
// MulticastInterrogate, MulticastAnswer, MulticastDeskRecord, MulticastTapeNote), and the code that carries each one out
// notes it here, naming whoever did it. What's worth stopping for - evidence bagged, a lie put on the record, a
// confession, a sign on the body - also pops up as a card for everyone who didn't do it themselves.
//
// Open it with N (UHUNotebookWidget): newest first, who and when. A small tab says when there's something new.
// The case board (AHUCaseBoard) and the case file show who bagged each piece (FoundBy) and who caught each lie (CaughtBy).
// It's a game-instance subsystem, so it lasts the case across levels; a new case starts a clean page.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "HUNotebook.generated.h"

class APawn;
class UHUCaseSubsystem;

enum class EHUNote : uint8 { Evidence, Heard, Lie, Confession, Lookup, Tape, Body, Count };

UCLASS()
class HOMICIDEUNIT_API UHUNotebook : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	struct FEntry
	{
		EHUNote Kind = EHUNote::Heard;
		FString Who;    // the detective, as the squad knows them
		FString Text;
		FString Ref;    // the clue or suspect it's about
		FString Clock;  // when, by this PC's clock ("21:40")
	};
	TArray<FEntry> Entries;            // oldest first
	TMap<FString, FString> FoundBy;    // clue id -> who bagged it
	TMap<FString, FString> CaughtBy;   // "suspect id|statement" -> who put that lie on the record
	int32 Seen = 0;                    // entries this player has had the notebook open for

	static UHUNotebook* Of(const UObject* WorldContext);
	// a detective's name for the squad ("A detective" when they have none)
	static FString WhoOf(const APawn* P);
	static const TCHAR* KindName(EHUNote K);
	static FLinearColor KindColor(EHUNote K);

	// Note it (every machine calls this as the action reaches it). Tell: also a card, titled so, for every player
	// who didn't do it themselves ("Mike: the parking tickets").
	void Note(EHUNote Kind, const APawn* By, const FString& Text, const FString& Ref = FString(), const TCHAR* Tell = nullptr, const FString& Short = FString());
	// what the lies on the record are right now ("suspect|statement"), to see which one a move just broke
	static TSet<FString> LiesNow(const UHUCaseSubsystem* C);
	// note every lie on the record now that wasn't in Before, as caught by By
	void NoteNewLies(const UHUCaseSubsystem* C, const APawn* By, const TSet<FString>& Before, bool bTell);
	// a new case: a clean page
	void Sync();
	int32 Unread() const { return FMath::Max(0, Entries.Num() - Seen); }

private:
	FString CaseKey;
};

// The notebook on screen: N opens it on the left, newest first (the mouse wheel scrolls); closed, a small tab says when
// there's something new. Drawn in code with the HUD's own kit (HUDraw).
UCLASS()
class HOMICIDEUNIT_API UHUNotebookWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY() class UHUInteractComponent* Owner = nullptr;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
