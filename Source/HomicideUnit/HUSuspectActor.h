// A person of interest at the scene (a stand-in figure for now).
//   E starts the interview (you talk to them out loud - see below).   Q runs their record.
//   The CUFFS on your belt (look down, pick them, click) arrest them: cuffed, hands behind their back, and they follow you.
//   Pepper spray or the taser (also on the belt) doubles them over for a few seconds.
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
	// ---- the interview: YOU TALK, THEY REACT (yours alone on screen; what's said is shared with the squad) ----
	//   Hold V and speak to them - anything. A few things you say change the case, and the game decides those:
	//     name a piece of evidence you've bagged ("explain this receipt")  -> confront them with it
	//     bring up their record ("you've got a record")                     -> raise it
	//     call it a lie ("you're lying", "tell me the truth")               -> press them
	//     "that's all" / "we're done"                                      -> end the interview
	//   Everything else (questions, orders like "sit down", "look at me", "show me your hands") goes to the
	//   THINKING brain, which decides what they say, how they feel and what their body does - including whether they
	//   obey. Their VOICE speaks it with that emotion. [T] types instead of speaking, [Q] leaves.
	UPROPERTY(BlueprintReadOnly, Category = "Interview") bool bInterviewing = false;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") FString LastReply;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") bool bTyping = false;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") FString Draft;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") bool bThinking = false;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") bool bMicOpen = false;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") FString Heard; // what you said last (as heard)
	// how they're doing it: their emotion and what their body just did (for the HUD)
	UPROPERTY(BlueprintReadOnly, Category = "Interview") uint8 Emotion = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Interview") uint8 LastAction = 0;
	// the thinking brain's answer (computed on the asker's PC), performed on every machine
	void ApplyAnswer(APawn* User, const FString& Question, const FString& Answer, uint8 InEmotion, uint8 InAction);
	TArray<FString> BaggedIds() const;
	// a move in the interview (0 confront/press, 1 raise their record; +0x80 = said out loud, the asker's brain
	// answers instead of the written line): runs on every machine, the same way
	void ApplyInterrogation(APawn* User, uint8 Action, int32 Statement, const FString& ClueId);

	// pepper spray / taser from someone's belt: doubled over for a few seconds (runs on every machine)
	void Subdue(APawn* User, bool bTaser);
	UFUNCTION(BlueprintPure, Category = "Suspect") bool IsSubdued() const { return StunLeft > 0.f; }

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
	float StunLeft = 0.f;
	void Pose();
	TWeakObjectPtr<APawn> Interviewer;
	void OpenInterview(APawn* User);
	void CloseInterview();
	void TypeKey(FKey Key);
	void Speak(const FString& Said, uint8 InEmotion = 255);
	TWeakObjectPtr<class UAudioComponent> VoiceAudio;
	void AskTyped();
	void MicDown(); void MicUp();
	// what you said: the game's part (evidence, record, lie), then the thinking brain
	void Handle(const FString& Said);
	void Think(const FString& Said, const FString& Situation, const FString& Fallback);
	FString LastSaid;
	FString MatchClue(const FString& Said) const;
	int32 MatchStatement(const FString& Said, const FString& ClueId) const;
	// acting it out: stance and arms stay, gestures play for a moment
	void Act(uint8 Action);
	void Animate(float DeltaSeconds);
	uint8 Stance = 0;    // 0 standing, 1 sitting
	uint8 Arms = 0;      // 0 at the sides, 1 crossed, 2 hands up
	float ArmsLeft = 0.f;
	uint8 Gesture = 0;
	float GestureT = -1.f;
	float FaceYaw = 0.f;
	bool bFacing = false;
	TArray<FTransform> BaseXf; // the parts' rest positions (legs, body, head, arms)
};
