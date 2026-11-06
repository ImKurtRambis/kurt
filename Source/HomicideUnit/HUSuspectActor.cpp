#include "HUSuspectActor.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"

using namespace HUBuild;

AHUSuspectActor::AHUSuspectActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	NameTag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(Root);
	NameTag->SetRelativeLocation(FVector(0, 0, 205));
	NameTag->SetHorizontalAlignment(EHTA_Center);
	NameTag->SetWorldSize(14.f);
	NameTag->SetTextRenderColor(FColor(232, 193, 90));
}

void AHUSuspectActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Pose();
}

// the figure: arms at the sides, or behind the back in cuffs
void AHUSuspectActor::Pose()
{
	Clear(Blocks);
	const FLinearColor Skin(0.6f, 0.42f, 0.32f), Pants(0.05f, 0.05f, 0.06f), Steel(0.75f, 0.77f, 0.8f);
	Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(0, 0, 45), FVector(40, 46, 90), Pants);
	Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(0, 0, 125), FVector(52, 60, 76), Coat);
	Block(this, RootComponent, Blocks, EShape::Sphere, FVector(0, 0, 178), FVector(26, 24, 30), Skin);
	if (!bCuffed)
	{
		for (float S : { -1.f, 1.f }) Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(0, S * 32, 118), FVector(12, 12, 62), Coat * 0.9f);
	}
	else
	{
		// arms drawn back, wrists together behind, the cuffs on
		for (float S : { -1.f, 1.f }) Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(-18, S * 22, 115), FVector(12, 12, 60), Coat * 0.9f, FRotator(0, 0, S * 22));
		Block(this, RootComponent, Blocks, EShape::Cube, FVector(-30, 0, 90), FVector(6, 14, 5), Steel);
	}
	if (bCuffed) for (UStaticMeshComponent* B : Blocks) if (B) B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // don't trip the officer walking them
}

void AHUSuspectActor::BeginPlay()
{
	Super::BeginPlay();
	NameTag->SetText(FText::FromString(DisplayName().ToUpper()));
}

FString AHUSuspectActor::DisplayName() const
{
	FHUSuspect S;
	const UHUCaseSubsystem* Cases = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	return Cases && Cases->GetSuspect(SuspectId, S) ? S.Name : FString(TEXT("the suspect"));
}

FText AHUSuspectActor::GetPrompt_Implementation(APawn* User) const
{
	FHUSuspect S;
	const UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!Cases || !Cases->GetSuspect(SuspectId, S)) return FText::GetEmpty();
	if (bCuffed)
		return FText::FromString(IsEscortedBy(User) ? FString::Printf(TEXT("[E] Let go of %s  ·  [Q] Take the cuffs off"), *S.Name)
			: FString::Printf(TEXT("[E] Take %s by the arm  ·  [Q] Take the cuffs off"), *S.Name));
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (I && I->IsHolding(EHUTool::Handcuffs))
		return FText::FromString(FString::Printf(TEXT("[E] Arrest %s — put the cuffs on  ·  [Q] Run their record"), *S.Name));
	return FText::FromString(FString::Printf(TEXT("[E] Talk to %s%s  ·  [Q] Run their record"), *S.Name, S.Role.IsEmpty() ? TEXT("") : *(TEXT(", ") + S.Role)));
}

void AHUSuspectActor::Interact_Implementation(APawn* User, bool bSecondary)
{
	FHUSuspect S;
	const UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!Cases || !Cases->GetSuspect(SuspectId, S) || !I) return;

	if (bCuffed)
	{
		if (bSecondary)
		{
			bCuffed = false; Escort = nullptr; Pose();
			I->Say(FString::Printf(TEXT("You take the cuffs off %s. \"About time.\""), *S.Name));
			return;
		}
		if (IsEscortedBy(User)) { Escort = nullptr; I->Say(FString::Printf(TEXT("You let go of %s's arm. They stay put."), *S.Name)); }
		else { Escort = User; I->PlayReach(); I->Say(FString::Printf(TEXT("You take %s by the arm. Walk them out to the van."), *S.Name)); }
		return;
	}
	// Q: run their record - does it fit the profile the evidence is building?
	if (bSecondary)
	{
		const FString* Rec = Cases->SuspectRecord.Find(SuspectId);
		I->Say(FString::Printf(TEXT("RECORD — %s: %s."), *S.Name.ToUpper(), Rec ? **Rec : TEXT("no record")));
		return;
	}
	// holding the cuffs: arrest
	if (I->IsHolding(EHUTool::Handcuffs))
	{
		bCuffed = true;
		Escort = User;
		Pose();
		I->PlayReach();
		const TCHAR* Protest[] = { TEXT("\"You're making a mistake!\""), TEXT("\"I want a lawyer. Now.\""), TEXT("\"Easy — easy! I'm not resisting.\""), TEXT("\"You've got the wrong person, detective.\""), TEXT("\"...\"") };
		I->Say(FString::Printf(TEXT("%s:  %s"), *S.Name.ToUpper(), Protest[FMath::RandRange(0, UE_ARRAY_COUNT(Protest) - 1)]));
		I->Notify(TEXT("ARRESTED"), FString::Printf(TEXT("%s — walk them to the van outside"), *S.Name), FLinearColor(1.f, 0.7f, 0.3f));
		return;
	}
	// otherwise, talk: intro first, then each statement in turn, round and round
	Line = (Line + 1) % (S.Statements.Num() + 1);
	const FString Said = Line == 0 ? S.Intro : S.Statements[Line - 1].Text;
	I->Subtitle = FText::FromString(FString::Printf(TEXT("%s:  \"%s\""), *S.Name.ToUpper(), *Said));
}

void AHUSuspectActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APawn* E = Escort.Get();
	if (!bCuffed || !E) return;
	// a step behind and to the side of whoever's walking them, facing the way they're going
	const FVector Behind = E->GetActorLocation() - E->GetActorForwardVector() * 95.f + E->GetActorRightVector() * 40.f;
	FVector Target(Behind.X, Behind.Y, GetActorLocation().Z);
	const FVector Now = GetActorLocation();
	const FVector Next = FMath::VInterpTo(Now, Target, DeltaSeconds, 5.f);
	SetActorLocation(Next);
	if (FVector::Dist2D(Now, Next) > 0.5f) SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0, (Next - Now).Rotation().Yaw, 0), DeltaSeconds, 8.f));
}

void AHUSuspectActor::LoadIntoVan()
{
	if (UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) Cases->Arrested.AddUnique(SuspectId);
	Escort = nullptr;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}
