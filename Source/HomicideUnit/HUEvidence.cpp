#include "HUEvidence.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

AHUEvidence::AHUEvidence()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

UHUCaseSubsystem* AHUEvidence::Cases() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
}

void AHUEvidence::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	using namespace HUBuild;
	Clear(Blocks);
	Lid = nullptr;
	USceneComponent* R = RootComponent;
	switch (Conceal)
	{
	case EHUConceal::Trace:
		// a smear on the floor: thin, and colored so it lights up under UV
		Block(this, R, Blocks, EShape::Cylinder, FVector(0, 0, 0.3f), FVector(34, 22, 0.6f), FLinearColor(2.5f, 0.25f, 2.0f));
		Block(this, R, Blocks, EShape::Cylinder, FVector(20, 10, 0.3f), FVector(10, 8, 0.6f), FLinearColor(2.5f, 0.25f, 2.0f));
		break;
	case EHUConceal::Prints:
		// a glass, smudged
		Block(this, R, Blocks, EShape::Cylinder, FVector(0, 0, 7), FVector(8, 8, 14), FLinearColor(0.55f, 0.62f, 0.7f));
		break;
	case EHUConceal::Locked:
		// a small steel strongbox with a padlock
		Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 10), FVector(42, 28, 20), FLinearColor(0.08f, 0.14f, 0.1f));
		Lid = Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 21), FVector(44, 30, 3), FLinearColor(0.06f, 0.1f, 0.08f));
		Block(this, R, Blocks, EShape::Cube, FVector(22, 0, 14), FVector(3, 8, 9), FLinearColor(0.6f, 0.5f, 0.2f));
		break;
	default:
		// plain and dark: something small and drab, easy to miss
		if (Mesh)
		{
			UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
			M->SetStaticMesh(Mesh);
			M->SetupAttachment(R);
			const FVector B = Mesh->GetBounds().BoxExtent * 2.f;
			M->SetRelativeScale3D(FVector(18.f / FMath::Max(1.f, B.GetMax())));
			M->RegisterComponent();
			Blocks.Add(M);
		}
		else Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 2), FVector(15, 10, 4), FLinearColor(0.22f, 0.18f, 0.13f));
		break;
	}
}

void AHUEvidence::BeginPlay()
{
	Super::BeginPlay();
	// dark and trace evidence can't be seen until a beam finds it
	if (Conceal == EHUConceal::Dark || Conceal == EHUConceal::Trace) Show(false);
}

void AHUEvidence::Show(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
}

void AHUEvidence::Hide() { bCovered = true; Show(false); }

void AHUEvidence::Reveal()
{
	bCovered = false;
	if ((Conceal != EHUConceal::Dark && Conceal != EHUConceal::Trace) || bSpotted) Show(true);
}

void AHUEvidence::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bSpotted || bCovered || (Conceal != EHUConceal::Dark && Conceal != EHUConceal::Trace)) return;
	APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	const UHUInteractComponent* I = P ? P->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (I && I->InBeam(GetActorLocation(), Conceal == EHUConceal::Trace))
	{
		bSpotted = true;
		Show(true);
		SetActorTickEnabled(false);
	}
}

FText AHUEvidence::GetPrompt_Implementation(APawn* User) const
{
	FHUClue C;
	const UHUCaseSubsystem* S = Cases();
	if (!S || !S->GetClue(ClueId, C)) return FText::GetEmpty();
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (Conceal == EHUConceal::Locked && !bOpened)
	{
		if (I && I->IsHolding(EHUTool::Crowbar)) return FText::FromString(TEXT("[E] Pry open the strongbox"));
		return FText::FromString(S->HasTool(EHUTool::Crowbar) ? TEXT("A locked strongbox — take out your crowbar (5)") : TEXT("A locked strongbox — you need a crowbar"));
	}
	if (Conceal == EHUConceal::Prints && !(I && I->IsHolding(EHUTool::PrintKit)))
		return FText::FromString(S->HasTool(EHUTool::PrintKit) ? TEXT("Smudged fingerprints — take out your print kit (4)") : TEXT("Smudged fingerprints — you need a print kit to lift them"));
	if (Conceal == EHUConceal::Prints) return FText::FromString(TEXT("[E] Dust and lift the prints — ") + C.Name.ToUpper());
	if (Conceal == EHUConceal::Trace) return FText::FromString(TEXT("[E] Swab the trace — ") + C.Name.ToUpper());
	return FText::FromString(TEXT("[E] Examine — ") + C.Name.ToUpper());
}

void AHUEvidence::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUCaseSubsystem* S = Cases();
	FHUClue C;
	if (!S || !S->GetClue(ClueId, C) || bSecondary) return;
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (Conceal == EHUConceal::Locked && !bOpened)
	{
		if (!(I && I->IsHolding(EHUTool::Crowbar))) { if (I) I->Say(S->HasTool(EHUTool::Crowbar) ? TEXT("It won't budge by hand. Take out the crowbar (5).") : TEXT("It won't budge. A crowbar from the supply locker would do it.")); return; }
		bOpened = true;
		if (I) I->PlayReach();
		if (Lid) { Lid->SetRelativeLocation(FVector(-18, 0, 30)); Lid->SetRelativeRotation(FRotator(0, 0, 70)); }
		if (I) I->Say(TEXT("The lock gives with a crack. There's something inside."));
		return;
	}
	if (Conceal == EHUConceal::Prints && !(I && I->IsHolding(EHUTool::PrintKit)))
	{
		if (I) I->Say(S->HasTool(EHUTool::PrintKit) ? TEXT("There are prints on it. Take out the print kit (4) to lift them.") : TEXT("There are prints on it, but you can't lift them by hand. Get a print kit from the supply locker."));
		return;
	}
	S->FindClue(ClueId);
	FString Line = FString::Printf(TEXT("BAGGED: %s — %s"), *C.Name, *C.Detail);
	const EHUProfile* P = S->ClueProfile.Find(ClueId);
	const FString* Sign = S->ClueSign.Find(ClueId);
	if (P && Sign && *P != EHUProfile::None)
		Line += FString::Printf(TEXT("   PROFILE: %s points to a %s."), **Sign, HUProfiles::Name(*P));
	if (I)
	{
		I->PlayReach(); // reach down and bag it
		I->Say(Line);
		I->Notify(TEXT("EVIDENCE BAGGED"), C.Name, FLinearColor(0.36f, 0.76f, 1.f));
		if (P && *P != EHUProfile::None) I->Notify(TEXT("PROFILE LEAD"), FString::Printf(TEXT("Points to a %s"), HUProfiles::Name(*P)), FLinearColor(0.8f, 0.55f, 0.98f));
	}
	Destroy();
}

// careful work takes a moment: picking it up, lifting prints, swabbing, prying a lock (only with the right tool in hand)
float AHUEvidence::UseTime(APawn* User, bool bSecondary) const
{
	if (bSecondary) return 0.f;
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	switch (Conceal)
	{
	case EHUConceal::Locked: return !bOpened ? (I && I->IsHolding(EHUTool::Crowbar) ? 3.f : 0.f) : 0.8f;
	case EHUConceal::Prints: return I && I->IsHolding(EHUTool::PrintKit) ? 2.5f : 0.f;
	case EHUConceal::Trace:  return 2.f;
	default:                 return 0.8f;
	}
}