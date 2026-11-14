// The suspects' brain: a small language model running ON THIS PC (llama.cpp's llama-server), so interviews
// cost nothing to play - no internet, no API, no tokens.
//
// Files (not in git - big; see AI\README.txt):
//   AI\llama\llama-server.exe     llama.cpp server, its Vulkan build (MIT)
//   AI\models\suspect.gguf        the model (a small Apache-2.0 instruct model)
// The first interview starts the server in the background on 127.0.0.1:8089; it shuts down with the game.
//
// WHERE IT RUNS is picked here, by itself (WarmUp):
//   1. the graphics card - any maker's. llama.cpp's Vulkan build runs on NVIDIA, AMD and Intel cards alike (on an
//      RTX 5070 Ti it writes as fast as the CUDA build, which is twelve times the download).
//   2. the processor - when llama.cpp finds no card, the server dies or never answers on the card, or the card
//      writes slower than a processor would (an old integrated chip, a model spilling out of video memory).
//      About ten times slower than a good card, but it answers.
// -HUBrain=card|cpu forces one: for testing, or as a launch option for a player whose card misbehaves.
//
// The game decides the facts. The prompt gives the suspect only what they know, the truth ONLY for lies that are
// already on the record, and their mood (from the pressure system). It is told never to confess unless the game
// says they've cracked, and never to invent evidence. If the brain isn't available, callers get an empty reply
// and fall back to written lines.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HUReaction.h"
#include "HUBrainSubsystem.generated.h"

DECLARE_DELEGATE_OneParam(FHUBrainReply, const FString& /*Reply, empty on failure*/);
DECLARE_DELEGATE_OneParam(FHUBrainReact, const FHUReaction& /*Say empty on failure*/);

UCLASS()
class HOMICIDEUNIT_API UHUBrainSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;

	// are the files there?
	UFUNCTION(BlueprintPure, Category = "Brain") bool IsInstalled() const;
	// is the server up and answering?
	UFUNCTION(BlueprintPure, Category = "Brain") bool IsReady() const { return bReady; }
	// start it (if installed) - safe to call often
	void WarmUp();
	// where the model is running: "graphics card" or "processor" (empty until it's up)
	UFUNCTION(BlueprintPure, Category = "Brain") FString RunsOn() const { return !bReady ? FString() : bOnCard ? TEXT("graphics card") : TEXT("processor"); }

	// ask a suspect something. Their conversation so far is remembered for this case.
	void AskSuspect(const FString& SuspectId, const FString& Question, FHUBrainReply Done);
	// THE THINKING BRAIN: what the detective just said (spoken), plus what the game decided happened (Situation:
	// evidence shown and whether it caught a lie, their record brought up...). Answers with the words to speak,
	// the emotion to speak them with, and what their body does - including obeying or refusing an order.
	void React(const FString& SuspectId, const FString& Said, const FString& Situation, FHUBrainReact Done);
	// forget every conversation (a new case)
	void ResetConversations() { History.Reset(); }

	// PREWRITTEN ANSWERS: once a case starts, the brain quietly writes every suspect's answers to the questions every
	// detective asks (where were you, how did you know the victim, what did you see, who did it, who can vouch, did
	// you do it) - one at a time, and only while nobody's waiting on a live answer. Asking one of those is instant.
	// An answer is thrown out (and written again) once that suspect's situation changes: a lie broken, their mood.
	void StartPrewrite();
	// a prewritten answer for this question, if there's a fresh one (it's then used up, remembered, and rewritten)
	bool TakeReady(const FString& SuspectId, const FString& Said, FHUReaction& Out);
	int32 ReadyCount() const { return Pre.Num(); }
	// stop writing answers ahead (the interview test bench measures live answers only)
	void StopPrewrite();
	// >= 0: the model samples with this seed, so the same model gives the same answers run to run (the test bench)
	int32 SampleSeed = -1;
	// the suspect's whole brief as the model gets it right now (the interview test bench logs it)
	FString PromptFor(const FString& SuspectId) const { return SystemPrompt(SuspectId); }
	bool IsWriting() const { return bPreBusy || LiveBusy > 0; }

	UPROPERTY(EditAnywhere, Category = "Brain") int32 Port = 8089;

private:
	FProcHandle Server;
	bool bStarting = false;
	bool bReady = false;
	double StartedAt = 0.0;
	TMap<FString, TArray<TPair<FString, FString>>> History; // suspect -> (question, answer)
	struct FPre { FHUReaction R; uint32 State = 0; };
	TMap<FString, FPre> Pre;                 // "suspect|topic" -> a prewritten answer
	TArray<TPair<FString, int32>> PreQueue;  // (suspect, topic) still to write
	bool bPreBusy = false;
	int32 LiveBusy = 0;                      // live answers in flight: they go first
	FTimerHandle PumpTimer;
	void Pump();
	uint32 StateOf(const FString& SuspectId) const;
	void Send(const FString& SuspectId, const FString& Said, const FString& Situation, bool bRemember, FHUBrainReact Done);
	FString Root() const;
	FString SystemPrompt(const FString& SuspectId) const;
	void Ping();

	// ---- where the model runs: the card, else the processor ----
	TArray<bool> Plan;        // what's still to try, in order: true = the graphics card, false = the processor
	bool bOnCard = true;      // what the server now running (or starting) uses
	bool bForced = false;     // -HUBrain= picked it: no second choice, no speed check
	FString Cards;            // the cards llama.cpp can use, as it names them (for the log)
	double AttemptAt = 0.0;   // when the current server was started
	double GaveUpAt = 0.0;    // when every choice had failed (no new try for five minutes)
	FProcHandle Lister;       // "llama-server --list-devices", writing to ListFile
	FString ListFile;
	void ListCards();         // wait for that list, then plan
	void StartNext();         // start the server on the next choice in Plan
	void GiveUpOn(const FString& Why); // this choice failed: stop its server, try the next
	void CheckSpeed();        // a card must write faster than a processor would
	void NowReady(double TokensPerSecond);
};
