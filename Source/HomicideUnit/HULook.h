// The detective's arm in blocks: the fallback first-person arms (HUArms::FirstPerson) on a PC where the detective
// model isn't built. Every player is the same detective (HUPeople::Detective), so there is nothing to choose here:
// the sleeve and the skin are the one detective's.
#pragma once

#include "CoreMinimal.h"

namespace HULook
{
	// ---- ONE ARM: the shirt sleeve (SleeveLen back from the wrist), the cuff, and the hand in a shape. Built in frame
	// Xf: the wrist at its origin, forearm along -X, fingers along +X, back of the hand +Z; a right hand's thumb is on
	// -Y (palm down, it points across you), a left hand's on +Y.
	enum class EHand : uint8 { Relaxed, Fist, Flat, Pinch };
	enum class ESeen : uint8 { Everyone, NotByOwner, OnlyOwner }; // a body / your own body / your first-person arms
	// ForearmDir (in Parent's space): bend at the wrist, the sleeve running that way instead of straight back from the hand
	// (first person: back, down and out to the side, so the arm leaves the view). Thickness scales the sleeve.
	HOMICIDEUNIT_API void BuildArm(AActor* Owner, USceneComponent* Parent, TArray<class UStaticMeshComponent*>& Out,
		const FTransform& Xf, float SleeveLen, bool bLeft, EHand Shape, ESeen Seen, FVector ForearmDir = FVector::ZeroVector, float Thickness = 1.f);
}
