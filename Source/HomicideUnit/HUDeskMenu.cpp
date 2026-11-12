#include "HUDeskMenu.h"
#include "HUProgress.h"
#include "HUListenSubsystem.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUMenuWidget.h"
#include "HUSquadSubsystem.h"
#include "HUVoiceSubsystem.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

using namespace HUBuild;

static const float DeskZ = 75.f; // the desk top

AHUDeskMenu::AHUDeskMenu()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	// you, sitting at the desk, looking down at it
	Seat = CreateDefaultSubobject<UCameraComponent>(TEXT("Seat"));
	Seat->SetupAttachment(Root);
	Seat->SetRelativeLocation(FVector(-118, 0, 138));
	Seat->SetRelativeRotation(FRotator(-33, 0, 0));
	Seat->SetFieldOfView(70.f);
	// the desk lamp, and the light that picks out whatever you're pointing at
	Lamp = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Root);
	Lamp->SetRelativeLocation(FVector(22, -62, 118));
	Lamp->SetIntensity(2600.f);
	Lamp->SetAttenuationRadius(420.f);
	Lamp->SetLightColor(FLinearColor(1.f, 0.78f, 0.5f));
	Highlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Highlight"));
	Highlight->SetupAttachment(Root);
	Highlight->SetRelativeRotation(FRotator(-90, 0, 0));
	Highlight->SetIntensity(3500.f);
	Highlight->SetAttenuationRadius(160.f);
	Highlight->SetInnerConeAngle(10.f);
	Highlight->SetOuterConeAngle(18.f);
	Highlight->SetLightColor(FLinearColor(0.75f, 0.9f, 1.f));
	PlateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PlateText"));
	PlateText->SetupAttachment(Root);
	PlateText->SetRelativeLocation(FVector(35.9f, 8, 80.f));
	PlateText->SetRelativeRotation(FRotator(0, 180, 0));
	PlateText->SetHorizontalAlignment(EHTA_Center);
	PlateText->SetVerticalAlignment(EVRTA_TextCenter);
	PlateText->SetWorldSize(3.8f);
	PlateText->SetTextRenderColor(FColor(30, 20, 10));
	for (FVector& P : ItemPos) P = FVector(0, 0, DeskZ);
}

const TCHAR* AHUDeskMenu::ItemName(EHUDeskItem I)
{
	switch (I)
	{
	case EHUDeskItem::Manual:    return TEXT("POLICE MANUAL");
	case EHUDeskItem::DeskPhone: return TEXT("DESK PHONE");
	case EHUDeskItem::CellPhone: return TEXT("YOUR CELL PHONE");
	case EHUDeskItem::Photo:     return TEXT("FRAMED PHOTO");
	case EHUDeskItem::CaseFiles: return TEXT("CASE FILES");
	case EHUDeskItem::NamePlate: return TEXT("YOUR NAME PLATE");
	case EHUDeskItem::Paper:     return TEXT("THE PAPER");
	case EHUDeskItem::Computer:  return TEXT("THE COMPUTER");
	case EHUDeskItem::CarKeys:   return TEXT("CAR KEYS");
	default:                     return TEXT("");
	}
}

FString AHUDeskMenu::ItemDoes(EHUDeskItem I) const
{
	const UHUCaseSubsystem* C = Cases();
	const bool bNamed = !Name().IsEmpty();
	switch (I)
	{
	case EHUDeskItem::Manual:    return TEXT("How to play");
	case EHUDeskItem::DeskPhone: return TEXT("Call in your squad — open the squad room and get a code");
	case EHUDeskItem::CellPhone: return TEXT("Answer a squad's page — join with their 6-digit code");
	case EHUDeskItem::Photo:     return TEXT("Credits");
	case EHUDeskItem::CaseFiles: return !bNamed ? TEXT("Start your shift (first, your name)") : C && C->CasesSolved > 0 ? TEXT("Start your shift — to the squad room") : TEXT("Start your career — to the squad room");
	case EHUDeskItem::NamePlate: return bNamed ? TEXT("Your personnel file") : TEXT("Put your name on it");
	case EHUDeskItem::Paper:     return TEXT("What's new");
	case EHUDeskItem::Computer:  return TEXT("Settings");
	case EHUDeskItem::CarKeys:   return TEXT("Go home — exit the game");
	default:                     return TEXT("");
	}
}

UHUCaseSubsystem* AHUDeskMenu::Cases() const { return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr; }
APlayerController* AHUDeskMenu::PC() const { return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr; }
FString AHUDeskMenu::Name() const { const UHUCaseSubsystem* C = Cases(); return C ? C->DetectiveName.TrimStartAndEnd() : FString(); }

