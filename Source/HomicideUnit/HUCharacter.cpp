#include "HUCharacter.h"
#include "HUInteractComponent.h"
#include "HUCaseSubsystem.h"
#include "HUSquadSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

AHUCharacter::AHUCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
	Eyes = CreateDefaultSubobject<UCameraComponent>(TEXT("Eyes"));
	Eyes->SetupAttachment(GetCapsuleComponent());
	Eyes->SetRelativeLocation(FVector(0, 0, 64));
	Eyes->bUsePawnControlRotation = true;
	Interact = CreateDefaultSubobject<UHUInteractComponent>(TEXT("HUInteract"));
	bUseControllerRotationYaw = true;
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->MaxWalkSpeed = WalkSpeed;
	M->GetNavAgentPropertiesRef().bCanCrouch = true;
	M->SetCrouchedHalfHeight(44.f); // low enough for crawlspaces
	M->MaxWalkSpeedCrouched = 180.f;
	M->JumpZVelocity = 380.f;
	M->AirControl = 0.2f;
}

void AHUCharacter::BeginPlay()
{
	Super::BeginPlay();
	const UHUCaseSubsystem* C = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	if (C) Eyes->SetFieldOfView(C->FieldOfView);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void AHUCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AHUCharacter::StartJump);
	Input->BindKey(EKeys::SpaceBar, IE_Released, this, &AHUCharacter::EndJump);
	Input->BindKey(EKeys::C, IE_Pressed, this, &AHUCharacter::ToggleCrouch);
	Input->BindKey(EKeys::LeftControl, IE_Pressed, this, &AHUCharacter::ToggleCrouch);
	Input->BindKey(EKeys::Escape, IE_Pressed, this, &AHUCharacter::BackToDesk);
}

void AHUCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	// look (works whatever input mappings the project has)
	const UHUCaseSubsystem* C = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	const float Sens = C ? C->MouseSensitivity : 1.f;
	float DX = 0, DY = 0;
	PC->GetInputMouseDelta(DX, DY);
	AddControllerYawInput(DX * Sens);
	AddControllerPitchInput(-DY * Sens);
	// seated: any move key gets you up
	if (bSeated)
	{
		if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::D)) StandUp();
		return;
	}
	// walk
	if (!PC->IsMoveInputIgnored())
	{
		const FRotator Yaw(0, GetControlRotation().Yaw, 0);
		const FVector Fwd = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
		const float F = (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down) ? 1.f : 0.f);
		const float Rt = (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left) ? 1.f : 0.f);
		if (F != 0.f) AddMovementInput(Fwd, F);
		if (Rt != 0.f) AddMovementInput(Right, Rt);
	}
	// the pushable furniture lowers MaxWalkSpeed while you drag it - only change speed when not dragging
	UCharacterMovementComponent* M = GetCharacterMovement();
	if (M->MaxWalkSpeed >= WalkSpeed - 1.f) M->MaxWalkSpeed = PC->IsInputKeyDown(EKeys::LeftShift) ? RunSpeed : WalkSpeed;
}

void AHUCharacter::SitDown(FVector SeatTop, float Yaw)
{
	if (bSeated) return;
	if (bIsCrouched) UnCrouch();
	StandSpot = GetActorLocation();
	bSeated = true;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	SetActorEnableCollision(false);
	// hips on the cushion, eyes about 80 cm above it
	SetActorLocation(SeatTop + FVector(0, 0, 50), false, nullptr, ETeleportType::TeleportPhysics);
	Eyes->SetRelativeLocation(FVector(0, 0, 30));
	if (AController* C = GetController()) C->SetControlRotation(FRotator(-5.f, Yaw, 0.f));
}

void AHUCharacter::StandUp()
{
	if (!bSeated) return;
	bSeated = false;
	// step back out to where you came from (or just beside the chair)
	const FVector Seat = GetActorLocation();
	const FVector Out = FVector::Dist2D(StandSpot, Seat) < 250.f ? StandSpot : Seat + GetActorForwardVector() * 70.f;
	SetActorLocation(FVector(Out.X, Out.Y, Seat.Z + 50.f), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorEnableCollision(true);
	Eyes->SetRelativeLocation(FVector(0, 0, 64));
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AHUCharacter::ToggleCrouch()
{
	if (bSeated) return;
	if (bIsCrouched) UnCrouch(); else Crouch();
}

void AHUCharacter::BackToDesk()
{
	if (UHUSquadSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUSquadSubsystem>() : nullptr) S->LeaveSquad();
	UGameplayStatics::OpenLevel(this, MenuLevel);
}

void AHUCharacter::ClientSitDown_Implementation(FVector SeatTop, float Yaw)
{
	SitDown(SeatTop, Yaw);
}
