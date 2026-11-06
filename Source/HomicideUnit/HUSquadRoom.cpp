#include "HUSquadRoom.h"
#include "HUBuild.h"
#include "HUSquadSubsystem.h"
#include "HUPlayerState.h"
#include "Components/InputComponent.h"
#include "HUCaseSubsystem.h"
#include "HUCaseGen.h"
#include "Net/UnrealNetwork.h"
#include "HUInteractComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

using namespace HUBuild;

// the room: X from -700 to 700, Y from -500 to 500. The board is on the +X wall, the roll-call door on the -X wall.
static const float RX = 700.f, RY = 500.f, RH = 340.f;

static UTextRenderComponent* MakeText(AActor* Owner, USceneComponent* Root, const TCHAR* Name, FVector Pos, float Yaw, float Size, FColor Col)
{
	UTextRenderComponent* T = Owner->CreateDefaultSubobject<UTextRenderComponent>(Name);
	T->SetupAttachment(Root);
	T->SetRelativeLocation(Pos);
	T->SetRelativeRotation(FRotator(0, Yaw, 0));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Col);
	return T;
}

AHUSquadRoom::AHUSquadRoom()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	// text on the board faces into the room (-X)
	BoardTitle = MakeText(this, Root, TEXT("BoardTitle"), FVector(RX - 8, 0, 270), 180, 24, FColor(235, 235, 225));
	BoardCode = MakeText(this, Root, TEXT("BoardCode"), FVector(RX - 8, 0, 215), 180, 46, FColor(255, 225, 120));
	BoardRoster = MakeText(this, Root, TEXT("BoardRoster"), FVector(RX - 8, 0, 125), 180, 15, FColor(225, 230, 235));
	DoorSign = MakeText(this, Root, TEXT("DoorSign"), FVector(-RX + 8, 0, 285), 0, 20, FColor(255, 181, 71));
	BoardTitle->SetText(FText::FromString(TEXT("SQUAD ROOM")));
	// the case-selection table by the door: a laptop for where, a folder for how hard
	LaptopScreen = MakeText(this, Root, TEXT("LaptopScreen"), FVector(-439.f, -360.f, 88.f), 0, 3.2f, FColor(150, 220, 255));
	FolderLabel = MakeText(this, Root, TEXT("FolderLabel"), FVector(-398.f, -295.f, 92.f), 0, 5.f, FColor(255, 225, 120));
	bReplicates = true;
	bAlwaysRelevant = true;
	DoorSign->SetText(FText::FromString(TEXT("ROLL CALL  >  BRIEFING")));
}

UHUSquadSubsystem* AHUSquadRoom::Squad() const { return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUSquadSubsystem>() : nullptr; }
bool AHUSquadRoom::IsHost() const { return GetNetMode() != NM_Client; }