// ---------- the room and the desk ----------
void AHUDeskMenu::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks);
	ItemOf.Reset();
	USceneComponent* R = RootComponent;
	auto Part = [&](EHUDeskItem It, EShape S, FVector P, FVector Size, FLinearColor Col, FRotator Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* M = Block(this, R, Blocks, S, P, Size, Col, Rot);
		if (M && It != EHUDeskItem::Count) ItemOf.Add(M, (int32)It);
		return M;
	};
	const EHUDeskItem None = EHUDeskItem::Count;

	// the office: dark floor, walls, a window full of city lights behind the desk
	Part(None, EShape::Cube, FVector(60, 0, -5), FVector(600, 600, 10), FLinearColor(0.06f, 0.04f, 0.03f));
	Part(None, EShape::Cube, FVector(230, 0, 150), FVector(10, 600, 300), FLinearColor(0.12f, 0.13f, 0.14f));
	for (float S : { -1.f, 1.f }) Part(None, EShape::Cube, FVector(60, S * 250, 150), FVector(600, 10, 300), FLinearColor(0.1f, 0.11f, 0.12f));
	Part(None, EShape::Cube, FVector(224, 0, 165), FVector(2, 240, 130), FLinearColor(0.01f, 0.02f, 0.05f));
	FRandomStream City(7);
	for (int32 i = 0; i < 70; i++)
		Part(None, EShape::Cube, FVector(222.5f, City.FRandRange(-115.f, 115.f), City.FRandRange(104.f, 160.f)), FVector(1, 3, 2),
			City.FRand() < 0.7f ? FLinearColor(3.f, 2.2f, 0.9f) : FLinearColor(0.9f, 1.6f, 3.f));
	for (float Y : { -120.f, 0.f, 120.f }) Part(None, EShape::Cube, FVector(222, Y, 165), FVector(4, 4, 134), FLinearColor(0.2f, 0.2f, 0.2f));
	for (float Z : { 100.f, 230.f }) Part(None, EShape::Cube, FVector(222, 0, Z), FVector(4, 244, 4), FLinearColor(0.2f, 0.2f, 0.2f));

	// the desk
	const FLinearColor Wood(0.2f, 0.1f, 0.04f);
	Part(None, EShape::Cube, FVector(0, 0, DeskZ - 3), FVector(92, 184, 6), Wood);
	for (float X : { -40.f, 40.f }) for (float Y : { -85.f, 85.f }) Part(None, EShape::Cube, FVector(X, Y, (DeskZ - 6) / 2), FVector(6, 6, DeskZ - 6), Wood * 0.7f);
	Part(None, EShape::Cube, FVector(0, 0, DeskZ - 0.4f), FVector(60, 100, 0.4f), FLinearColor(0.05f, 0.12f, 0.06f)); // blotter

	// the lamp
	Part(None, EShape::Cylinder, FVector(22, -70, DeskZ + 1), FVector(14, 14, 2), FLinearColor(0.15f, 0.25f, 0.12f));
	Part(None, EShape::Cylinder, FVector(22, -70, DeskZ + 22), FVector(1.5f, 1.5f, 42), FLinearColor(0.6f, 0.5f, 0.2f));
	Part(None, EShape::Cylinder, FVector(22, -62, DeskZ + 42), FVector(18, 9, 9), FLinearColor(0.1f, 0.3f, 0.12f), FRotator(0, 0, 90));

	// CASE FILES: a stack of manila folders, the top one stamped
	for (int32 i = 0; i < 4; i++) Part(EHUDeskItem::CaseFiles, EShape::Cube, FVector(0, -5, DeskZ + 0.9f + i * 1.7f), FVector(30, 24, 1.5f), FLinearColor(0.75f, 0.6f, 0.32f), FRotator(0, -6 + i * 4, 0));
	Part(EHUDeskItem::CaseFiles, EShape::Cube, FVector(-4, -5, DeskZ + 7.6f), FVector(8, 16, 0.2f), FLinearColor(0.6f, 0.05f, 0.03f), FRotator(0, 6, 0));
	ItemPos[(int32)EHUDeskItem::CaseFiles] = FVector(0, -5, DeskZ);

	// THE COMPUTER: a beige CRT and keyboard
	Part(EHUDeskItem::Computer, EShape::Cube, FVector(30, 50, DeskZ + 17), FVector(36, 38, 32), FLinearColor(0.62f, 0.58f, 0.48f));
	Part(EHUDeskItem::Computer, EShape::Cube, FVector(11.8f, 50, DeskZ + 19), FVector(0.6f, 30, 22), FLinearColor(0.15f, 0.45f, 1.4f));
	Part(EHUDeskItem::Computer, EShape::Cube, FVector(-8, 46, DeskZ + 1), FVector(13, 38, 2), FLinearColor(0.55f, 0.52f, 0.44f));
	ItemPos[(int32)EHUDeskItem::Computer] = FVector(20, 50, DeskZ);

	// DESK PHONE: black, handset on the cradle
	Part(EHUDeskItem::DeskPhone, EShape::Cube, FVector(4, -46, DeskZ + 3.5f), FVector(18, 20, 7), FLinearColor(0.02f, 0.02f, 0.02f));
	Part(EHUDeskItem::DeskPhone, EShape::Cylinder, FVector(4, -46, DeskZ + 8.5f), FVector(3.5f, 3.5f, 22), FLinearColor(0.03f, 0.03f, 0.03f), FRotator(0, 0, 90));
	ItemPos[(int32)EHUDeskItem::DeskPhone] = FVector(4, -46, DeskZ);

	// YOUR CELL PHONE: a flip phone, its little screen lit
	Part(EHUDeskItem::CellPhone, EShape::Cube, FVector(-26, 26, DeskZ + 1), FVector(9, 5, 2), FLinearColor(0.08f, 0.08f, 0.1f), FRotator(0, 20, 0));
	Part(EHUDeskItem::CellPhone, EShape::Cube, FVector(-26.5f, 26, DeskZ + 2.1f), FVector(3, 3, 0.2f), FLinearColor(0.3f, 1.2f, 0.6f), FRotator(0, 20, 0));
	ItemPos[(int32)EHUDeskItem::CellPhone] = FVector(-26, 26, DeskZ);

	// POLICE MANUAL: a thick navy binder
	Part(EHUDeskItem::Manual, EShape::Cube, FVector(16, -80, DeskZ + 2.5f), FVector(24, 18, 5), FLinearColor(0.04f, 0.07f, 0.2f), FRotator(0, 12, 0));
	Part(EHUDeskItem::Manual, EShape::Cube, FVector(16, -80, DeskZ + 5.1f), FVector(10, 8, 0.2f), FLinearColor(0.7f, 0.6f, 0.2f), FRotator(0, 12, 0));
	ItemPos[(int32)EHUDeskItem::Manual] = FVector(16, -80, DeskZ);

	// THE PAPER: folded, a dark headline band
	Part(EHUDeskItem::Paper, EShape::Cube, FVector(-22, -32, DeskZ + 0.6f), FVector(28, 20, 1.2f), FLinearColor(0.75f, 0.73f, 0.66f), FRotator(0, 18, 0));
	Part(EHUDeskItem::Paper, EShape::Cube, FVector(-26, -32, DeskZ + 1.3f), FVector(5, 17, 0.2f), FLinearColor(0.05f, 0.05f, 0.05f), FRotator(0, 18, 0));
	ItemPos[(int32)EHUDeskItem::Paper] = FVector(-22, -32, DeskZ);

	// YOUR NAME PLATE: wood with a brass face, at the back edge, facing you
	Part(EHUDeskItem::NamePlate, EShape::Cube, FVector(40, 8, DeskZ + 4), FVector(7, 44, 8), Wood * 1.4f);
	Part(EHUDeskItem::NamePlate, EShape::Cube, FVector(36.3f, 8, DeskZ + 5), FVector(0.4f, 40, 6), FLinearColor(0.75f, 0.55f, 0.15f));
	ItemPos[(int32)EHUDeskItem::NamePlate] = FVector(40, 8, DeskZ);

	// FRAMED PHOTO: standing, sepia
	Part(EHUDeskItem::Photo, EShape::Cube, FVector(34, -34, DeskZ + 11), FVector(2, 18, 22), FLinearColor(0.03f, 0.02f, 0.02f), FRotator(0, -20, 0));
	Part(EHUDeskItem::Photo, EShape::Cube, FVector(32.8f, -33.6f, DeskZ + 11), FVector(0.4f, 14, 17), FLinearColor(0.55f, 0.42f, 0.28f), FRotator(0, -20, 0));
	ItemPos[(int32)EHUDeskItem::Photo] = FVector(34, -34, DeskZ);

	// CAR KEYS: a ring and a key
	Part(EHUDeskItem::CarKeys, EShape::Cylinder, FVector(-28, 66, DeskZ + 0.4f), FVector(6, 6, 0.6f), FLinearColor(0.7f, 0.7f, 0.72f));
	Part(EHUDeskItem::CarKeys, EShape::Cube, FVector(-24, 70, DeskZ + 0.4f), FVector(8, 2, 0.5f), FLinearColor(0.75f, 0.65f, 0.3f), FRotator(0, 30, 0));
	Part(EHUDeskItem::CarKeys, EShape::Cube, FVector(-30, 62, DeskZ + 0.8f), FVector(4, 3, 1.5f), FLinearColor(0.05f, 0.05f, 0.05f));
	ItemPos[(int32)EHUDeskItem::CarKeys] = FVector(-28, 66, DeskZ);

	PlateText->SetText(FText::FromString(TEXT("DETECTIVE")));
}

