#include "HUSecurityDesk.h"
#include "HUBuild.h"
#include "HUInteractComponent.h"
#include "HUSuspectActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

using namespace HUBuild;

AHUSecurityDesk::AHUSecurityDesk()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->FOVAngle = 88.f;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	// the look of an old security tape: no colour, heavy grain, dark corners
	FPostProcessSettings& PP = Capture->PostProcessSettings;
	PP.bOverride_ColorSaturation = true;  PP.ColorSaturation = FVector4(0, 0, 0, 1);
	PP.bOverride_ColorContrast = true;    PP.ColorContrast = FVector4(1.25f, 1.25f, 1.25f, 1);
	PP.bOverride_FilmGrainIntensity = true; PP.FilmGrainIntensity = 0.9f;
	PP.bOverride_VignetteIntensity = true;  PP.VignetteIntensity = 0.9f;
	PP.bOverride_AutoExposureBias = true;   PP.AutoExposureBias = 0.6f;
	Capture->PostProcessBlendWeight = 1.f;
}

void AHUSecurityDesk::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks);
	USceneComponent* R = RootComponent;
	const FLinearColor Desk(0.18f, 0.12f, 0.07f), Beige(0.55f, 0.52f, 0.44f), Steel(0.25f, 0.27f, 0.3f);
	// a desk with a bank of two small CRT monitors and a tape recorder; +X faces the person sitting at it
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 37), FVector(70, 140, 6), Desk);
	for (float Y : { -62.f, 62.f }) Block(this, R, Blocks, EShape::Cube, FVector(0, Y, 17), FVector(64, 6, 34), Desk * 0.7f);
	for (float Y : { -34.f, 34.f })
	{
		Block(this, R, Blocks, EShape::Cube, FVector(-10, Y, 59), FVector(34, 40, 38), Beige);
		Block(this, R, Blocks, EShape::Cube, FVector(7.5f, Y, 61), FVector(1, 32, 26), FLinearColor(0.25f, 0.3f, 0.32f));
	}
	Block(this, R, Blocks, EShape::Cube, FVector(18, 0, 42), FVector(22, 50, 8), Steel);                                          // the VCR
	Block(this, R, Blocks, EShape::Cube, FVector(29.5f, 14, 43), FVector(1, 6, 2), FLinearColor(2.f, 0.1f, 0.1f), FRotator::ZeroRotator, false); // its REC light
	Block(this, R, Blocks, EShape::Cube, FVector(55, 0, 24), FVector(40, 40, 6), Steel);                                          // a stool
}

void AHUSecurityDesk::BeginPlay()
{
	Super::BeginPlay();
	Feed = NewObject<UTextureRenderTarget2D>(this);
	Feed->RenderTargetFormat = RTF_RGBA8;
	Feed->InitAutoFormat(640, 480);
	Feed->UpdateResourceImmediate(true);
	Capture->TextureTarget = Feed;

	// the people on the tape: a dark figure for each, visible only to the cameras
	const int32 N = Frames.Num() > 0 ? Frames[0].People.Num() : 0;
	for (int32 i = 0; i < N; i++)
	{
		USceneComponent* G = NewObject<USceneComponent>(this);
		G->SetupAttachment(RootComponent);
		G->SetUsingAbsoluteLocation(true);
		G->RegisterComponent();
		const bool bVictim = i == N - 1;
		const FLinearColor Coat = bVictim ? FLinearColor(0.5f, 0.5f, 0.5f) : (Coats.IsValidIndex(i) ? Coats[i] : FLinearColor(0.2f, 0.2f, 0.2f));
		TArray<UStaticMeshComponent*> Parts;
		Block(this, G, Parts, EShape::Cylinder, FVector(0, 0, 45), FVector(40, 46, 90), FLinearColor(0.05f, 0.05f, 0.06f), FRotator::ZeroRotator, false);
		Block(this, G, Parts, EShape::Cylinder, FVector(0, 0, 125), FVector(52, 60, 76), Coat, FRotator::ZeroRotator, false);
		Block(this, G, Parts, EShape::Sphere, FVector(0, 0, 178), FVector(26, 24, 30), FLinearColor(0.45f, 0.35f, 0.3f), FRotator::ZeroRotator, false);
		for (UStaticMeshComponent* P : Parts) { P->SetVisibleInSceneCaptureOnly(true); P->SetCastShadow(false); }
		Ghosts.Add(G);
	}
}

