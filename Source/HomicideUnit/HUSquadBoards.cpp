#include "HUSquadBoards.h"
#include "HUBuild.h"
#include "HUProgress.h"
#include "HUCaseSubsystem.h"
#include "HUPlayerState.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

// both boards: +X is the front (they're spawned against a wall, turned to face the room)
static UTextRenderComponent* MakeText(AActor* Owner, USceneComponent* Root, const TCHAR* Name, FVector At, float Size, FColor Col, EHorizTextAligment H = EHTA_Left)
{
	UTextRenderComponent* T = Owner->CreateDefaultSubobject<UTextRenderComponent>(Name);
	T->SetupAttachment(Root);
	T->SetRelativeLocation(At);
	T->SetHorizontalAlignment(H);
	T->SetVerticalAlignment(EVRTA_TextTop);
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Col);
	return T;
}

// ---------------- upgrades board ----------------
AHUUpgradeBoard::AHUUpgradeBoard()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Title = MakeText(this, Root, TEXT("Title"), FVector(8, 0, 262), 16.f, FColor(232, 200, 106), EHTA_Center);
	Title->SetText(FText::FromString(TEXT("PRECINCT UPGRADES")));
	Body = MakeText(this, Root, TEXT("Body"), FVector(8, -140, 238), 7.5f, FColor(225, 232, 240));
	Squad = MakeText(this, Root, TEXT("Squad"), FVector(8, -140, 150), 7.f, FColor(150, 210, 255));
}

UHUCaseSubsystem* AHUUpgradeBoard::Cases() const { return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr; }

void AHUUpgradeBoard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	using namespace HUBuild;
	Clear(Blocks);
	// a cork board in a dark wood frame
	Block(this, RootComponent, Blocks, EShape::Cube, FVector(3, 0, 190), FVector(6, 300, 170), FLinearColor(0.16f, 0.09f, 0.04f));
	Block(this, RootComponent, Blocks, EShape::Cube, FVector(6.5f, 0, 190), FVector(1, 284, 154), FLinearColor(0.45f, 0.32f, 0.18f), FRotator::ZeroRotator, false);
}

void AHUUpgradeBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if ((Refresh -= DeltaSeconds) > 0.f) return;
	Refresh = 0.5f;
	const UHUCaseSubsystem* C = Cases();
	const int32 Have = HUProgress::SquadUpgrades(GetWorld());
	FString T = FString::Printf(TEXT("YOUR CASH  $%d<br><br>"), C ? C->Cash : 0);
	for (int32 i = 0; i < HUProgress::UpgradeCount; i++)
	{
		const bool bMine = C && (C->Upgrades & (1 << i));
		const bool bSquad = (Have & (1 << i)) != 0;
		T += FString::Printf(TEXT("%s %s   %s<br>"), i == Selected ? TEXT(">") : TEXT("  "), HUProgress::UpgradeName(i),
			bMine ? TEXT("YOURS") : bSquad ? TEXT("SQUAD HAS IT") : *FString::Printf(TEXT("$%d"), HUProgress::UpgradePrice(i)));
	}
	Body->SetText(FText::FromString(T));
	// the squad: rank and specialty of everyone here
	FString S = TEXT("THE SQUAD<br>");
	if (const AGameStateBase* GS = GetWorld()->GetGameState())
		for (const APlayerState* P : GS->PlayerArray)
			if (const AHUPlayerState* HP = Cast<AHUPlayerState>(P))
				S += FString::Printf(TEXT("%s  -  %s  -  %s%s<br>"), *P->GetPlayerName().Replace(TEXT("_"), TEXT(" ")), HUProgress::RankName(HP->Rank), HUProgress::SpecName(HP->Specialty),
					HP->Specialty ? *FString::Printf(TEXT(" %d"), HP->SpecLevel) : TEXT(""));
	Squad->SetText(FText::FromString(S));
}

