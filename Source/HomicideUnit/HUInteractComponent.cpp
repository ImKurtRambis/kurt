#include "HUInteractComponent.h"
#include "HUProgress.h"
#include "HUInteractable.h"
#include "HUPromptWidget.h"
#include "HUCaseSubsystem.h"
#include "HUEvidence.h"
#include "HUBuild.h"
#include "HUCharacter.h"
#include "HUSuspectActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameUserSettings.h"
#include "HUVoiceSubsystem.h"
#include "HUListenSubsystem.h"
#include "Components/InputComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "HUCaseDesk.h"
#include "HUPlayerState.h"

// the beam: how far it reaches and how wide (half-angle, degrees)
static const float BeamRange = 1100.f, BeamCone = 22.f;

UHUInteractComponent::UHUInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true); // carries the squad's interactions
}

UHUCaseSubsystem* UHUInteractComponent::Cases() const
{
	return GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
}

void UHUInteractComponent::Setup(APlayerController* PC)
{
	// our own input layer, so these keys work whatever the character's input mapping is
	Keys = NewObject<UInputComponent>(PC, TEXT("HUInteractKeys"));
	Keys->bBlockInput = false;
	Keys->BindKey(EKeys::E, IE_Pressed, this, &UHUInteractComponent::UsePrimary);
	Keys->BindKey(EKeys::Q, IE_Pressed, this, &UHUInteractComponent::UseSecondary);
	Keys->BindKey(EKeys::E, IE_Released, this, &UHUInteractComponent::ReleasePrimary);
	Keys->BindKey(EKeys::Q, IE_Released, this, &UHUInteractComponent::ReleaseSecondary);
	Keys->BindKey(EKeys::F, IE_Pressed, this, &UHUInteractComponent::ToggleFlashlight);
	Keys->BindKey(EKeys::V, IE_Pressed, this, &UHUInteractComponent::ToggleUV);
	Keys->BindKey(EKeys::R, IE_Pressed, this, &UHUInteractComponent::Scan);
	Keys->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &UHUInteractComponent::UseHeld);
	Keys->BindKey(EKeys::One, IE_Pressed, this, &UHUInteractComponent::Slot1);
	Keys->BindKey(EKeys::Two, IE_Pressed, this, &UHUInteractComponent::Slot2);
	Keys->BindKey(EKeys::Three, IE_Pressed, this, &UHUInteractComponent::Slot3);
	Keys->BindKey(EKeys::Four, IE_Pressed, this, &UHUInteractComponent::Slot4);
	Keys->BindKey(EKeys::Five, IE_Pressed, this, &UHUInteractComponent::Slot5);
	Keys->BindKey(EKeys::Six, IE_Pressed, this, &UHUInteractComponent::Slot6);
	Keys->BindKey(EKeys::G, IE_Pressed, this, &UHUInteractComponent::Ping);
	Keys->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &UHUInteractComponent::WheelUp);
	Keys->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &UHUInteractComponent::WheelDown);
	Keys->RegisterComponent();
	PC->PushInputComponent(Keys);

	Widget = CreateWidget<UHUPromptWidget>(PC, UHUPromptWidget::StaticClass());
	if (Widget) { Widget->Owner = this; Widget->AddToViewport(10); }

	// the light and the hand both ride on the camera
	APawn* Pawn = Cast<APawn>(GetOwner());
	UCameraComponent* Cam = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	USceneComponent* Attach = Cam ? (USceneComponent*)Cam : GetOwner()->GetRootComponent();
	Beam = NewObject<USpotLightComponent>(GetOwner(), TEXT("HUBeam"));
	Beam->SetupAttachment(Attach);
	if (!Cam) Beam->SetRelativeLocation(FVector(0, 0, 60));
	Beam->SetAttenuationRadius(BeamRange);
	Beam->SetInnerConeAngle(BeamCone * 0.6f);
	Beam->SetOuterConeAngle(BeamCone);
	Beam->SetCastShadows(true);
	Beam->RegisterComponent();
	Hand = NewObject<USceneComponent>(GetOwner(), TEXT("HUHand"));
	Hand->SetupAttachment(Attach);
	Hand->SetRelativeLocation(Cam ? FVector(30, 15, -15) : FVector(30, 15, 45));
	Hand->RegisterComponent();
	BuildHand();
	UpdateBeam();
	// the belt rides on the body (turns with you, doesn't tilt with your view)
	Belt = NewObject<USceneComponent>(GetOwner(), TEXT("HUBelt"));
	Belt->SetupAttachment(GetOwner()->GetRootComponent());
	Belt->SetRelativeLocation(FVector(0, 0, 2));
	Belt->RegisterComponent();
	BuildBelt();
	// the light that drifts over whatever you're looking at, like the desk menu's
	Glint = NewObject<USpotLightComponent>(GetOwner(), TEXT("HUGlint"));
	Glint->SetupAttachment(GetOwner()->GetRootComponent());
	Glint->SetUsingAbsoluteLocation(true);
	Glint->SetUsingAbsoluteRotation(true);
	Glint->SetWorldRotation(FRotator(-90, 0, 0));
	Glint->SetIntensity(0.f);
	Glint->SetAttenuationRadius(260.f);
	Glint->SetInnerConeAngle(14.f);
	Glint->SetOuterConeAngle(26.f);
	Glint->SetLightColor(FLinearColor(0.8f, 0.92f, 1.f));
	Glint->SetCastShadows(false);
	Glint->RegisterComponent();
}

// ---------- the hotbar ----------
bool UHUInteractComponent::SlotUsable(int32 Slot) const
{
	if (Slot == 0) return true;
	const UHUCaseSubsystem* C = Cases();
	return C && Slot > 0 && Slot < SlotCount && C->HasTool((EHUTool)(Slot - 1));
}

void UHUInteractComponent::SelectSlot(int32 Slot)
{
	if (Interviewing.IsValid()) return; // 1-6 ask questions in an interview
	if (Slot < 0 || Slot >= SlotCount || Slot == Held) return;
	if (!SlotUsable(Slot)) { Say(FString::Printf(TEXT("You don't have a %s yet. The supply locker in the motor pool sells one."), HUTools::Name((EHUTool)(Slot - 1)))); return; }
	Held = Slot;
	bLightOn = false;
	BuildHand();
	UpdateBeam();
}

void UHUInteractComponent::WheelUp() { if (bBeltOpen) { SetBeltSel(BeltSel - 1, BeltYaw); return; } for (int32 i = 1; i < SlotCount; i++) { const int32 S = (Held + SlotCount - i) % SlotCount; if (SlotUsable(S)) { SelectSlot(S); return; } } }
void UHUInteractComponent::WheelDown() { if (bBeltOpen) { SetBeltSel(BeltSel + 1, BeltYaw); return; } for (int32 i = 1; i < SlotCount; i++) { const int32 S = (Held + i) % SlotCount; if (SlotUsable(S)) { SelectSlot(S); return; } } }

void UHUInteractComponent::UseHeld()
{
	if (bBeltOpen) { UseBelt(); return; }
	if (Held >= SlotCount) { BeltAction(Held - SlotCount); return; } // something off the belt
	if (IsHolding(EHUTool::Flashlight) || IsHolding(EHUTool::UVLight)) { bLightOn = !bLightOn; UpdateBeam(); return; }
	if (IsHolding(EHUTool::Scanner)) { Scan(); return; }
	// hands, the print kit and the crowbar work on whatever you're looking at
	BeginUse(false);
}

void UHUInteractComponent::ToggleFlashlight()
{
	if (!SlotUsable((int32)EHUTool::Flashlight + 1)) { Say(TEXT("You don't have a flashlight. The supply locker in the motor pool sells one.")); return; }
	if (!IsHolding(EHUTool::Flashlight)) { SelectSlot((int32)EHUTool::Flashlight + 1); bLightOn = true; }
	else bLightOn = !bLightOn;
	UpdateBeam();
}

