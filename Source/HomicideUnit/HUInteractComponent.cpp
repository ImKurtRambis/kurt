#include "HUInteractComponent.h"
#include "HUInteractable.h"
#include "HUPromptWidget.h"
#include "HUCaseSubsystem.h"
#include "HUEvidence.h"
#include "HUBuild.h"
#include "HUCharacter.h"
#include "HUSuspectActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

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
	Keys->BindKey(EKeys::Seven, IE_Pressed, this, &UHUInteractComponent::Slot7);
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

void UHUInteractComponent::WheelUp() { for (int32 i = 1; i < SlotCount; i++) { const int32 S = (Held + SlotCount - i) % SlotCount; if (SlotUsable(S)) { SelectSlot(S); return; } } }
void UHUInteractComponent::WheelDown() { for (int32 i = 1; i < SlotCount; i++) { const int32 S = (Held + i) % SlotCount; if (SlotUsable(S)) { SelectSlot(S); return; } } }

void UHUInteractComponent::UseHeld()
{
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
	bFlashlightOn = bLightOn && IsHolding(EHUTool::Flashlight);
	bUVOn = bLightOn && IsHolding(EHUTool::UVLight);
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
	case 6: // handcuffs: two steel rings and the chain
		for (float Sd : { -1.f, 1.f }) P(EShape::Cylinder, FVector(10, Sd * 4.5f, 2), FVector(7, 7, 1.2f), FLinearColor(0.7f, 0.72f, 0.75f), FRotator(0, 0, 90));
		P(EShape::Cube, FVector(10, 0, 2), FVector(1.5f, 4, 1.5f), FLinearColor(0.6f, 0.62f, 0.65f));
		break;
	default: break; // empty hand
	}
}

// ---------- the kit ----------
bool UHUInteractComponent::InBeam(FVector Point, bool bUV) const
{
	if (bUV ? !bUVOn : !bFlashlightOn) return false;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC) return false;
	FVector Eye; FRotator Look;
	PC->GetPlayerViewPoint(Eye, Look);
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
	if (!PC || !PC->IsLocalController()) return; // only the player at this screen
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

void UHUInteractComponent::SendAnswer(const FString& SuspectPath, const FString& Question, const FString& Answer)
{
	const ENetMode Mode = GetWorld()->GetNetMode();
	if (Mode == NM_Standalone) { MulticastAnswer_Implementation(SuspectPath, Question, Answer); return; }
	if (GetOwner()->HasAuthority()) MulticastAnswer(SuspectPath, Question, Answer);
	else ServerAnswer(SuspectPath, Question, Answer);
}
void UHUInteractComponent::ServerAnswer_Implementation(const FString& SuspectPath, const FString& Question, const FString& Answer)
{
	MulticastAnswer(SuspectPath, Question.Left(200), Answer.Left(400));
}
void UHUInteractComponent::MulticastAnswer_Implementation(const FString& SuspectPath, const FString& Question, const FString& Answer)
{
	if (AHUSuspectActor* S = FindObject<AHUSuspectActor>(nullptr, *SuspectPath)) S->ApplyAnswer(Cast<APawn>(GetOwner()), Question, Answer);
}

void UHUInteractComponent::ServerInterrogate_Implementation(const FString& SuspectPath, uint8 Action, int32 Statement, const FString& ClueId)
{
	MulticastInterrogate(SuspectPath, Action, Statement, ClueId);
}

void UHUInteractComponent::MulticastInterrogate_Implementation(const FString& SuspectPath, uint8 Action, int32 Statement, const FString& ClueId)
{
	if (AHUSuspectActor* S = FindObject<AHUSuspectActor>(nullptr, *SuspectPath)) S->ApplyInterrogation(Cast<APawn>(GetOwner()), Action, Statement, ClueId);
}