// ---------- taking over the screen ----------
void AHUDeskMenu::BeginPlay()
{
	Super::BeginPlay();
	BuildFigurine();
	APlayerController* P = PC();
	if (!P) return;
	P->SetViewTarget(this);
	P->SetIgnoreMoveInput(true);
	P->SetIgnoreLookInput(true);
	P->bShowMouseCursor = true;
	P->bEnableClickEvents = true;
	P->bEnableMouseOverEvents = true;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	P->SetInputMode(Mode);

	EnableInput(P);
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AHUDeskMenu::Prev);
		InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AHUDeskMenu::Prev);
		InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AHUDeskMenu::Next);
		InputComponent->BindKey(EKeys::D, IE_Pressed, this, &AHUDeskMenu::Next);
		InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AHUDeskMenu::Up);
		InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AHUDeskMenu::Up);
		InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AHUDeskMenu::Down);
		InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AHUDeskMenu::Down);
		InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AHUDeskMenu::Activate);
		InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AHUDeskMenu::Activate);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHUDeskMenu::Back);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AHUDeskMenu::Click);
		InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AHUDeskMenu::OnAnyKey);
	}
	Widget = CreateWidget<UHUMenuWidget>(P, UHUMenuWidget::StaticClass());
	if (Widget) { Widget->Owner = this; Widget->SetVisibility(ESlateVisibility::HitTestInvisible); Widget->AddToViewport(20); }
	// a new detective's first job: put a name on the plate
	if (Name().IsEmpty()) { Pick = (int32)EHUDeskItem::NamePlate; }
}

