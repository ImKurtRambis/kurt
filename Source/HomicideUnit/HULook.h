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

	// ---- ONE ARM, shared by the body everyone sees and your own first-person view, so they're the same arm ----
	// the coat sleeve (SleeveLen back from the wrist), the shirt cuff, and the hand in a shape. Built in frame Xf: the
	// wrist at its origin, forearm along -X, fingers along +X, back of the hand +Z; a right hand's thumb is on -Y
	// (palm down, it points across you), a left hand's on +Y.
	enum class EHand : uint8 { Relaxed, Fist, Flat, Pinch };
	enum class ESeen : uint8 { Everyone, NotByOwner, OnlyOwner }; // a body / your own body / your first-person arms
	// ForearmDir (in Parent's space): bend at the wrist, the sleeve running that way instead of straight back from the hand
	// (first person: back, down and out to the side, so the arm leaves the view). Thickness scales the sleeve.
	HOMICIDEUNIT_API void BuildArm(AActor* Owner, USceneComponent* Parent, TArray<class UStaticMeshComponent*>& Out, const FHULook& L,
		const FTransform& Xf, float SleeveLen, bool bLeft, EHand Shape, ESeen Seen, FVector ForearmDir = FVector::ZeroVector, float Thickness = 1.f);
	// the sleeve's thickness for this build (tools in the hand are placed around the fist)
	HOMICIDEUNIT_API float SleeveThickness(const FHULook& L);
}