void UHUInteractComponent::ToggleUV()
{
	if (!SlotUsable((int32)EHUTool::UVLight + 1)) { Say(TEXT("You don't have a UV light. The supply locker in the motor pool sells one.")); return; }
	if (!IsHolding(EHUTool::UVLight)) { SelectSlot((int32)EHUTool::UVLight + 1); bLightOn = true; }
	else bLightOn = !bLightOn;
	UpdateBeam();
}

void UHUInteractComponent::UpdateBeam()
{
	const bool bWasF = bFlashlightOn, bWasUV = bUVOn;
	bFlashlightOn = bLightOn && IsHolding(EHUTool::Flashlight);
	bUVOn = bLightOn && IsHolding(EHUTool::UVLight);
	// tell the squad, so they see your beam and what it lights up
	if ((bWasF != bFlashlightOn || bWasUV != bUVOn) && GetWorld() && GetWorld()->GetNetMode() != NM_Standalone)
	{
		if (GetOwner()->HasAuthority()) MulticastLights(bFlashlightOn, bUVOn); else ServerLights(bFlashlightOn, bUVOn);
	}
	if (Beam)
	{
		Beam->SetVisibility(bFlashlightOn || bUVOn);
		if (bUVOn) { Beam->SetLightColor(FLinearColor(0.45f, 0.1f, 1.f)); Beam->SetIntensity(2500.f); }
		else { Beam->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f)); Beam->SetIntensity(9000.f); }
	}
	if (Lens)
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Lens->GetMaterial(0)))
			M->SetVectorParameterValue(TEXT("Color"), bFlashlightOn ? FLinearColor(8, 7.5f, 6) : bUVOn ? FLinearColor(3, 0.5f, 8) : FLinearColor(0.2f, 0.2f, 0.22f));
}

// ---------- the hand, and what's in it ----------
void UHUInteractComponent::BuildHand()
{
	if (!Hand) return;
	using namespace HUBuild;
	Clear(HandParts);
	Lens = nullptr;
	AActor* O = GetOwner();
	auto P = [&](EShape S, FVector Pos, FVector Size, FLinearColor Col, FRotator Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* M = Block(O, Hand, HandParts, S, Pos, Size, Col, Rot, false);
		if (M) { M->SetCastShadow(false); M->bOnlyOwnerSee = true; }
		return M;
	};
	const FRotator AlongX(90, 0, 0); // lays a cylinder along the hand's forward axis
	const FLinearColor Coat(0.07f, 0.08f, 0.11f), Skin(0.6f, 0.42f, 0.32f), Black(0.02f, 0.02f, 0.02f);
	// dragging furniture: both hands out in front, palms flat against it
	if (Grabbed.IsValid())
	{
		for (float Side : { -1.f, 1.f })
		{
			P(EShape::Cylinder, FVector(-16, Side * 17, -2), FVector(10, 10, 30), Coat, FRotator(90, -Side * 8, 0));
			P(EShape::Cylinder, FVector(-1, Side * 15, -1), FVector(10.5f, 10.5f, 2), FLinearColor(0.75f, 0.75f, 0.72f), AlongX);
			P(EShape::Cube, FVector(4, Side * 15, 0), FVector(4, 9, 10), Skin);           // the palm, flat
			P(EShape::Cube, FVector(5.5f, Side * 15, 6.5f), FVector(3, 8, 4), Skin);      // fingers
		}
		return;
	}
	// sleeve, cuff, hand
	P(EShape::Cylinder, FVector(-14, 0, -3), FVector(10, 10, 26), Coat, AlongX);
	P(EShape::Cylinder, FVector(-1, 0, -3), FVector(10.5f, 10.5f, 2), FLinearColor(0.75f, 0.75f, 0.72f), AlongX);
	P(EShape::Cube, FVector(4, 0, -2.5f), FVector(7, 8, 5), Skin);
	switch (Held)
	{
	case 1: // flashlight
	case 2: // UV light
	{
		const FLinearColor Body = Held == 1 ? Black : FLinearColor(0.2f, 0.06f, 0.3f);
		P(EShape::Cylinder, FVector(11, 0, 0), FVector(4, 4, 18), Body, AlongX);
		P(EShape::Cylinder, FVector(21, 0, 0), FVector(6.5f, 6.5f, 4), Body, AlongX);
		Lens = P(EShape::Cylinder, FVector(23.2f, 0, 0), FVector(5.5f, 5.5f, 0.4f), FLinearColor(0.2f, 0.2f, 0.22f), AlongX);
		break;
	}
	case 3: // print kit: the case in your hand, a brush across it
		P(EShape::Cube, FVector(9, 0, 1), FVector(11, 9, 5), Black);
		P(EShape::Cylinder, FVector(12, 3, 5), FVector(1.6f, 1.6f, 11), FLinearColor(0.45f, 0.28f, 0.12f), AlongX);
		P(EShape::Cylinder, FVector(18, 3, 5), FVector(3, 3, 3), FLinearColor(0.1f, 0.1f, 0.1f), AlongX);
		break;
	case 4: // crowbar, angled up
		P(EShape::Cylinder, FVector(12, 0, 10), FVector(2.2f, 2.2f, 42), FLinearColor(0.55f, 0.05f, 0.04f), FRotator(60, 0, 0));
		P(EShape::Cube, FVector(23, 0, 28), FVector(6, 2, 2), FLinearColor(0.55f, 0.05f, 0.04f), FRotator(-30, 0, 0));
		break;
	case 5: // scanner, screen up
		P(EShape::Cube, FVector(10, 0, 2), FVector(13, 7, 3), FLinearColor(0.15f, 0.16f, 0.17f), FRotator(-20, 0, 0));
		P(EShape::Cube, FVector(10.5f, 0, 3.8f), FVector(7, 5, 0.3f), FLinearColor(0.2f, 2.f, 0.6f), FRotator(-20, 0, 0));
		break;
	// off the belt
	case SlotCount + HUBelt::Cuffs: // two steel rings and the chain
		for (float Sd : { -1.f, 1.f }) P(EShape::Cylinder, FVector(10, Sd * 4.5f, 2), FVector(7, 7, 1.2f), FLinearColor(0.7f, 0.72f, 0.75f), FRotator(0, 0, 90));
		P(EShape::Cube, FVector(10, 0, 2), FVector(1.5f, 4, 1.5f), FLinearColor(0.6f, 0.62f, 0.65f));
		break;
	case SlotCount + HUBelt::Pepper: // the can upright, thumb on the button
		P(EShape::Cylinder, FVector(8, 0, 4), FVector(4, 4, 12), FLinearColor(0.75f, 0.12f, 0.05f));
		P(EShape::Cube, FVector(8, 0, 11), FVector(3, 2.5f, 2), Black);
		break;
	case SlotCount + HUBelt::Taser: // grip down, barrel forward
		P(EShape::Cube, FVector(4, 0, -2), FVector(4, 3, 8), Black, FRotator(-15, 0, 0));
		P(EShape::Cube, FVector(11, 0, 3), FVector(14, 3.5f, 4.5f), FLinearColor(0.85f, 0.7f, 0.05f));
		P(EShape::Cube, FVector(18.2f, 0, 3), FVector(0.5f, 3, 3.5f), Black);
		break;
	case SlotCount + HUBelt::Radio: // held up, antenna high
		P(EShape::Cube, FVector(8, 0, 5), FVector(4, 6, 12), FLinearColor(0.08f, 0.08f, 0.09f));
		P(EShape::Cylinder, FVector(8, 2, 15), FVector(1, 1, 9), FLinearColor(0.08f, 0.08f, 0.09f));
		break;
	default: break; // empty hand
	}
}

