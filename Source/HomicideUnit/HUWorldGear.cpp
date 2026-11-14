// WHAT THE SQUAD SEES YOU DO: your body on a partner's screen carries what you do in first person.
// Your machine sends what's in your hand and what you're doing (SendGear: idle, working - bagging, printing,
// anything you hold E for - cuffing, or dragging furniture). On everyone else's machine your body's right arm
// swings up from the shoulder to hold the same tool, aims it where you look, reaches when you pick something up
// (the use itself reaches every machine, so PlayReach runs there too), works over what you're bagging, holds the
// cuffs out while you cuff, and your torch's beam comes from the lens of the torch in that hand.
#include "HUInteractComponent.h"
#include "HUBuild.h"
#include "HUCharacter.h"
#include "HUPersonAnimation.h"
#include "Components/SkeletalMeshComponent.h"
#include "HUTools.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/Pawn.h"

void UHUInteractComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHUInteractComponent, RepHeld);
	DOREPLIFETIME(UHUInteractComponent, RepPose);
}

void UHUInteractComponent::ServerGear_Implementation(uint8 InHeld, uint8 InPose) { RepHeld = InHeld; RepPose = InPose; }

void UHUInteractComponent::SendGear()
{
	const uint8 Pose = Grabbed.IsValid() ? PoseDrag : bHolding ? (Held == SlotCount + HUBelt::Cuffs ? PoseCuff : PoseWork) : PoseIdle;
	if (Held == SentHeld && Pose == SentPose) return;
	SentHeld = Held; SentPose = Pose;
	if (GetWorld()->GetNetMode() == NM_Standalone) return;
	if (GetOwner()->HasAuthority()) { RepHeld = (uint8)Held; RepPose = Pose; }
	else ServerGear((uint8)Held, Pose);
}

// a partner's copy of you on this screen
void UHUInteractComponent::TickWorldGear(APawn* Pawn, float Dt)
{
	using namespace HUBuild;
	const AHUCharacter* Character = Cast<AHUCharacter>(Pawn);
	USkeletalMeshComponent* Body = Character ? Character->VisualBody() : nullptr;

	// reaching for something (the use that reached this machine set ReachT)
	float ReachS = 0.f;
	if (ReachT >= 0.f)
	{
		ReachT += Dt;
		ReachS = FMath::Sin(PI * FMath::Clamp(ReachT / 0.45f, 0.f, 1.f));
		if (ReachT > 0.45f) ReachT = -1.f;
	}
	WorldT += Dt;

	// how far the arm swings up from hanging: 0 down by the side, 90 straight out, more is up
	const float Aim = FRotator::NormalizeAxis(Pawn->GetBaseAimRotation().Pitch);
	float Want = 0.f;
	if (RepHeld != 0) Want = FMath::Clamp(80.f + Aim * 0.85f, 25.f, 150.f);
	switch (RepPose)
	{
	case PoseWork: Want = 38.f + 9.f * FMath::Sin(WorldT * 7.f); break;        // working at something low, in short strokes
	case PoseCuff: Want = 70.f + 4.f * FMath::Sin(WorldT * 9.f); break;        // the cuffs held out, ratcheting
	case PoseDrag: Want = 82.f; break;                                          // a hand flat on the furniture
	default: break;
	}
	if (ReachS > 0.f) Want = FMath::Lerp(Want, 45.f, ReachS);                   // out and down for it
	WorldPitch = FMath::FInterpTo(WorldPitch, Want, Dt, 12.f);
	const bool bRaised = WorldPitch > 2.f;

	if (Body && Body->DoesSocketExist(TEXT("hand_r")))
	{
		if (UHUPersonAnimation* Animation = Cast<UHUPersonAnimation>(Body->GetAnimInstance())) Animation->RightArm = WorldPitch;
		if (!IsValid(WorldArm))
		{
			WorldArm = NewObject<USceneComponent>(GetOwner(), TEXT("HUWorldArm"));
			WorldArm->SetupAttachment(Pawn->GetRootComponent());
			WorldArm->RegisterComponent();
		}
		// A changed detective model has a new skeleton. Reattach without retaining primitive-arm tags.
		if (WorldArm->GetAttachParent() != Body)
			WorldArm->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
		if (WorldBuiltHeld != RepHeld)
		{
			WorldBuiltHeld = RepHeld;
			Clear(WorldArmParts);
			WorldLens = nullptr;
			HeldItemParts([&](uint8 Shape, FVector Pos, FVector Size, FLinearColor Col, FRotator Rot)
			{
				UStaticMeshComponent* Part = Block(GetOwner(), WorldArm, WorldArmParts, (EShape)Shape, Pos - FVector(4, 0, 0), Size, Col, Rot, false);
				if (Part) { Part->SetOwnerNoSee(true); Part->SetCastShadow(true); }
				return Part;
			}, RepHeld, WorldLens);
		}
		WorldArm->SetVisibility(bRaised && RepHeld != 0, true);
	}
	else if (WorldArm) WorldArm->SetVisibility(false, true);

	// their light, from the torch in their hand (or their eyes, if the arm isn't up yet)
	if (Beam)
	{
		const FVector From = WorldLens && WorldLens->IsVisible() ? WorldLens->GetComponentLocation() : Pawn->GetPawnViewLocation();
		Beam->SetWorldLocationAndRotation(From, Pawn->GetBaseAimRotation());
	}
}
