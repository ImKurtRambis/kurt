#include "HUArms.h"
#include "HUBuild.h"
#include "HUInteractComponent.h"
#include "HUCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

using HULook::EHand;

namespace
{
	// where a hand closes on what it holds, in the hand's own frame (wrist at the origin, fingers +X, back of the hand +Z)
	FVector GripOf(EHand S)
	{
		switch (S)
		{
		case EHand::Fist:  return FVector(4.3f, 0, -1.5f); // the handle runs through the fist along X
		case EHand::Pinch: return FVector(10.f, 0, -1.6f); // between the fingertips and the thumb
		default:           return FVector(4.f, 0, 0);      // the middle of the palm
		}
	}
}

void HUArms::FirstPerson(AActor* Owner, USceneComponent* Root, TArray<UStaticMeshComponent*>& Out, const FHULook& L, int32 InHeld, bool bDrag)
{
	// a hand closing on Grip (a point on the tool) turned Rot; bent at the wrist, the sleeve runs back, down and out to
	// its own side so it leaves the view (a touch slimmer than the body's: it's right under your nose)
	auto Arm = [&](bool bLeft, EHand S, FVector Grip, FRotator Rot, FVector Fore = FVector::ZeroVector)
	{
		const FQuat Q = Rot.Quaternion();
		if (Fore.IsNearlyZero()) Fore = bLeft ? FVector(-0.55f, -0.4f, -0.73f) : FVector(-0.72f, 0.42f, -0.55f); // the left one drops away steeply: it's in the middle of the view
		HULook::BuildArm(Owner, Root, Out, L, FTransform(Q, Grip - Q.RotateVector(GripOf(S))), 34.f, bLeft, S, HULook::ESeen::OnlyOwner, Fore, 0.85f);
	};
	// pushing furniture: both palms flat against it, fingers up
	if (bDrag)
	{
		Arm(false, EHand::Flat, FVector(4, 15, 2), FRotator(70, -8, 0), FVector(-0.55f, 0.25f, -0.8f));
		Arm(true, EHand::Flat, FVector(4, -15, 2), FRotator(70, 8, 0), FVector(-0.55f, -0.25f, -0.8f));
		return;
	}
	const int32 Belt = UHUInteractComponent::SlotCount;
	switch (InHeld)
	{
	case 1: // flashlight
	case 2: // UV light: the fist round the barrel
		Arm(false, EHand::Fist, FVector(6, 0, 0), FRotator::ZeroRotator);
		break;
	case 3: // print kit: the case on your left palm, the brush between right finger and thumb
		Arm(true, EHand::Relaxed, FVector(8.5f, 0, -3.3f), FRotator(0, 25, 180));
		Arm(false, EHand::Pinch, FVector(9, 3, 5), FRotator(0, -10, 0));
		break;
	case 4: // crowbar: right fist low on the bar, left fist higher up it
	{
		const FVector Mid(12, 0, 10), Along = FRotator(60, 0, 0).Vector();
		Arm(false, EHand::Fist, Mid - Along * 14.f, FRotator(60, 0, 0));
		Arm(true, EHand::Fist, Mid - Along * 3.f, FRotator(60, 30, 0), FVector(-0.45f, -0.35f, -0.82f));
		break;
	}
	case 5: // scanner: on your palm, screen up
		Arm(false, EHand::Relaxed, FVector(8.9f, 0, -1.1f), FRotator(-20, 0, 180));
		break;
	default:
		if (InHeld == Belt + HUBelt::Cuffs) Arm(false, EHand::Pinch, FVector(10, 0, 2), FRotator::ZeroRotator);       // by the chain
		else if (InHeld == Belt + HUBelt::Pepper) Arm(false, EHand::Fist, FVector(8, 0, 3), FRotator(80, 0, 0));      // round the can
		else if (InHeld == Belt + HUBelt::Taser) Arm(false, EHand::Fist, FVector(4, 0, -2), FRotator(75, 0, 0));      // the pistol grip
		else if (InHeld == Belt + HUBelt::Radio) Arm(false, EHand::Fist, FVector(8, 0, 3), FRotator(80, 0, 0));       // held up
		else Arm(false, EHand::Relaxed, FVector(4, 0, -2.5f), FRotator(8, -6, 0));                                    // empty
		break;
	}
}

#if !UE_BUILD_SHIPPING
// -HUHandShots: hold each thing in turn (whether you own it or not), and the push pose, and screenshot each
void UHUInteractComponent::HandShots(float Dt)
{
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("HUHandShots"));
	if (!bOn || ShotStep < 0) return;
	// -HUHandLook=Coat,Skin,Build: dress yourself first, to see the arms follow (the tick rebuilds them)
	FString Dress;
	if (ShotStep == 0 && ShotClock == 0.f && FParse::Value(FCommandLine::Get(), TEXT("HUHandLook="), Dress, false))
		if (AHUCharacter* Ch = Cast<AHUCharacter>(GetOwner()))
		{
			TArray<FString> V; Dress.ParseIntoArray(V, TEXT(","));
			if (V.Num() > 0) Ch->Look.Coat = (uint8)FCString::Atoi(*V[0]);
			if (V.Num() > 1) Ch->Look.Skin = (uint8)FCString::Atoi(*V[1]);
			if (V.Num() > 2) Ch->Look.Build = (uint8)FCString::Atoi(*V[2]);
		}
	const int32 Poses[] = { 0, 1, 2, 3, 4, 5, SlotCount + HUBelt::Cuffs, SlotCount + HUBelt::Pepper, SlotCount + HUBelt::Taser, SlotCount + HUBelt::Radio, -1 };
	const int32 N = UE_ARRAY_COUNT(Poses);
	ShotClock += Dt;
	const float At = 5.f + ShotStep * 1.5f;
	if (ShotStep >= N) { if (ShotClock > At) { ShotStep = -1; FPlatformMisc::RequestExit(false); } return; }
	if (!bShotPosed && ShotClock >= At)
	{
		const int32 H = Poses[ShotStep];
		Held = H < 0 ? 0 : H;
		if (H < 0) // the push pose, built straight in (there's no furniture in your hands)
		{
			HUBuild::Clear(HandParts);
			HUArms::FirstPerson(GetOwner(), Hand, HandParts, HandLook, 0, true);
		}
		else BuildHand();
		bShotPosed = true;
	}
	if (bShotPosed && ShotClock >= At + 0.7f)
	{
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Hands_%02d"), ShotStep), false, false);
		ShotStep++; bShotPosed = false;
	}
}
#else
void UHUInteractComponent::HandShots(float) {}
#endif
