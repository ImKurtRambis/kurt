// Narrow editor bridge for refreshing the canonical station maps in Tools/RefreshStationMaps.py.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HUStationMapLibrary.generated.h"

UCLASS()
class HOMICIDEUNIT_API UHUStationMapLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,Category="Station maps") static void RefreshBuilder(AActor* Builder);
};
