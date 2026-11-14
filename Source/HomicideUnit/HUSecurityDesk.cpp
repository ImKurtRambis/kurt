#include "HUSecurityDesk.h"
#include "HUBuild.h"
#include "HUProps.h"
#include "HUPeople.h"
#include "Components/SkeletalMeshComponent.h"
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
	// a desk with a bank of two small CRT monitors and a tape recorder; +X faces the person sitting at it
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 37), FVector(70, 140, 6), Desk);
	for (float Y : { -62.f, 62.f }) Block(this, R, Blocks, EShape::Cube, FVector(0, Y, 17), FVector(64, 6, 34), Desk * 0.7f);
	for (float Y : { -34.f, 34.f })
	{
		if (Prop(this, R, Blocks, TEXT("CRTMonitor"), FVector(-10, Y, 59), FVector(0, 0, 38))) continue; // a real CRT when the art is in
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
	// Historical people use the same casting as the scene and holding, rendered only by the tape cameras.
	const UHUCaseSubsystem* C = Cases();
	const int32 Count = Frames.Num() > 0 ? Frames[0].People.Num() : 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const bool bSuspect = C && C->Current.Suspects.IsValidIndex(Index);
		const bool bVictim = Index == Count - 1;
		const FString Face = bSuspect ? HUPeople::SuspectFace(C->Current, C->Current.Suspects[Index].Id)
			: bVictim && C ? HUPeople::VictimFace(C->Current.Victim, C->Current.Number) : HUPeople::CastFace(Index);
		AActor* Person = HUPeople::Stand(this, RootComponent, Face, FVector::ZeroVector, 0.f);
		Ghosts.Add(Person);
		if (!Person) continue;
		if (bVictim && C && C->Current.Victim.Contains(TEXT("age "))) Person->SetActorRelativeScale3D(FVector(0.65f));
		TInlineComponentArray<UPrimitiveComponent*> Parts(Person);
		for (UPrimitiveComponent* Part : Parts) { Part->SetVisibleInSceneCaptureOnly(true); Part->SetCastShadow(false); }
		if (USkeletalMeshComponent* Body = HUPeople::BodyOf(Person))
		{
			Body->TickAnimation(0.f, false);
			Body->RefreshBoneTransforms();
			Body->bPauseAnims = true;
		}
		Person->SetActorHiddenInGame(true);
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
	for (AActor* Person : Ghosts) if (Person) Person->SetActorHiddenInGame(true);
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
		if (!Ghosts[i]) continue; Ghosts[i]->SetActorHiddenInGame(!bThere);
		if (bThere) Ghosts[i]->SetActorLocation(F.People[i]);
	}
	// Owned MetaHuman actors render independently of their gameplay actor's HiddenActors entry.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (It->ActorHasTag(TEXT("HUPerson")) && It->GetOwner() != this)
		{
			Capture->HiddenActors.Remove(*It);
			if (It->GetOwner() != VictimActor || !F.bAfter) Capture->HiddenActors.Add(*It);
		}
	// and the body only once it's happened
	Capture->HiddenActors.Remove(VictimActor);
	if (VictimActor && !F.bAfter) Capture->HiddenActors.Add(VictimActor);
	// the figures were just moved: push that to the renderer now, or every moment pulled in one go records the same frame
	GetWorld()->SendAllEndOfFrameUpdates();
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

void AHUSecurityDesk::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bViewing) Close();
	HUPeople::DestroyOwned(this);
	Ghosts.Empty();
	Super::EndPlay(Reason);
}