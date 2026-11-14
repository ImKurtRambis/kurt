// YOUR REAL ARMS, POSED (HUArms.h builds them): your MetaHuman's own body - the same mesh, outfit and proportions the
// squad sees - drawn only for you, its head folded away and its eyes where your camera is, so only the arms come into
// view. Each hand reaches for its grip on the tool (two-bone IK, the elbow toward its pole), turns to the grip's frame
// and closes its fingers in the grip's shape; a hand with nothing to hold hangs down out of sight. Like
// HUPersonAnimation, every turn is measured off where the bones are, not off how a skeleton happens to roll them.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "HUArmsAnimation.generated.h"

// how a hand closes: degrees each finger joint bends toward the palm (knuckle, middle, tip), and the thumb
struct FHUHandShape
{
	float Finger[4][3] = {};  // index, middle, ring, pinky
	float ThumbAcross = 0.f;  // the thumb swung in under the palm
	float Thumb[2] = {};      // its two outer joints bending
};

struct FHUArmGrip
{
	bool bOn = false;
	// in the Frame component's space: the wrist, X along the hand toward the knuckles, Z out of the back of the hand
	// (a right hand's thumb is on -Y, a left hand's on +Y)
	FTransform Hand;
	FHUHandShape Shape;
	FVector Elbow = FVector(0, 0, -1); // which way the elbow points, in Frame
};

UCLASS(Transient)
class HOMICIDEUNIT_API UHUArmsAnimation : public UAnimSingleNodeInstance
{
	GENERATED_BODY()
public:
	FHUArmGrip Grip[2];                     // 0 left, 1 right
	TWeakObjectPtr<USceneComponent> Frame;  // the grips are in this component's space (the hand the tools hang on)
	TWeakObjectPtr<AActor> Source;          // the MetaHuman these arms are a copy of
	float Reach = 1.f, HandScale = 1.f;     // its arm and hand, next to the ones the grips were made for (HUArms)

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};
