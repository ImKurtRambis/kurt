#include "HUSecurityDesk.h"
#include "HUBuild.h"
#include "HUInteractComponent.h"
#include "HUSuspectActor.h"
#include "HUCaseSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"

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

UHUCaseSubsystem* AHUSecurityDesk::Cases() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
}

void AHUSecurityDesk::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks);
	if (bAtPrecinct) return; // the precinct copy is just the player: the squad desk computers are its screens
	USceneComponent* R = RootComponent;
	const FLinearColor Desk(0.18f, 0.12f, 0.07f), Beige(0.55f, 0.52f, 0.44f), Steel(0.25f, 0.27f, 0.3f);
	UMaterialInterface* Wood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/Precinct/MI_HU_Walnut.MI_HU_Walnut"));
	UMaterialInterface* PropParent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/Props/M_HU_PropWorldAlignedSurface.M_HU_PropWorldAlignedSurface"));
	auto ApplyProp = [this, PropParent](UStaticMeshComponent* Part, const TCHAR* Texture, FLinearColor Tint, float Tile, float Roughness = 0.9f)
	{
		if (Part && PropParent) if (UMaterialInterface* Surface = HUBuild::PropSurface(this, PropParent, Texture, Tint, Tile, Roughness)) Part->SetMaterial(0, Surface);
	};
	const TCHAR* PlasticTexture = TEXT("/Game/Art/Materials/Textures/M_ComputerPlastic_BaseColor.M_ComputerPlastic_BaseColor");
	// a desk with a bank of two small CRT monitors and a tape recorder; +X faces the person sitting at it
	UStaticMeshComponent* Desktop = Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 37), FVector(70, 140, 6), Desk);
	if (Desktop && Wood) Desktop->SetMaterial(0, Wood);
	for (float Y : { -62.f, 62.f })
	{
		UStaticMeshComponent* SidePanel = Block(this, R, Blocks, EShape::Cube, FVector(0, Y, 17), FVector(64, 6, 34), Desk * 0.7f);
		if (SidePanel && Wood) SidePanel->SetMaterial(0, Wood);
	}
	for (float Y : { -24.f, 24.f })
	{
		UStaticMeshComponent* Drawer = Block(this, R, Blocks, EShape::Cube, FVector(32.f, Y, 25.f), FVector(2, 39, 12), Desk * 0.82f, FRotator::ZeroRotator, false);
		if (Drawer && Wood) Drawer->SetMaterial(0, Wood);
		Block(this, R, Blocks, EShape::Cylinder, FVector(33.4f, Y, 25.f), FVector(2.f, 2.f, 3.f), FLinearColor(0.52f, 0.43f, 0.27f), FRotator(90, 0, 0), false);
	}
	for (float Y : { -34.f, 34.f })
	{
		UStaticMeshComponent* RearCase = Block(this, R, Blocks, EShape::Cube, FVector(-10, Y, 59), FVector(28, 36, 33), Beige);
		ApplyProp(RearCase, PlasticTexture, FLinearColor(0.94f, 0.91f, 0.81f), 42.f);
		UStaticMeshComponent* Bezel = Block(this, R, Blocks, EShape::Cube, FVector(5, Y, 59), FVector(8, 42, 38), Beige * 0.96f, FRotator::ZeroRotator, false);
		ApplyProp(Bezel, PlasticTexture, FLinearColor(0.97f, 0.94f, 0.84f), 42.f, 0.82f);
		Block(this, R, Blocks, EShape::Cube, FVector(9.4f, Y, 60), FVector(0.6f, 33, 27), FLinearColor(0.025f, 0.06f, 0.08f), FRotator::ZeroRotator, false);
		// Tape stills rendered into the glass as subdued scan bars until a case recording is played.
		for (int32 Scan = 0; Scan < 5; ++Scan)
			Block(this, R, Blocks, EShape::Cube, FVector(9.8f, Y, 51.f + Scan * 3.4f), FVector(0.16f, 24.f - (Scan % 2) * 4.f, 0.18f), FLinearColor(0.12f, 0.2f, 0.22f), FRotator::ZeroRotator, false);
		Block(this, R, Blocks, EShape::Cube, FVector(9.8f, Y, 42.5f), FVector(0.22f, 7.f, 1.4f), FLinearColor(0.34f, 0.3f, 0.23f), FRotator::ZeroRotator, false); // lower model plaque
		Block(this, R, Blocks, EShape::Cube, FVector(9.9f, Y + 14.5f, 43.f), FVector(0.28f, 1.3f, 1.3f), FLinearColor(0.9f, 0.08f, 0.04f), FRotator::ZeroRotator, false); // recording light
		UStaticMeshComponent* Pedestal = Block(this, R, Blocks, EShape::Cube, FVector(-4, Y, 38), FVector(12, 17, 6), Beige * 0.82f, FRotator::ZeroRotator, false);
		ApplyProp(Pedestal, PlasticTexture, FLinearColor(0.9f, 0.87f, 0.78f), 42.f);
	}
	Block(this, R, Blocks, EShape::Cube, FVector(-27.f, 0.f, 39.f), FVector(3.f, 3.f, 2.f), FLinearColor(0.08f, 0.08f, 0.075f), FRotator::ZeroRotator, false); // tucked-away power cable
	UStaticMeshComponent* VCR = Block(this, R, Blocks, EShape::Cube, FVector(18, 0, 42), FVector(22, 50, 8), Steel); // VHS recorder
	Block(this, R, Blocks, EShape::Cube, FVector(29.5f, 0, 43), FVector(0.8f, 34, 5), FLinearColor(0.14f, 0.15f, 0.15f), FRotator::ZeroRotator, false); // tape bay
	Block(this, R, Blocks, EShape::Cube, FVector(30.f, -14.f, 43), FVector(1.1f, 6, 2), FLinearColor(2.f, 0.08f, 0.035f), FRotator::ZeroRotator, false); // REC lamp
	for (int32 Button = 0; Button < 5; ++Button)
		Block(this, R, Blocks, EShape::Cylinder, FVector(30.f, 8.f + Button * 6.f, 43.f), FVector(2.f, 2.f, 1.2f),
			(Button == 0) ? FLinearColor(0.55f, 0.44f, 0.27f) : Steel * 1.25f, FRotator(90, 0, 0), false); // deck controls
	if (VCR) Block(this, R, Blocks, EShape::Cube, FVector(30.f, 0, 45.1f), FVector(0.25f, 18.f, 0.7f), FLinearColor(0.35f, 0.72f, 0.44f), FRotator::ZeroRotator, false); // tape counter
	UStaticMeshComponent* StoolSeat = Block(this, R, Blocks, EShape::Cylinder, FVector(55, 0, 31), FVector(18, 18, 5), Steel * 0.8f);
	Block(this, R, Blocks, EShape::Cylinder, FVector(55, 0, 14), FVector(3, 3, 30), Steel);
	for (int32 Spoke = 0; Spoke < 4; ++Spoke)
		Block(this, R, Blocks, EShape::Cube, FVector(55.f + FMath::Cos(Spoke * PI / 2.f) * 12.f, FMath::Sin(Spoke * PI / 2.f) * 12.f, 3), FVector(15, 3, 2), Steel, FRotator(0, Spoke * 90.f, 0), false);
	(void)StoolSeat;
}

