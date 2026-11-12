// Per-player state everyone in the squad can see: are you ready for the shift?
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "HUPlayerState.generated.h"

UCLASS()
class HOMICIDEUNIT_API AHUPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Squad") bool bReady = false;
	// your own desk at the precinct (0..3; -1 = none yet). Handed out by the host as you arrive.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Squad") int32 Desk = -1;

	// your progress, shown to the squad and used by everyone's machine for perks (see HUProgress)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Squad") uint8 Specialty = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Squad") int32 SpecLevel = 1;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Squad") int32 Rank = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Squad") int32 Upgrades = 0;
	UFUNCTION(Server, Reliable) void ServerSetProgress(uint8 InSpecialty, int32 InSpecLevel, int32 InRank, int32 InUpgrades);
	// send this machine's saved progress to the squad (on joining, and whenever it changes)
	static void PushLocal(class UWorld* World);
	virtual void BeginPlay() override;

	// ask the host to mark you ready / not ready (works from any player's machine)
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Squad") void ServerSetReady(bool bNewReady);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