void AHUDeskMenu::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// the mic level meter runs while the Microphone setting is picked
	if (UHUListenSubsystem* ML = GetGameInstance()->GetSubsystem<UHUListenSubsystem>())
	{
		const bool bWant = Panel == EHUMenuPanel::Settings && Row == 9;
		if (bWant && !ML->IsMonitoring()) ML->StartMonitor(); else if (!bWant && ML->IsMonitoring()) ML->StopMonitor();
	}
	APlayerController* P = PC();
	Hover = -1;
	FHitResult Hit;
	if (Panel == EHUMenuPanel::None && P && P->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		if (const int32* It = ItemOf.Find(Hit.GetComponent())) { Hover = *It; Pick = *It; }
	// the light drifts over to whatever you're pointing at
	const FVector Target = ItemPos[Pick] + FVector(0, 0, 55);
	Highlight->SetRelativeLocation(FMath::VInterpTo(Highlight->GetRelativeLocation(), Target, DeltaSeconds, 12.f));
	Highlight->SetVisibility(Panel == EHUMenuPanel::None);
	// your name on the plate (with a cursor while you type it)
	const FString Shown = bTyping && TypingField == 0 ? Typed + (FMath::Fmod(GetWorld()->GetTimeSeconds(), 1.0) < 0.5 ? TEXT("_") : TEXT(" ")) : Name();
	PlateText->SetText(FText::FromString(Shown.IsEmpty() ? TEXT("— YOUR NAME —") : (TEXT("DET. ") + Shown).ToUpper()));
}

// ---------- input ----------
// ← → walk across the desk in this order, left to right as you sit
static const EHUDeskItem DeskOrder[] = { EHUDeskItem::Manual, EHUDeskItem::DeskPhone, EHUDeskItem::Photo, EHUDeskItem::Paper, EHUDeskItem::CaseFiles,
	EHUDeskItem::NamePlate, EHUDeskItem::CellPhone, EHUDeskItem::Computer, EHUDeskItem::CarKeys };
static int32 OrderIndex(int32 Item) { for (int32 i = 0; i < UE_ARRAY_COUNT(DeskOrder); i++) if ((int32)DeskOrder[i] == Item) return i; return 0; }

int32 AHUDeskMenu::RowsIn(EHUMenuPanel P) const
{
	switch (P) { case EHUMenuPanel::Settings: return 10; case EHUMenuPanel::Join: return 2; case EHUMenuPanel::Personnel: return 2 + HULook::Fields; /* name, looks, specialty */ default: return 0; }
}

void AHUDeskMenu::Prev()
{
	if (bTyping) return;
	if (Panel == EHUMenuPanel::Settings) { ChangeSetting(-1); return; }
	if (Panel == EHUMenuPanel::Personnel) { ChangeLook(-1); return; }
	if (Panel != EHUMenuPanel::None) return;
	Pick = (int32)DeskOrder[(OrderIndex(Pick) + UE_ARRAY_COUNT(DeskOrder) - 1) % UE_ARRAY_COUNT(DeskOrder)];
}
void AHUDeskMenu::Next()
{
	if (bTyping) return;
	if (Panel == EHUMenuPanel::Settings) { ChangeSetting(1); return; }
	if (Panel == EHUMenuPanel::Personnel) { ChangeLook(1); return; }
	if (Panel != EHUMenuPanel::None) return;
	Pick = (int32)DeskOrder[(OrderIndex(Pick) + 1) % UE_ARRAY_COUNT(DeskOrder)];
}

// the name plate: ← → change the picked part of your look (row 0 is your name)
void AHUDeskMenu::ChangeLook(int32 Dir)
{
	UHUCaseSubsystem* C = Cases();
	if (!C || Row < 1) return;
	if (Row == 1 + HULook::Fields) // the specialty
	{
		C->SetSpecialty((uint8)((C->Specialty + Dir + HUProgress::SpecCount) % HUProgress::SpecCount));
		return;
	}
	HULook::Step(C->Look, Row - 1, Dir);
	C->SaveProgress();
	BuildFigurine();
}

// your detective, in miniature, standing on the desk behind the name plate
void AHUDeskMenu::BuildFigurine()
{
	const UHUCaseSubsystem* C = Cases();
	if (!C) return;
	if (!Figurine)
	{
		Figurine = NewObject<USceneComponent>(this, TEXT("HUFigurine"));
		Figurine->SetupAttachment(RootComponent);
		Figurine->SetRelativeLocation(FVector(52, 30, DeskZ));
		Figurine->SetRelativeRotation(FRotator(0, 200, 0)); // facing you, a little turned
		Figurine->SetRelativeScale3D(FVector(0.15f));
		Figurine->RegisterComponent();
	}
	HULook::BuildFigure(this, Figurine, FigureParts, C->Look, false);
}
void AHUDeskMenu::Up() { const int32 N = RowsIn(Panel); if (!bTyping && N > 0) Row = (Row + N - 1) % N; }
void AHUDeskMenu::Down() { const int32 N = RowsIn(Panel); if (!bTyping && N > 0) Row = (Row + 1) % N; }

void AHUDeskMenu::Back()
{
	if (bTyping) return; // the typing handler deals with ESC
	if (Panel != EHUMenuPanel::None) { Panel = EHUMenuPanel::None; return; }
	Panel = EHUMenuPanel::Quit; // ESC on the desk asks if you're going home
}

