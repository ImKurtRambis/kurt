// LOOKING SOMETHING OVER: a piece of evidence held up in front of you, turned over with the mouse (part of
// UHUInteractComponent - see "LOOKING SOMETHING OVER" in its header, and AHUEvidence for picking up and bagging).
//
// What you hold is a copy of the thing's own parts, on your camera, brought to about a hand's size and turned about
// its middle; you straighten up with it, so it's seen against the room. You stand still, and the mouse turns it
// instead of your view (the view is put back every frame). Its mark is on its underside - a blood smear, scratched
// initials, a paper tag or a pawn ticket, picked by the clue so the squad sees the same one - and you start out
// looking at its top, so you have to turn it over. Hold the mark facing you for a moment and you read it: the clue's
// detail, and the offender profile it points to.
//
// -HUInspectTest on L_Scene (not in Shipping) runs it all by script: InspectTest at the end of this file.
#include "HUInteractComponent.h"
#include "HUEvidence.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace HUInspect
{
	constexpr float Hold = 34.f;     // how far in front of your eyes it's held (cm)
	constexpr float Across = 15.f;   // it's brought to about this size across (cm), whatever its own size
	constexpr float Facing = 0.77f;  // the mark faces you within about 40 degrees...
	constexpr float Look = 0.35f;    // ...held there this long (s), and you've seen it
	constexpr float Turn = 1.4f;     // degrees it turns for a degree of mouse
	constexpr float Raise = 0.25f;   // seconds to bring it up from where your hand was
}