// ---------- the kit ----------
bool UHUInteractComponent::InBeam(FVector Point, bool bUV) const
{
	if (bUV ? !bUVOn : !bFlashlightOn) return false;
	// works for a partner's copy too (no controller here): their eyes and where they're aiming, as replicated
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return false;
	FVector Eye; FRotator Look;
	if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()); PC && PC->IsLocalController()) PC->GetPlayerViewPoint(Eye, Look);
	else { Eye = Pawn->GetPawnViewLocation(); Look = Pawn->GetBaseAimRotation(); }
	const FVector To = Point - Eye;
	const float Dist = To.Size();
	if (Dist > BeamRange || Dist < 1.f) return false;
	return FVector::DotProduct(To / Dist, Look.Vector()) > FMath::Cos(FMath::DegreesToRadians(BeamCone + 4.f));
}

void UHUInteractComponent::Scan()
{
	if (!SlotUsable((int32)EHUTool::Scanner + 1)) { Say(TEXT("You don't have an evidence scanner. The supply locker in the motor pool sells one.")); return; }
	if (!IsHolding(EHUTool::Scanner)) SelectSlot((int32)EHUTool::Scanner + 1);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastScan < 8.0) { Say(FString::Printf(TEXT("SCANNER: recharging (%d s)"), FMath::CeilToInt(8.0 - (Now - LastScan)))); return; }
	LastScan = Now;
	APawn* Pawn = Cast<APawn>(GetOwner());
	AHUEvidence* Best = nullptr; float BestD = TNumericLimits<float>::Max();
	for (TActorIterator<AHUEvidence> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation());
		if (D < BestD) { BestD = D; Best = *It; }
	}
	if (!Best) { Say(TEXT("SCANNER: nothing left to find here.")); return; }
	const FVector To = Best->GetActorLocation() - Pawn->GetActorLocation();
	const FRotator Facing(0.f, Pawn->GetControlRotation().Yaw, 0.f);
	const float Ahead = FVector::DotProduct(To, Facing.Vector()), Right = FVector::DotProduct(To, FRotationMatrix(Facing).GetUnitAxis(EAxis::Y));
	FString Dir = FMath::Abs(Ahead) > FMath::Abs(Right) ? (Ahead > 0 ? TEXT("ahead") : TEXT("behind you")) : (Right > 0 ? TEXT("to your right") : TEXT("to your left"));
	if (To.Z > 150.f) Dir += TEXT(", up high");
	else if (To.Z < -150.f) Dir += TEXT(", down below");
	Say(FString::Printf(TEXT("SCANNER: beep... beep.. something %d m %s."), FMath::RoundToInt(BestD / 100.f), *Dir));
}

void UHUInteractComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	SetHover(TArray<UPrimitiveComponent*>());
	if (Keys) if (APlayerController* PC = Cast<APlayerController>(Keys->GetOuter())) PC->PopInputComponent(Keys);
	if (Widget) Widget->RemoveFromParent();
	Super::EndPlay(Reason);
}

void UHUInteractComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Focus = nullptr;
	FocusPart = nullptr;
	Prompt = FText::GetEmpty();
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		// a partner's copy on this screen: just their light, aimed where they aim
		if (Beam && Pawn) { Beam->SetWorldLocationAndRotation(Pawn->GetPawnViewLocation(), Pawn->GetBaseAimRotation()); }
		return;
	}
	TickMarkers();
	if (UHUListenSubsystem* L = GetWorld()->GetGameInstance()->GetSubsystem<UHUListenSubsystem>())
	{
		const bool bWant = bMenuOpen && MenuRow == 6;
		if (bWant && !L->IsMonitoring()) L->StartMonitor(); else if (!bWant && L->IsMonitoring()) L->StopMonitor();
	}
	if (!Keys) Setup(PC);

	// the hand sways a little as you walk
	if (Hand)
	{
		const float Speed = Pawn->GetVelocity().Size2D();
		Bob += DeltaTime * (2.f + Speed * 0.025f);
		const float Amt = FMath::Clamp(Speed / 400.f, 0.f, 1.f);
		FVector Pos(30, 15 + FMath::Cos(Bob * 0.5f) * 0.6f * Amt, -15 + FMath::Sin(Bob) * (0.25f + 0.8f * Amt));
		// dragging: hands centred and pushed out, shoving in rhythm with your steps (leaning in to push, back to pull)
		GrabBlend = FMath::FInterpTo(GrabBlend, Grabbed.IsValid() ? 1.f : 0.f, DeltaTime, 10.f);
		if (GrabBlend > 0.01f)
		{
			const float Fwd = FVector::DotProduct(Pawn->GetVelocity(), Pawn->GetActorForwardVector());
			const float Shove = FMath::Sin(Bob * 2.f) * 2.5f * Amt + FMath::Clamp(Fwd / 220.f, -1.f, 1.f) * 4.f;
			Pos = FMath::Lerp(Pos, FVector(38 + Shove, 0, -21), GrabBlend);
		}
		// picking something up: reach out and down, then back
		if (ReachT >= 0.f)
		{
			ReachT += DeltaTime;
			const float S = FMath::Sin(PI * FMath::Clamp(ReachT / 0.45f, 0.f, 1.f));
			Pos += FVector(24 * S, -9 * S, -14 * S);
			if (ReachT > 0.45f) ReachT = -1.f;
		}
		Hand->SetRelativeLocation(Pos);
	}

	FVector Eye; FRotator Look;
	PC->GetPlayerViewPoint(Eye, Look);
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("HUInteract"), false, Pawn);
	// a slightly fat line so you don't have to aim pixel-perfect, but small things still need looking at
	TArray<UPrimitiveComponent*> Parts;
	if (GetWorld()->SweepSingleByChannel(Hit, Eye, Eye + Look.Vector() * Reach, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(8.f), Params))
	{
		AActor* A = Hit.GetActor();
		if (A && A->GetClass()->ImplementsInterface(UHUInteractable::StaticClass()))
		{
			Focus = A;
			FocusPart = Hit.GetComponent();
			Prompt = IHUInteractable::Execute_GetPrompt(A, Pawn);
			// only things with something to do light up
		}
	}
	// no glow or spotlight in the game (that's only for the dark desk menu): the crosshair and the prompt do the job.
	// The prompt eases in again whenever you look at something new.
	if (Focus != LastFocus || FocusPart != LastFocusPart) { PromptAge = 0.f; LastFocus = Focus; LastFocusPart = FocusPart; }
	// a timed task: keep holding, keep looking at it
	if (bHolding)
	{
		if (Focus != HoldTarget.Get() || FocusPart != HoldPart.Get()) CancelHold();
		else if ((HoldProgress += DeltaTime / FMath::Max(0.05f, HoldTotal)) >= 1.f) { CancelHold(); Use(bHoldSecondary); }
	}
	PromptAge += DeltaTime;

	// the belt: look well down with nothing to pick up there. Opens past 55 degrees, closes back above 45.
	const float Pitch = FRotator::NormalizeAxis(Look.Pitch);
	const bool bFree = !bMenuOpen && !Focus && !Interviewing.IsValid() && !Watching.IsValid() && !Reading.IsValid() && !Grabbed.IsValid() && !bHolding;
	const bool bWant = bFree && Pitch < (bBeltOpen ? -45.f : -55.f);
	if (bWant != bBeltOpen)
	{
		bBeltOpen = bWant;
		// the body (and the belt on it) holds still while you look it over; only your view moves
		Pawn->bUseControllerRotationYaw = !bBeltOpen;
		BuildBelt();
	}
	if (bBeltOpen && Belt)
	{
		// pick the pouch the crosshair is on: where your line of sight crosses the belt's height
		const FVector Dir = Look.Vector();
		const float BeltZ = Belt->GetComponentLocation().Z;
		if (Dir.Z < -0.05f)
		{
			const FVector OnBelt = Eye + Dir * ((BeltZ - Eye.Z) / Dir.Z);
			int32 Best = -1; float BestD = 18.f;
			for (int32 i = 0; i < HUBelt::Count; i++)
			{
				const float D = FVector::Dist2D(OnBelt, Belt->GetComponentTransform().TransformPosition(PouchSpot(i)));
				if (D < BestD) { BestD = D; Best = i; }
			}
			if (Best >= 0 && Best != BeltSel) { BeltSel = Best; BuildBelt(); }
		}
	}
}