void AHUDeskMenu::Activate()
{
	if (bTyping) return; // the typing handler deals with ENTER
	switch (Panel)
	{
	case EHUMenuPanel::None: Use((EHUDeskItem)Pick); break;
	case EHUMenuPanel::Settings: ChangeSetting(1); break;
	case EHUMenuPanel::Personnel: if (Row == 0) StartTyping(0); else ChangeLook(1); break;
	case EHUMenuPanel::Join:
		if (Row == 0) StartTyping(1);
		else if (const UHUCaseSubsystem* C = Cases())
			if (UHUSquadSubsystem* Sq = GetGameInstance()->GetSubsystem<UHUSquadSubsystem>()) { Sq->JoinByCode(C->JoinAddress); Panel = EHUMenuPanel::Squad; }
		break;
	case EHUMenuPanel::Quit: UKismetSystemLibrary::QuitGame(this, PC(), EQuitPreference::Quit, false); break;
	default: Panel = EHUMenuPanel::None; break;
	}
}

void AHUDeskMenu::Click()
{
	if (bTyping) return;
	if (Panel == EHUMenuPanel::None) { if (Hover >= 0) Use((EHUDeskItem)Hover); return; }
	Activate();
}

// ---------- typing your name / the host's address ----------
void AHUDeskMenu::StartTyping(int32 Field)
{
	const UHUCaseSubsystem* C = Cases();
	TypingField = Field;
	Typed = Field == 0 ? Name() : (C && C->JoinAddress.IsNumeric() ? C->JoinAddress : FString());
	bTyping = true;
}

void AHUDeskMenu::FinishTyping(bool bKeep)
{
	bTyping = false;
	UHUCaseSubsystem* C = Cases();
	if (!bKeep || !C) { bPlayAfterName = false; return; }
	if (TypingField == 0) C->DetectiveName = Typed.TrimStartAndEnd();
	else C->JoinAddress = Typed.TrimStartAndEnd();
	C->SaveProgress();
	if (TypingField == 0 && bPlayAfterName && !Name().IsEmpty()) { bPlayAfterName = false; Panel = EHUMenuPanel::None; Use(EHUDeskItem::CaseFiles); }
}

void AHUDeskMenu::OnAnyKey(FKey Key)
{
	if (!bTyping) return;
	if (Key == EKeys::Enter) { FinishTyping(true); return; }
	if (Key == EKeys::Escape) { FinishTyping(false); return; }
	if (Key == EKeys::BackSpace) { if (Typed.Len()) Typed.LeftChopInline(1); return; }
	const APlayerController* P = PC();
	const bool bShift = P && (P->IsInputKeyDown(EKeys::LeftShift) || P->IsInputKeyDown(EKeys::RightShift));
	const FString K = Key.GetFName().ToString();
	static const TMap<FString, FString> Digits = {
		{ TEXT("Zero"), TEXT("0") }, { TEXT("One"), TEXT("1") }, { TEXT("Two"), TEXT("2") }, { TEXT("Three"), TEXT("3") }, { TEXT("Four"), TEXT("4") },
		{ TEXT("Five"), TEXT("5") }, { TEXT("Six"), TEXT("6") }, { TEXT("Seven"), TEXT("7") }, { TEXT("Eight"), TEXT("8") }, { TEXT("Nine"), TEXT("9") } };
	FString Ch;
	if (K.Len() == 1 && FChar::IsAlpha(K[0]))
	{
		const bool bStartOfWord = Typed.IsEmpty() || Typed.EndsWith(TEXT(" ")) || Typed.EndsWith(TEXT("-"));
		Ch = (bShift || (TypingField == 0 && bStartOfWord)) ? K.ToUpper() : K.ToLower();
	}
	else if (const FString* D = Digits.Find(K)) Ch = *D;
	else if (K.StartsWith(TEXT("NumPad")) && Digits.Find(K.RightChop(6))) Ch = *Digits.Find(K.RightChop(6));
	else if (K == TEXT("SpaceBar")) Ch = TEXT(" ");
	else if (K == TEXT("Period") || K == TEXT("Decimal")) Ch = TEXT(".");
	else if (K == TEXT("Hyphen") || K == TEXT("Subtract")) Ch = TEXT("-");
	else if (K == TEXT("Semicolon")) Ch = TEXT(":");
	else if (K == TEXT("Apostrophe")) Ch = TEXT("'");
	if (TypingField == 1 && !(Ch.Len() == 1 && FChar::IsDigit(Ch[0]))) return; // a squad code is just digits
	const int32 Max = TypingField == 0 ? 24 : 6;
	if (!Ch.IsEmpty() && Typed.Len() < Max) Typed += Ch;
}

// ---------- going places ----------
bool AHUDeskMenu::LevelExists(FName Level) const
{
	FString Found;
	return FPackageName::SearchForPackageOnDisk(Level.ToString() + FPackageName::GetMapPackageExtension(), nullptr, &Found);
}

void AHUDeskMenu::GoTo(FName Level, const FString& Options)
{
	if (!LevelExists(Level)) { MissingLevel = Level.ToString(); Panel = EHUMenuPanel::Missing; return; }
	if (APlayerController* P = PC()) { P->SetInputMode(FInputModeGameOnly()); P->bShowMouseCursor = false; }
	UGameplayStatics::OpenLevel(this, Level, true, Options);
}

