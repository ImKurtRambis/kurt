// A person of interest at the scene (a stand-in figure for now).
//   E talks: their intro, then each statement in turn.   Q runs their record.
//   Holding the HANDCUFFS (slot 7), E arrests them: cuffed, hands behind their back, and they follow you.
//   Walk them to the police van outside and put them in the back; the van takes them to the precinct.
//   While cuffed: E lets go of / takes their arm again, Q takes the cuffs off.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUSuspectActor.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUSuspectActor : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUSuspectActor();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* NameTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspect") FString SuspectId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspect") FLinearColor Coat = FLinearColor(0.2f, 0.15f, 0.1f);

	UPROPERTY(BlueprintReadOnly, Category = "Suspect") bool bCuffed = false;
	UFUNCTION(BlueprintPure, Category = "Suspect") bool IsEscortedBy(const APawn* P) const { return bCuffed && Escort.Get() == P; }
	UFUNCTION(BlueprintPure, Category = "Suspect") FString DisplayName() const;
	// ---- the interview (yours alone on screen; what's said is shared with the squad) ----
	//   W/S statement · A/D evidence (or none, to just press them) · E confront / press · R their record · Q leave
	UPROPERTY(BlueprintReadOnly, Category = "Interview") bool bInterviewing = false;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") int32 StatementSel = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") int32 EvidenceSel = -1; // -1: no evidence, just press
	UPROPERTY(BlueprintReadOnly, Category = "Interview") FString LastReply;
	// T: type your own question (the local AI brain answers in character; questions alone don't add pressure)
	UPROPERTY(BlueprintReadOnly, Category = "Interview") bool bTyping = false;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") FString Draft;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") bool bThinking = false;
	// the brain's answer (computed on the asker's PC), shown on every machine
	void ApplyAnswer(APawn* User, const FString& Question, const FString& Answer);
	TArray<FString> BaggedIds() const;
	// a move in the interview (0 confront/press, 1 raise their record): runs on every machine, the same way
	void ApplyInterrogation(APawn* User, uint8 Action, int32 Statement, const FString& ClueId);

	// into the back of the van: gone from the scene, on the way to the precinct
	UFUNCTION(BlueprintCallable, Category = "Suspect") void LoadIntoVan();

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	TWeakObjectPtr<APawn> Escort;
	int32 Line = -1;
	void Pose();
	TWeakObjectPtr<APawn> Interviewer;
	void OpenInterview(APawn* User);
	void CloseInterview();
	void SendMove(uint8 Action);
	void StmtUp(); void StmtDown(); void EvLeft(); void EvRight(); void RecordKey();
	void TypeKey(FKey Key);
	void AskTyped();
};