void AHUSecurityDesk::BeginPlay()
{
	Super::BeginPlay();
	if (bAtPrecinct)
	{
		// what's on the tapes, from the case
		if (const UHUCaseSubsystem* C = Cases())
		{
			CameraNames = C->TapeCamNames;
			Frames.Reset();
			for (int32 m = 0; m < C->TapeMoments(); m++) { FHUFootageFrame F; F.Clock = C->TapeClock[m]; F.bLost = C->TapeLost.IsValidIndex(m) && C->TapeLost[m]; Frames.Add(F); }
			Cameras.SetNum(C->TapeCams());
		}
		return;
	}
	// the people on the tape: a figure for each, visible only to the cameras
	// (suspects in their coats, then strangers passing through, then the victim last)
	const int32 N = Frames.Num() > 0 ? Frames[0].People.Num() : 0;
	for (int32 i = 0; i < N; i++)
	{
		USceneComponent* G = NewObject<USceneComponent>(this);
		G->SetupAttachment(RootComponent);
		G->SetUsingAbsoluteLocation(true);
		G->RegisterComponent();
		const bool bVictim = i == N - 1;
		const FLinearColor Coat = bVictim ? FLinearColor(0.5f, 0.5f, 0.5f) : Coats.IsValidIndex(i) ? Coats[i] : FLinearColor(0.32f, 0.3f, 0.27f);
		TArray<UStaticMeshComponent*> Parts;
		Block(this, G, Parts, EShape::Cylinder, FVector(0, 0, 45), FVector(40, 46, 90), FLinearColor(0.05f, 0.05f, 0.06f), FRotator::ZeroRotator, false);
		Block(this, G, Parts, EShape::Cylinder, FVector(0, 0, 125), FVector(52, 60, 76), Coat, FRotator::ZeroRotator, false);
		Block(this, G, Parts, EShape::Sphere, FVector(0, 0, 178), FVector(26, 24, 30), FLinearColor(0.45f, 0.35f, 0.3f), FRotator::ZeroRotator, false);
		if (!bVictim && !Coats.IsValidIndex(i)) Block(this, G, Parts, EShape::Cylinder, FVector(0, 0, 196), FVector(34, 34, 8), FLinearColor(0.08f, 0.08f, 0.08f), FRotator::ZeroRotator, false); // a stranger in a hat
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
	if (bAtPrecinct) return FText::GetEmpty();
	const UHUCaseSubsystem* C = Cases();
	if (C && C->bTapesPulled) return FText::FromString(TEXT("The recorder — tapes pulled. Watch them on a desk computer back at the precinct."));
	return FText::FromString(FString::Printf(TEXT("[Hold E] Pull the security tapes (%d cameras) — they're watched back at the precinct"), Cameras.Num()));
}

float AHUSecurityDesk::UseTime(APawn* User, bool bSecondary) const
{
	const UHUCaseSubsystem* C = Cases();
	return !bAtPrecinct && !bSecondary && C && !C->bTapesPulled ? 3.f : 0.f;
}

void AHUSecurityDesk::Interact_Implementation(APawn* User, bool bSecondary)
{
	if (bAtPrecinct || bSecondary) return;
	UHUCaseSubsystem* C = Cases();
	if (!C || C->bTapesPulled) return;
	PullTapes(User); // on every machine (each records its own copy of the identical scene)
}

// record the whole night, camera by camera, moment by moment, into the case - so it plays back anywhere
void AHUSecurityDesk::PullTapes(APawn* User)
{
	UHUCaseSubsystem* C = Cases();
	if (!C) return;
	const int32 NC = Cameras.Num(), NM = Frames.Num();
	C->TapeCamNames = CameraNames;
	C->TapeClock.Reset(); C->TapeLost.Reset();
	for (const FHUFootageFrame& F : Frames) { C->TapeClock.Add(F.Clock); C->TapeLost.Add(F.bLost); }
	C->TapeNotes = Notes;
	C->TapeCatch = CatchFrame;
	C->TapeCatchSuspect = CatchSuspect;
	C->TapeFrames.Reset();
	// keep the present out of the past: the real people
	Capture->HiddenActors.Reset();
	for (TActorIterator<AHUSuspectActor> It(GetWorld()); It; ++It) Capture->HiddenActors.Add(*It);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It) Capture->HiddenActors.Add(*It);
	for (int32 c = 0; c < NC; c++)
		for (int32 m = 0; m < NM; m++)
		{
			if (Frames[m].bLost) { C->TapeFrames.Add(nullptr); continue; }
			UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(C); // owned by the case: it survives the drive back
			RT->RenderTargetFormat = RTF_RGBA8;
			RT->InitAutoFormat(480, 360);
			RT->UpdateResourceImmediate(true);
			Cam = c; Frame = m;
			Capture->TextureTarget = RT;
			Show();
			C->TapeFrames.Add(RT);
		}
	Capture->TextureTarget = nullptr;
	C->bTapesPulled = true;
	if (UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr)
	{
		I->PlayReach();
		I->Say(TEXT("You eject the tapes and bag them. Nothing to watch them on out here - they go back to the precinct."));
		I->Notify(TEXT("TAPES PULLED"), FString::Printf(TEXT("%d cameras, the half hour before it happened. Watch them on a desk computer at the precinct."), NC), FLinearColor(0.6f, 0.85f, 1.f));
	}
}