void AHUDeskMenu::Use(EHUDeskItem Item)
{
	Row = 0;
	switch (Item)
	{
	case EHUDeskItem::CaseFiles:
	case EHUDeskItem::DeskPhone:
		if (Name().IsEmpty()) { Panel = EHUMenuPanel::Personnel; bPlayAfterName = true; StartTyping(0); break; } // name first
		if (!LevelExists(TEXT("L_Lobby"))) { MissingLevel = TEXT("L_Lobby"); Panel = EHUMenuPanel::Missing; break; }
		// every shift starts in the squad room, with a code your friends can join on
		if (UHUSquadSubsystem* Sq = GetGameInstance()->GetSubsystem<UHUSquadSubsystem>()) { Panel = EHUMenuPanel::Squad; if (APlayerController* P = PC()) { P->SetInputMode(FInputModeGameOnly()); P->bShowMouseCursor = false; } Sq->HostSquad(); }
		break;
	case EHUDeskItem::CellPhone: Panel = EHUMenuPanel::Join; break;
	case EHUDeskItem::Computer:  Panel = EHUMenuPanel::Settings; break;
	case EHUDeskItem::Manual:    Panel = EHUMenuPanel::Manual; break;
	case EHUDeskItem::Paper:     Panel = EHUMenuPanel::Paper; break;
	case EHUDeskItem::Photo:     Panel = EHUMenuPanel::Credits; break;
	case EHUDeskItem::NamePlate: Panel = EHUMenuPanel::Personnel; if (Name().IsEmpty()) StartTyping(0); break;
	case EHUDeskItem::CarKeys:   Panel = EHUMenuPanel::Quit; break;
	default: break;
	}
}

// ---------- the computer: settings (saved) ----------
static const float FpsCaps[] = { 0.f, 30.f, 60.f, 120.f, 144.f };

void AHUDeskMenu::ChangeSetting(int32 Dir)
{
	UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	UHUCaseSubsystem* C = Cases();
	if (!S) return;
	switch (Row)
	{
	case 0: S->SetFullscreenMode((EWindowMode::Type)(((int32)S->GetFullscreenMode() + Dir + 3) % 3)); break;
	case 1:
	{
		TArray<FIntPoint> Res;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Res);
		if (Res.Num() == 0) break;
		int32 I = Res.IndexOfByKey(S->GetScreenResolution());
		I = I == INDEX_NONE ? Res.Num() - 1 : (I + Dir + Res.Num()) % Res.Num();
		S->SetScreenResolution(Res[I]);
		break;
	}
	case 2: S->SetOverallScalabilityLevel((FMath::Max(0, S->GetOverallScalabilityLevel()) + Dir + 4) % 4); break;
	case 3: S->SetVSyncEnabled(!S->IsVSyncEnabled()); break;
	case 4:
	{
		int32 I = 0; for (int32 i = 0; i < UE_ARRAY_COUNT(FpsCaps); i++) if (FMath::IsNearlyEqual(S->GetFrameRateLimit(), FpsCaps[i])) I = i;
		S->SetFrameRateLimit(FpsCaps[(I + Dir + UE_ARRAY_COUNT(FpsCaps)) % UE_ARRAY_COUNT(FpsCaps)]);
		break;
	}
	case 5: if (C) { C->MouseSensitivity = FMath::Clamp(FMath::RoundToFloat((C->MouseSensitivity + Dir * 0.1f) * 10.f) / 10.f, 0.2f, 3.f); C->SaveProgress(); } return;
	case 6: if (C) { C->FieldOfView = FMath::Clamp(C->FieldOfView + Dir * 5.f, 70.f, 110.f); C->SaveProgress(); } return;
	case 7: if (C) { C->VoiceProvider = (uint8)((C->VoiceProvider + Dir + 3) % 3); C->SaveProgress(); } return;
	case 8:
		if (C)
		{
			// ENTER / → pastes the key you copied; ← removes it
			if (Dir < 0) C->VoiceKey.Reset();
			else { FString Clip; FPlatformApplicationMisc::ClipboardPaste(Clip); Clip.TrimStartAndEndInline(); if (Clip.Len() > 10 && !Clip.Contains(TEXT(" "))) C->VoiceKey = Clip; }
			C->SaveProgress();
		}
		return;
	case 9: if (UHUListenSubsystem* L = GetGameInstance()->GetSubsystem<UHUListenSubsystem>()) L->CycleMic(Dir); return; // the microphone
	}
	S->ApplySettings(false);
	S->SaveSettings();
}

