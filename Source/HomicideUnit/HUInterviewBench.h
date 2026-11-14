// THE INTERVIEW TEST BENCH (-HUInterviewBench): measures the suspects' brain the way the game uses it, so every
// change to the model or its prompt can be compared against the last one.
//
// Fixed cases (the same seeds every run). In each, three kinds of suspect - the culprit, an innocent who lies, an
// honest one - get the scripted interview in Tools/InterviewBench/questions.json (about 50 lines each): ordinary
// questions, questions fishing for invented facts, attempts to break character, accusations, and the game's own
// pressing / wrong evidence / right evidence / criminal record, sent with exactly the GAME: lines the interview sends
// (UHUCaseSubsystem::InterrogationSituation). The model samples with a fixed seed.
//
// Every turn goes to <out>/turns.jsonl with the suspect's brief at that moment; Tools/InterviewBench/score.py scores
// it. Run the whole thing with Tools\InterviewBench.ps1.
//   -HUBenchOut=<dir>   where to write (default Saved/InterviewBench)
//   -HUBenchCases=<n>   how many cases (default 2)
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "HUInterviewBench.generated.h"

UCLASS()
class HOMICIDEUNIT_API UHUInterviewBench : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	struct FLine { FString Id, Ask, Do; TArray<FString> Kinds, Checks; };
	struct FRun { FString SuspectId, Kind; };
	TArray<FLine> Lines;
	TArray<int32> Seeds;
	TArray<FRun> Runs;     // this case's suspects, one of each kind
	TArray<int32> Script;  // the lines for the current run's kind
	int32 SeedAt = -1, RunAt = 0, LineAt = 0, LastBroken = INDEX_NONE, Turns = 0;
	bool bBrainUp = false, bWaiting = false, bDone = false;
	double StartedAt = 0.0, AskedAt = 0.0;
	FString OutDir, TurnsFile;
	FTSTicker::FDelegateHandle Ticker;

	bool Tick(float DeltaSeconds);
	bool NextCase();
	void StartRun();
	void Step();
	void Finish(const FString& Why);
	void Write(const TSharedRef<class FJsonObject>& Obj);
	FString Fill(const FString& Text, const struct FHUSuspect& S, int32 Statement) const;
	class UHUCaseSubsystem* Cases() const;
	class UHUBrainSubsystem* Brain() const;
};