void UHUInteractComponent::StartInspect(AHUEvidence* E)
{
	using namespace HUBuild;
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!E || !PC || !PC->IsLocalController()) return; // a partner's game: it's just gone from the floor there
	if (Inspecting.IsValid() || InspectRoot) EndInspect();
	CancelHold();
	Inspecting = E;
	bMarkSeen = false; MarkHeld = 0.f; InspectIn = 0.f;
	InspectView = PC->GetControlRotation();
	InspectView.Roll = 0.f;
	// you straighten up with it: held at chest height against the room, not low over the floor
	InspectPitch = FMath::Clamp(FRotator::NormalizeAxis(InspectView.Pitch), -12.f, 6.f);
	PC->SetIgnoreMoveInput(true); bInspectLocked = true;
	// (after the controller has taken this frame's mouse, so putting the view back never shows)
	PrimaryComponentTick.AddPrerequisite(PC, PC->PrimaryActorTick);

	UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>();
	USceneComponent* Attach = Cam ? (USceneComponent*)Cam : Pawn->GetRootComponent();
	InspectRoot = NewObject<USceneComponent>(Pawn);
	InspectRoot->SetupAttachment(Attach);
	InspectRoot->RegisterComponent();
	InspectPivot = NewObject<USceneComponent>(Pawn);
	InspectPivot->SetupAttachment(InspectRoot);
	InspectPivot->RegisterComponent();

	// its own parts, as they sit on it (a strongbox's contents: a small packet)
	TArray<UStaticMeshComponent*> From;
	E->HandLook(From);
	struct FPart { UStaticMeshComponent* C; FTransform T; };
	TArray<FPart> Parts;
	FBox Box(ForceInit);
	for (UStaticMeshComponent* C : From)
	{
		if (!C || !C->GetStaticMesh()) continue;
		const FTransform T = C->GetComponentTransform().GetRelativeTransform(E->GetActorTransform());
		Box += C->GetStaticMesh()->GetBoundingBox().TransformBy(T);
		Parts.Add({ C, T });
	}
	if (Parts.Num() == 0) Box = FBox(FVector(-7.f, -4.5f, -1.5f), FVector(7.f, 4.5f, 1.5f));
	const FVector Mid = Box.GetCenter(), Ext = Box.GetExtent().ComponentMax(FVector(1.5f));
	for (const FPart& P : Parts)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(Pawn);
		M->SetStaticMesh(P.C->GetStaticMesh());
		for (int32 i = 0; i < P.C->GetNumMaterials(); i++) M->SetMaterial(i, P.C->GetMaterial(i));
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		M->SetupAttachment(InspectPivot);
		FTransform T = P.T;
		T.SetLocation(T.GetLocation() - Mid); // turned about its middle
		M->SetRelativeTransform(T);
		M->RegisterComponent();
		InspectParts.Add(M);
	}
	if (Parts.Num() == 0) Block(Pawn, InspectPivot, InspectParts, EShape::Cube, FVector::ZeroVector, Ext * 2.f, FLinearColor(0.5f, 0.4f, 0.26f), FRotator::ZeroRotator, false); // a manila packet

	// THE MARK, on its underside. Which kind is the clue's own, so every squad player sees the same one.
	// (big enough to catch the eye on a thing turned in your hand, and pale or bright: an underside is in shadow)
	MarkSide = FVector(0, 0, -1);
	const uint32 Kind = GetTypeHash(E->ClueId);
	const float Z = -Ext.Z - 0.12f, W = FMath::Max(4.f, 1.5f * FMath::Min(Ext.X, Ext.Y));
	const FRotator Flat(0.f, (float)(Kind % 50) - 25.f, 0.f);
	switch (Kind % 4)
	{
	case 0: // a smear of dried blood
		Block(Pawn, InspectPivot, InspectParts, EShape::Cylinder, FVector(0, 0, Z), FVector(W, W * 0.65f, 0.2f), FLinearColor(0.42f, 0.05f, 0.035f), Flat, false);
		Block(Pawn, InspectPivot, InspectParts, EShape::Cylinder, FVector(0, 0, Z - 0.03f) + Flat.RotateVector(FVector(W * 0.38f, W * 0.22f, 0)), FVector(W * 0.4f, W * 0.3f, 0.2f), FLinearColor(0.3f, 0.035f, 0.025f), Flat, false);
		break;
	case 1: // initials scratched in, bright in the metal or the grain
		for (int32 i = 0; i < 3; i++)
			Block(Pawn, InspectPivot, InspectParts, EShape::Cube, FVector(0, 0, Z) + Flat.RotateVector(FVector((i - 1) * W * 0.3f, 0, 0)), FVector(0.7f, W * 0.55f, 0.2f), FLinearColor(0.9f, 0.86f, 0.76f), Flat + FRotator(0.f, i == 1 ? 0.f : (i ? 22.f : -22.f), 0.f), false);
		break;
	case 2: // a paper tag, written on
		Block(Pawn, InspectPivot, InspectParts, EShape::Cube, FVector(0, 0, Z), FVector(W, W * 0.6f, 0.2f), FLinearColor(0.92f, 0.88f, 0.76f), Flat, false);
		for (int32 i = 0; i < 2; i++)
			Block(Pawn, InspectPivot, InspectParts, EShape::Cube, FVector(0, 0, Z - 0.05f) + Flat.RotateVector(FVector(0, (i ? 1.f : -1.f) * W * 0.13f, 0)), FVector(W * 0.7f, 0.45f, 0.2f), FLinearColor(0.06f, 0.06f, 0.1f), Flat, false);
		break;
	default: // a pawn ticket stuck on, its number in red
		Block(Pawn, InspectPivot, InspectParts, EShape::Cube, FVector(0, 0, Z), FVector(W * 0.8f, W * 0.5f, 0.2f), FLinearColor(0.85f, 0.7f, 0.25f), Flat, false);
		Block(Pawn, InspectPivot, InspectParts, EShape::Cube, FVector(0, 0, Z - 0.05f), FVector(W * 0.45f, W * 0.14f, 0.2f), FLinearColor(0.5f, 0.06f, 0.04f), Flat, false);
		break;
	}
	for (UStaticMeshComponent* M : InspectParts) if (M) { M->SetCastShadow(false); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); }

	// about a hand's size, top towards you: the underside, with its mark, faces away
	InspectRoot->SetRelativeScale3D(FVector(FMath::Clamp(HUInspect::Across / (2.f * Ext.GetMax()), 0.4f, 3.f)));
	InspectTurn = FQuat(FRotator(40.f, 15.f + (float)(Kind % 3) * 10.f, 0.f));
	InspectPivot->SetRelativeRotation(InspectTurn);
	if (Hand) Hand->SetVisibility(false, true); // both hands on it
	FHUClue C;
	const UHUCaseSubsystem* S = Cases();
	Say(FString::Printf(TEXT("%s. Turn it over - move the mouse - and look for marks."), S && S->GetClue(E->ClueId, C) ? *C.Name : TEXT("You pick it up")));
	TickInspect(0.f);
}

