// Small helpers for building placeholder rooms out of the engine's basic shapes.
#pragma once

#include "CoreMinimal.h"
#include "HUSurfaces.h"

class UStaticMeshComponent;
class UStaticMesh;

namespace HUBuild
{
	enum class EShape : uint8 { Cube, Cylinder, Sphere };

	// a colored block, SizeCm in centimeters, attached to Parent and remembered in Out (so it can be cleared on rebuild).
	// Surface: what it's made of (HUSurfaces.h) - the texture is tinted to average Color; None = flat Color.
	HOMICIDEUNIT_API UStaticMeshComponent* Block(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out,
		EShape Shape, FVector Pos, FVector SizeCm, FLinearColor Color, FRotator Rot = FRotator::ZeroRotator, bool bCollide = true,
		EHUSurface Surface = EHUSurface::None);

	HOMICIDEUNIT_API void Clear(TArray<UStaticMeshComponent*>& Out);

	// a wall along X or Y with a doorway cut in it (DoorAt = where along the wall, 0 = middle)
	HOMICIDEUNIT_API void WallWithDoor(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out,
		bool bAlongY, FVector Center, float Length, float Height, float Thick, float DoorAt, float DoorW, float DoorH, FLinearColor Color,
		EHUSurface Surface = EHUSurface::None);
}