// where each pouch sits on the belt (belt space): round the hips, the belt sitting back on the body
FVector UHUInteractComponent::PouchSpot(int32 i)
{
	static const float Ang[] = { -70.f, -42.f, 26.f, 52.f, 78.f };
	const float A = FMath::DegreesToRadians(Ang[FMath::Clamp(i, 0, HUBelt::Count - 1)]);
	return FVector(FMath::Cos(A) * 25.f - 10.f, FMath::Sin(A) * 25.f, 0.f);
}

static FString DirWords(const APawn* From, const FVector& Where);

// ---------- the menu: ESC, or the pager on your belt ----------
// It doesn't pause the world (your squad keeps playing): it just takes your keys and stops you moving.
void UHUInteractComponent::OpenMenu()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || bMenuOpen) return;
	bMenuOpen = true; MenuRow = 0;
	if (!MenuKeys)
	{
		MenuKeys = NewObject<UInputComponent>(PC, TEXT("HUMenuKeys"));
		MenuKeys->bBlockInput = true; // nothing underneath gets the keys while it's open
		MenuKeys->Priority = 100;
		for (FKey K : { EKeys::Up, EKeys::W }) MenuKeys->BindKey(K, IE_Pressed, this, &UHUInteractComponent::MenuUp);
		for (FKey K : { EKeys::Down, EKeys::S }) MenuKeys->BindKey(K, IE_Pressed, this, &UHUInteractComponent::MenuDown);
		for (FKey K : { EKeys::Left, EKeys::A }) MenuKeys->BindKey(K, IE_Pressed, this, &UHUInteractComponent::MenuLeft);
		for (FKey K : { EKeys::Right, EKeys::D }) MenuKeys->BindKey(K, IE_Pressed, this, &UHUInteractComponent::MenuRight);
		for (FKey K : { EKeys::Enter, EKeys::E, EKeys::SpaceBar, EKeys::LeftMouseButton }) MenuKeys->BindKey(K, IE_Pressed, this, &UHUInteractComponent::MenuEnter);
		MenuKeys->BindKey(EKeys::Escape, IE_Pressed, this, &UHUInteractComponent::CloseMenu);
		MenuKeys->RegisterComponent();
	}
	PC->PushInputComponent(MenuKeys);
	PC->SetIgnoreLookInput(true);
	PC->SetIgnoreMoveInput(true);
	bBeltOpen = false; CancelHold();
}

void UHUInteractComponent::CloseMenu()
{
	if (!bMenuOpen) return;
	bMenuOpen = false;
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->PopInputComponent(MenuKeys);
		PC->ResetIgnoreLookInput();
		PC->ResetIgnoreMoveInput();
	}
}

void UHUInteractComponent::MenuUp() { MenuRow = (MenuRow + MenuRows - 1) % MenuRows; }
void UHUInteractComponent::MenuDown() { MenuRow = (MenuRow + 1) % MenuRows; }
void UHUInteractComponent::MenuLeft() { ChangeMenu(-1); }
void UHUInteractComponent::MenuRight() { ChangeMenu(1); }
void UHUInteractComponent::MenuEnter()
{
	if (MenuRow == 0) { CloseMenu(); return; }
	if (MenuRow == MenuRows - 1) { CloseMenu(); if (AHUCharacter* Ch = Cast<AHUCharacter>(GetOwner())) Ch->BackToDesk(); return; }
	ChangeMenu(1);
}

void UHUInteractComponent::ChangeMenu(int32 Dir)
{
	UHUCaseSubsystem* C = Cases();
	UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!C || !S) return;
	switch (MenuRow)
	{
	case 1: C->MouseSensitivity = FMath::Clamp(FMath::RoundToFloat((C->MouseSensitivity + Dir * 0.1f) * 10.f) / 10.f, 0.2f, 3.f); C->SaveProgress(); break;
	case 2:
		C->FieldOfView = FMath::Clamp(C->FieldOfView + Dir * 5.f, 70.f, 110.f); C->SaveProgress();
		if (const AHUCharacter* Ch = Cast<AHUCharacter>(GetOwner()); Ch && Ch->Eyes) Ch->Eyes->SetFieldOfView(C->FieldOfView);
		break;
	case 3: S->SetOverallScalabilityLevel((FMath::Max(0, S->GetOverallScalabilityLevel()) + Dir + 4) % 4); S->ApplySettings(false); S->SaveSettings(); break;
	case 4: S->SetFullscreenMode((EWindowMode::Type)(((int32)S->GetFullscreenMode() + Dir + 3) % 3)); S->ApplySettings(false); S->SaveSettings(); break;
	case 5: C->VoiceProvider = (uint8)((C->VoiceProvider + Dir + 3) % 3); C->SaveProgress(); break;
	case 6: if (UHUListenSubsystem* L = GetWorld()->GetGameInstance()->GetSubsystem<UHUListenSubsystem>()) L->CycleMic(Dir); break; // the microphone
	default: break;
	}
}

FString UHUInteractComponent::MenuLine(int32 Row) const
{
	const UHUCaseSubsystem* C = Cases();
	const UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const TCHAR* Modes[] = { TEXT("Fullscreen"), TEXT("Windowed fullscreen"), TEXT("Windowed") };
	const TCHAR* Quality[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
	switch (Row)
	{
	case 0: return TEXT("Back to the case");
	case 1: return FString::Printf(TEXT("Mouse sensitivity:  %.1f"), C ? C->MouseSensitivity : 1.f);
	case 2: return FString::Printf(TEXT("Field of view:  %d°"), C ? FMath::RoundToInt(C->FieldOfView) : 90);
	case 3: return FString::Printf(TEXT("Graphics:  %s"), S ? Quality[FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 3)] : TEXT("?"));
	case 4: return FString::Printf(TEXT("Window:  %s"), S ? Modes[FMath::Clamp((int32)S->GetFullscreenMode(), 0, 2)] : TEXT("?"));
	case 5: return FString::Printf(TEXT("Voices:  %s"), C ? *UHUVoiceSubsystem::ProviderName((EHUVoiceProvider)C->VoiceProvider) : TEXT(""));
	case 6:
	{
		// the microphone, with a live level while it's picked: talk and the bar should move
		UHUListenSubsystem* L = GetWorld()->GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
		if (!L) return TEXT("Microphone:  ?");
		const int32 N = FMath::RoundToInt(L->Level() * 12.f);
		return FString::Printf(TEXT("Microphone:  %s%s"), *L->MicLabel(), MenuRow == 6 && L->IsMonitoring() ? *(FString(TEXT("   [")) + FString::ChrN(N, '|') + FString::ChrN(12 - N, ' ') + TEXT("]")) : TEXT(""));
	}
	case 7: return TEXT("Leave the case — back to the desk");
	default: return FString();
	}
}

