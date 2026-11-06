#include "HUPlayerState.h"
#include "Net/UnrealNetwork.h"

void AHUPlayerState::ServerSetReady_Implementation(bool bNewReady)
{
	bReady = bNewReady;
}

void AHUPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHUPlayerState, bReady);
}
