#include "HUCruiser.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

AHUCruiser::AHUCruiser()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	RedLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RedLight"));
	RedLight->SetupAttachment(Root);
	RedLight->SetRelativeLocation(FVector(-10, -40, 175));
	RedLight->SetLightColor(FLinearColor(1, 0.05f, 0.05f));
	RedLight->SetAttenuationRadius(700.f);
	RedLight->SetIntensity(0.f);
	BlueLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BlueLight"));
	BlueLight->SetupAttachment(Root);
	BlueLight->SetRelativeLocation(FVector(-10, 40, 175));
	BlueLight->SetLightColor(FLinearColor(0.1f, 0.25f, 1));
	BlueLight->SetAttenuationRadius(700.f);
	BlueLight->SetIntensity(0.f);
}

// a black-and-white sedan out of blocks; +X is the nose
void AHUCruiser::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	using namespace HUBuild;
	Clear(Blocks);
	USceneComponent* R = RootComponent;
	const FLinearColor Black(0.015f, 0.015f, 0.018f), White(0.85f, 0.86f, 0.88f), Glass(0.02f, 0.03f, 0.05f), Chrome(0.6f, 0.6f, 0.62f), Tire(0.01f, 0.01f, 0.01f);
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 60), FVector(480, 190, 60), Black);            // body
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 60), FVector(200, 192, 58), White);            // white doors
	Block(this, R, Blocks, EShape::Cube, FVector(150, 0, 92), FVector(170, 186, 6), Black);           // hood
	Block(this, R, Blocks, EShape::Cube, FVector(-15, 0, 118), FVector(230, 170, 56), White);         // cabin
	Block(this, R, Blocks, EShape::Cube, FVector(-15, 0, 120), FVector(200, 174, 40), Glass);         // side glass
	Block(this, R, Blocks, EShape::Cube, FVector(102, 0, 116), FVector(6, 160, 44), Glass, FRotator(-35, 0, 0)); // windshield
	Block(this, R, Blocks, EShape::Cube, FVector(242, 0, 50), FVector(6, 180, 22), Chrome);           // bumpers
	Block(this, R, Blocks, EShape::Cube, FVector(-242, 0, 50), FVector(6, 180, 22), Chrome);
	for (int32 S = -1; S <= 1; S += 2)
	{
		Block(this, R, Blocks, EShape::Cube, FVector(243, S * 65, 68), FVector(4, 30, 14), FLinearColor(4, 4, 3.5f), FRotator::ZeroRotator, false); // headlights
		Block(this, R, Blocks, EShape::Cube, FVector(-243, S * 70, 70), FVector(4, 30, 12), FLinearColor(2, 0.05f, 0.05f), FRotator::ZeroRotator, false); // tail lights
		for (float X : { 150.f, -150.f })
		{
			Block(this, R, Blocks, EShape::Cylinder, FVector(X, S * 92, 34), FVector(68, 68, 26), Tire, FRotator(0, 0, 90));
			Block(this, R, Blocks, EShape::Cylinder, FVector(X, S * 106, 34), FVector(36, 36, 2), Chrome, FRotator(0, 0, 90), false);
		}
	}
	// the light bar
	Block(this, R, Blocks, EShape::Cube, FVector(-10, 0, 150), FVector(30, 150, 10), Black);
	Block(this, R, Blocks, EShape::Cube, FVector(-10, -40, 158), FVector(26, 64, 10), FLinearColor(1.5f, 0.02f, 0.02f), FRotator::ZeroRotator, false);
	Block(this, R, Blocks, EShape::Cube, FVector(-10, 40, 158), FVector(26, 64, 10), FLinearColor(0.03f, 0.1f, 1.5f), FRotator::ZeroRotator, false);
}

void AHUCruiser::CountSquad(int32& AtCar, int32& Total) const
{
	AtCar = 0; Total = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APawn* P = PC ? PC->GetPawn() : nullptr;
		if (!P) continue;
		Total++;
		if (FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) <= GatherRadius) AtCar++;
	}
}

bool AHUCruiser::Ready(FString* Why) const
{
	const UHUCaseSubsystem* Cases = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	if (!Cases || !Cases->bCaseActive) { if (Why) *Why = TEXT("Your cruiser — no case on the board"); return false; }
	if (!Cases->bBriefed) { if (Why) *Why = TEXT("Your cruiser — the Captain wants you at the briefing first"); return false; }
	int32 AtCar, Total; CountSquad(AtCar, Total);
	if (AtCar < Total) { if (Why) *Why = FString::Printf(TEXT("Waiting for your squad — %d of %d at the car"), AtCar, Total); return false; }
	return true;
}

FText AHUCruiser::GetPrompt_Implementation(APawn* User) const
{
	if (bLeaving) return FText::GetEmpty();
	if (!bPlayerCar) return FText::FromString(TEXT("A patrol car — yours is the one parked by the door"));
	FString Why;
	if (!Ready(&Why)) return FText::FromString(Why);
	const UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	return FText::FromString(TEXT("[E] Drive to ") + (DestinationLabel.IsEmpty() ? Cases->Current.Place : DestinationLabel));
}

void AHUCruiser::Interact_Implementation(APawn* User, bool bSecondary)
{
	if (bPlayerCar && !bSecondary && !bLeaving && Ready()) Depart();
}

void AHUCruiser::Depart()
{
	bLeaving = true;
	const UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	const FString Line = FString::Printf(TEXT("En route to %s..."), !DestinationLabel.IsEmpty() ? *DestinationLabel : Cases ? *Cases->Current.Place : TEXT("the scene"));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 1.2f, FLinearColor::Black, false, true);
		if (APawn* P = PC->GetPawn())
			if (UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>()) I->Subtitle = FText::FromString(Line);
		PC->SetIgnoreMoveInput(true);
	}
	GetWorldTimerManager().SetTimer(TravelTimer, this, &AHUCruiser::Travel, DriveSeconds, false);
}

void AHUCruiser::Travel()
{
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client) return; // only the host moves the squad; everyone else follows
	const FString Map = SceneLevel.ToString();
	// a hosted co-op game takes everyone along; solo just opens the level
	if (W->GetNetMode() == NM_ListenServer || W->GetNetMode() == NM_DedicatedServer) W->ServerTravel(Map);
	else UGameplayStatics::OpenLevel(this, SceneLevel);
}

void AHUCruiser::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UHUCaseSubsystem* Cases = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	const bool bOn = Cases && Cases->bCaseActive && Cases->bBriefed;
	Flash += DeltaSeconds;
	const bool bRed = FMath::Fmod(Flash, 0.6f) < 0.3f;
	RedLight->SetIntensity(bOn && bRed ? 4000.f : 0.f);
	BlueLight->SetIntensity(bOn && !bRed ? 4000.f : 0.f);
}