// ---------- squad markers ----------
void UHUInteractComponent::Ping()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || Interviewing.IsValid() || Watching.IsValid() || Reading.IsValid()) return;
	FVector Eye; FRotator Look;
	PC->GetPlayerViewPoint(Eye, Look);
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("HUPing"), false, Pawn);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Look.Vector() * 4000.f, ECC_Visibility, Params)) { Say(TEXT("Nothing there to mark.")); return; }
	AActor* A = Hit.GetActor();
	const uint8 Kind = Cast<AHUEvidence>(A) ? MarkEvidence : Cast<AHUSuspectActor>(A) ? MarkSuspect : MarkPlace;
	const FString Path = Kind != MarkPlace ? A->GetPathName() : FString();
	const FVector At = Kind != MarkPlace ? A->GetActorLocation() : Hit.ImpactPoint;
	if (GetWorld()->GetNetMode() == NM_Standalone) MulticastPing_Implementation(At, Kind, Path);
	else if (GetOwner()->HasAuthority()) MulticastPing(At, Kind, Path);
	else ServerPing(At, Kind, Path);
}

void UHUInteractComponent::ServerPing_Implementation(FVector At, uint8 Kind, const FString& TargetPath) { MulticastPing(At, Kind, TargetPath); }

void UHUInteractComponent::MulticastPing_Implementation(FVector At, uint8 Kind, const FString& TargetPath)
{
	APawn* User = Cast<APawn>(GetOwner());
	APlayerController* Local = GetWorld()->GetFirstPlayerController();
	APawn* Me = Local ? Local->GetPawn() : nullptr;
	UHUInteractComponent* Mine = Me ? Me->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!Mine || !User) return;
	FMarker M;
	M.By = User; M.At = At; M.Kind = Kind; M.Born = GetWorld()->GetRealTimeSeconds();
	M.Who = User == Me ? FString(TEXT("You")) : User->GetPlayerState() ? User->GetPlayerState()->GetPlayerName() : FString(TEXT("Partner"));
	if (!TargetPath.IsEmpty()) { M.Target = FindObject<AActor>(nullptr, *TargetPath); M.bHadTarget = M.Target.IsValid(); if (M.Target.IsValid()) M.At = M.Target->GetActorLocation(); }
	// one marker each: a new one replaces your old one
	Mine->Markers.RemoveAll([User](const FMarker& O) { return O.By.Get() == User; });
	Mine->Markers.Add(M);
	if (User != Me)
	{
		const TCHAR* What = Kind == MarkEvidence ? TEXT("evidence here") : Kind == MarkSuspect ? TEXT("watch this one") : TEXT("check here");
		Mine->Notify(TEXT("MARKED"), FString::Printf(TEXT("%s: %s  —  %d m %s"), *M.Who, What,
			FMath::RoundToInt(FVector::Dist(M.At, Me->GetActorLocation()) / 100.f), *DirWords(Me, M.At)), FLinearColor(1.f, 0.75f, 0.3f));
	}
}

void UHUInteractComponent::TickMarkers()
{
	const float Now = GetWorld()->GetRealTimeSeconds();
	// gone after 45 s, or once the evidence is bagged / the suspect's taken away
	Markers.RemoveAll([Now](const FMarker& M) { return Now - M.Born > 45.f || (M.bHadTarget && (!M.Target.IsValid() || (M.Target->IsHidden() && !Cast<AHUEvidence>(M.Target.Get())))); });
	for (FMarker& M : Markers) if (M.Target.IsValid()) M.At = M.Target->GetActorLocation();
}

void UHUInteractComponent::ServerLights_Implementation(bool bF, bool bUV) { MulticastLights(bF, bUV); }

void UHUInteractComponent::MulticastLights_Implementation(bool bF, bool bUV)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || Pawn->IsLocallyControlled()) return; // your own light is already right
	bFlashlightOn = bF; bUVOn = bUV;
	if (!Beam)
	{
		// a partner's light on this screen
		Beam = NewObject<USpotLightComponent>(GetOwner(), TEXT("HUBeam"));
		Beam->SetupAttachment(GetOwner()->GetRootComponent());
		Beam->SetUsingAbsoluteLocation(true);
		Beam->SetUsingAbsoluteRotation(true);
		Beam->SetAttenuationRadius(BeamRange);
		Beam->SetInnerConeAngle(BeamCone * 0.6f);
		Beam->SetOuterConeAngle(BeamCone);
		Beam->SetCastShadows(true);
		Beam->RegisterComponent();
	}
	Beam->SetVisibility(bF || bUV);
	if (bUV) { Beam->SetLightColor(FLinearColor(0.45f, 0.1f, 1.f)); Beam->SetIntensity(2500.f); }
	else { Beam->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f)); Beam->SetIntensity(9000.f); }
}

// ---------- the utility belt ----------
// which way something is, in words, from where this pawn is facing
static FString DirWords(const APawn* From, const FVector& Where)
{
	const FVector To = Where - From->GetActorLocation();
	const FRotator Facing(0.f, From->GetControlRotation().Yaw, 0.f);
	const float Ahead = FVector::DotProduct(To, Facing.Vector()), Right = FVector::DotProduct(To, FRotationMatrix(Facing).GetUnitAxis(EAxis::Y));
	FString Dir = FMath::Abs(Ahead) > FMath::Abs(Right) ? (Ahead > 0 ? TEXT("ahead") : TEXT("behind you")) : (Right > 0 ? TEXT("to your right") : TEXT("to your left"));
	if (To.Z > 150.f) Dir += TEXT(", up high");
	else if (To.Z < -150.f) Dir += TEXT(", down below");
	return Dir;
}

void UHUInteractComponent::SetBeltSel(int32 Sel, float LookYaw)
{
	BeltSel = FMath::Clamp(Sel, 0, HUBelt::Count - 1);
	BuildBelt();
}

float UHUInteractComponent::BeltWait(int32 Item) const
{
	if (Item < 0 || Item >= HUBelt::Count || !GetWorld()) return 0.f;
	return FMath::Max(0.f, (float)(BeltUsed[Item] + HUBelt::Cooldown(Item) - GetWorld()->GetTimeSeconds()));
}

void UHUInteractComponent::BuildBelt()
{
	if (!Belt) return;
	using namespace HUBuild;
	Clear(BeltParts);
	AActor* O = GetOwner();
	auto P = [&](EShape S, FVector Pos, FVector Size, FLinearColor Col, FRotator Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* M = Block(O, Belt, BeltParts, S, Pos, Size, Col, Rot, false);
		if (M) { M->SetCastShadow(false); M->bOnlyOwnerSee = true; }
		return M;
	};
	const FLinearColor Leather(0.05f, 0.04f, 0.035f), Buckle(0.55f, 0.5f, 0.35f);
	// the band, curving round the front of the waist
	for (int32 i = -3; i <= 3; i++)
	{
		const float A = FMath::DegreesToRadians(i * 22.f);
		P(EShape::Cube, FVector(FMath::Cos(A) * 22.f - 10.f, FMath::Sin(A) * 22.f, 0), FVector(3, 10, 5), Leather, FRotator(0, i * 22.f, 0));
	}
	P(EShape::Cube, FVector(13.5f, 0, 0), FVector(1, 6, 5), Buckle);
	// the four pouches; the picked one rides up while you're looking at the belt
	const float Ang[] = { -70.f, -42.f, 26.f, 52.f, 78.f };
	for (int32 i = 0; i < HUBelt::Count; i++)
	{
		const bool bPick = bBeltOpen && i == BeltSel;
		const FVector At = PouchSpot(i) + FVector(0, 0, bPick ? 4.f : -2.f);
		const FRotator R(0, Ang[i], 0);
		P(EShape::Cube, At, FVector(5, 7, 9), Leather, R); // the pouch
		if (Held != SlotCount + i) // (empty while it's in your hand)
		switch (i)
		{
		case HUBelt::Cuffs:
			for (float Sd : { -1.f, 1.f }) P(EShape::Cylinder, At + R.RotateVector(FVector(1.5f, Sd * 2.2f, 5.5f)), FVector(4.5f, 4.5f, 0.8f), FLinearColor(0.7f, 0.72f, 0.75f), R + FRotator(0, 0, 90));
			break;
		case HUBelt::Pepper:
			P(EShape::Cylinder, At + FVector(0, 0, 6.5f), FVector(3, 3, 7), FLinearColor(0.75f, 0.12f, 0.05f));
			P(EShape::Cube, At + FVector(0, 0, 10.5f), FVector(2, 2, 1.5f), FLinearColor(0.05f, 0.05f, 0.05f));
			break;
		case HUBelt::Taser:
			P(EShape::Cube, At + R.RotateVector(FVector(0, 0, 7)), FVector(3, 4, 8), FLinearColor(0.85f, 0.7f, 0.05f), R);
			break;
		case HUBelt::Radio:
			P(EShape::Cube, At + FVector(0, 0, 6.5f), FVector(3.5f, 5, 9), FLinearColor(0.08f, 0.08f, 0.09f), R);
			P(EShape::Cylinder, At + R.RotateVector(FVector(0, 1.5f, 14)), FVector(0.8f, 0.8f, 7), FLinearColor(0.08f, 0.08f, 0.09f));
			break;
		case HUBelt::Pager: // clipped on: a small black box with a green screen
			P(EShape::Cube, At + FVector(0, 0, 5.f), FVector(2.5f, 6, 4), FLinearColor(0.06f, 0.06f, 0.07f), R);
			P(EShape::Cube, At + R.RotateVector(FVector(1.3f, 0, 5.8f)), FVector(0.3f, 4.2f, 1.8f), FLinearColor(0.2f, 1.2f, 0.4f), R);
			break;
		}
		if (bPick) P(EShape::Cube, At + FVector(0, 0, -6.f), FVector(5.5f, 7.5f, 0.6f), FLinearColor(1.f, 0.75f, 0.3f), R); // a gold rim under it
	}
}

