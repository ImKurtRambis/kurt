#include "HUEvidence.h"
#include "HUSound.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"

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
	// back at the scene a second time (the motor pool door): what the squad already bagged isn't here any more
	if (const UHUCaseSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr)
		if (S->HasFound(ClueId)) { UE_LOG(LogTemp, Log, TEXT("HomicideUnit: %s was bagged on an earlier visit - left out"), *ClueId); Destroy(); return; }
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
	if (Conceal != EHUConceal::Dark && Conceal != EHUConceal::Trace) Show(true); // dark / trace: Tick shows it when lit
}

void AHUEvidence::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// in someone's hand: nothing on the floor. If they're gone (a partner who left), it goes back where it was.
	if (bHeld) { if (!HeldBy.IsValid()) PutBack(nullptr); return; }
	if (bCovered || (Conceal != EHUConceal::Dark && Conceal != EHUConceal::Trace)) return;
	// any detective's beam (yours, or a partner's you can see) - a moment's grace so a wobbling beam doesn't flicker it
	const float Now = GetWorld()->GetTimeSeconds();
	APawn* Lit = nullptr;
	for (TActorIterator<APawn> It(GetWorld()); It && !Lit; ++It)
		if (const UHUInteractComponent* I = It->FindComponentByClass<UHUInteractComponent>())
			if (I->InBeam(GetActorLocation(), Conceal == EHUConceal::Trace)) Lit = *It;
	if (Lit) { LitBy = Lit; LitUntil = Now + 0.6f; }
	const bool bShow = Now < LitUntil;
	if (bShow != !IsHidden()) Show(bShow);
}

APawn* AHUEvidence::LitByPawn() const { return GetWorld() && GetWorld()->GetTimeSeconds() < LitUntil ? LitBy.Get() : nullptr; }

FText AHUEvidence::GetPrompt_Implementation(APawn* User) const
{
	FHUClue C;
	const UHUCaseSubsystem* S = Cases();
	if (!S || !S->GetClue(ClueId, C)) return FText::GetEmpty();
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (bHeld)
	{
		if (User && HeldBy.Get() == User) return FText::FromString(TEXT("[E] Bag it   ·   [Q] Put it back   ·   move the mouse to turn it over"));
		const APawn* H = HeldBy.Get();
		return FText::FromString(FString::Printf(TEXT("%s is looking it over"), H && H->GetPlayerState() ? *H->GetPlayerState()->GetPlayerName() : TEXT("Your partner")));
	}
	if (Conceal == EHUConceal::Locked && !bOpened)
	{
		if (I && I->IsHolding(EHUTool::Crowbar)) return FText::FromString(TEXT("[E] Pry open the strongbox"));
		return FText::FromString(S->HasTool(EHUTool::Crowbar) ? TEXT("A locked strongbox — take out your crowbar (5)") : TEXT("A locked strongbox — you need a crowbar"));
	}
	if (Conceal == EHUConceal::Prints && !(I && I->IsHolding(EHUTool::PrintKit)))
		return FText::FromString(S->HasTool(EHUTool::PrintKit) ? TEXT("Smudged fingerprints — take out your print kit (4)") : TEXT("Smudged fingerprints — you need a print kit to lift them"));
	if (Conceal == EHUConceal::Prints) return FText::FromString(TEXT("[E] Dust and lift the prints — ") + C.Name.ToUpper());
	// dark / trace: whose light is on it
	FString Help;
	if (Conceal == EHUConceal::Dark || Conceal == EHUConceal::Trace)
	{
		const APawn* L = LitByPawn();
		if (L && L != User) Help = FString::Printf(TEXT("   (%s's light — quick)"), L->GetPlayerState() ? *L->GetPlayerState()->GetPlayerName() : TEXT("your partner"));
		else Help = TEXT("   (one hand on the light — slow. A partner holding it is faster)");
	}
	if (Conceal == EHUConceal::Trace) return FText::FromString(TEXT("[E] Swab the trace — ") + C.Name.ToUpper() + Help);
	if (Conceal == EHUConceal::Locked) return FText::FromString(TEXT("[E] Take out what's inside — ") + C.Name.ToUpper());
	return FText::FromString(TEXT("[E] Pick up — ") + C.Name.ToUpper() + Help);
}

void AHUEvidence::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUCaseSubsystem* S = Cases();
	FHUClue C;
	if (!S || !S->GetClue(ClueId, C)) return;
	// in someone's hand: E bags it, Q puts it back (only theirs to do)
	if (bHeld)
	{
		if (!User || HeldBy.Get() != User) return;
		if (bSecondary) PutBack(User); else Bag(User);
		return;
	}
	if (bSecondary) return;
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (Conceal == EHUConceal::Locked && !bOpened)
	{
		if (!(I && I->IsHolding(EHUTool::Crowbar))) { if (I) I->Say(S->HasTool(EHUTool::Crowbar) ? TEXT("It won't budge by hand. Take out the crowbar (5).") : TEXT("It won't budge. A crowbar from the supply locker would do it.")); return; }
		bOpened = true;
		HUSound::Play(this, TEXT("SFX_Crowbar"), GetActorLocation());
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
	// a thing you can hold: pick it up and look it over. Prints lifted, a trace swabbed: bagged as it's done.
	if (IsHandled()) PickUp(User); else Bag(User);
}