void AHUSquadRoom::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks); BoardParts.Reset(); DoorParts.Reset(); LaptopParts.Reset(); FolderParts.Reset(); ComputerParts.Reset(); CoffeeParts.Reset(); ClockParts.Reset();
	USceneComponent* R = RootComponent;
	const FLinearColor Floor(0.16f, 0.12f, 0.08f), Wall(0.32f, 0.36f, 0.33f), Trim(0.12f, 0.08f, 0.05f), Desk(0.25f, 0.14f, 0.06f), Steel(0.3f, 0.32f, 0.34f);
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, -5), FVector(RX * 2, RY * 2, 10), Floor);
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, RH + 5), FVector(RX * 2, RY * 2, 10), Wall * 0.6f);
	for (float S : { -1.f, 1.f })
	{
		Block(this, R, Blocks, EShape::Cube, FVector(S * (RX + 5), 0, RH / 2), FVector(10, RY * 2, RH), Wall);
		Block(this, R, Blocks, EShape::Cube, FVector(0, S * (RY + 5), RH / 2), FVector(RX * 2, 10, RH), Wall);
		Block(this, R, Blocks, EShape::Cube, FVector(0, S * (RY - 2), 45), FVector(RX * 2, 4, 90), Trim);
	}
	// the chalkboard
	BoardParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(RX - 4, 0, 200), FVector(4, 560, 190), FLinearColor(0.03f, 0.06f, 0.04f)));
	BoardParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(RX - 5, 0, 200), FVector(6, 580, 210), Trim));
	BoardParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(RX - 12, 0, 96), FVector(14, 560, 4), Trim));
	// the roll-call door: double doors with a glass panel
	for (float S : { -1.f, 1.f })
	{
		DoorParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 4, S * 46, 120), FVector(6, 90, 240), FLinearColor(0.2f, 0.12f, 0.05f)));
		DoorParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 7.5f, S * 46, 170), FVector(1, 50, 60), FLinearColor(0.6f, 0.75f, 0.85f)));
		DoorParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 9, S * 10, 115), FVector(4, 3, 18), FLinearColor(0.75f, 0.6f, 0.2f)));
	}
	// the time clock by the doors: punch in when you're ready
	ClockParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 14, 260, 140), FVector(18, 36, 46), FLinearColor(0.55f, 0.5f, 0.38f)));
	ClockParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-RX + 23.5f, 260, 152), FVector(22, 22, 1), FLinearColor(0.9f, 0.88f, 0.8f), FRotator(0, 0, 90)));
	ClockParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 24, 260, 126), FVector(2, 14, 3), FLinearColor(0.05f, 0.05f, 0.05f)));
	ClockParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 8, 305, 140), FVector(6, 30, 60), FLinearColor(0.35f, 0.25f, 0.12f))); // the card rack
	// the squad's desks, pushed together, with chairs and a coffee pot
	for (float X : { -150.f, 150.f }) for (float Y : { -220.f, 220.f })
	{
		Block(this, R, Blocks, EShape::Cube, FVector(X, Y, 74), FVector(150, 90, 6), Desk);
		for (float DX : { -65.f, 65.f }) for (float DY : { -38.f, 38.f }) Block(this, R, Blocks, EShape::Cube, FVector(X + DX, Y + DY, 36), FVector(5, 5, 72), Desk * 0.7f);
		Block(this, R, Blocks, EShape::Cube, FVector(X - 10, Y + (Y > 0 ? -70 : 70), 45), FVector(45, 45, 6), Steel);
		ComputerParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(X + 30, Y, 90), FVector(30, 34, 26), FLinearColor(0.6f, 0.57f, 0.48f)));
	}
	Block(this, R, Blocks, EShape::Cube, FVector(-500, 420, 45), FVector(120, 60, 90), Desk);
	CoffeeParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-500, 420, 105), FVector(16, 16, 22), FLinearColor(0.05f, 0.05f, 0.05f)));
	CoffeeParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-470, 420, 95), FVector(9, 9, 10), FLinearColor(0.85f, 0.85f, 0.8f)));
	// the case-selection table: a laptop (where) and a case folder (how hard)
	Block(this, R, Blocks, EShape::Cube, FVector(-420, -330, 72), FVector(90, 150, 6), Desk);
	for (float DX : { -40.f, 40.f }) for (float DY : { -68.f, 68.f }) Block(this, R, Blocks, EShape::Cube, FVector(-420 + DX, -330 + DY, 35), FVector(5, 5, 70), Desk * 0.7f);
	LaptopParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-428, -360, 76), FVector(24, 34, 2), FLinearColor(0.12f, 0.12f, 0.13f)));
	LaptopParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-441, -360, 88), FVector(1.5f, 34, 22), FLinearColor(0.12f, 0.12f, 0.13f)));
	LaptopParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-440.1f, -360, 88), FVector(0.2f, 31, 19), FLinearColor(0.02f, 0.06f, 0.12f)));
	FolderParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-405, -295, 75.8f), FVector(24, 32, 1.5f), FLinearColor(0.75f, 0.6f, 0.32f), FRotator(0, 8, 0)));
	FolderParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-398, -295, 84), FVector(1, 26, 14), FLinearColor(0.85f, 0.82f, 0.72f)));
	// overhead lamps
	for (float X : { -350.f, 0.f, 350.f }) Block(this, R, Blocks, EShape::Cube, FVector(X, 0, RH - 4), FVector(140, 30, 4), FLinearColor(3, 3, 2.7f), FRotator::ZeroRotator, false);
}

