#include "HUBriefingLaptop.h"
#include "HUCaseSubsystem.h"
#include "HUBriefingRoom.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AHUBriefingLaptop::AHUBriefingLaptop()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Laptop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Laptop"));
	Laptop->SetupAttachment(Root);
	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Root);
	// a stand-in laptop: a flat dark block with a lid, until there's a real mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeF.Succeeded())
	{
		Laptop->SetStaticMesh(CubeF.Object);
		Laptop->SetRelativeLocation(FVector(0.f, 0.f, 1.f));
		Laptop->SetRelativeScale3D(FVector(0.24f, 0.34f, 0.02f));
		Lid->SetStaticMesh(CubeF.Object);
		Lid->SetRelativeLocation(FVector(12.f, 0.f, 12.f));
		Lid->SetRelativeRotation(FRotator(15.f, 0.f, 0.f));
		Lid->SetRelativeScale3D(FVector(0.015f, 0.34f, 0.22f));
	}
	Laptop->SetCollisionProfileName(TEXT("BlockAll"));
	Lid->SetCollisionProfileName(TEXT("BlockAll"));
}

void AHUBriefingLaptop::BeginPlay()
{
	Super::BeginPlay();
	UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!Cases) return;
	if (!Cases->bCaseActive) Cases->StartCase(-1); // walking straight into the room starts a case
	Slides = Cases->BuildBriefingSlides(Cases->DetectiveName.IsEmpty() ? DetectiveName : Cases->DetectiveName);
	Index = 0;
	if (Slides.Num() > 0) OnSlideChanged(Slides[0], 0, Slides.Num());
}

void AHUBriefingLaptop::OnSlideChanged_Implementation(const FHUSlide& Slide, int32 SlideIndex, int32 SlideCount)
{
	if (Room) Room->ShowSlide(Slide, SlideIndex, SlideCount);
}

void AHUBriefingLaptop::Go(int32 Delta)
{
	const int32 Next = FMath::Clamp(Index + Delta, 0, FMath::Max(0, Slides.Num() - 1));
	if (Next == Index || !Slides.IsValidIndex(Next)) return;
	Index = Next;
	OnSlideChanged(Slides[Index], Index, Slides.Num());
	// reaching the last slide ends the briefing: the cruisers in the motor pool are released
	if (IsLastSlide())
		if (UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) Cases->bBriefed = true;
}

FText AHUBriefingLaptop::GetPrompt_Implementation(APawn* User) const
{
	if (IsLastSlide()) return FText::FromString(TEXT("Briefing's over — head to the motor pool  ·  [Q] Previous slide"));
	return FText::FromString(Index == 0 ? TEXT("[E] Next slide") : TEXT("[E] Next slide  ·  [Q] Previous slide"));
}

void AHUBriefingLaptop::Interact_Implementation(APawn* User, bool bSecondary)
{
	if (bSecondary) { Go(-1); return; }
	if (!IsLastSlide()) Go(1); // on the last slide, E does nothing: the way out is the motor pool
}

void AHUBriefingLaptop::RollOut_Implementation()
{
	UGameplayStatics::OpenLevel(this, SceneLevel);
}
