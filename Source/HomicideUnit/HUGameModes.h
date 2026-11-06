// HUGameMode: the game itself (you play AHUCharacter). HUMenuGameMode: the desk menu (no body; the desk's camera).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HUGameModes.generated.h"

UCLASS()
class HOMICIDEUNIT_API AHUGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AHUGameMode();
};

UCLASS()
class HOMICIDEUNIT_API AHUMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AHUMenuGameMode();
};