FText AHUUpgradeBoard::GetPrompt_Implementation(APawn* User) const
{
	const UHUCaseSubsystem* C = Cases();
	const int32 i = Selected;
	const bool bMine = C && (C->Upgrades & (1 << i));
	if (bMine) return FText::FromString(FString::Printf(TEXT("%s (yours: the squad has it while you're here)  ·  [Q] Next"), HUProgress::UpgradeName(i)));
	return FText::FromString(FString::Printf(TEXT("[E] Buy %s — $%d: %s  ·  [Q] Next  ·  you have $%d"), HUProgress::UpgradeName(i), HUProgress::UpgradePrice(i), HUProgress::UpgradePitch(i), C ? C->Cash : 0));
}

void AHUUpgradeBoard::Interact_Implementation(APawn* User, bool bSecondary)
{
	if (!User || !User->IsLocallyControlled()) return; // your cash: only your own game handles your buying
	UHUCaseSubsystem* C = Cases();
	UHUInteractComponent* I = User->FindComponentByClass<UHUInteractComponent>();
	if (bSecondary)
	{
		Selected = (Selected + 1) % HUProgress::UpgradeCount;
		if (I) I->Say(FString::Printf(TEXT("%s — %s"), HUProgress::UpgradeName(Selected), HUProgress::UpgradePitch(Selected)));
	}
	else if (C && I)
	{
		if (C->Upgrades & (1 << Selected)) I->Say(TEXT("You've already paid for that one."));
		else if (C->BuyUpgrade(Selected)) I->Notify(TEXT("PRECINCT UPGRADED"), FString::Printf(TEXT("%s — %s"), HUProgress::UpgradeName(Selected), HUProgress::UpgradePitch(Selected)), FLinearColor(0.42f, 0.9f, 0.6f));
		else I->Say(FString::Printf(TEXT("The %s is $%d. You have $%d — close some cases first."), HUProgress::UpgradeName(Selected), HUProgress::UpgradePrice(Selected), C->Cash));
	}
	Refresh = 0.f;
}

// ---------------- case wall ----------------
AHUCaseWall::AHUCaseWall()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Title = MakeText(this, Root, TEXT("Title"), FVector(4, 0, 275), 16.f, FColor(232, 200, 106), EHTA_Center);
	Title->SetText(FText::FromString(TEXT("CASES CLOSED")));
	for (int32 i = 0; i < 12; i++)
	{
		const float Y = -165.f + (i % 3) * 165.f, Z = 245.f - (i / 3) * 45.f;
		UTextRenderComponent* P = MakeText(this, Root, *FString::Printf(TEXT("Plaque%d"), i), FVector(5, Y, Z), 4.5f, FColor(30, 26, 20), EHTA_Center);
		Plaques.Add(P);
	}
}

void AHUCaseWall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	using namespace HUBuild;
	Clear(Blocks);
	// twelve frames: a cream card in a black frame each
	for (int32 i = 0; i < 12; i++)
	{
		const float Y = -165.f + (i % 3) * 165.f, Z = 235.f - (i / 3) * 45.f;
		Block(this, RootComponent, Blocks, EShape::Cube, FVector(2, Y, Z), FVector(3, 150, 38), FLinearColor(0.03f, 0.03f, 0.03f), FRotator::ZeroRotator, false);
		Block(this, RootComponent, Blocks, EShape::Cube, FVector(3.8f, Y, Z), FVector(1, 140, 30), FLinearColor(0.85f, 0.8f, 0.68f), FRotator::ZeroRotator, false);
	}
}

void AHUCaseWall::BeginPlay()
{
	Super::BeginPlay();
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, this, &AHUCaseWall::Fill, 2.f, true); // a case submitted here goes up at once
	Fill();
}

void AHUCaseWall::Fill()
{
	const UHUCaseSubsystem* C = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	const int32 N = C ? C->CaseWall.Num() : 0;
	for (int32 i = 0; i < Plaques.Num(); i++)
	{
		// newest first; each card breaks onto two lines at the " / " before the names
		FString Line = i < N ? C->CaseWall[N - 1 - i] : FString(i == 0 && N == 0 ? TEXT("(empty - close a case)") : TEXT(""));
		Line.ReplaceInline(TEXT("  /  "), TEXT("<br>"));
		Plaques[i]->SetText(FText::FromString(Line));
	}
}
