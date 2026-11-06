#include "HUOfficer.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"

using namespace HUBuild;

AHUOfficer::AHUOfficer()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AHUOfficer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks);
	const FLinearColor Navy(0.03f, 0.05f, 0.12f), Skin = Variant % 3 == 0 ? FLinearColor(0.6f, 0.44f, 0.34f) : Variant % 3 == 1 ? FLinearColor(0.4f, 0.27f, 0.18f) : FLinearColor(0.72f, 0.55f, 0.44f);
	USceneComponent* R = RootComponent;
	Block(this, R, Blocks, EShape::Cylinder, FVector(0, 0, 45), FVector(38, 44, 90), Navy * 0.8f);
	Block(this, R, Blocks, EShape::Cylinder, FVector(0, 0, 125), FVector(52, 58, 76), Navy);
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 92), FVector(54, 60, 6), FLinearColor(0.02f, 0.02f, 0.02f));          // duty belt
	Block(this, R, Blocks, EShape::Cube, FVector(25, 12, 145), FVector(2, 6, 7), FLinearColor(0.85f, 0.7f, 0.25f));           // badge
	Block(this, R, Blocks, EShape::Sphere, FVector(0, 0, 178), FVector(26, 24, 30), Skin);
	Block(this, R, Blocks, EShape::Cylinder, FVector(0, 0, 193), FVector(30, 30, 8), Navy * 0.7f);                             // cap
	Block(this, R, Blocks, EShape::Cube, FVector(10, 0, 190), FVector(16, 26, 2), FLinearColor(0.02f, 0.02f, 0.02f));         // visor
}

FText AHUOfficer::GetPrompt_Implementation(APawn* User) const { return FText::FromString(TEXT("[E] Talk to the patrol officer")); }

void AHUOfficer::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return;
	const UHUCaseSubsystem* C = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	const FString Place = C ? C->Current.Place : FString(TEXT("the building"));
	const FString Lines[] = {
		FString::Printf(TEXT("\"Evening, detective. Nobody's gone in or out of %s since we got here.\""), *Place),
		TEXT("\"Everyone who was inside is still inside. We told them to sit tight.\""),
		TEXT("\"Your car's right there at the curb whenever you're done.\""),
		TEXT("\"Crowd's starting to gather. Reporters'll be next.\""),
	};
	I->Say(TEXT("PATROL OFFICER:  ") + Lines[(Line++ + Variant) % UE_ARRAY_COUNT(Lines)]);
}
