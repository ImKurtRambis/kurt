#include "HUPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "HUProgress.h"
#include "HUCaseSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

void AHUPlayerState::ServerSetReady_Implementation(bool bNewReady)
{
	bReady = bNewReady;
}

void AHUPlayerState::ServerSetProgress_Implementation(uint8 InSpecialty, int32 InSpecLevel, int32 InRank, int32 InUpgrades)
{
	Specialty = InSpecialty < HUProgress::SpecCount ? InSpecialty : 0;
	SpecLevel = FMath::Clamp(InSpecLevel, 1, 5);
	Rank = FMath::Clamp(InRank, 0, HUProgress::RankCount - 1);
	Upgrades = InUpgrades & ((1 << HUProgress::UpgradeCount) - 1);
}

void AHUPlayerState::PushLocal(UWorld* World)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AHUPlayerState* PS = PC ? PC->GetPlayerState<AHUPlayerState>() : nullptr;
	const UHUCaseSubsystem* C = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	if (!PS || !C) return;
	PS->ServerSetProgress(C->Specialty, HUProgress::SpecLevel(C->SpecXPOf(C->Specialty)), HUProgress::RankOf(C->SquadXP), C->Upgrades);
}

void AHUPlayerState::BeginPlay()
{
	Super::BeginPlay();
	// our own player state: tell the squad who we are (a moment later, once the controller owns it)
	FTimerHandle H;
	TWeakObjectPtr<AHUPlayerState> Self(this);
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self]()
	{
		if (!Self.IsValid()) return;
		const APlayerController* PC = Self->GetPlayerController();
		if (PC && PC->IsLocalController()) PushLocal(Self->GetWorld());
	}), 1.f, false);
}

void AHUPlayerState::ServerSetOffice_Implementation(FHUOfficeLook NewLook) { Office = NewLook; }

void AHUPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHUPlayerState, bReady);
	DOREPLIFETIME(AHUPlayerState, Desk);
	DOREPLIFETIME(AHUPlayerState, Office);
	DOREPLIFETIME(AHUPlayerState, Specialty);
	DOREPLIFETIME(AHUPlayerState, SpecLevel);
	DOREPLIFETIME(AHUPlayerState, Rank);
	DOREPLIFETIME(AHUPlayerState, Upgrades);
}