// clicking a pouch: take that item into your hand (or put it back). The pager opens the menu instead.
void UHUInteractComponent::UseBelt()
{
	if (BeltSel == HUBelt::Pager) { OpenMenu(); return; }
	if (Held == SlotCount + BeltSel) { Held = 0; Say(FString::Printf(TEXT("You put the %s back on your belt."), HUBelt::Name(BeltSel))); }
	else { Held = SlotCount + BeltSel; PlayReach(); Say(FString::Printf(TEXT("%s in hand — LEFT CLICK to use it. 1-6 or the belt to put it away."), HUBelt::Name(BeltSel))); }
	bLightOn = false;
	UpdateBeam();
	BuildHand();
	BuildBelt();
}

// using the belt item in your hand (LEFT CLICK)
void UHUInteractComponent::BeltAction(int32 Item)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !GetWorld()) return;
	// the cuffs: on the suspect you're looking at, the same as E
	if (Item == HUBelt::Cuffs && Cast<AHUSuspectActor>(Focus)) { BeginUse(false); return; }
	if (const float Wait = BeltWait(Item); Wait > 0.f)
	{
		Say(FString::Printf(TEXT("%s: %s (%d s)"), HUBelt::Name(Item), Item == HUBelt::Taser ? TEXT("loading a new cartridge") : TEXT("not ready"), FMath::CeilToInt(Wait)));
		return;
	}
	// aimed the way you're looking
	const FVector From = Pawn->GetActorLocation();
	const FVector Fwd = FRotator(0.f, Pawn->GetControlRotation().Yaw, 0.f).Vector();
	auto InFront = [&](AHUSuspectActor* S, float Range, float Cone)
	{
		if (!S || S->bCuffed || S->IsHidden()) return false;
		const FVector To = (S->GetActorLocation() - From) * FVector(1, 1, 0);
		const float D = To.Size();
		return D < Range && (D < 60.f || FVector::DotProduct(To / D, Fwd) > FMath::Cos(FMath::DegreesToRadians(Cone)));
	};
	auto Nearest = [&](float Range, float Cone)
	{
		AHUSuspectActor* Best = nullptr; float BestD = Range;
		for (TActorIterator<AHUSuspectActor> It(GetWorld()); It; ++It)
			if (InFront(*It, Range, Cone))
			{
				const float D = FVector::Dist2D(It->GetActorLocation(), From);
				if (D < BestD) { BestD = D; Best = *It; }
			}
		return Best;
	};
	switch (Item)
	{
	case HUBelt::Cuffs:
	{
		AHUSuspectActor* S = Nearest(220.f, 50.f);
		if (!S) { Say(TEXT("Nobody close enough to cuff. Get right up to them, facing them.")); return; }
		BeltUsed[Item] = GetWorld()->GetTimeSeconds();
		SendBelt(Item, S->GetPathName());
		return;
	}
	case HUBelt::Pepper:
	{
		BeltUsed[Item] = GetWorld()->GetTimeSeconds();
		PlayReach();
		int32 Hit = 0;
		for (TActorIterator<AHUSuspectActor> It(GetWorld()); It; ++It)
			if (InFront(*It, 330.f, 35.f)) { SendBelt(Item, It->GetPathName()); Hit++; }
		if (!Hit) Say(TEXT("A hiss of pepper spray into the air. Nobody in it."));
		return;
	}
	case HUBelt::Taser:
	{
		// the closest one you're facing, with nothing in the way
		AHUSuspectActor* S = nullptr; float BestD = 750.f;
		for (TActorIterator<AHUSuspectActor> It(GetWorld()); It; ++It)
		{
			if (!InFront(*It, 750.f, 14.f)) continue;
			FHitResult Hit;
			FCollisionQueryParams Params(TEXT("HUTaser"), false, Pawn);
			if (GetWorld()->LineTraceSingleByChannel(Hit, From + FVector(0, 0, 40), It->GetActorLocation() + FVector(0, 0, 110), ECC_Visibility, Params) && Hit.GetActor() != *It) continue;
			const float D = FVector::Dist2D(It->GetActorLocation(), From);
			if (D < BestD) { BestD = D; S = *It; }
		}
		BeltUsed[Item] = GetWorld()->GetTimeSeconds();
		PlayReach();
		if (!S) { Say(TEXT("The taser cracks - the probes hit nothing. Face them squarely; it reaches about 7 m.")); return; }
		SendBelt(Item, S->GetPathName());
		return;
	}
	case HUBelt::Radio:
		BeltUsed[Item] = GetWorld()->GetTimeSeconds();
		SendBelt(Item, FString());
		return;
	case HUBelt::Pager:
	{
		// just yours: each click steps back one message (newest first), then wraps round
		BeltUsed[Item] = GetWorld()->GetTimeSeconds();
		if (PagerLog.IsEmpty()) { Say(TEXT("PAGER: no messages.")); return; }
		const int32 Back = PagerRead++ % PagerLog.Num();
		Say(FString::Printf(TEXT("PAGER  %d/%d:  %s"), Back + 1, PagerLog.Num(), *PagerLog[PagerLog.Num() - 1 - Back]));
		return;
	}
	}
}

void UHUInteractComponent::SendBelt(uint8 Item, const FString& TargetPath)
{
	const ENetMode Mode = GetWorld()->GetNetMode();
	if (Mode == NM_Standalone) { MulticastBelt_Implementation(Item, TargetPath); return; }
	if (GetOwner()->HasAuthority()) MulticastBelt(Item, TargetPath);
	else ServerBelt(Item, TargetPath);
}

void UHUInteractComponent::ServerBelt_Implementation(uint8 Item, const FString& TargetPath)
{
	MulticastBelt(Item, TargetPath);
}