// ---------- what the panels say ----------
void AHUDeskMenu::PanelText(FString& Title, TArray<FString>& Lines) const
{
	Lines.Reset();
	const UHUCaseSubsystem* C = Cases();
	const FString Caret = FMath::Fmod(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, 1.0) < 0.5 ? TEXT("_") : TEXT(" ");
	auto Sel = [this](int32 R) { return Row == R ? TEXT("▸ ") : TEXT("   "); };
	switch (Panel)
	{
	case EHUMenuPanel::Manual:
		Title = TEXT("POLICE MANUAL — HOW TO PLAY");
		Lines = {
			TEXT("WASD walk · mouse look · SHIFT run · SPACE jump · C crouch · E use / talk · Q other option · ESC back to the desk."),
			TEXT("Every case starts at the Captain's briefing. Walk to the laptop: E next slide, Q back."),
			TEXT("Then the motor pool: grab kit at the supply locker, gather the squad at a cruiser, E to roll."),
			TEXT("At the scene, search everything: up the stairs, up ladders, down narrow passages, through the crawlspace."),
			TEXT("E grabs furniture: push and pull it — evidence hides underneath."),
			TEXT("Some evidence needs tools: F flashlight (the dark), V UV light (traces), the print kit, the crowbar, R scanner."),
			TEXT("Every piece that proves a lie also points at what kind of offender you're dealing with."),
			TEXT("Talk to everyone (E). Run their records (Q). Catch enough lies on the record, then name who did it."),
		};
		break;
	case EHUMenuPanel::Paper:
		Title = TEXT("THE METRO DAILY — WHAT'S NEW");
		Lines = {
			TEXT("CITY CASES NOW RANDOM: any crime, any offender, any address. 'You never know what you'll find.'"),
			TEXT("EVERY SCENE DIFFERENT: open halls, warrens of rooms, warehouse aisles, balconies — a new floor plan every case."),
			TEXT("DETECTIVES' KIT: flashlights, UV lamps, print kits, crowbars and scanners at the motor-pool supply locker."),
			TEXT("PROFILERS JOIN THE SQUAD: evidence now tells you what kind of offender you're after."),
			TEXT("BRIEFINGS ON THE BIG SCREEN: the Captain's slides, the laptop at the side table."),
		};
		break;
	case EHUMenuPanel::Credits:
		Title = TEXT("FRAMED PHOTO — CREDITS");
		Lines = { TEXT("HOMICIDE UNIT"), TEXT("Design and direction: you."), TEXT("Code: written with Claude."), TEXT("Engine: Unreal Engine 5."), TEXT(""), TEXT("For everyone who works the night shift.") };
		break;
	case EHUMenuPanel::Personnel:
		Title = TEXT("PERSONNEL FILE");
		if (bTyping) Lines.Add(FString::Printf(TEXT("▸ NAME:  %s%s"), *Typed, *Caret));
		else Lines.Add(FString::Printf(TEXT("%sNAME:  %s"), Sel(0), Name().IsEmpty() ? TEXT("(none yet)") : *Name()));
		// how you look - the figure on the desk changes as you go
		if (C) for (int32 f = 0; f < HULook::Fields; f++)
			Lines.Add(FString::Printf(TEXT("%s%s:   <  %s  >"), Sel(f + 1), HULook::FieldName(f), *HULook::ValueName(C->Look, f)));
		if (C)
		{
			// your specialty and how far up you are
			const int32 Lv = HUProgress::SpecLevel(C->SpecXPOf(C->Specialty));
			Lines.Add(FString::Printf(TEXT("%sSPECIALTY:   <  %s%s  >"), Sel(1 + HULook::Fields), HUProgress::SpecName(C->Specialty), C->Specialty ? *FString::Printf(TEXT("  (level %d%s)"), Lv, Lv < 5 ? *FString::Printf(TEXT(", %d / %d XP"), C->SpecXPOf(C->Specialty), HUProgress::SpecLevelXP(Lv + 1)) : TEXT(", max")) : TEXT("")));
			Lines.Add(FString(TEXT("      ")) + HUProgress::SpecPerk(C->Specialty));
			const int32 Rk = HUProgress::RankOf(C->SquadXP);
			Lines.Add(FString::Printf(TEXT("   RANK:  %s   (%d XP%s)"), HUProgress::RankName(Rk), C->SquadXP, Rk < HUProgress::RankCount - 1 ? *FString::Printf(TEXT(", next at %d"), HUProgress::RankXP(Rk + 1)) : TEXT("")));
			Lines.Add(FString::Printf(TEXT("   CASES CLOSED:  %d"), C->CasesSolved));
			Lines.Add(FString::Printf(TEXT("   CASH:  $%d"), C->Cash));
			FString Kit;
			for (int32 i = 0; i < (int32)EHUTool::Count; i++) if (C->HasTool((EHUTool)i)) Kit += (Kit.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(HUTools::Name((EHUTool)i));
			Lines.Add(TEXT("   KIT:  ") + (Kit.IsEmpty() ? FString(TEXT("nothing signed out yet")) : Kit));
		}
		Lines.Add(TEXT(""));
		Lines.Add(bTyping ? TEXT("Type your name   ·   ENTER save   ·   ESC cancel") : TEXT("↑ ↓ choose   ·   ← → change your look   ·   ENTER on NAME to type it   ·   saved automatically   ·   ESC back"));
		break;
	case EHUMenuPanel::Settings:
	{
		Title = TEXT("THE COMPUTER — SETTINGS");
		const UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (!S) break;
		const TCHAR* Modes[] = { TEXT("Fullscreen"), TEXT("Windowed fullscreen"), TEXT("Windowed") };
		const TCHAR* Quality[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
		const float Fps = S->GetFrameRateLimit();
		const FIntPoint R = S->GetScreenResolution();
		Lines.Add(FString::Printf(TEXT("%sWindow:  %s"), Sel(0), Modes[FMath::Clamp((int32)S->GetFullscreenMode(), 0, 2)]));
		Lines.Add(FString::Printf(TEXT("%sResolution:  %d × %d"), Sel(1), R.X, R.Y));
		Lines.Add(FString::Printf(TEXT("%sGraphics:  %s"), Sel(2), Quality[FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 3)]));
		Lines.Add(FString::Printf(TEXT("%sV-Sync:  %s"), Sel(3), S->IsVSyncEnabled() ? TEXT("On") : TEXT("Off")));
		Lines.Add(FString::Printf(TEXT("%sFrame rate cap:  %s"), Sel(4), Fps <= 0.f ? TEXT("None") : *FString::FromInt(FMath::RoundToInt(Fps))));
		Lines.Add(FString::Printf(TEXT("%sMouse sensitivity:  %.1f"), Sel(5), C ? C->MouseSensitivity : 1.f));
		Lines.Add(FString::Printf(TEXT("%sField of view:  %d°"), Sel(6), C ? FMath::RoundToInt(C->FieldOfView) : 90));
		Lines.Add(FString::Printf(TEXT("%sVoices:  %s"), Sel(7), C ? *UHUVoiceSubsystem::ProviderName((EHUVoiceProvider)C->VoiceProvider) : TEXT("")));
		if (C && C->VoiceKey.Len() > 10) Lines.Add(FString::Printf(TEXT("%sVoice API key:  %s...%s   (ENTER paste a new one  ·  ← remove)"), Sel(8), *C->VoiceKey.Left(3), *C->VoiceKey.Right(4)));
		else if (const UHUVoiceSubsystem* V = GetGameInstance()->GetSubsystem<UHUVoiceSubsystem>(); V && V->Key().Len() > 10) Lines.Add(FString::Printf(TEXT("%sVoice API key:  %s...%s  (from ElevenLabsKey.txt)"), Sel(8), *V->Key().Left(3), *V->Key().Right(4)));
		else Lines.Add(FString::Printf(TEXT("%sVoice API key:  not set — put it in ElevenLabsKey.txt in the project folder, or copy it and press ENTER here"), Sel(8)));
		Lines.Add(TEXT(""));
		{
			// THE MICROPHONE: this game is played by talking. A live level while it's picked - talk and the bar moves.
			UHUListenSubsystem* L = GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
			if (L)
			{
				const int32 N = FMath::RoundToInt(L->Level() * 16.f);
				const FString Meter = Row == 9 && L->IsMonitoring() ? FString(TEXT("   [")) + FString::ChrN(N, '|') + FString::ChrN(16 - N, '.') + TEXT("]") : FString();
				Lines.Add(FString::Printf(TEXT("%sMicrophone (required — you talk to suspects):  %s%s%s"), Sel(9), *L->MicLabel(), *Meter,
					!L->IsInstalled() ? TEXT("   (speech files not installed — see AI\\README.txt)") : TEXT("")));
			}
		}
		Lines.Add(TEXT("↑ ↓ choose   ·   ← → / ENTER change   ·   saved automatically   ·   ESC back"));
		break;
	}
	case EHUMenuPanel::Join:
		Title = TEXT("YOUR CELL PHONE — JOIN A SQUAD");
		if (bTyping && TypingField == 1) Lines.Add(FString::Printf(TEXT("▸ SQUAD CODE:  %s%s"), *Typed, *Caret));
		else Lines.Add(FString::Printf(TEXT("%sSQUAD CODE:  %s"), Sel(0), C && C->JoinAddress.IsNumeric() && C->JoinAddress.Len() == 6 ? *C->JoinAddress : TEXT("______")));
		Lines.Add(FString::Printf(TEXT("%sANSWER THE PAGE — JOIN"), Sel(1)));
		Lines.Add(TEXT(""));
		Lines.Add(bTyping ? TEXT("Type the 6 numbers your host sees on their squad-room board   ·   ENTER save   ·   ESC cancel") : TEXT("↑ ↓ choose   ·   ENTER select   ·   ESC hang up"));
		if (const UHUSquadSubsystem* Sq = GetGameInstance()->GetSubsystem<UHUSquadSubsystem>()) Lines.Add(FString::Printf(TEXT("Searching on the %s."), *Sq->NetworkName()));
		break;
	case EHUMenuPanel::Squad:
		Title = TEXT("THE SQUAD");
		if (const UHUSquadSubsystem* Sq = GetGameInstance()->GetSubsystem<UHUSquadSubsystem>())
		{
			Lines.Add(Sq->Status.IsEmpty() ? FString(TEXT("...")) : Sq->Status);
			Lines.Add(TEXT(""));
			Lines.Add(Sq->State == EHUSquadState::Failed ? TEXT("ESC back to the desk") : TEXT("Hold the line..."));
		}
		break;
	case EHUMenuPanel::Quit:
		Title = TEXT("CAR KEYS — GO HOME?");
		Lines = { TEXT("The city will still be here tomorrow night."), TEXT(""), TEXT("ENTER / click go home   ·   ESC stay") };
		break;
	case EHUMenuPanel::Missing:
		Title = TEXT("THE PRECINCT ISN'T BUILT YET");
		Lines = { FString::Printf(TEXT("There's no level called %s in the project."), *MissingLevel),
			TEXT("Make a level with that name and drop in the matching HU actor (Briefing Room / Crime Scene), or run Tools/CreateLevels.py."), TEXT(""), TEXT("ESC back") };
		break;
	default: break;
	}
}
