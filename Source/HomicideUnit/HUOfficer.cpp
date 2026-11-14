#include "HUOfficer.h"
#include "HUPeople.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"

AHUOfficer::AHUOfficer()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	InteractionBody = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractionBody"));
	InteractionBody->SetupAttachment(Root);
	InteractionBody->SetRelativeLocation(FVector(0, 0, 95));
	InteractionBody->InitCapsuleSize(32.f, 95.f);
	InteractionBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	InteractionBody->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBody->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}



void AHUOfficer::BeginPlay()
{
	Super::BeginPlay();
	// The real officer faces +X; the capsule supplies conversation and movement collision.
	Look = HUPeople::Stand(this, RootComponent, HUPeople::OfficerFace(Variant), FVector::ZeroVector, 0.f);
	if (Look) HUPeople::Uniform(Look);
}

void AHUOfficer::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Look) { Look->Destroy(); Look = nullptr; }
	Super::EndPlay(Reason);
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
