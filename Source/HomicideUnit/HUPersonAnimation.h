// HOW THE CAST MOVES. Every MetaHuman's body runs this: the engine's own motion-captured clips underneath (the
// MetaHumanCharacter plugin's stand idle, walk and run loops - see HUPeople::Motion), and on top of them the poses the
// game needs and no clip covers, solved each frame from the skeleton itself:
//   SITTING     hips down onto a seat SeatHeight above the feet, thighs level, feet planted in front; hands in the
//               lap, or resting on a table (bTable) in front of them
//   CROUCHING   knees bent deep, back bent forward (a partner going through a crawlspace)
//   CUFFED      hands behind the back at the belt, elbows back - walking, standing or sitting
//   TALKING     TalkFor(seconds): hands come up and move with the words, the head nods on the beats
//   FLINCHING   Flinch(): caught out - a jolt back, shoulders up, head turning away, hands half up
//   ARMS        crossed (Arms 1) or hands up (Arms 2); RightArm aims the right arm (a partner's torch or tool)
//   HEAD        HeadYaw / HeadPitch turn the head (degrees: + to their right / + down)
// Limbs are placed with two-bone IK, and every turn is built from directions in the body's own frame (up, and right
// and forward measured off the shoulders), so nothing depends on how a particular skeleton's bones are rolled.
// -HUBodyTest on L_Precinct photographs every one of these (HUBodyTest.cpp).
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "HUPersonAnimation.generated.h"

UCLASS(Transient)
class HOMICIDEUNIT_API UHUPersonAnimation : public UAnimSingleNodeInstance
{
	GENERATED_BODY()
public:
	float RightArm = 0.f;                 // the right arm raised to this pitch (0 hanging, 90 level ahead, 150 up)
	float HeadPitch = 0.f, HeadYaw = 0.f; // degrees: + down, + to their right
	bool bSeated = false, bCrouched = false, bCuffed = false;
	float SeatHeight = 46.f;              // the seat's top, cm above the feet
	bool bTable = false;                  // seated at a table: hands rest on it
	float TableHeight = 76.f;             // the table top, cm above the feet
	uint8 Arms = 0;                       // 0 relaxed, 1 crossed, 2 raised

	// talk with the hands and head for this long (seconds), from now
	void TalkFor(float Seconds) { TalkLeft = FMath::Max(TalkLeft, Seconds); }
	void StopTalking() { TalkLeft = 0.f; }
	// caught out: a jolt (1 = a lie proven; more for worse)
	void Flinch(float Strength = 1.f) { FlinchStrength = Strength; ++FlinchCount; }

	// read by the pose each frame
	float TalkLeft = 0.f;
	int32 FlinchCount = 0;
	float FlinchStrength = 1.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