void UHUInteractComponent::EndInspect()
{
	const bool bWas = Inspecting.IsValid() || InspectRoot != nullptr;
	Inspecting.Reset();
	bMarkSeen = false; MarkHeld = 0.f;
	for (UStaticMeshComponent* M : InspectParts) if (M) M->DestroyComponent();
	InspectParts.Reset();
	if (InspectPivot) { InspectPivot->DestroyComponent(); InspectPivot = nullptr; }
	if (InspectRoot) { InspectRoot->DestroyComponent(); InspectRoot = nullptr; }
	if (!bWas) return;
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && bInspectLocked) PC->SetIgnoreMoveInput(false);
	bInspectLocked = false;
	if (Hand) { Hand->SetVisibility(true, true); BuildHand(); }
}

FVector UHUInteractComponent::MarkFacing() const { return InspectTurn.RotateVector(MarkSide); }

void UHUInteractComponent::TickInspect(float Dt)
{
	AHUEvidence* E = Inspecting.Get();
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	// bagged, put back, gone with the level, or somebody else's now: your hands are free
	if (!E || !PC || !InspectRoot || !InspectPivot || E->HeldByPawn() != Pawn) { EndInspect(); return; }
	// you stand still (the menu letting you go mustn't let you walk off with it)
	if (!PC->IsMoveInputIgnored()) PC->SetIgnoreMoveInput(true);
	// the mouse turns it, not your view: right turns its near side right, up turns it up
	const FRotator Now = PC->GetControlRotation();
	const float DY = FRotator::NormalizeAxis(Now.Yaw - InspectView.Yaw), DP = FRotator::NormalizeAxis(Now.Pitch - InspectView.Pitch);
	InspectView.Pitch = FMath::FInterpTo(FRotator::NormalizeAxis(InspectView.Pitch), InspectPitch, Dt, 9.f);
	if (DY != 0.f || DP != 0.f || !FMath::IsNearlyEqual(FRotator::NormalizeAxis(Now.Pitch), FRotator::NormalizeAxis(InspectView.Pitch), 0.01f)) PC->SetControlRotation(InspectView);
	InspectTurn = FQuat(FRotator(0.f, -DY * HUInspect::Turn, 0.f)) * FQuat(FRotator(-DP * HUInspect::Turn, 0.f, 0.f)) * InspectTurn;
	InspectTurn.Normalize();
	InspectPivot->SetRelativeRotation(InspectTurn);
	// brought up from where your hand was
	InspectIn += Dt;
	const float A = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(InspectIn / HUInspect::Raise, 0.f, 1.f));
	InspectRoot->SetRelativeLocation(FMath::Lerp(FVector(HUInspect::Hold, 14.f, -16.f), FVector(HUInspect::Hold, 0.f, -2.f), A));
	// its mark: turned to face you, and held there a moment
	MarkHeld = -MarkFacing().X > HUInspect::Facing ? MarkHeld + Dt : 0.f;
	if (!bMarkSeen && MarkHeld >= HUInspect::Look)
	{
		bMarkSeen = true;
		Say(TEXT("Turned over: ") + E->MarkWords());
		FString Title, Body;
		E->MarkLead(Title, Body);
		if (!Title.IsEmpty()) Notify(Title, Body, FLinearColor(0.8f, 0.55f, 0.98f));
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit: %s looked over - its mark seen"), *E->ClueId);
	}
}

#if UE_BUILD_SHIPPING
void UHUInteractComponent::InspectTest(float) {}
#else
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

