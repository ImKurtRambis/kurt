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

	// ask the host to mark you ready / not ready (works from any player's machine)
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Squad") void ServerSetReady(bool bNewReady);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
