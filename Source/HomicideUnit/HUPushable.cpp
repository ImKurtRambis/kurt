#include "HUPushable.h"
#include "HUEvidence.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AHUPushable::AHUPushable()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeF.Succeeded()) Body->SetStaticMesh(CubeF.Object);
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	Body->SetMobility(EComponentMobility::Movable);
}

// the mesh's pivot is its middle; the actor's origin sits on the floor
void AHUPushable::Apply()
{
	if (Mesh)
	{
		Body->SetStaticMesh(Mesh);
		const FVector B = Mesh->GetBounds().BoxExtent * 2.f;
		const float S = FMath::Min3(SizeCm.X / FMath::Max(B.X, 1.f), SizeCm.Y / FMath::Max(B.Y, 1.f), SizeCm.Z / FMath::Max(B.Z, 1.f));
		Body->SetRelativeScale3D(FVector(S));
		return;
	}
	Body->SetRelativeScale3D(SizeCm / 100.f);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, this);
		D->SetVectorParameterValue(TEXT("Color"), Color);
		Body->SetMaterial(0, D);
	}
}

void AHUPushable::BeginPlay()
{
	Super::BeginPlay();
	Start = GetActorLocation();
}

FText AHUPushable::GetPrompt_Implementation(APawn* User) const
{
	if (Holder.IsValid()) return FText::FromString(TEXT("[E] Let go of the ") + Label);
	return FText::FromString(TEXT("[E] Grab the ") + Label + TEXT(" (push / pull)"));
}

void AHUPushable::Interact_Implementation(APawn* User, bool bSecondary)
{
	if (Holder.IsValid()) { Release(); return; }
	if (!User) return;
	Holder = User;
	Offset = GetActorLocation() - User->GetActorLocation();
	Offset.Z = 0.f;
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// lift it a hair off the floor while it's dragged, so the slide test doesn't snag on the floor itself
	GroundZ = GetActorLocation().Z;
	SetActorLocation(GetActorLocation() + FVector(0, 0, 2.f));
	if (UHUInteractComponent* I = User->FindComponentByClass<UHUInteractComponent>()) I->SetGrabbing(this);
	if (ACharacter* C = Cast<ACharacter>(User))
	{
		SavedSpeed = C->GetCharacterMovement()->MaxWalkSpeed;
		C->GetCharacterMovement()->MaxWalkSpeed = 220.f; // heavy
	}
}

void AHUPushable::Release()
{
	if (ACharacter* C = Cast<ACharacter>(Holder.Get()))
		if (SavedSpeed > 0.f) C->GetCharacterMovement()->MaxWalkSpeed = SavedSpeed;
	if (APawn* P = Holder.Get()) if (UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>()) I->SetGrabbing(nullptr);
	if (Holder.IsValid()) SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, GroundZ)); // set it back down
	Holder = nullptr;
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

void AHUPushable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APawn* P = Holder.Get();
	if (!P) return;
	FVector Target = P->GetActorLocation() + Offset;
	Target.Z = GroundZ + 2.f;
	FHitResult Hit;
	SetActorLocation(Target, true, &Hit); // slides along walls instead of going through them
	if (Hit.bStartPenetrating) SetActorLocation(Target, false); // wedged at the start: don't stay stuck
	// dragged too far from you (stuck on something): let go
	if (FVector::Dist2D(GetActorLocation(), P->GetActorLocation()) > Offset.Size2D() + 150.f) { Release(); return; }
	if (Hidden.Num() > 0 && FVector::Dist2D(GetActorLocation(), Start) > RevealDistance)
	{
		for (AHUEvidence* C : Hidden) if (C) C->Reveal();
		Hidden.Reset();
		if (UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>())
		{
			I->Subtitle = FText::FromString(TEXT("There was something hidden under the ") + Label + TEXT("."));
			I->Notify(TEXT("SOMETHING HIDDEN"), TEXT("Under the ") + Label, FLinearColor(1.f, 0.7f, 0.3f));
		}
	}
}
