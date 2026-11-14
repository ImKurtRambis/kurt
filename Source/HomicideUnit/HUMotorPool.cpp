#include "HUMotorPool.h"
#include "HUCruiser.h"
#include "HUSupplyLocker.h"
#include "HUBuild.h"
#include "Components/ChildActorComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"

static const float HalfX = 900.f, HalfY = 1000.f, PoolH = 600.f;
static const float BayY[3] = { -600.f, 0.f, 600.f };

AHUMotorPool::AHUMotorPool()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	for (int32 i = 0; i < 3; i++)
	{
		UChildActorComponent* C = CreateDefaultSubobject<UChildActorComponent>(*FString::Printf(TEXT("Cruiser%d"), i));
		C->SetupAttachment(Root);
		C->SetChildActorClass(AHUCruiser::StaticClass());
		C->SetRelativeLocation(FVector(-150.f, BayY[i], 0.f));
		C->SetRelativeRotation(FRotator(0.f, 180.f, 0.f)); // nose toward the roll-up doors
		Cars.Add(C);
	}
	Locker = CreateDefaultSubobject<UChildActorComponent>(TEXT("SupplyLocker"));
	Locker->SetupAttachment(Root);
	Locker->SetChildActorClass(AHUSupplyLocker::StaticClass());
	Locker->SetRelativeLocation(FVector(HalfX - 35.f, -420.f, 0.f));
	Locker->SetRelativeRotation(FRotator(0.f, 180.f, 0.f)); // facing into the garage
	BaySign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BaySign"));
	BaySign->SetupAttachment(Root);
	BaySign->SetRelativeLocation(FVector(-HalfX + 4, 0, 380));
	BaySign->SetHorizontalAlignment(EHTA_Center);
	BaySign->SetWorldSize(34.f);
	BaySign->SetTextRenderColor(FColor(232, 200, 106));
	BaySign->SetText(FText::FromString(TEXT("HOMICIDE · UNIT 2-H-14")));
}

void AHUMotorPool::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	using namespace HUBuild;
	Clear(Blocks);
	const FLinearColor Concrete(0.3f, 0.3f, 0.29f), Wall(0.5f, 0.5f, 0.48f), Stripe(0.9f, 0.75f, 0.1f), Roll(0.45f, 0.48f, 0.52f), Red(0.6f, 0.05f, 0.03f), Rubber(0.02f, 0.02f, 0.02f);
	USceneComponent* R = RootComponent;

	// a concrete garage (HUSurfaces.h): poured floor and ceiling, block walls, painted steel doors and chests
	const FRotator Z0 = FRotator::ZeroRotator;
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, -5), FVector(HalfX * 2, HalfY * 2, 10), Concrete, Z0, true, EHUSurface::Concrete);
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, PoolH + 5), FVector(HalfX * 2, HalfY * 2, 10), Wall * 0.6f, Z0, true, EHUSurface::Concrete);
	// the doorway from the corridor is in the +X wall; the roll-up doors fill the -X wall
	WallWithDoor(this, R, Blocks, true, FVector(HalfX + 5, 0, PoolH / 2), HalfY * 2, PoolH, 10, 0, 180, 250, Wall, EHUSurface::Concrete);
	Block(this, R, Blocks, EShape::Cube, FVector(-HalfX - 5, 0, PoolH / 2), FVector(10, HalfY * 2, PoolH), Wall, Z0, true, EHUSurface::Concrete);
	for (int32 S = -1; S <= 1; S += 2) Block(this, R, Blocks, EShape::Cube, FVector(0, S * (HalfY + 5), PoolH / 2), FVector(HalfX * 2, 10, PoolH), Wall, Z0, true, EHUSurface::Concrete);
	for (float Y : BayY)
	{
		Block(this, R, Blocks, EShape::Cube, FVector(-HalfX + 3, Y, 180), FVector(6, 420, 360), Roll, Z0, true, EHUSurface::PaintedMetal);
		for (int32 k = 0; k < 12; k++) Block(this, R, Blocks, EShape::Cube, FVector(-HalfX + 7, Y, 20 + k * 30), FVector(2, 420, 3), Roll * 0.6f, FRotator::ZeroRotator, false);
	}
	// yellow bay lines and the walkway to the door
	for (float Y : { -900.f, -300.f, 300.f, 900.f }) Block(this, R, Blocks, EShape::Cube, FVector(-150, Y, 1), FVector(800, 10, 2), Stripe, FRotator::ZeroRotator, false);
	for (int32 S = -1; S <= 1; S += 2) Block(this, R, Blocks, EShape::Cube, FVector(550, S * 100, 1), FVector(700, 8, 2), Stripe, FRotator::ZeroRotator, false);
	// overhead lights
	for (float X : { -500.f, 0.f, 500.f }) for (float Y : { -600.f, 0.f, 600.f })
		Block(this, R, Blocks, EShape::Cube, FVector(X, Y, PoolH - 4), FVector(160, 30, 4), FLinearColor(3.f, 3.f, 2.8f), FRotator::ZeroRotator, false);
	// tool chests and a stack of tires along the side walls
	for (float X : { 200.f, 400.f }) Block(this, R, Blocks, EShape::Cube, FVector(X, HalfY - 40, 55), FVector(120, 60, 110), Red, Z0, true, EHUSurface::PaintedMetal);
	for (int32 i = 0; i < 4; i++) Block(this, R, Blocks, EShape::Cylinder, FVector(600, HalfY - 60, 12 + i * 24), FVector(70, 70, 22), Rubber);
	for (int32 i = 0; i < 3; i++) Block(this, R, Blocks, EShape::Cylinder, FVector(600 - i * 90, -HalfY + 60, 45), FVector(56, 56, 90), FLinearColor(0.1f, 0.2f, 0.45f));
}

void AHUMotorPool::BeginPlay()
{
	Super::BeginPlay();
	for (float X : { -450.f, 300.f })
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetRelativeLocation(FVector(X, 0, PoolH - 60));
		L->SetIntensity(12000.f);
		L->SetAttenuationRadius(1800.f);
		L->SetLightColor(FLinearColor(0.85f, 0.92f, 1.f));
		L->RegisterComponent();
	}
}
