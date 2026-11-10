// How your detective looks: picked at your NAME PLATE on the desk (your personnel file), saved, and sent to the
// squad so everyone sees you that way. The figure is a stand-in built from blocks, like the suspects, until
// there are real character models.
#pragma once

#include "CoreMinimal.h"
#include "HULook.generated.h"

USTRUCT(BlueprintType)
struct FHULook
{
	GENERATED_BODY()
	UPROPERTY() uint8 Build = 1;     // body shape: slim, average, broad, heavy
	UPROPERTY() uint8 Height = 3;    // 165 cm + 5 per step (0..7)
	UPROPERTY() uint8 Skin = 2;
	UPROPERTY() uint8 Hair = 1;      // bald, short, slicked back, long, curly
	UPROPERTY() uint8 HairColor = 0;
	UPROPERTY() uint8 Coat = 0;
	UPROPERTY() uint8 Hat = 1;       // none, fedora, flat cap

	bool operator==(const FHULook& O) const { return Build == O.Build && Height == O.Height && Skin == O.Skin && Hair == O.Hair && HairColor == O.HairColor && Coat == O.Coat && Hat == O.Hat; }
};

namespace HULook
{
	// the choices, in order: Build, Height, Skin, Hair, Hair colour, Coat, Hat
	constexpr int32 Fields = 7;
	HOMICIDEUNIT_API const TCHAR* FieldName(int32 Field);
	HOMICIDEUNIT_API int32 Options(int32 Field);
	HOMICIDEUNIT_API FString ValueName(const FHULook& L, int32 Field);
	HOMICIDEUNIT_API void Step(FHULook& L, int32 Field, int32 Dir);
	HOMICIDEUNIT_API float HeightCm(const FHULook& L);

	// builds the figure: feet at the parent's origin, facing +X. bOwnerNoSee hides it from its own player's camera.
	HOMICIDEUNIT_API void BuildFigure(AActor* Owner, USceneComponent* Parent, TArray<class UStaticMeshComponent*>& Out, const FHULook& L, bool bOwnerNoSee);
}