void AHUEvidence::PickUp(APawn* User)
{
	HeldBy = User; bHeld = true;
	if (Conceal != EHUConceal::Locked) Show(false); // off the floor (a strongbox stays put: it's what was inside you hold)
	if (UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr) { I->PlayReach(); I->StartInspect(this); }
}

void AHUEvidence::PutBack(APawn* User)
{
	HeldBy.Reset(); bHeld = false;
	if (Conceal == EHUConceal::Dark) LitUntil = GetWorld()->GetTimeSeconds() + 0.6f; // back in the dark: seen again only in a beam
	else Show(true);
	if (UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr)
	{
		I->EndInspect();
		I->PlayReach();
		I->Say(TEXT("You put it back where you found it."));
	}
}

FString AHUEvidence::MarkWords() const
{
	const UHUCaseSubsystem* S = Cases();
	FHUClue C;
	if (!S || !S->GetClue(ClueId, C)) return FString();
	FString Line = FString::Printf(TEXT("%s — %s"), *C.Name.ToUpper(), *C.Detail);
	const EHUProfile* P = S->ClueProfile.Find(ClueId);
	const FString* Sign = S->ClueSign.Find(ClueId);
	if (P && Sign && *P != EHUProfile::None) Line += FString::Printf(TEXT("   PROFILE: %s points to a %s."), **Sign, HUProfiles::Name(*P));
	return Line;
}

void AHUEvidence::MarkLead(FString& OutTitle, FString& OutBody) const
{
	OutTitle.Reset(); OutBody.Reset();
	const UHUCaseSubsystem* S = Cases();
	const EHUProfile* P = S ? S->ClueProfile.Find(ClueId) : nullptr;
	if (P && *P != EHUProfile::None) { OutTitle = TEXT("PROFILE LEAD"); OutBody = FString::Printf(TEXT("Points to a %s"), HUProfiles::Name(*P)); }
}

void AHUEvidence::HandLook(TArray<UStaticMeshComponent*>& OutParts) const
{
	OutParts.Reset();
	if (Conceal == EHUConceal::Locked) return; // the box stays; what's inside is a small packet the hand draws
	for (UStaticMeshComponent* B : Blocks) if (B && B != Lid) OutParts.Add(B);
}

void AHUEvidence::Bag(APawn* User)
{
	UHUCaseSubsystem* S = Cases();
	FHUClue C;
	if (!S || !S->GetClue(ClueId, C)) return;
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	const bool bLooked = I && I->SawMark(this); // (only the holder's own game knows; the others show nothing anyway)
	if (I) I->EndInspect();
	HeldBy.Reset(); bHeld = false;
	S->FindClue(ClueId);
	HUSound::Play(this, TEXT("SFX_Bag"), User ? User->GetActorLocation() : GetActorLocation(), 0.8f);
	if (I)
	{
		I->PlayReach(); // into the bag
		FString Title, Body;
		MarkLead(Title, Body);
		if (!IsHandled())
		{
			// lifted or swabbed: worked over as it was taken, so what it shows is known now
			I->Say(TEXT("BAGGED: ") + MarkWords());
			I->Notify(TEXT("EVIDENCE BAGGED"), C.Name, FLinearColor(0.36f, 0.76f, 1.f));
			if (!Title.IsEmpty()) I->Notify(Title, Body, FLinearColor(0.8f, 0.55f, 0.98f));
		}
		else
		{
			// its mark was read (and its lead noted) when it was turned to face you; bagged unread, the lab reads it
			I->Say(bLooked ? FString::Printf(TEXT("BAGGED: %s."), *C.Name)
				: FString::Printf(TEXT("BAGGED: %s. You didn't turn it over - what's on it will be in the case file."), *C.Name));
			I->Notify(TEXT("EVIDENCE BAGGED"), C.Name, FLinearColor(0.36f, 0.76f, 1.f));
		}
	}
	Destroy();
}

// careful work takes a moment: picking it up, lifting prints, swabbing, prying a lock (only with the right tool in hand).
// In your hand, bagging it and putting it back are at once.
float AHUEvidence::UseTime(APawn* User, bool bSecondary) const
{
	if (bSecondary || bHeld) return 0.f;
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	switch (Conceal)
	{
	case EHUConceal::Locked: return !bOpened ? (I && I->IsHolding(EHUTool::Crowbar) ? 3.f : 0.f) : 0.5f;
	case EHUConceal::Prints: return I && I->IsHolding(EHUTool::PrintKit) ? 2.5f : 0.f;
	case EHUConceal::Trace:  { const APawn* L = LitByPawn(); return L && L != User ? 2.f : 5.f; }
	case EHUConceal::Dark:   { const APawn* L = LitByPawn(); return L && L != User ? 0.8f : 2.f; } // one hand on the light
	default:                 return 0.5f;
	}
}