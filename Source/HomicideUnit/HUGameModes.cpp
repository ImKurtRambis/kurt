#include "HUGameModes.h"
#include "HUCharacter.h"
#include "HUPlayerState.h"

AHUGameMode::AHUGameMode()
{
	DefaultPawnClass = AHUCharacter::StaticClass();
	PlayerStateClass = AHUPlayerState::StaticClass();
	// keep the squad's connection open between levels (squad room -> briefing -> scene)
	bUseSeamlessTravel = true;
}

AHUMenuGameMode::AHUMenuGameMode()
{
	DefaultPawnClass = nullptr; // at the desk you're just a camera
}
