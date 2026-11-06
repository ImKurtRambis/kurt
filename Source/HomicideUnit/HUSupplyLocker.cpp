#include "HUSupplyLocker.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"

// +X is the front of the locker
AHUSupplyLocker::AHUSupplyLocker()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Root);
	Sign->SetRelativeLocation(FVector(32, 0, 235));
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetWorldSize(18.f);
	Sign->SetTextRenderColor(FColor(232, 200, 106));
	Sign->SetText(FText::FromString(TEXT("SUPPLY LOCKER")));
	Shelf = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Shelf"));
	Shelf->SetupAttachment(Root);
	Shelf->SetRelativeLocation(FVector(33, 0, 150));
	Shelf->SetHorizontalAlignment(EHTA_Center);
	Shelf->SetVerticalAlignment(EVRTA_TextCenter);
	Shelf->SetWorldSize(9.f);
	Shelf->SetTextRenderColor(FColor(220, 230, 240));
}

UHUCaseSubsystem* AHUSupplyLocker::Cases() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
}

void AHUSupplyLocker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	using namespace HUBuild;
	Clear(Blocks);
	const FLinearColor Steel(0.18f, 0.24f, 0.3f), Dark(0.05f, 0.06f, 0.07f), Cage(0.4f, 0.42f, 0.44f);
	// a tall steel cabinet with a wire-mesh window and a counter in front
	Block(this, RootComponent, Blocks, EShape::Cube, FVector(0, 0, 110), FVector(60, 220, 220), Steel);
	Block(this, RootComponent, Blocks, EShape::Cube, FVector(31, 0, 150), FVector(2, 180, 100), Dark, FRotator::ZeroRotator, false);
	for (float Y = -90.f; Y <= 90.f; Y += 20.f) Block(this, RootComponent, Blocks, EShape::Cube, FVector(32, Y, 150), FVector(1, 1.5f, 100), Cage, FRotator::ZeroRotator, false);
	Block(this, RootComponent, Blocks, EShape::Cube, FVector(55, 0, 95), FVector(50, 220, 6), FLinearColor(0.3f, 0.18f, 0.08f));
	// a few tools on the counter
	Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(55, -60, 104), FVector(6, 6, 22), FLinearColor(0.1f, 0.1f, 0.1f), FRotator(0, 0, 90), false);
	Block(this, RootComponent, Blocks, EShape::Cube, FVector(55, 10, 102), FVector(26, 18, 8), FLinearColor(0.5f, 0.1f, 0.08f), FRotator::ZeroRotator, false);
	Block(this, RootComponent, Blocks, EShape::Cube, FVector(55, 70, 101), FVector(4, 50, 3), FLinearColor(0.6f, 0.1f, 0.1f), FRotator(0, 25, 0), false);
}

void AHUSupplyLocker::BeginPlay()
{
	Super::BeginPlay();
	Refresh();
}

// the shelf list on the cage: ✓ owned, > selected
void AHUSupplyLocker::Refresh()
{
	const UHUCaseSubsystem* S = Cases();
	FString Text = FString::Printf(TEXT("CASH  $%d<br><br>"), S ? S->Cash : 0);
	for (int32 i = 0; i < (int32)EHUTool::Count; i++)
	{
		const EHUTool T = (EHUTool)i;
		const bool bOwned = S && S->HasTool(T);
		Text += FString::Printf(TEXT("%s %s   %s<br>"), i == Selected ? TEXT(">") : TEXT("  "), HUTools::Name(T), bOwned ? TEXT("OWNED") : *FString::Printf(TEXT("$%d"), HUTools::Price(T)));
	}
	Shelf->SetText(FText::FromString(Text));
}

FText AHUSupplyLocker::GetPrompt_Implementation(APawn* User) const
{
	const UHUCaseSubsystem* S = Cases();
	const EHUTool T = (EHUTool)Selected;
	if (S && S->HasTool(T)) return FText::FromString(FString::Printf(TEXT("%s (owned)  ·  [Q] Next item"), HUTools::Name(T)));
	return FText::FromString(FString::Printf(TEXT("[E] Buy %s — $%d  ·  [Q] Next item  ·  you have $%d"), HUTools::Name(T), HUTools::Price(T), S ? S->Cash : 0));
}

void AHUSupplyLocker::Interact_Implementation(APawn* User, bool bSecondary)
{
	if (!User || !User->IsLocallyControlled()) return; // your kit and cash are yours: only your own game handles your shopping
	UHUCaseSubsystem* S = Cases();
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	const EHUTool T = (EHUTool)Selected;
	if (bSecondary)
	{
		Selected = (Selected + 1) % (int32)EHUTool::Count;
		const EHUTool N = (EHUTool)Selected;
		if (I) I->Say(FString::Printf(TEXT("%s — %s"), HUTools::Name(N), HUTools::Pitch(N)));
	}
	else if (S)
	{
		if (S->HasTool(T)) { if (I) I->Say(FString::Printf(TEXT("You already have the %s."), HUTools::Name(T))); }
		else if (S->BuyTool(T)) { if (I) { I->Say(FString::Printf(TEXT("Signed out: %s. %s"), HUTools::Name(T), HUTools::Pitch(T))); I->Notify(TEXT("SIGNED OUT"), FString::Printf(TEXT("%s  ·  $%d left"), HUTools::Name(T), S->Cash), FLinearColor(0.42f, 0.9f, 0.6f)); } }
		else if (I) I->Say(FString::Printf(TEXT("The %s is $%d. You have $%d — close some cases first."), HUTools::Name(T), HUTools::Price(T), S->Cash));
	}
	Refresh();
}