FString AHUSecurityDesk::CamLabel() const
{
	return CameraNames.IsValidIndex(Cam) ? CameraNames[Cam] : FString::Printf(TEXT("CAM %d"), Cam + 1);
}

FText AHUSecurityDesk::GetPrompt_Implementation(APawn* User) const
{
	if (bViewing && Viewer.Get() == User) return FText::FromString(TEXT("[A] [D] camera  ·  [W] [S] tape  ·  [Q] step away"));
	return FText::FromString(FString::Printf(TEXT("[E] Review the security footage (%d cameras)"), Cameras.Num()));
}

void AHUSecurityDesk::Interact_Implementation(APawn* User, bool bSecondary)
{
	if (!User || !User->IsLocallyControlled()) return; // watching the tape is yours alone
	if (bViewing) Close(); else if (!bSecondary) Open(User);
}

void AHUSecurityDesk::Open(APawn* User)
{
	APlayerController* PC = Cast<APlayerController>(User->GetController());
	if (!PC || Cameras.Num() == 0) return;
	Viewer = User;
	bViewing = true;
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	if (UHUInteractComponent* I = User->FindComponentByClass<UHUInteractComponent>()) { I->Watching = this; I->Say(TEXT("You rewind the tapes. Grainy, but it's all there.")); }
	EnableInput(PC);
	if (InputComponent && InputComponent->KeyBindings.Num() == 0)
	{
		InputComponent->BindKey(EKeys::D, IE_Pressed, this, &AHUSecurityDesk::NextCam);
		InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AHUSecurityDesk::NextCam);
		InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AHUSecurityDesk::PrevCam);
		InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AHUSecurityDesk::PrevCam);
		InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AHUSecurityDesk::NextFrame);
		InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AHUSecurityDesk::NextFrame);
		InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AHUSecurityDesk::PrevFrame);
		InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AHUSecurityDesk::PrevFrame);
		InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AHUSecurityDesk::CloseKey);
	}
	Show();
}

void AHUSecurityDesk::Close()
{
	APawn* User = Viewer.Get();
	if (APlayerController* PC = User ? Cast<APlayerController>(User->GetController()) : nullptr)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		DisableInput(PC);
	}
	if (UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr) I->Watching = nullptr;
	bViewing = false;
	Viewer = nullptr;
}

void AHUSecurityDesk::CloseKey() { if (bViewing) Close(); }
void AHUSecurityDesk::NextCam() { if (!bViewing) return; Cam = (Cam + 1) % FMath::Max(1, Cameras.Num()); Show(); }
void AHUSecurityDesk::PrevCam() { if (!bViewing) return; Cam = (Cam + Cameras.Num() - 1) % FMath::Max(1, Cameras.Num()); Show(); }
void AHUSecurityDesk::NextFrame() { if (!bViewing) return; Frame = FMath::Min(Frame + 1, Frames.Num() - 1); Show(); }
void AHUSecurityDesk::PrevFrame() { if (!bViewing) return; Frame = FMath::Max(Frame - 1, 0); Show(); }

void AHUSecurityDesk::Show()
{
	if (!Cameras.IsValidIndex(Cam) || !Frames.IsValidIndex(Frame)) return;
	const FHUFootageFrame& F = Frames[Frame];
	Capture->SetWorldTransform(Cameras[Cam]);
	// the people on the tape, where they were at this moment
	for (int32 i = 0; i < Ghosts.Num(); i++)
	{
		const bool bThere = F.People.IsValidIndex(i) && !F.People[i].IsNearlyZero();
		Ghosts[i]->SetVisibility(bThere, true);
		if (bThere) Ghosts[i]->SetWorldLocation(F.People[i]);
	}
	// keep the present out of the past: the real people, and the body before it happened
	Capture->HiddenActors.Reset();
	for (TActorIterator<AHUSuspectActor> It(GetWorld()); It; ++It) Capture->HiddenActors.Add(*It);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It) Capture->HiddenActors.Add(*It);
	if (VictimActor && !F.bAfter) Capture->HiddenActors.Add(VictimActor);
	Capture->CaptureScene();
}

void AHUSecurityDesk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bViewing) return;
	// walk away (or get moved) and the tape stops
	if (APawn* V = Viewer.Get()) { if (FVector::Dist2D(V->GetActorLocation(), GetActorLocation()) > 400.f) Close(); }
	else { Close(); return; }
	// the picture rolls a little: recapture now and then for the grain
	Flicker += DeltaSeconds;
	if (Flicker > 0.2f) { Flicker = 0.f; Capture->CaptureScene(); }
}