// ---------- watching (at the precinct) ----------
void AHUSecurityDesk::Open(APawn* User)
{
	APlayerController* PC = Cast<APlayerController>(User->GetController());
	if (!PC || Frames.Num() == 0 || Cameras.Num() == 0 || bViewing) return;
	Viewer = User;
	bViewing = true; bPaused = false; PlayT = 0.f; Frame = 0;
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	if (UHUInteractComponent* I = User->FindComponentByClass<UHUInteractComponent>()) { I->Watching = this; I->Say(TEXT("You slot the tape in. Grainy, but it's all there.")); }
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
		InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AHUSecurityDesk::PlayKey);
		InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AHUSecurityDesk::FlagKey);
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
void AHUSecurityDesk::NextFrame() { if (!bViewing) return; bPaused = true; Frame = FMath::Min(Frame + 1, Frames.Num() - 1); Show(); }
void AHUSecurityDesk::PrevFrame() { if (!bViewing) return; bPaused = true; Frame = FMath::Max(Frame - 1, 0); Show(); }
void AHUSecurityDesk::PlayKey() { if (bViewing) { bPaused = !bPaused; PlayT = 0.f; } }

// E: write down what you see at this moment - and if it's THE moment, the culprit's lie goes on the record
void AHUSecurityDesk::FlagKey()
{
	if (!bViewing) return;
	UHUCaseSubsystem* C = Cases();
	UHUInteractComponent* I = Viewer.IsValid() ? Viewer->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!C || !I) return;
	bPaused = true;
	const int32 Idx = Cam * Frames.Num() + Frame;
	const bool bLost = Frames.IsValidIndex(Frame) && Frames[Frame].bLost;
	const FString Note = bLost ? FString::Printf(TEXT("%s — the recorder was switched off. Somebody didn't want this part on tape."), *Frames[Frame].Clock)
		: C->TapeNotes.IsValidIndex(Idx) && !C->TapeNotes[Idx].IsEmpty() ? C->TapeNotes[Idx] : FString::Printf(TEXT("%s, %s — nobody in shot."), Frames.IsValidIndex(Frame) ? *Frames[Frame].Clock : TEXT(""), *CamLabel());
	I->SendTapeNote(Note, !bLost && Idx == C->TapeCatch && !C->bTapeCaught);
}

