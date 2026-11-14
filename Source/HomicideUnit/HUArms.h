// YOUR ARMS IN FIRST PERSON: the same arms as your body (HULook::BuildArm - your coat's sleeve, your shirt cuff,
// your skin, as thick as your build), posed round whatever you're holding. They're built in the hand frame the
// tools are built in (UHUInteractComponent::HeldItemParts), so each hand closes on its tool's handle.
// -HUHandShots on any level screenshots every pose (Saved/Screenshots/<platform>/Hands_*.png), then quits.
#pragma once

#include "CoreMinimal.h"
#include "HULook.h"

namespace HUArms
{
	// InHeld as UHUInteractComponent::Held. bDrag: both palms flat on the furniture you're pushing.
	HOMICIDEUNIT_API void FirstPerson(AActor* Owner, USceneComponent* Root, TArray<class UStaticMeshComponent*>& Out, const FHULook& L, int32 InHeld, bool bDrag);
}