void AHUSquadRoom::BeginPlay()
{
	Super::BeginPlay();
	// back from a scene (or a fresh shift): the last case is over, and nobody's punched in yet
	if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) { C->ResolveArrests(); C->bCaseActive = false; C->bBriefed = false; }
	if (IsHost())
		if (const AGameStateBase* GS = GetWorld()->GetGameState())
			for (APlayerState* P : GS->PlayerArray) if (AHUPlayerState* HP = Cast<AHUPlayerState>(P)) HP->bReady = false;
	for (float X : { -350.f, 0.f, 350.f })
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetRelativeLocation(FVector(X, 0, RH - 50));
		L->SetIntensity(7000.f);
		L->SetAttenuationRadius(1100.f);
		L->SetLightColor(FLinearColor(1.f, 0.9f, 0.75f));
		L->RegisterComponent();
	}
}

void AHUSquadRoom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UHUSquadSubsystem* S = Squad();
	const FString Code = S && !S->Code.IsEmpty() ? S->Code : FString(TEXT("------"));
	BoardCode->SetText(FText::FromString(TEXT("CODE  ") + Code.Left(3) + TEXT(" ") + Code.Mid(3)));
	FString Roster = FString::Printf(TEXT("ON THE %s<br>"), S ? *S->NetworkName().ToUpper() : TEXT("NETWORK"));
	int32 N = 0;
	if (const AGameStateBase* GS = GetWorld()->GetGameState())
		for (const APlayerState* P : GS->PlayerArray)
			if (P)
			{
				const AHUPlayerState* HP = Cast<AHUPlayerState>(P);
				++N;
				Roster += FString::Printf(TEXT("%s  DET. %s%s<br>"), HP && HP->bReady ? TEXT("[READY]") : TEXT("[ - - ]"), *P->GetPlayerName().Replace(TEXT("_"), TEXT(" ")).ToUpper(), N == 1 ? TEXT("  (HOST)") : TEXT(""));
			}
	if (const UHUCaseSubsystem* CS = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>(); CS && !CS->LastVerdict.IsEmpty()) Roster += TEXT("<br>LAST SHIFT:  ") + CS->LastVerdict.Left(60) + TEXT("<br>");
	Roster += FString::Printf(TEXT("<br>CASE:  %s  ·  %s"), *PlaceLabel(), DifficultyName(DifficultyChoice));
	Roster += Countdown >= 0 ? FString::Printf(TEXT("<br>EVERYONE'S IN — ROLL CALL IN %d"), Countdown) : FString::Printf(TEXT("<br>%d / 4  —  PUNCH IN AT THE TIME CLOCK (OR PRESS Y) WHEN READY"), N);
	BoardRoster->SetText(FText::FromString(Roster));

	LaptopScreen->SetText(FText::FromString(TEXT("LOCATION<br>") + PlaceLabel()));
	FolderLabel->SetText(FText::FromString(FString(DifficultyName(DifficultyChoice)).ToUpper()));
	// everyone's machine remembers the host's picks, so the briefing deals the same case to the whole squad
	if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>())
	{
		C->ChosenPlace = HUCaseGen::Places().IsValidIndex(PlaceChoice) ? HUCaseGen::Places()[PlaceChoice] : FString();
		C->ChosenDifficulty = DifficultyChoice >= 0 ? FString(DifficultyName(DifficultyChoice)).ToLower() : FString();
	}

	// Y readies you up from anywhere in the room
	if (!bKeysBound)
		if (APlayerController* LPC = GetWorld()->GetFirstPlayerController())
		{
			EnableInput(LPC);
			if (InputComponent) { InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &AHUSquadRoom::ToggleReady); bKeysBound = true; }
		}

	// the host watches for everyone being ready, then counts down and starts the shift
	if (IsHost() && !bStarting)
	{
		int32 Total = 0, Ready = 0;
		if (const AGameStateBase* GS = GetWorld()->GetGameState())
			for (const APlayerState* P : GS->PlayerArray) { Total++; if (const AHUPlayerState* HP = Cast<AHUPlayerState>(P)) if (HP->bReady) Ready++; }
		if (Total > 0 && Ready == Total)
		{
			if (Countdown < 0) { Countdown = 5; Tock = 1.f; }
			Tock -= DeltaSeconds;
			if (Tock <= 0.f) { Tock += 1.f; Countdown--; }
			if (Countdown <= 0) { Countdown = 0; bStarting = true; if (UHUSquadSubsystem* SQ = Squad()) SQ->StartShift(); }
		}
		else Countdown = -1;
	}

	// say hello once the player's here
	if (!bWelcomed)
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (APawn* P = PC->GetPawn())
				if (UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>())
				{
					bWelcomed = true;
					if (const UHUCaseSubsystem* CS = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>(); CS && !CS->LastVerdict.IsEmpty()) { I->Notify(TEXT("LAST SHIFT"), CS->LastVerdict, FLinearColor(0.95f, 0.76f, 0.32f)); }
					I->Say(IsHost() ? FString::Printf(TEXT("Your squad code is %s — friends enter it on their cell phone at the desk. Pick the case at the table by the doors, then everyone punches in at the time clock (or presses Y)."), *Code)
						: TEXT("You're in the squad room. Punch in at the time clock by the doors (or press Y) when you're ready."));
				}
}

