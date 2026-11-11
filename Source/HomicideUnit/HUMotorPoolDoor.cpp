#include "HUMotorPoolDoor.h"
#include "HUBuild.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HUCaseSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

AHUMotorPoolDoor::AHUMotorPoolDoor()
{
	bReturnTrip = true;
	DestinationLabel = FString(); // the case's place
	SceneLevel = TEXT("L_Scene");
	UTextRenderComponent* Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Root);
	Sign->SetRelativeLocation(FVector(8, 0, 268));
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetVerticalAlignment(EVRTA_TextCenter);
	Sign->SetWorldSize(20.f);
	Sign->SetTextRenderColor(FColor(255, 181, 71));
	Sign->SetText(FText::FromString(TEXT("MOTOR POOL  >  BACK TO THE SCENE")));
}

// a steel double door with wired-glass windows and a push bar, set in the wall; +X faces into the room
void AHUMotorPoolDoor::OnConstruction(const FTransform& Transform)
{
	AActor::OnConstruction(Transform); // not the car
	using namespace HUBuild;
	Clear(Blocks);
	USceneComponent* R = RootComponent;
	const FLinearColor Frame(0.12f, 0.12f, 0.13f), Steel(0.28f, 0.33f, 0.3f), Glass(0.5f, 0.62f, 0.7f), Bar(0.7f, 0.7f, 0.66f);
	Block(this, R, Blocks, EShape::Cube, FVector(1, 0, 125), FVector(4, 200, 250), Frame);
	for (float S : { -1.f, 1.f })
	{
		Block(this, R, Blocks, EShape::Cube, FVector(4, S * 47, 120), FVector(5, 90, 236), Steel);
		Block(this, R, Blocks, EShape::Cube, FVector(6.8f, S * 47, 172), FVector(1, 46, 54), Glass);
		for (float Z : { 155.f, 172.f, 189.f }) Block(this, R, Blocks, EShape::Cube, FVector(7.4f, S * 47, Z), FVector(0.4f, 46, 0.6f), Frame, FRotator::ZeroRotator, false);
		Block(this, R, Blocks, EShape::Cube, FVector(10, S * 47, 105), FVector(5, 76, 6), Bar);
	}
	// a caged lamp over the door
	Block(this, R, Blocks, EShape::Cube, FVector(10, 0, 248), FVector(14, 20, 10), FLinearColor(2.5f, 1.4f, 0.4f), FRotator::ZeroRotator, false);
}

void AHUMotorPoolDoor::Tick(float DeltaSeconds)
{
	AActor::Tick(DeltaSeconds); // no light bar flashing on a door

	// -HUReturnTest: pretend the squad is back from the scene with some evidence and a prisoner, then drive back out
	if (!FParse::Param(FCommandLine::Get(), TEXT("HUReturnTest"))) return;
	const float Was = TestClock;
	TestClock += DeltaSeconds;
	auto Passed = [&](float T) { return Was < T && TestClock >= T; };
	UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!C || !P) return;
	if (Passed(1.f))
	{
		if (!C->bCaseActive) C->StartCase(-1);
		for (int32 e = 0; e < FMath::Min(3, C->Current.Clues.Num()); e++) C->Found.AddUnique(C->Current.Clues[e].Id);
		if (C->Current.Suspects.Num()) C->Arrested.AddUnique(C->Current.Suspects[0].Id);
		C->bAwaitingCaptain = true; C->bBriefed = false;
		UE_LOG(LogTemp, Log, TEXT("HUReturnTest: case %d, bagged %d, in holding %s"), C->Current.Number, C->Found.Num(), C->Arrested.Num() ? *C->Arrested[0] : TEXT("-"));
	}
	if (Passed(2.f))
	{
		P->SetActorLocation(GetActorLocation() + GetActorForwardVector() * 320.f + FVector(0, 0, 95));
		PC->SetControlRotation(FRotator(5, (-GetActorForwardVector()).Rotation().Yaw, 0));
	}
	if (Passed(3.f)) UE_LOG(LogTemp, Log, TEXT("HUReturnTest: door at %s facing %s, detective at %s looking %s"), *GetActorLocation().ToString(), *GetActorForwardVector().ToString(), *P->GetActorLocation().ToString(), *PC->GetControlRotation().ToString());
	if (Passed(4.f)) FScreenshotRequest::RequestScreenshot(TEXT("MotorPoolDoor"), false, false);
	if (Passed(5.f))
	{
		UE_LOG(LogTemp, Log, TEXT("HUReturnTest: prompt '%s'"), *IHUInteractable::Execute_GetPrompt(this, P).ToString());
		IHUInteractable::Execute_Interact(this, P, false);
	}
}
