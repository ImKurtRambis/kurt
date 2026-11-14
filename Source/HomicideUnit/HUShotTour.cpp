#include "HUShotTour.h"
#include "HUSquadRoom.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

void UHUShotTour::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Spec;
	if (!FParse::Value(FCommandLine::Get(), TEXT("HUShots="), Spec, false)) return;
	FParse::Value(FCommandLine::Get(), TEXT("HUShotsName="), Prefix);
	TArray<FString> Parts;
	Spec.ParseIntoArray(Parts, TEXT("|"));
	for (const FString& S : Parts)
	{
		TArray<FString> N;
		S.ParseIntoArray(N, TEXT(","));
		if (N.Num() >= 3) Stops.Add(FVector4(FCString::Atof(*N[0]), FCString::Atof(*N[1]), FCString::Atof(*N[2]), N.Num() > 3 ? FCString::Atof(*N[3]) : -10.f));
	}
	UE_LOG(LogTemp, Log, TEXT("HUShots: %d stops, saved as %s_<n>"), Stops.Num(), *Prefix);
}

// each stop: 6 s for the level to settle the first time, then stand there, wait a second for streaming, shoot
void UHUShotTour::Tick(float DeltaTime)
{
	UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) return;
	Clock += DeltaTime;
	const float Start = 6.f + Done * 2.5f;
	if (Done >= Stops.Num()) { if (Clock > Start + 1.f) { Stops.Reset(); FPlatformMisc::RequestExit(false); } return; }
	if (!bPlaced && Clock >= Start)
	{
		FTransform Frame = FTransform::Identity;
		for (TActorIterator<AHUSquadRoom> It(W); It; ++It) { Frame = It->GetActorTransform(); break; }
		const FVector4& S = Stops[Done];
		P->SetActorLocation(Frame.TransformPosition(FVector(S.X, S.Y, P->GetActorLocation().Z - Frame.GetLocation().Z)), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(S.W, S.Z + Frame.Rotator().Yaw, 0));
		bPlaced = true;
	}
	if (bPlaced && Clock >= Start + 1.2f)
	{
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s_%02d"), *Prefix, Done), false, false);
		UE_LOG(LogTemp, Log, TEXT("HUShots: %s_%02d at %s"), *Prefix, Done, *P->GetActorLocation().ToString());
		Done++; bPlaced = false;
	}
}