void AHUSecurityDesk::Show()
{
	if (!Cameras.IsValidIndex(Cam) || !Frames.IsValidIndex(Frame)) return;
	if (bAtPrecinct)
	{
		// the recording, as it was pulled
		const UHUCaseSubsystem* C = Cases();
		const int32 Idx = Cam * Frames.Num() + Frame;
		Feed = C && C->TapeFrames.IsValidIndex(Idx) ? C->TapeFrames[Idx] : nullptr;
		return;
	}
	const FHUFootageFrame& F = Frames[Frame];
	Capture->SetWorldTransform(Cameras[Cam]);
	// the people on the tape, where they were at this moment
	for (int32 i = 0; i < Ghosts.Num(); i++)
	{
		const bool bThere = F.People.IsValidIndex(i) && !F.People[i].IsNearlyZero();
		Ghosts[i]->SetVisibility(bThere, true);
		if (bThere) Ghosts[i]->SetWorldLocation(F.People[i]);
	}
	// and the body only once it's happened
	Capture->HiddenActors.Remove(VictimActor);
	if (VictimActor && !F.bAfter) Capture->HiddenActors.Add(VictimActor);
	Capture->CaptureScene();
}

void AHUSecurityDesk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bViewing) return;
	if (!Viewer.IsValid()) { Close(); return; }
	// it plays: a frame every 0.8 s, round and round
	if (!bPaused && (PlayT += DeltaSeconds) > 0.8f)
	{
		PlayT = 0.f;
		Frame = (Frame + 1) % FMath::Max(1, Frames.Num());
		Show();
	}
}
