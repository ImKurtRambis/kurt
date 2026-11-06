// What carries over between play sessions: your cash and the tools you've bought.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "HUSave.generated.h"

UCLASS()
class HOMICIDEUNIT_API UHUSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 Cash = 250;
	UPROPERTY() int32 OwnedTools = 0; // bitmask of EHUTool
	UPROPERTY() int32 CasesSolved = 0;
	UPROPERTY() FString DetectiveName;
	UPROPERTY() FString JoinAddress = TEXT("127.0.0.1");
	UPROPERTY() float MouseSensitivity = 1.f;
	UPROPERTY() float FieldOfView = 90.f;
	UPROPERTY() uint8 VoiceProvider = 2; // ElevenLabs
	UPROPERTY() FString VoiceKey;
	UPROPERTY() bool bVoiceQuestions = false;
};
