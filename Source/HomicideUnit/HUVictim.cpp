#include "HUVictim.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUEvidence.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"

using namespace HUBuild;

AHUVictim::AHUVictim()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

UHUCaseSubsystem* AHUVictim::Cases() const { return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr; }

// local +X runs from the feet to the head
void AHUVictim::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks);
	USceneComponent* R = RootComponent;
	const FLinearColor Skin(0.55f, 0.43f, 0.36f), Pants(0.05f, 0.05f, 0.06f), Shoe(0.02f, 0.015f, 0.01f), Blood(0.22f, 0.01f, 0.01f), Tape(0.95f, 0.95f, 0.92f);
	const FRotator AlongX(90, 0, 0);
	auto Line = [&](FVector A, FVector B) // a strip of white tape on the floor from A to B
	{
		const FVector D = B - A;
		Block(this, R, Blocks, EShape::Cube, (A + B) / 2 + FVector(0, 0, 0.3f), FVector(D.Size2D(), 2.5f, 0.6f), Tape, FRotator(0, D.Rotation().Yaw, 0), false);
	};
	switch (Kind)
	{
	case EHUVictimKind::Body:
	{
		// a pool of blood under the head and shoulders
		Block(this, R, Blocks, EShape::Cylinder, FVector(30, 10, 0.3f), FVector(80, 62, 0.6f), Blood);
		Block(this, R, Blocks, EShape::Cylinder, FVector(62, 30, 0.3f), FVector(26, 18, 0.6f), Blood);
		// the body, face up, one arm flung out, one knee bent
		Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 10), FVector(62, 36, 18), Coat);
		Block(this, R, Blocks, EShape::Cube, FVector(-30, 0, 9), FVector(10, 34, 16), Coat * 0.8f);
		Block(this, R, Blocks, EShape::Sphere, FVector(43, 2, 11), FVector(21, 19, 20), Skin);
		Block(this, R, Blocks, EShape::Sphere, FVector(46, 2, 14), FVector(19, 18, 12), FLinearColor(0.08f, 0.06f, 0.04f));
		Block(this, R, Blocks, EShape::Cylinder, FVector(-68, 9, 7), FVector(13, 13, 72), Pants, AlongX);
		Block(this, R, Blocks, EShape::Cylinder, FVector(-60, -14, 9), FVector(13, 13, 70), Pants, FRotator(90, -14, 0));
		Block(this, R, Blocks, EShape::Cube, FVector(-108, 9, 6), FVector(12, 10, 10), Shoe);
		Block(this, R, Blocks, EShape::Cube, FVector(-98, -24, 6), FVector(12, 10, 10), Shoe);
		Block(this, R, Blocks, EShape::Cylinder, FVector(18, 38, 6), FVector(10, 10, 54), Coat, FRotator(90, 55, 0));   // flung out
		Block(this, R, Blocks, EShape::Sphere, FVector(34, 60, 5), FVector(9, 7, 6), Skin);
		Block(this, R, Blocks, EShape::Cylinder, FVector(-2, -22, 7), FVector(10, 10, 50), Coat, FRotator(90, -8, 0));   // at the side
		Block(this, R, Blocks, EShape::Sphere, FVector(-28, -25, 5), FVector(9, 7, 6), Skin);
		break;
	}
	case EHUVictimKind::Outline:
	{
		// where they lay until the ambulance came: tape outline and a smear of blood
		Block(this, R, Blocks, EShape::Cylinder, FVector(25, 6, 0.3f), FVector(56, 40, 0.6f), Blood);
		Line({ 32, -18, 0 }, { 32, 18, 0 }); Line({ 32, 18, 0 }, { -32, 20, 0 }); Line({ -32, 20, 0 }, { -32, -20, 0 }); Line({ -32, -20, 0 }, { 32, -18, 0 });
		Line({ 32, -12, 0 }, { 56, -12, 0 }); Line({ 56, -12, 0 }, { 56, 12, 0 }); Line({ 56, 12, 0 }, { 32, 12, 0 });
		Line({ 30, 18, 0 }, { 50, 62, 0 }); Line({ 30, -18, 0 }, { -10, -34, 0 });
		Line({ -32, 14, 0 }, { -110, 12, 0 }); Line({ -32, -14, 0 }, { -100, -36, 0 });
		break;
	}
	case EHUVictimKind::Belongings:
	{
		// a folded coat, a handbag on its side, a hat
		Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 4), FVector(46, 34, 8), Coat);
		Block(this, R, Blocks, EShape::Cube, FVector(40, 20, 10), FVector(30, 12, 20), FLinearColor(0.3f, 0.16f, 0.06f), FRotator(0, 25, 80));
		Block(this, R, Blocks, EShape::Cylinder, FVector(-30, 30, 3), FVector(30, 30, 3), FLinearColor(0.08f, 0.07f, 0.06f));
		Block(this, R, Blocks, EShape::Cylinder, FVector(-30, 30, 8), FVector(18, 18, 8), FLinearColor(0.08f, 0.07f, 0.06f));
		break;
	}
	case EHUVictimKind::Scorch:
	{
		// where the fire started: a burnt patch, a melted bin, charred junk
		Block(this, R, Blocks, EShape::Cylinder, FVector(0, 0, 0.3f), FVector(220, 190, 0.6f), FLinearColor(0.02f, 0.015f, 0.01f));
		Block(this, R, Blocks, EShape::Cylinder, FVector(10, -10, 22), FVector(36, 36, 44), FLinearColor(0.04f, 0.035f, 0.03f));
		for (int32 i = 0; i < 6; i++) Block(this, R, Blocks, EShape::Cube, FVector(-60 + i * 22, 30 - (i % 2) * 50, 3), FVector(26, 8, 6), FLinearColor(0.03f, 0.025f, 0.02f), FRotator(0, i * 37, 0));
		break;
	}
	}
	// a couple of yellow evidence markers around it
	for (const FVector& M : { FVector(70, -40, 0), FVector(-40, 50, 0) })
	{
		Block(this, R, Blocks, EShape::Cube, M + FVector(0, 0, 9), FVector(12, 2, 18), FLinearColor(1.f, 0.75f, 0.05f), FRotator(0, 0, 18), false);
		Block(this, R, Blocks, EShape::Cube, M + FVector(0, 4, 9), FVector(12, 2, 18), FLinearColor(1.f, 0.75f, 0.05f), FRotator(0, 0, -18), false);
	}
}