// -HUInspectTest on L_Scene: picks a piece of evidence up, turns it over until its mark faces the camera, puts it
// back, then picks it up again and bags it unread - checking each step ("HUInspectTest: ok ..." or "WRONG ..."), then
// "all checks passed" or how many were wrong; the game quits.
//   -HUInspectShots: also saves what's on screen at each step (Saved\Screenshots\Inspect_*.png; needs a window or
//   -RenderOffscreen).
void UHUInteractComponent::InspectTest(float Dt)
{
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("HUInspectTest"));
	if (!bOn || InspectStep >= 100) return;
	FApp::SetVolumeMultiplier(0.f);
	static const bool bShots = FParse::Param(FCommandLine::Get(), TEXT("HUInspectShots"));
	auto Shot = [](const TCHAR* Name) { if (bShots) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name, true, false); };
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* W = GetWorld();
	UHUCaseSubsystem* Cs = Cases();
	if (!PC || !W || !Cs) return;
	auto Check = [this](bool bOk, const FString& What) { if (!bOk) InspectWrong++; UE_LOG(LogTemp, Display, TEXT("HUInspectTest: %s  %s"), bOk ? TEXT("ok   ") : TEXT("WRONG"), *What); };
	auto Finish = [this, W]()
	{
		UE_LOG(LogTemp, Display, TEXT("HUInspectTest: %s"), InspectWrong ? *FString::Printf(TEXT("%d CHECKS WRONG"), InspectWrong) : TEXT("all checks passed"));
		InspectStep = 100;
		FTimerHandle Quit;
		W->GetTimerManager().SetTimer(Quit, FTimerDelegate::CreateLambda([]() { FPlatformMisc::RequestExit(false); }), 2.f, false);
	};
	auto Next = [this]() { InspectStep++; InspectClock = 0.f; };
	InspectClock += Dt;
	AHUEvidence* E = InspectSubject.Get();
	static FString Id; // (kept: once it's bagged the evidence is gone)
	if (E) Id = E->ClueId;
	// stand 70 cm from it on open floor, looking at it
	auto StandBy = [&](AHUEvidence* Ev)
	{
		const ACharacter* Ch = Cast<ACharacter>(Pawn);
		const float Half = Ch && Ch->GetCapsuleComponent() ? Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 92.f;
		const FVector At = Ev->GetComponentsBoundingBox().GetCenter();
		FCollisionQueryParams Q(TEXT("HUInspectTest"), false, Pawn);
		Q.AddIgnoredActor(Ev);
		for (int32 i = 0; i < 12; i++)
		{
			const FVector Spot = At + FRotator(0.f, i * 30.f, 0.f).Vector() * 70.f;
			FHitResult Floor;
			if (!W->LineTraceSingleByChannel(Floor, Spot + FVector(0, 0, 150), Spot - FVector(0, 0, 250), ECC_Visibility, Q)) continue;
			if (Floor.ImpactPoint.Z > At.Z + 10.f) continue; // a table top, not the floor
			const FVector Body = Floor.ImpactPoint + FVector(0, 0, Half + 2.f);
			const FVector Eye = Body + FVector(0, 0, Pawn->BaseEyeHeight);
			if (W->OverlapAnyTestByChannel(Body, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.f, Half - 4.f), Q)) continue;
			FHitResult See;
			if (W->LineTraceSingleByChannel(See, Eye, At, ECC_Visibility, Q)) continue;
			Pawn->SetActorLocation(Body, false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation((At - Eye).Rotation());
			return true;
		}
		return false;
	};

	switch (InspectStep)
	{
	case -1: // the scene is built: a plain piece lying in the open, with room to stand by it
	{
		if (InspectClock < 4.f) return;
		for (TActorIterator<AHUEvidence> It(W); It; ++It)
			if (It->Conceal == EHUConceal::Plain && !It->IsHidden() && It->GetActorEnableCollision() && StandBy(*It)) { InspectSubject = *It; break; }
		if (!InspectSubject.IsValid())
		{
			if (InspectClock > 40.f) { Check(false, TEXT("a plain piece of evidence lying in the open (run it on L_Scene)")); Finish(); }
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("HUInspectTest: standing by %s (%s)"), *InspectSubject->ClueId, *InspectSubject->GetName());
		Next();
		return;
	}
	case 0: // looking at it: what E will do
		if (InspectClock < 0.8f) return;
		Check(Focus == E && Prompt.ToString().StartsWith(TEXT("[E] Pick up")), FString::Printf(TEXT("looking at it, the prompt is \"%s\""), *Prompt.ToString()));
		Shot(TEXT("Inspect_1_Prompt"));
		Next();
		return;
	case 1: // E: picked up, in front of you
		if (InspectClock < 0.4f) return;
		if (InspectClock < 0.5f) { UsePrimary(); return; } // (held: it takes a moment)
		if (!Inspecting.IsValid() && InspectClock < 4.f) return;
		Check(Inspecting.Get() == E && E->HeldByPawn() == Pawn, TEXT("E picks it up: it's in your hands"));
		Check(E && E->IsHidden() && !E->GetActorEnableCollision(), TEXT("and gone from the floor"));
		Check(InspectParts.Num() > 0 && InspectRoot && (!Hand || !Hand->IsVisible()), FString::Printf(TEXT("held up in front of you (%d parts, mark included), the tool hand put away"), InspectParts.Num()));
		Next();
		return;
	case 2: // walking and looking around do nothing to you; the mouse turns it
	{
		if (InspectClock < 0.35f) return;
		static FVector Was; static FQuat TurnWas;
		if (InspectClock < 0.45f) { Shot(TEXT("Inspect_2_Held")); Was = Pawn->GetActorLocation(); TurnWas = InspectTurn; return; }
		if (InspectClock - Dt < 0.45f) Check(PC->IsMoveInputIgnored() && !bMarkSeen && Prompt.ToString().Contains(TEXT("Bag it")), FString::Printf(TEXT("you stand still, its mark isn't seen yet, the prompt is \"%s\""), *Prompt.ToString()));
		if (InspectClock < 1.2f) { Pawn->AddMovementInput(Pawn->GetActorForwardVector(), 1.f); PC->SetControlRotation(PC->GetControlRotation() + FRotator(0.f, 60.f * Dt, 0.f)); return; }
		Check(FVector::Dist2D(Was, Pawn->GetActorLocation()) < 5.f, FString::Printf(TEXT("pushing forward doesn't walk you away with it (moved %.1f cm)"), FVector::Dist2D(Was, Pawn->GetActorLocation())));
		Check(FMath::Abs(FRotator::NormalizeAxis(PC->GetControlRotation().Yaw - InspectView.Yaw)) < 0.5f && InspectTurn.AngularDistance(TurnWas) > 0.5f,
			FString::Printf(TEXT("the mouse turned it %.0f degrees and left your view where it was"), FMath::RadiansToDegrees(InspectTurn.AngularDistance(TurnWas))));
		Next();
		return;
	}
	case 3: // turned over (the mouse pulled towards you), until its mark faces you
		if (!bMarkSeen && InspectClock < 10.f) { PC->SetControlRotation(PC->GetControlRotation() + FRotator(-55.f * Dt, 0.f, 0.f)); return; }
		Check(bMarkSeen && Subtitle.ToString().StartsWith(TEXT("Turned over:")), FString::Printf(TEXT("turned over, its mark faces you after %.1f s: %s"), InspectClock, *Subtitle.ToString().Left(140)));
		Check(-MarkFacing().X > 0.7f, FString::Printf(TEXT("the mark is towards the camera (%.2f)"), -MarkFacing().X));
		Shot(TEXT("Inspect_3_Mark"));
		Next();
		return;
	case 4: // Q: back where it was, not bagged
		if (InspectClock < 0.6f) return;
		if (InspectClock < 0.7f) { UseSecondary(); return; }
		if (InspectClock < 1.0f) return;
		Check(!Inspecting.IsValid() && !InspectRoot && E && !E->IsHidden() && E->GetActorEnableCollision() && !E->HeldByPawn(), TEXT("Q puts it back: on the floor again, out of your hands"));
		Check(!Cs->HasFound(Id), TEXT("and not bagged"));
		Check(!PC->IsMoveInputIgnored() && (!Hand || Hand->IsVisible()), TEXT("you can walk again, the tool hand is back"));
		Shot(TEXT("Inspect_4_PutBack"));
		Next();
		return;
	case 5: // picked up again, and bagged without turning it over
		if (InspectClock < 0.4f) { StandBy(E); return; }
		if (InspectClock < 0.5f) { UsePrimary(); return; }
		if (!Inspecting.IsValid()) { if (InspectClock > 4.f) { Check(false, TEXT("picked up a second time")); Finish(); } return; }
		Check(!bMarkSeen, TEXT("picked up a second time, its mark is to find again"));
		UsePrimary(); // E in your hands: bag it
		Next();
		return;
	case 6:
		if (InspectClock < 0.5f) return;
		Check(Cs->HasFound(Id) && !IsValid(E), TEXT("E in your hands bags it: on the case's evidence, gone from the scene"));
		Check(!Inspecting.IsValid() && !PC->IsMoveInputIgnored(), TEXT("your hands are free and you can walk"));
		Check(Subtitle.ToString().Contains(TEXT("didn't turn it over")), FString::Printf(TEXT("bagged unread, you're told so: %s"), *Subtitle.ToString()));
		Shot(TEXT("Inspect_5_Bagged"));
		Finish();
		return;
	default: return;
	}
}
#endif