FText AHUSquadRoom::GetPrompt_Implementation(APawn* User) const
{
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return FText::GetEmpty();
	if (BoardParts.Contains(I->FocusPart)) return FText::FromString(TEXT("[E] Copy the squad code"));
	if (CoffeeParts.Contains(I->FocusPart)) return FText::FromString(TEXT("[E] Pour a coffee"));
	if (ComputerParts.Contains(I->FocusPart)) return FText::FromString(TEXT("[E] Check the case archive"));
	if (LaptopParts.Contains(I->FocusPart))
		return FText::FromString(IsHost() ? FString::Printf(TEXT("Location: %s  ·  [E] next  ·  [Q] previous"), *PlaceLabel()) : FString::Printf(TEXT("Location: %s  (the host picks)"), *PlaceLabel()));
	if (FolderParts.Contains(I->FocusPart))
		return FText::FromString(IsHost() ? FString::Printf(TEXT("Difficulty: %s  ·  [E] next  ·  [Q] previous"), DifficultyName(DifficultyChoice)) : FString::Printf(TEXT("Difficulty: %s  (the host picks)"), DifficultyName(DifficultyChoice)));
	if (DoorParts.Contains(I->FocusPart))
	{
		if (!IsHost()) return FText::FromString(TEXT("The doors open when everyone's punched in"));
		int32 N = 0; if (const AGameStateBase* GS = GetWorld()->GetGameState()) N = GS->PlayerArray.Num();
		return FText::FromString(FString::Printf(TEXT("[E] Start now without waiting (%d detective%s)"), N, N == 1 ? TEXT("") : TEXT("s")));
	}
	if (ClockParts.Contains(I->FocusPart))
	{
		const AHUPlayerState* PS = LocalState();
		return FText::FromString(PS && PS->bReady ? TEXT("[E] Punch out — not ready") : TEXT("[E] Punch in — ready for the shift"));
	}
	return FText::GetEmpty();
}