FString AHUVictim::Notes() const
{
	const UHUCaseSubsystem* S = Cases();
	if (!S || !S->bCaseActive) return TEXT("");
	const FHUCase& C = S->Current;
	switch (Kind)
	{
	case EHUVictimKind::Body:
		return FString::Printf(TEXT("M.E.'S NOTES — %s. Cause of death: %s. Time of death close to %s. Lividity says the body hasn't been moved: it happened right here."), *C.Victim, *C.Weapon, *C.Time);
	case EHUVictimKind::Outline:
		return FString::Printf(TEXT("%s lay here until the ambulance came, just after %s. Attacked with %s. Alive — but not talking yet."), *C.Victim, *C.Time, *C.Weapon);
	case EHUVictimKind::Belongings:
		return FString::Printf(TEXT("%s's coat and bag, left behind around %s. The wallet's still inside — whoever's behind this wasn't after money."), *C.Victim, *C.Time);
	default:
		return FString::Printf(TEXT("The fire started here, around %s: a waste bin and a lot of accelerant. %s was pulled out of the next room."), *C.Time, *C.Victim);
	}
}

FText AHUVictim::GetPrompt_Implementation(APawn* User) const
{
	const TCHAR* Look = Kind == EHUVictimKind::Body ? TEXT("[E] Examine the body") : Kind == EHUVictimKind::Outline ? TEXT("[E] Examine where the victim lay")
		: Kind == EHUVictimKind::Belongings ? TEXT("[E] Examine the victim's things") : TEXT("[E] Examine where the fire started");
	FString P = Look;
	if (!bSearched && Kind != EHUVictimKind::Scorch && Kind != EHUVictimKind::Outline)
		P += Kind == EHUVictimKind::Body ? TEXT("  ·  [Q] Search the pockets") : TEXT("  ·  [Q] Search the bag");
	return FText::FromString(P);
}

void AHUVictim::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return;
	if (!bSecondary) { I->Say(Notes()); return; }
	if (bSearched || Kind == EHUVictimKind::Scorch || Kind == EHUVictimKind::Outline) return;
	bSearched = true;
	I->PlayReach();
	if (Pocket)
	{
		// lay what you found beside the body
		Pocket->SetActorLocation(GetActorLocation() + GetActorRightVector() * -55.f + GetActorForwardVector() * 10.f);
		Pocket->Reveal();
		I->Say(Kind == EHUVictimKind::Body ? TEXT("In the inside pocket of the coat, something they didn't want found. It's on the floor beside them now — take a look.")
			: TEXT("Tucked in the lining of the bag: something worth a closer look. It's on the floor beside it now."));
		Pocket = nullptr;
	}
	else I->Say(Kind == EHUVictimKind::Body ? TEXT("Keys, some change, a bus transfer. Nothing that helps.") : TEXT("A compact, tissues, a bus transfer. Nothing that helps."));
}