void UHUInteractComponent::MulticastBelt_Implementation(uint8 Item, const FString& TargetPath)
{
	APawn* User = Cast<APawn>(GetOwner());
	if (!User) return;
	AHUSuspectActor* S = TargetPath.IsEmpty() ? nullptr : FindObject<AHUSuspectActor>(nullptr, *TargetPath);
	switch (Item)
	{
	case HUBelt::Cuffs:
		if (S && !S->bCuffed)
		{
			// the arrest is the same as with cuffs in hand: act as if holding them, then put the hand back
			const int32 Saved = Held;
			Held = (int32)EHUTool::Handcuffs + 1;
			IHUInteractable::Execute_Interact(S, User, false);
			Held = Saved;
		}
		break;
	case HUBelt::Pepper:
	case HUBelt::Taser:
		if (S) S->Subdue(User, Item == HUBelt::Taser);
		break;
	case HUBelt::Radio:
	{
		// whoever's at this screen hears it
		APlayerController* Local = GetWorld()->GetFirstPlayerController();
		APawn* Me = Local ? Local->GetPawn() : nullptr;
		UHUInteractComponent* Mine = Me ? Me->FindComponentByClass<UHUInteractComponent>() : nullptr;
		if (!Mine) break;
		const FLinearColor RadioCol(0.55f, 0.8f, 1.f);
		if (Me == User)
		{
			if (GetWorld()->GetNetMode() == NM_Standalone)
			{
				const UHUCaseSubsystem* C = Cases();
				Mine->Notify(TEXT("RADIO"), FString::Printf(TEXT("DISPATCH: \"Copy, Detective. %d piece%s of evidence bagged, %d booked. The van's out front.\""),
					C ? C->Found.Num() : 0, C && C->Found.Num() == 1 ? TEXT("") : TEXT("s"), C ? C->Arrested.Num() : 0), RadioCol);
			}
			else Mine->Notify(TEXT("RADIO"), TEXT("You key the radio. Your squad hears you, and where you are."), RadioCol);
		}
		else
		{
			const FString Who = User->GetPlayerState() ? User->GetPlayerState()->GetPlayerName() : FString(TEXT("Your partner"));
			Mine->Notify(TEXT("RADIO"), FString::Printf(TEXT("%s: \"Over here.\"  —  %d m %s"), *Who,
				FMath::RoundToInt(FVector::Dist(User->GetActorLocation(), Me->GetActorLocation()) / 100.f), *DirWords(Me, User->GetActorLocation())), RadioCol);
		}
		break;
	}
	}
}

// ---------- hover feedback ----------
void UHUInteractComponent::SetHover(const TArray<UPrimitiveComponent*>& Parts)
{
	bool bSame = Parts.Num() == Lit.Num();
	for (int32 i = 0; bSame && i < Parts.Num(); i++) bSame = Lit[i].Get() == Parts[i];
	if (bSame) return;
	// put the old ones back
	for (int32 i = 0; i < Lit.Num(); i++)
		if (UPrimitiveComponent* P = Lit[i].Get())
			if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(P->GetMaterial(0))) M->SetVectorParameterValue(TEXT("Color"), LitColor[i]);
	Lit.Reset(); LitColor.Reset();
	for (UPrimitiveComponent* P : Parts)
	{
		if (!P || !P->IsVisible()) continue;
		FLinearColor Col = FLinearColor::White;
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(P->GetMaterial(0))) M->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color")), Col);
		Lit.Add(P); LitColor.Add(Col);
	}
	PromptAge = 0.f;
}

void UHUInteractComponent::AnimateHover(float DeltaTime)
{
	PromptAge += DeltaTime;
	const float T = GetWorld()->GetTimeSeconds();
	// a soft glow that breathes, easing in over the first moment
	const float In = FMath::Clamp(PromptAge * 5.f, 0.f, 1.f);
	const float Glow = 1.f + In * (0.35f + 0.2f * FMath::Sin(T * 5.f));
	FBox Box(ForceInit);
	for (int32 i = 0; i < Lit.Num(); i++)
		if (UPrimitiveComponent* P = Lit[i].Get())
		{
			if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(P->GetMaterial(0)))
				M->SetVectorParameterValue(TEXT("Color"), LitColor[i] * Glow + FLinearColor(0.02f, 0.025f, 0.03f) * In);
			Box += P->Bounds.GetBox();
		}
	if (Glint)
	{
		const bool bOn = Box.IsValid != 0;
		if (bOn)
		{
			const FVector Target = Box.GetCenter() + FVector(0, 0, Box.GetExtent().Z + 70.f);
			Glint->SetWorldLocation(Glint->Intensity < 50.f ? Target : FMath::VInterpTo(Glint->GetComponentLocation(), Target, DeltaTime, 14.f));
		}
		Glint->SetIntensity(FMath::FInterpTo(Glint->Intensity, bOn ? 2200.f : 0.f, DeltaTime, 10.f));
	}
}

void UHUInteractComponent::Use(bool bSecondary)
{
	if (!Focus) return;
	const ENetMode Mode = GetWorld()->GetNetMode();
	if (Mode == NM_Standalone) { IHUInteractable::Execute_Interact(Focus, Cast<APawn>(GetOwner()), bSecondary); return; }
	const FString Path = Focus->GetPathName();
	const FName Part = FocusPart ? FocusPart->GetFName() : NAME_None;
	if (GetOwner()->HasAuthority()) MulticastUse(Path, Part, bSecondary, Held); // the host: straight out to everyone
	else ServerUse(Path, Part, bSecondary, Held);                              // a squadmate: ask the host
}

// ---- PrecinctOS: screens and shared windows ----
static AHUCaseDesk* DeskByIndex(UWorld* W, int32 Desk)
{
	for (TActorIterator<AHUCaseDesk> It(W); It; ++It) if (It->DeskIndex == Desk) return *It;
	return nullptr;
}
void UHUInteractComponent::SendDeskView(int32 Desk, bool bOn, int32 App)
{
	if (GetWorld()->GetNetMode() == NM_Standalone) { MulticastDeskView_Implementation(Desk, bOn, App); return; }
	if (GetOwner()->HasAuthority()) MulticastDeskView(Desk, bOn, App); else ServerDeskView(Desk, bOn, App);
}
void UHUInteractComponent::ServerDeskView_Implementation(int32 Desk, bool bOn, int32 App) { MulticastDeskView(Desk, bOn, App); }
void UHUInteractComponent::MulticastDeskView_Implementation(int32 Desk, bool bOn, int32 App)
{
	if (AHUCaseDesk* D = DeskByIndex(GetWorld(), Desk)) D->SetOwnerView(bOn, App);
}
void UHUInteractComponent::SendDeskShare(int32 Desk, int32 App)
{
	if (GetWorld()->GetNetMode() == NM_Standalone) { MulticastDeskShare_Implementation(Desk, App); return; }
	if (GetOwner()->HasAuthority()) MulticastDeskShare(Desk, App); else ServerDeskShare(Desk, App);
}
void UHUInteractComponent::ServerDeskShare_Implementation(int32 Desk, int32 App) { MulticastDeskShare(Desk, App); }
void UHUInteractComponent::MulticastDeskShare_Implementation(int32 Desk, int32 App)
{
	APawn* User = Cast<APawn>(GetOwner());
	APlayerController* Local = GetWorld()->GetFirstPlayerController();
	APawn* Me = Local ? Local->GetPawn() : nullptr;
	UHUInteractComponent* Mine = Me ? Me->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!Mine) return;
	const FString AppName = AHUCaseDesk::AppName(App);
	if (User == Me) { Mine->Say(FString::Printf(TEXT("You send %s to the squad's desks."), *AppName)); return; }
	const FString Who = User && User->GetPlayerState() ? User->GetPlayerState()->GetPlayerName().Replace(TEXT("_"), TEXT(" ")) : FString(TEXT("Your partner"));
	// it lands on your own desk (or, with no desk of your own, any free one)
	for (TActorIterator<AHUCaseDesk> It(GetWorld()); It; ++It)
		if (It->OwnerState() && (APlayerState*)It->OwnerState() == Me->GetPlayerState()) { It->ReceiveShare(App, Who); break; }
	Mine->Notify(AHUCaseDesk::OSName, FString::Printf(TEXT("%s sent you %s - it's waiting on your desktop"), *Who, *AppName), FLinearColor(0.3f, 0.55f, 0.95f));
}

