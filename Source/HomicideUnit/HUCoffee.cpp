#include "HUCoffee.h"
#include "HUBuild.h"
#include "HUCharacter.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"

using namespace HUBuild;

AHUCoffee::AHUCoffee()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AHUCoffee::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks);
	// a folding table, a cardboard carrier, four paper cups
	Block(this, RootComponent, Blocks, EShape::Cube, FVector(0, 0, 72), FVector(60, 90, 4), FLinearColor(0.5f, 0.5f, 0.48f));
	for (float X : { -26.f, 26.f }) for (float Y : { -40.f, 40.f }) Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(X, Y, 36), FVector(3, 3, 72), FLinearColor(0.2f, 0.2f, 0.2f));
	Block(this, RootComponent, Blocks, EShape::Cube, FVector(0, 0, 78), FVector(24, 24, 8), FLinearColor(0.55f, 0.42f, 0.25f));
	for (float X : { -6.f, 6.f }) for (float Y : { -6.f, 6.f })
	{
		Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(X, Y, 86), FVector(9, 9, 14), FLinearColor(0.9f, 0.9f, 0.87f));
		Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(X, Y, 93.5f), FVector(9.5f, 9.5f, 1.5f), FLinearColor(0.1f, 0.1f, 0.1f));
	}
}

FText AHUCoffee::GetPrompt_Implementation(APawn* User) const
{
	return FText::FromString(TEXT("[E] Grab a coffee  (faster work for a while)"));
}

void AHUCoffee::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	AHUCharacter* Ch = Cast<AHUCharacter>(User);
	if (!I || !Ch || bSecondary) return;
	Ch->DrinkCoffee();
	I->PlayReach();
	I->Say(TEXT("Lukewarm and too sweet. Patrol's idea of a kindness. It'll do."));
	I->Notify(TEXT("COFFEE"), TEXT("Quicker hands, quicker feet - for a couple of minutes"), FLinearColor(0.85f, 0.6f, 0.35f));
}
