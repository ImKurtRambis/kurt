#include "HUPoliceVan.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "HUSuspectActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

using namespace HUBuild;

// +X is the front of the van; the back doors face -X
AHUPoliceVan::AHUPoliceVan()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SideText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SideText"));
	SideText->SetupAttachment(Root);
	SideText->SetRelativeLocation(FVector(-40, -112, 150));
	SideText->SetRelativeRotation(FRotator(0, -90, 0));
	SideText->SetHorizontalAlignment(EHTA_Center);
	SideText->SetWorldSize(22.f);
	SideText->SetTextRenderColor(FColor(240, 240, 235));
	SideText->SetText(FText::FromString(TEXT("METRO POLICE  ·  PRISONER TRANSPORT")));
}

void AHUPoliceVan::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks);
	USceneComponent* R = RootComponent;
	const FLinearColor Body(0.06f, 0.08f, 0.16f), Stripe(0.85f, 0.85f, 0.82f), Glass(0.02f, 0.03f, 0.05f), Tire(0.01f, 0.01f, 0.01f), Steel(0.4f, 0.42f, 0.45f);
	Block(this, R, Blocks, EShape::Cube, FVector(-50, 0, 140), FVector(420, 220, 220), Body);                 // the box
	Block(this, R, Blocks, EShape::Cube, FVector(230, 0, 105), FVector(140, 214, 150), Body);                // the cab
	Block(this, R, Blocks, EShape::Cube, FVector(296, 0, 140), FVector(8, 190, 70), Glass, FRotator(-15, 0, 0)); // windshield
	for (float S : { -1.f, 1.f })
	{
		Block(this, R, Blocks, EShape::Cube, FVector(-50, S * 111, 110), FVector(420, 2, 18), Stripe, FRotator::ZeroRotator, false);
		Block(this, R, Blocks, EShape::Cube, FVector(-120, S * 111, 190), FVector(60, 2, 30), Glass, FRotator::ZeroRotator, false); // barred window
		for (float X : { 200.f, -180.f }) Block(this, R, Blocks, EShape::Cylinder, FVector(X, S * 100, 40), FVector(80, 80, 30), Tire, FRotator(0, 0, 90));
	}
	// the back doors, with a step
	for (float S : { -1.f, 1.f }) Block(this, R, Blocks, EShape::Cube, FVector(-262, S * 52, 140), FVector(6, 100, 200), Body * 1.3f);
	Block(this, R, Blocks, EShape::Cube, FVector(-280, 0, 32), FVector(40, 180, 8), Steel);
	Block(this, R, Blocks, EShape::Cube, FVector(-266, 0, 160), FVector(2, 4, 40), Steel); // handle
	Block(this, R, Blocks, EShape::Cube, FVector(-40, 0, 254), FVector(60, 140, 10), FLinearColor(0.02f, 0.02f, 0.02f)); // roof bar
	Block(this, R, Blocks, EShape::Cube, FVector(-40, -40, 261), FVector(50, 60, 8), FLinearColor(1.5f, 0.02f, 0.02f), FRotator::ZeroRotator, false);
	Block(this, R, Blocks, EShape::Cube, FVector(-40, 40, 261), FVector(50, 60, 8), FLinearColor(0.03f, 0.1f, 1.5f), FRotator::ZeroRotator, false);
}

AHUSuspectActor* AHUPoliceVan::EscortedBy(const APawn* User) const
{
	for (TActorIterator<AHUSuspectActor> It(GetWorld()); It; ++It)
		if (It->IsEscortedBy(User) && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < LoadRange) return *It;
	return nullptr;
}

int32 AHUPoliceVan::Inside() const
{
	const UHUCaseSubsystem* C = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	return C ? C->Arrested.Num() : 0;
}

FText AHUPoliceVan::GetPrompt_Implementation(APawn* User) const
{
	if (const AHUSuspectActor* S = EscortedBy(User)) return FText::FromString(FString::Printf(TEXT("[E] Put %s in the van"), *S->DisplayName()));
	const int32 N = Inside();
	return FText::FromString(N == 0 ? FString(TEXT("Prisoner van — empty. Cuff a suspect (7) and walk them here"))
		: FString::Printf(TEXT("Prisoner van — %d inside, riding to the precinct with you"), N));
}

void AHUPoliceVan::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	AHUSuspectActor* S = EscortedBy(User);
	if (!I || !S || bSecondary) return;
	const FString Name = S->DisplayName();
	S->LoadIntoVan();
	I->PlayReach();
	I->Say(FString::Printf(TEXT("You put %s in the back of the van. The doors slam shut."), *Name));
	I->Notify(TEXT("IN THE VAN"), FString::Printf(TEXT("%s — on the way to the precinct"), *Name), FLinearColor(1.f, 0.7f, 0.3f));
}
