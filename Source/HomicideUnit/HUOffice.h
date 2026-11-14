// A detective's PERSONAL OFFICE at the precinct (one per player, the same office as their desk: AHUPlayerState::Desk).
// The squad room builds the walls, the desk and the computer; this dresses the room the way its owner likes it:
// wall paint, carpet, and the furniture and odds and ends they've put in.
// Decorating: the CATALOGUE on the wall by the door. E picks a line, Q changes it (owner only).
// The look is saved on your PC (its own save slot) and shared with the squad through your player state, so partners
// walking past see your office as you made it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "HUInteractable.h"
#include "HUOffice.generated.h"

// how an office is done up
USTRUCT(BlueprintType)
struct FHUOfficeLook
{
	GENERATED_BODY()
	UPROPERTY() uint8 Wall = 0;    // paint
	UPROPERTY() uint8 Floor = 0;   // carpet
	UPROPERTY() uint8 Poster = 0;  // what hangs over the desk (0 none)
	UPROPERTY() int32 Items = 0;   // bit per piece of furniture (EHUOfficeItem)
	bool operator==(const FHUOfficeLook& O) const { return Wall == O.Wall && Floor == O.Floor && Poster == O.Poster && Items == O.Items; }
	bool operator!=(const FHUOfficeLook& O) const { return !(*this == O); }
};

// the office look, kept on this PC
UCLASS()
class HOMICIDEUNIT_API UHUOfficeSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() FHUOfficeLook Look;
	UPROPERTY() bool bSet = false;
};

UCLASS()
class HOMICIDEUNIT_API AHUOffice : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUOffice();

	int32 Index = 0;  // which office (= which desk)

	// the catalogue's lines (edit the lists in HUOffice.cpp to add options)
	static int32 RowCount();
	static FString RowLabel(int32 Row, const FHUOfficeLook& L);
	static void ChangeRow(int32 Row, FHUOfficeLook& L);

	// your saved look (or the default)
	static FHUOfficeLook LoadSaved();
	static void Save(const FHUOfficeLook& L);

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() USceneComponent* Root;
	UPROPERTY() TArray<UStaticMeshComponent*> Decor;      // rebuilt when the look changes
	UPROPERTY() TArray<UStaticMeshComponent*> Fixed;      // the catalogue
	UPROPERTY() TArray<UPrimitiveComponent*> CatalogueParts;
	UPROPERTY() class UTextRenderComponent* CatalogueText = nullptr;
	UPROPERTY() class UTextRenderComponent* DoorName = nullptr;
	int32 Row = 0;                // the catalogue line picked (the owner's machine)
	FHUOfficeLook Built;          // what's built now
	bool bBuiltOnce = false;
	bool bSentSaved = false;      // this machine's owner has sent their saved look
	class AHUPlayerState* OwnerState() const;
	bool IsOwner(const APawn* P) const;
	void Dress(const FHUOfficeLook& L);
	void RefreshText(const FHUOfficeLook& L);
};
