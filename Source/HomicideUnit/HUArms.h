// YOUR ARMS IN FIRST PERSON, posed round whatever you're holding, in the hand frame the tools are built in
// (UHUInteractComponent::HeldItemParts), so each hand closes on its tool's handle.
//  - RealArms: your own body's arms. A copy of the detective model the squad sees you as (HUPeople::Detective) - its
//    body, its outfit - that only you see, its head folded away and its eyes at your camera; UHUArmsAnimation reaches
//    each hand for its grip and closes the fingers.
//  - FirstPerson: the fallback when the detective model isn't built on this PC: block arms in the detective's colours
//    (HULook::BuildArm - sleeve, shirt cuff, skin).
// -HUHandShots on any level screenshots every pose (Saved/Screenshots/<platform>/Hands_*.png), then quits. In
// development builds Saved/HUArmsTune.txt, if it's there, overrides the real arms' grips (see HUArms.cpp).
#pragma once

#include "CoreMinimal.h"
#include "HULook.h"

namespace HUArms
{
	// InHeld as UHUInteractComponent::Held. bDrag: both palms flat on the furniture you're pushing. Frame: the hand
	// component the tools hang on (it rides on the camera). False when there's no MetaHuman to copy: build the blocks.
	HOMICIDEUNIT_API bool RealArms(AActor* Owner, USceneComponent* Frame, int32 InHeld, bool bDrag);
	// your body came, went or was built again since your arms were made: build them again
	HOMICIDEUNIT_API bool Stale(const AActor* Owner);
	// where the tools' frame sits from your eyes (camera space) while you hold InHeld, before it sways
	HOMICIDEUNIT_API FVector HandAt(const AActor* Owner, int32 InHeld);
	HOMICIDEUNIT_API void FirstPerson(AActor* Owner, USceneComponent* Root, TArray<class UStaticMeshComponent*>& Out, int32 InHeld, bool bDrag);
}