void UHUInteractComponent::SendUseOn(AActor* Target, UPrimitiveComponent* Part, bool bSecondary)
{
	if (!Target) return;
	AActor* SavedFocus = Focus; UPrimitiveComponent* SavedPart = FocusPart;
	Focus = Target; FocusPart = Part;
	Use(bSecondary);
	Focus = SavedFocus; FocusPart = SavedPart;
}

void UHUInteractComponent::ServerUse_Implementation(const FString& TargetPath, FName Part, bool bSecondary, int32 HeldSlot)
{
	MulticastUse(TargetPath, Part, bSecondary, HeldSlot);
}

void UHUInteractComponent::MulticastUse_Implementation(const FString& TargetPath, FName Part, bool bSecondary, int32 HeldSlot)
{
	// find this machine's copy of the thing, and the part of it that was used
	AActor* Target = FindObject<AActor>(nullptr, *TargetPath);
	if (!Target || !Target->GetClass()->ImplementsInterface(UHUInteractable::StaticClass())) return;
	UPrimitiveComponent* P = nullptr;
	if (Part != NAME_None) { TArray<UPrimitiveComponent*> Comps; Target->GetComponents<UPrimitiveComponent>(Comps); for (UPrimitiveComponent* C : Comps) if (C && C->GetFName() == Part) { P = C; break; } }
	// act as that player: what they were looking at and what was in their hand
	AActor* SavedFocus = Focus; UPrimitiveComponent* SavedPart = FocusPart; const int32 SavedHeld = Held;
	Focus = Target; FocusPart = P; Held = HeldSlot;
	IHUInteractable::Execute_Interact(Target, Cast<APawn>(GetOwner()), bSecondary);
	Focus = SavedFocus; FocusPart = SavedPart;
	if (!Cast<APawn>(GetOwner()) || !Cast<APawn>(GetOwner())->IsLocallyControlled()) Held = SavedHeld; // (your own hand stays as you set it)
}

void UHUInteractComponent::Notify(const FString& Title, const FString& Body, FLinearColor Color)
{
	FNote N; N.Title = Title; N.Body = Body; N.Color = Color; N.Born = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	Notes.Add(N);
	// everything that pops up is kept on the pager (the newest 20)
	PagerLog.Add(Body.IsEmpty() ? Title : Title + TEXT(" — ") + Body); PagerRead = 0;
	if (PagerLog.Num() > 20) PagerLog.RemoveAt(0);
	if (Notes.Num() > 4) Notes.RemoveAt(0);
}

void UHUInteractComponent::SetGrabbing(AActor* Thing)
{
	Grabbed = Thing;
	if (Thing) { bLightOn = false; UpdateBeam(); }
	BuildHand(); // switches between the two-handed push pose and whatever you're holding
}
// E / Q pressed: instant things happen now; careful work (UseTime) needs holding, faster with coffee
void UHUInteractComponent::BeginUse(bool bSecondary)
{
	if (!Focus) return;
	const IHUInteractable* II = Cast<IHUInteractable>(Focus);
	APawn* Pawn = Cast<APawn>(GetOwner());
	float T = II ? II->UseTime(Pawn, bSecondary) : 0.f;
	if (const AHUCharacter* Ch = Cast<AHUCharacter>(Pawn)) T /= Ch->TaskSpeed();
	T /= HUProgress::TaskMult(Pawn, Focus); // specialty perks and precinct upgrades
	if (T <= 0.05f) { Use(bSecondary); return; }
	bHolding = true; bHoldSecondary = bSecondary; HoldTotal = T; HoldProgress = 0.f;
	HoldTarget = Focus; HoldPart = FocusPart;
}
// ---------- interviews, shared with the squad ----------
void UHUInteractComponent::SendInterrogation(const FString& SuspectPath, uint8 Action, int32 Statement, const FString& ClueId)
{
	const ENetMode Mode = GetWorld()->GetNetMode();
	if (Mode == NM_Standalone) { MulticastInterrogate_Implementation(SuspectPath, Action, Statement, ClueId); return; }
	if (GetOwner()->HasAuthority()) MulticastInterrogate(SuspectPath, Action, Statement, ClueId);
	else ServerInterrogate(SuspectPath, Action, Statement, ClueId);
}

void UHUInteractComponent::SendAnswer(const FString& SuspectPath, const FString& Question, const FString& Answer, uint8 Emotion, uint8 Action)
{
	const ENetMode Mode = GetWorld()->GetNetMode();
	if (Mode == NM_Standalone) { MulticastAnswer_Implementation(SuspectPath, Question, Answer, Emotion, Action); return; }
	if (GetOwner()->HasAuthority()) MulticastAnswer(SuspectPath, Question, Answer, Emotion, Action);
	else ServerAnswer(SuspectPath, Question, Answer, Emotion, Action);
}
void UHUInteractComponent::ServerAnswer_Implementation(const FString& SuspectPath, const FString& Question, const FString& Answer, uint8 Emotion, uint8 Action)
{
	MulticastAnswer(SuspectPath, Question.Left(200), Answer.Left(400), Emotion, Action);
}
void UHUInteractComponent::MulticastAnswer_Implementation(const FString& SuspectPath, const FString& Question, const FString& Answer, uint8 Emotion, uint8 Action)
{
	if (AHUSuspectActor* S = FindObject<AHUSuspectActor>(nullptr, *SuspectPath)) S->ApplyAnswer(Cast<APawn>(GetOwner()), Question, Answer, Emotion, Action);
}

void UHUInteractComponent::SendTapeNote(const FString& Note, bool bCatch)
{
	if (GetWorld()->GetNetMode() == NM_Standalone) { MulticastTapeNote_Implementation(Note, bCatch); return; }
	if (GetOwner()->HasAuthority()) MulticastTapeNote(Note, bCatch); else ServerTapeNote(Note, bCatch);
}
void UHUInteractComponent::ServerTapeNote_Implementation(const FString& Note, bool bCatch) { MulticastTapeNote(Note.Left(300), bCatch); }
void UHUInteractComponent::MulticastTapeNote_Implementation(const FString& Note, bool bCatch)
{
	APawn* User = Cast<APawn>(GetOwner());
	APlayerController* Local = GetWorld()->GetFirstPlayerController();
	APawn* Me = Local ? Local->GetPawn() : nullptr;
	UHUInteractComponent* Mine = Me ? Me->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!Mine) return;
	const FString Who = User == Me ? FString(TEXT("You")) : User && User->GetPlayerState() ? User->GetPlayerState()->GetPlayerName() : FString(TEXT("Your partner"));
	Mine->Notify(TEXT("ON THE TAPE"), FString::Printf(TEXT("%s: %s"), *Who, *Note), FLinearColor(0.6f, 0.85f, 1.f));
	if (!bCatch) return;
	UHUCaseSubsystem* C = Cases();
	const FString Res = C ? C->CatchByTape() : FString();
	if (!Res.IsEmpty()) Mine->Notify(TEXT("THE TAPE CATCHES A LIE"), Res, FLinearColor(0.95f, 0.35f, 0.3f));
}

void UHUInteractComponent::ServerInterrogate_Implementation(const FString& SuspectPath, uint8 Action, int32 Statement, const FString& ClueId)
{
	MulticastInterrogate(SuspectPath, Action, Statement, ClueId);
}

void UHUInteractComponent::MulticastInterrogate_Implementation(const FString& SuspectPath, uint8 Action, int32 Statement, const FString& ClueId)
{
	if (AHUSuspectActor* S = FindObject<AHUSuspectActor>(nullptr, *SuspectPath)) S->ApplyInterrogation(Cast<APawn>(GetOwner()), Action, Statement, ClueId);
}