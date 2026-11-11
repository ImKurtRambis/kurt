// THE CASE BOARD: a cork board on the back wall of the squad room, where the squad works the case together.
//   Top left: THE SCENE (where, what, the victim, when) and the MURDER WEAPON.
//   Across the top: a mugshot of every suspect with their name and role.
//   At the bottom: an index card for every piece of evidence bagged at the scene.
// Working it:
//   E on an evidence card takes it off the board (it sticks out, yellow) - then E on a suspect's photo pins it
//   under them with red string. E on the card you're holding puts it back with the loose evidence.
//   E on a photo (with no card in hand) marks them: PRIME SUSPECT -> CLEARED -> unmarked.   Q reads a card or a file.
// The pins and marks live on UHUCaseSubsystem (BoardPins / BoardMarks), so they last the whole case.
// Every machine spawns its own board with the same name and replays the same uses, so the squad sees one board.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUCaseBoard.generated.h"

UCLASS()
class HOMICIDEUNIT_API AHUCaseBoard : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUCaseBoard();

	static constexpr int32 MaxSuspects = 8;
	static constexpr int32 MaxEvidence = 24;

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const override { Out.Add(Part); }
	virtual bool Highlights() const override { return false; }
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() USceneComponent* Root;
	UPROPERTY() TArray<UStaticMeshComponent*> Frame;   // the cork and its frame (built once)
	UPROPERTY() TArray<UStaticMeshComponent*> Deco;    // photos, string, pins (rebuilt whenever the board changes)
	UPROPERTY() TArray<class UTextRenderComponent*> Texts;
	// the cards you can use, made once with fixed names so a squadmate's use finds the same card on every machine
	UPROPERTY() TArray<UStaticMeshComponent*> SusCards;
	UPROPERTY() TArray<UStaticMeshComponent*> EvCards;
	UPROPERTY() UStaticMeshComponent* SceneCard = nullptr;
	UPROPERTY() UStaticMeshComponent* WeaponCard = nullptr;

	// the evidence card each detective has taken off the board (clue id)
	TMap<TWeakObjectPtr<APawn>, FString> Holding;
	// what the board showed last time it was built; rebuilt when this changes
	FString Shown;

	class UHUCaseSubsystem* Cases() const;
	// the bagged evidence, in case order: EvCards[i] shows Bagged()[i]
	TArray<FString> Bagged() const;
	FString Signature() const;
	void Rebuild();
	UStaticMeshComponent* MakeCard(const TCHAR* Name);
	void PlaceCard(UStaticMeshComponent* C, float U, float Z, float W, float H, FLinearColor Color, float Depth = 1.5f);
	class UTextRenderComponent* Write(const FString& S, float U, float Z, float Size, FColor Color, float Depth = 3.f);
	void Thread(FVector A, FVector B);
	int32 SuspectSlot(const UPrimitiveComponent* Part) const { return SusCards.IndexOfByKey(Part); }
	int32 EvidenceSlot(const UPrimitiveComponent* Part) const { return EvCards.IndexOfByKey(Part); }

	// -HUBoardTest: deal a case, bag some evidence, work the board a little, and photograph it (Saved/Screenshots)
	float TestClock = -1.f;
	void TestStep(float DeltaSeconds);
};
