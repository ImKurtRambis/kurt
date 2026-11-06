#include "HULadder.h"
#include "HUBuild.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

AHULadder::AHULadder()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Grab = CreateDefaultSubobject<UBoxComponent>(TEXT("Grab"));
	Grab->SetupAttachment(Root);
	Grab->SetCollisionProfileName(TEXT("Trigger"));
	Grab->SetGenerateOverlapEvents(true);
}

void AHULadder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	using namespace HUBuild;
	Clear(Blocks);
	// the grab zone: in front of the ladder, from the floor to a little above the top
	Grab->SetRelativeLocation(FVector(45.f, 0.f, (Height + 90.f) / 2.f));
	Grab->SetBoxExtent(FVector(55.f, 45.f, (Height + 90.f) / 2.f));
	if (LadderMesh)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
		M->SetStaticMesh(LadderMesh);
		M->SetupAttachment(RootComponent);
		const FVector B = LadderMesh->GetBounds().BoxExtent * 2.f;
		M->SetRelativeScale3D(FVector(1.f, 1.f, B.Z > 1.f ? Height / B.Z : 1.f));
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->RegisterComponent();
		Blocks.Add(M);
		return;
	}
	const FLinearColor Steel(0.32f, 0.3f, 0.26f);
	for (int32 S = -1; S <= 1; S += 2)
		Block(this, RootComponent, Blocks, EShape::Cube, FVector(8, S * 25, Height / 2 + 30), FVector(5, 5, Height + 60), Steel, FRotator::ZeroRotator, false);
	for (float Z = 30.f; Z < Height; Z += 30.f)
		Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(8, 0, Z), FVector(3.5f, 3.5f, 50), Steel, FRotator(0, 0, 90), false);
}

void AHULadder::BeginPlay()
{
	Super::BeginPlay();
	Grab->OnComponentBeginOverlap.AddDynamic(this, &AHULadder::OnEnter);
	Grab->OnComponentEndOverlap.AddDynamic(this, &AHULadder::OnLeave);
}

void AHULadder::OnEnter(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Body, bool bSweep, const FHitResult& Hit)
{
	ACharacter* C = Cast<ACharacter>(Other);
	if (!C || Climbers.Contains(C)) return;
	UCharacterMovementComponent* M = C->GetCharacterMovement();
	SavedBraking.Add(C, M->BrakingDecelerationFlying);
	SavedSpeed.Add(C, M->MaxFlySpeed);
	M->BrakingDecelerationFlying = 4000.f; // stop dead when you let go of the keys
	M->MaxFlySpeed = ClimbSpeed;
	M->SetMovementMode(MOVE_Flying);
	Climbers.Add(C);
}

void AHULadder::OnLeave(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Body)
{
	ACharacter* C = Cast<ACharacter>(Other);
	if (!C || !Climbers.Contains(C)) return;
	UCharacterMovementComponent* M = C->GetCharacterMovement();
	if (const float* B = SavedBraking.Find(C)) M->BrakingDecelerationFlying = *B;
	if (const float* S = SavedSpeed.Find(C)) M->MaxFlySpeed = *S;
	M->SetMovementMode(MOVE_Walking); // falls if you let go mid-climb
	Climbers.Remove(C);
	SavedBraking.Remove(C);
	SavedSpeed.Remove(C);
}

void AHULadder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (const TWeakObjectPtr<ACharacter>& W : Climbers)
	{
		ACharacter* C = W.Get();
		if (!C) continue;
		// how hard you're pushing "forward" relative to where you're looking
		const FRotator View = C->GetControlRotation();
		const FVector Flat = FRotator(0.f, View.Yaw, 0.f).Vector();
		const float Fwd = FVector::DotProduct(C->GetLastMovementInputVector(), Flat);
		if (FMath::Abs(Fwd) < 0.1f) continue;
		const float Pitch = FRotator::NormalizeAxis(View.Pitch);
		const float Dir = Pitch < -25.f ? -1.f : 1.f; // look down to climb down
		C->AddMovementInput(FVector::UpVector, Fwd * Dir);
	}
}