void AHUSquadRoom::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return;
	if (LaptopParts.Contains(I->FocusPart) || FolderParts.Contains(I->FocusPart))
	{
		if (!(IsHost() && User->IsLocallyControlled())) { if (User->IsLocallyControlled()) I->Say(TEXT("The host picks the case.")); return; }
		const int32 Dir = bSecondary ? -1 : 1;
		if (LaptopParts.Contains(I->FocusPart))
		{
			const int32 N = HUCaseGen::Places().Num() + 1; // + random
			PlaceChoice = ((PlaceChoice + 1 + Dir + N) % N) - 1;
			I->Say(TEXT("Location: ") + PlaceLabel());
		}
		else
		{
			DifficultyChoice = ((DifficultyChoice + 1 + Dir + 4) % 4) - 1;
			I->Say(FString(TEXT("Difficulty: ")) + DifficultyName(DifficultyChoice));
		}
		return;
	}
	if (bSecondary) return;
	if (ClockParts.Contains(I->FocusPart)) { if (User->IsLocallyControlled()) ToggleReady(); return; } // only your own punch card
	if (CoffeeParts.Contains(I->FocusPart)) { const TCHAR* L[] = { TEXT("Burnt, bitter, hot. It does the job."), TEXT("Somebody made a fresh pot. A miracle."), TEXT("You top off the cup. Long night ahead.") }; I->Say(L[FMath::RandRange(0, 2)]); return; }
	if (ComputerParts.Contains(I->FocusPart))
	{
		if (const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>())
			I->Say(FString::Printf(TEXT("CASE ARCHIVE — Det. %s: %d case%s closed. Pay on file: $%d."), C->DetectiveName.IsEmpty() ? TEXT("(no name)") : *C->DetectiveName, C->CasesSolved, C->CasesSolved == 1 ? TEXT("") : TEXT("s"), C->Cash));
		return;
	}
	UHUSquadSubsystem* S = Squad();
	if (BoardParts.Contains(I->FocusPart))
	{
		if (S && !S->Code.IsEmpty() && User->IsLocallyControlled()) { FPlatformApplicationMisc::ClipboardCopy(*S->Code); I->Say(FString::Printf(TEXT("Copied %s. Send it to your squad."), *S->Code)); }
		return;
	}
	if (DoorParts.Contains(I->FocusPart))
	{
		if (!(IsHost() && User->IsLocallyControlled())) { if (User->IsLocallyControlled()) I->Say(TEXT("The doors open when everyone's punched in at the time clock.")); return; }
		if (S) S->StartShift();
	}
}

const TCHAR* AHUSquadRoom::DifficultyName(int32 I)
{
	switch (I) { case 0: return TEXT("Rookie"); case 1: return TEXT("Detective"); case 2: return TEXT("Veteran"); default: return TEXT("Random"); }
}

FString AHUSquadRoom::PlaceLabel() const
{
	return HUCaseGen::Places().IsValidIndex(PlaceChoice) ? HUCaseGen::Places()[PlaceChoice] : FString(TEXT("RANDOM — ANYWHERE IN THE CITY"));
}

void AHUSquadRoom::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHUSquadRoom, PlaceChoice);
	DOREPLIFETIME(AHUSquadRoom, DifficultyChoice);
	DOREPLIFETIME(AHUSquadRoom, Countdown);
}
void AHUSquadRoom::HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const
{
	for (const TArray<UPrimitiveComponent*>* G : { &BoardParts, &DoorParts, &LaptopParts, &FolderParts, &CoffeeParts, &ClockParts })
		if (G->Contains(Part)) { Out.Append(*G); return; }
	if (ComputerParts.Contains(Part)) Out.Add(Part); // just the one computer
}
AHUPlayerState* AHUSquadRoom::LocalState() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC ? PC->GetPlayerState<AHUPlayerState>() : nullptr;
}

void AHUSquadRoom::ToggleReady()
{
	AHUPlayerState* PS = LocalState();
	if (!PS) return;
	const bool bNow = !PS->bReady;
	PS->ServerSetReady(bNow);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (APawn* P = PC->GetPawn())
			if (UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>())
				I->Say(bNow ? TEXT("*KA-CHUNK* You punch in. Ready for the shift.") : TEXT("You punch back out. Not ready."));
}