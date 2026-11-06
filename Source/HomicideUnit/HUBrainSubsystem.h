// The suspects' brain: a small language model running ON THIS PC (llama.cpp's llama-server), so interviews
// cost nothing to play - no internet, no API, no tokens.
//
// Files (not in git - big; see AI\README.txt):
//   AI\llama\llama-server.exe     llama.cpp server build (MIT)
//   AI\models\suspect.gguf        the model (a small Apache-2.0 instruct model)
// The first interview starts the server in the background on 127.0.0.1:8089; it shuts down with the game.
//
// The game decides the facts. The prompt gives the suspect only what they know, the truth ONLY for lies that are
// already on the record, and their mood (from the pressure system). It is told never to confess unless the game
// says they've cracked, and never to invent evidence. If the brain isn't available, callers get an empty reply
// and fall back to written lines.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HUBrainSubsystem.generated.h"

DECLARE_DELEGATE_OneParam(FHUBrainReply, const FString& /*Reply, empty on failure*/);

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

	// ask a suspect something. Their conversation so far is remembered for this case.
	void AskSuspect(const FString& SuspectId, const FString& Question, FHUBrainReply Done);
	// forget every conversation (a new case)
	void ResetConversations() { History.Reset(); }

	UPROPERTY(EditAnywhere, Category = "Brain") int32 Port = 8089;

private:
	FProcHandle Server;
	bool bStarting = false;
	bool bReady = false;
	double StartedAt = 0.0;
	TMap<FString, TArray<TPair<FString, FString>>> History; // suspect -> (question, answer)
	FString Root() const;
	FString SystemPrompt(const FString& SuspectId) const;
	void Ping();
};
