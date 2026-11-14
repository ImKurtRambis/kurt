// REAL FURNITURE in place of blocks.
//
// The meshes live in /Game/Art/Props/SM_<Name>: made by Tools/PropKit/make_props.py (built for the game, plus five
// CC0 models from Poly Haven) and imported by Tools/ImportProps.py. Each is modelled at real size facing +X:
// floor props with their pivot on the floor, wall props (WallClock, CorkBoard*) with it on the wall.
//
// Prop() takes the same box a Block() call does - centre, size, rotation - and fits the mesh into it, so layouts,
// seats and collision stay where they were. It returns nullptr when the mesh isn't in this build (or with -HUNoProps,
// to compare): build the old blocks then. That keeps the game running without the art.
//   if (!Prop(this, R, Blocks, TEXT("Locker"), Pos, Size, FRotator(0, -90, 0))) { Block(...); ... }
//
// Names: Desk, OfficeChair, StackChair, CRTMonitor, Keyboard, DeskPhone, FilingCabinet, Locker, LockerOpen,
//        Bench (180 cm), BenchLong (420), RollCallBench (290, with a back), WaterCooler, WallClock,
//        CorkBoard, CorkBoardNotices (120 x 90), CorkBoardWide (250 x 150), CRTTV, DeskLamp
#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;
class UStaticMesh;

namespace HUBuild
{
	// Center/SizeCm/Rot as for Block: SizeCm is in the prop's own frame (X = depth front-to-back, Y = width, Z = height).
	// An axis given as 0 keeps the prop's proportions (e.g. (0, 0, 100): a chair 100 cm tall); all 0 = its real size.
	// FitAs: size and place it as that other prop would be (LockerOpen fitted like Locker, so its swung door sticks out)
	HOMICIDEUNIT_API UStaticMeshComponent* Prop(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out,
		const TCHAR* Name, FVector Center, FVector SizeCm, FRotator Rot = FRotator::ZeroRotator, bool bCollide = true, const TCHAR* FitAs = nullptr);

	// the furniture mesh by name, or nullptr (looked up once: a build without the art doesn't keep searching)
	HOMICIDEUNIT_API UStaticMesh* PropMesh(const TCHAR* Name);
}
