#include "HUSquadRoom.h"
#include "HUBuild.h"
#include "HUSquadSubsystem.h"
#include "HUCharacter.h"
#include "HUPlayerState.h"
#include "Components/InputComponent.h"
#include "HUCaseSubsystem.h"
#include "HUCaseGen.h"
#include "HUCaseBoard.h"
#include "Kismet/GameplayStatics.h"
#include "HUMotorPoolDoor.h"
#include "HUSecurityDesk.h"
#include "HUCaseDesk.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "HUInteractComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

using namespace HUBuild;

// the room: X from -700 to 700, Y from -500 to 500. The board is on the +X wall, the roll-call door on the -X wall.
static const float RX = 700.f, RY = 500.f, RH = 340.f;
// the precinct (the base, a level of its own): a full 32 x 24 m station, with a central bullpen and four wings
static const float PX = 1600.f, PY = 1200.f;
// The four squad desks are spaced to leave a broad central circulation aisle.
static const FVector2D DeskSpots[] = { FVector2D(-700, -380), FVector2D(-250, -380), FVector2D(-700, 380), FVector2D(-250, 380) };

static UTextRenderComponent* MakeText(AActor* Owner, USceneComponent* Root, const TCHAR* Name, FVector Pos, float Yaw, float Size, FColor Col)
{
	UTextRenderComponent* T = Owner->CreateDefaultSubobject<UTextRenderComponent>(Name);
	T->SetupAttachment(Root);
	T->SetRelativeLocation(Pos);
	T->SetRelativeRotation(FRotator(0, Yaw, 0));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Col);
	return T;
}

AHUSquadRoom::AHUSquadRoom()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorF(TEXT("/Game/Art/Materials/Precinct/MI_HU_Linoleum.MI_HU_Linoleum"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallF(TEXT("/Game/Art/Materials/Precinct/MI_HU_Plaster.MI_HU_Plaster"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WoodF(TEXT("/Game/Art/Materials/Precinct/MI_HU_Walnut.MI_HU_Walnut"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PropF(TEXT("/Game/Art/Materials/Props/M_HU_PropWorldAlignedSurface.M_HU_PropWorldAlignedSurface"));
	FloorSurface = FloorF.Object;
	WallSurface = WallF.Object;
	WoodSurface = WoodF.Object;
	PropSurfaceParent = PropF.Object;
	VisualGrade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("VisualGrade"));
	VisualGrade->SetupAttachment(RootComponent);
	VisualGrade->bUnbound = true;
	VisualGrade->BlendWeight = 1.f;
	FPostProcessSettings& Grade = VisualGrade->Settings;
	Grade.bOverride_ColorSaturation = true;
	Grade.ColorSaturation = FVector4(0.92f, 0.96f, 1.02f, 1.f);
	Grade.bOverride_ColorContrast = true;
	Grade.ColorContrast = FVector4(1.05f, 1.05f, 1.05f, 1.f);
	Grade.bOverride_VignetteIntensity = true;
	Grade.VignetteIntensity = 0.14f;
	Grade.bOverride_AutoExposureBias = true;
	Grade.AutoExposureBias = -0.3f;
	// text on the board faces into the room (-X)
	BoardTitle = MakeText(this, Root, TEXT("BoardTitle"), FVector(RX - 8, 0, 270), 180, 24, FColor(235, 235, 225));
	BoardCode = MakeText(this, Root, TEXT("BoardCode"), FVector(RX - 8, 0, 215), 180, 46, FColor(255, 225, 120));
	BoardRoster = MakeText(this, Root, TEXT("BoardRoster"), FVector(RX - 8, 0, 125), 180, 15, FColor(225, 230, 235));
	DoorSign = MakeText(this, Root, TEXT("DoorSign"), FVector(-RX + 8, 0, 285), 0, 20, FColor(255, 181, 71));
	BoardTitle->SetText(FText::FromString(TEXT("ROLL CALL")));
	// the case-selection table by the door: a laptop for where, a folder for how hard
	LaptopScreen = MakeText(this, Root, TEXT("LaptopScreen"), FVector(-439.f, -360.f, 88.f), 0, 3.2f, FColor(150, 220, 255));
	FolderLabel = MakeText(this, Root, TEXT("FolderLabel"), FVector(-398.f, -295.f, 92.f), 0, 5.f, FColor(255, 225, 120));
	bReplicates = true;
	bAlwaysRelevant = true;
	DoorSign->SetText(FText::FromString(TEXT("ROLL CALL  >  BRIEFING")));
}

UHUSquadSubsystem* AHUSquadRoom::Squad() const { return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUSquadSubsystem>() : nullptr; }
bool AHUSquadRoom::IsHost() const { return GetNetMode() != NM_Client; }

void AHUSquadRoom::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks); BoardParts.Reset(); DoorParts.Reset(); LaptopParts.Reset(); FolderParts.Reset(); ComputerParts.Reset(); CoffeeParts.Reset(); ClockParts.Reset(); CounterParts.Reset(); TrayParts.Reset(); ShiftParts.Reset();
	for (UTextRenderComponent* Sign : RoomSigns) if (Sign) Sign->DestroyComponent();
	RoomSigns.Reset();
	USceneComponent* R = RootComponent;
	UMaterialInterface* TunedFloorSurface = this->FloorSurface;
	UMaterialInterface* TunedWallSurface = this->WallSurface;
	UMaterialInterface* TunedWoodSurface = this->WoodSurface;
	auto TunedSurface = [this](UMaterialInterface* Source, FLinearColor Tint) -> UMaterialInterface*
	{
		if (!Source) return nullptr;
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Source, this);
		if (Dynamic) Dynamic->SetVectorParameterValue(TEXT("SurfaceTint"), Tint);
		return Dynamic ? Dynamic : Source;
	};
	TunedFloorSurface = TunedSurface(TunedFloorSurface, FLinearColor(1.f, 0.98f, 0.92f));
	TunedWallSurface = TunedSurface(TunedWallSurface, FLinearColor(0.94f, 0.92f, 0.85f));
	TunedWoodSurface = TunedSurface(TunedWoodSurface, FLinearColor(1.22f, 1.12f, 0.96f));
	const FLinearColor Floor(0.16f, 0.12f, 0.08f), Wall(0.32f, 0.36f, 0.33f), Trim(0.12f, 0.08f, 0.05f), Desk(0.25f, 0.14f, 0.06f), Steel(0.3f, 0.32f, 0.34f);
	const float W = bPrecinct ? PX : RX, D = bPrecinct ? PY : RY;
	auto TexturedBlock = [this, R](FVector Pos, FVector Size, FLinearColor Color, UMaterialInterface* Surface, bool bCollide = true)
	{
		UStaticMeshComponent* Part = Block(this, R, Blocks, EShape::Cube, Pos, Size, Color, FRotator::ZeroRotator, bCollide);
		if (Part && Surface) Part->SetMaterial(0, Surface);
		return Part;
	};
	auto ApplyPropSurface = [this](UStaticMeshComponent* Component, const TCHAR* BaseColor, FLinearColor Tint, float Tile, float Roughness)
	{
		if (!Component) return;
		if (UMaterialInterface* Surface = HUBuild::PropSurface(this, PropSurfaceParent, BaseColor, Tint, Tile, Roughness)) Component->SetMaterial(0, Surface);
	};
	auto Detail = [this, R](FVector Pos, FVector Size, FLinearColor Color, FRotator Rot = FRotator::ZeroRotator)
	{
		return Block(this, R, Blocks, EShape::Cube, Pos, Size, Color, Rot, false);
	};
	auto RoomSign = [this, R](const FString& Label, FVector Pos, float Yaw, float Size = 18.f)
	{
		UTextRenderComponent* Sign = NewObject<UTextRenderComponent>(this);
		Sign->SetupAttachment(R);
		Sign->SetRelativeLocation(Pos);
		Sign->SetRelativeRotation(FRotator(0, Yaw, 0));
		Sign->SetHorizontalAlignment(EHTA_Center);
		Sign->SetVerticalAlignment(EVRTA_TextCenter);
		Sign->SetWorldSize(Size);
		Sign->SetTextRenderColor(FColor(236, 206, 132));
		Sign->SetText(FText::FromString(Label));
		Sign->RegisterComponent();
		RoomSigns.Add(Sign);
	};
	auto PartitionAtX = [this, &Detail, &TexturedBlock, R, TunedWallSurface, TunedWoodSurface](float X, float Y0, float Y1, TArray<float> Doors)
	{
		const float DoorWidth = 156.f, DoorHeight = 226.f, WallHeight = 280.f;
		Doors.Sort();
		auto WallSegment = [&](float A, float B, float Z0, float Z1)
		{
			if (B - A < 1.f || Z1 - Z0 < 1.f) return;
			TexturedBlock(FVector(X, (A + B) * 0.5f, (Z0 + Z1) * 0.5f), FVector(14.f, B - A, Z1 - Z0), FLinearColor::White, TunedWallSurface);
		};
		float Cursor = Y0;
		for (int32 Index = 0; Index < Doors.Num(); ++Index)
		{
			const float Center = Doors[Index], Half = DoorWidth * 0.5f;
			WallSegment(Cursor, Center - Half, 0, WallHeight);
			WallSegment(Center - Half, Center + Half, DoorHeight, WallHeight);
			for (float Side : { -1.f, 1.f }) Detail(FVector(X, Center + Side * (Half - 3.f), DoorHeight * 0.5f), FVector(22.f, 9.f, DoorHeight), FLinearColor(0.25f, 0.16f, 0.08f));
			Detail(FVector(X, Center, DoorHeight - 4.f), FVector(22.f, DoorWidth, 8.f), FLinearColor(0.25f, 0.16f, 0.08f));
			const float Hinge = (Index % 2 == 0) ? 1.f : -1.f;
			const FRotator OpenYaw(0, Hinge * 68.f, 0);
			const FVector LeafAt(X + 37.f, Center + Hinge * (Half - 37.f), DoorHeight * 0.5f);
			UStaticMeshComponent* Leaf = Block(this, R, Blocks, EShape::Cube, LeafAt, FVector(5.f, 76.f, 216.f), FLinearColor(0.27f, 0.17f, 0.085f), OpenYaw, false);
			if (Leaf && TunedWoodSurface) Leaf->SetMaterial(0, TunedWoodSurface);
			Detail(LeafAt + OpenYaw.RotateVector(FVector(3.f, 0, 0)), FVector(1.5f, 48.f, 106.f), FLinearColor(0.18f, 0.11f, 0.055f), OpenYaw);
			Detail(LeafAt + OpenYaw.RotateVector(FVector(4.f, -Hinge * 25.f, 0)), FVector(2.f, 3.f, 3.f), FLinearColor(0.58f, 0.47f, 0.28f), OpenYaw);
			Cursor = Center + Half;
		}
		WallSegment(Cursor, Y1, 0, WallHeight);
	};
	auto PartitionAtY = [this, &Detail, &TexturedBlock, R, TunedWallSurface, TunedWoodSurface](float Y, float X0, float X1, TArray<float> Doors)
	{
		const float DoorWidth = 156.f, DoorHeight = 226.f, WallHeight = 280.f;
		Doors.Sort();
		auto WallSegment = [&](float A, float B, float Z0, float Z1)
		{
			if (B - A < 1.f || Z1 - Z0 < 1.f) return;
			TexturedBlock(FVector((A + B) * 0.5f, Y, (Z0 + Z1) * 0.5f), FVector(B - A, 14.f, Z1 - Z0), FLinearColor::White, TunedWallSurface);
		};
		float Cursor = X0;
		for (int32 Index = 0; Index < Doors.Num(); ++Index)
		{
			const float Center = Doors[Index], Half = DoorWidth * 0.5f;
			WallSegment(Cursor, Center - Half, 0, WallHeight);
			WallSegment(Center - Half, Center + Half, DoorHeight, WallHeight);
			for (float Side : { -1.f, 1.f }) Detail(FVector(Center + Side * (Half - 3.f), Y, DoorHeight * 0.5f), FVector(9.f, 22.f, DoorHeight), FLinearColor(0.25f, 0.16f, 0.08f));
			Detail(FVector(Center, Y, DoorHeight - 4.f), FVector(DoorWidth, 22.f, 8.f), FLinearColor(0.25f, 0.16f, 0.08f));
			const float Hinge = (Index % 2 == 0) ? 1.f : -1.f;
			const FRotator OpenYaw(0, -Hinge * 68.f, 0);
			const FVector LeafAt(Center + Hinge * (Half - 37.f), Y - 37.f, DoorHeight * 0.5f);
			UStaticMeshComponent* Leaf = Block(this, R, Blocks, EShape::Cube, LeafAt, FVector(76.f, 5.f, 216.f), FLinearColor(0.27f, 0.17f, 0.085f), OpenYaw, false);
			if (Leaf && TunedWoodSurface) Leaf->SetMaterial(0, TunedWoodSurface);
			Detail(LeafAt + OpenYaw.RotateVector(FVector(0, -3.f, 0)), FVector(48.f, 1.5f, 106.f), FLinearColor(0.18f, 0.11f, 0.055f), OpenYaw);
			Detail(LeafAt + OpenYaw.RotateVector(FVector(Hinge * 25.f, -4.f, 0)), FVector(3.f, 2.f, 3.f), FLinearColor(0.58f, 0.47f, 0.28f), OpenYaw);
			Cursor = Center + Half;
		}
		WallSegment(Cursor, X1, 0, WallHeight);
	};
	TexturedBlock(FVector(0, 0, -5), FVector(W * 2, D * 2, 10), Floor, TunedFloorSurface);
	TexturedBlock(FVector(0, 0, RH + 5), FVector(W * 2, D * 2, 10), Wall * 0.6f, TunedWallSurface);
	for (float S : { -1.f, 1.f })
	{
		TexturedBlock(FVector(S * (W + 5), 0, RH / 2), FVector(10, D * 2, RH), Wall, TunedWallSurface);
		TexturedBlock(FVector(0, S * (D + 5), RH / 2), FVector(W * 2, 10, RH), Wall, TunedWallSurface);
		TexturedBlock(FVector(0, S * (D - 2), 45), FVector(W * 2, 4, 90), Trim, TunedWoodSurface, false);
	}
	const TArray<float> FixtureXs = bPrecinct ? TArray<float>{ -0.75f * W, -0.25f * W, 0.25f * W, 0.75f * W } : TArray<float>{ -0.5f * W, 0.f, 0.5f * W };
	const TArray<float> FixtureYs = bPrecinct ? TArray<float>{ -0.67f * D, 0.f, 0.67f * D } : TArray<float>{ -0.45f * D, 0.45f * D };
	for (float X : FixtureXs) for (float Y : FixtureYs)
	{
		Detail(FVector(X, Y, RH - 7), FVector(150, 38, 6), FLinearColor(0.2f, 0.21f, 0.2f)); // shallow metal troffer
		Detail(FVector(X, Y, RH - 3.6f), FVector(132, 27, 1.2f), FLinearColor(1.05f, 0.98f, 0.84f)); // aged fluorescent diffuser
		for (float Offset : { -7.f, 7.f })
			Detail(FVector(X, Y + Offset, RH - 2.8f), FVector(116, 0.7f, 0.3f), FLinearColor(1.12f, 1.05f, 0.92f));
	}
	// Fine, irregularly spaced worn tracks catch the light along the regular foot paths.
	if (bPrecinct)
	{
		const FLinearColor ScuffA(0.19f, 0.15f, 0.11f), ScuffB(0.22f, 0.18f, 0.14f);
		const FVector WalkPoints[] = { FVector(-690, -260, 0.18f), FVector(-540, -260, 0.18f), FVector(-390, -260, 0.18f), FVector(-240, -260, 0.18f), FVector(-90, -260, 0.18f), FVector(60, -260, 0.18f), FVector(210, -260, 0.18f), FVector(360, -260, 0.18f), FVector(510, -260, 0.18f), FVector(650, -260, 0.18f) };
		for (int32 I = 0; I < UE_ARRAY_COUNT(WalkPoints); ++I)
		{
			const FVector P = WalkPoints[I];
			Detail(P, FVector(44.f + (I % 3) * 9.f, 0.6f + (I % 2) * 0.3f, 0.08f), (I % 2) ? ScuffA : ScuffB, FRotator(0, (I * 19) % 34 - 17, 0));
			Detail(P + FVector(12, 9 + (I % 4) * 2, 0), FVector(18.f + (I % 3) * 5.f, 0.35f, 0.06f), ScuffA, FRotator(0, -12 + I * 3, 0));
		}
	}

	if (!bPrecinct)
	{
		// ======== ROLL CALL: just where the squad clocks in for the shift ========
		// the chalkboard: the squad code and who's punched in
		BoardParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(RX - 4, 0, 200), FVector(4, 560, 190), FLinearColor(0.03f, 0.06f, 0.04f)));
		BoardParts.Add(TexturedBlock(FVector(RX - 5, 0, 200), FVector(6, 580, 210), Trim, TunedWoodSurface));
		BoardParts.Add(TexturedBlock(FVector(RX - 12, 0, 96), FVector(14, 560, 4), Trim, TunedWoodSurface));
		// the roll-call door to the briefing: double doors with a glass panel
		for (float S : { -1.f, 1.f })
		{
			DoorParts.Add(TexturedBlock(FVector(-RX + 4, S * 46, 120), FVector(6, 90, 240), FLinearColor(0.2f, 0.12f, 0.05f), TunedWoodSurface));
			DoorParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 7.5f, S * 46, 170), FVector(1, 50, 60), FLinearColor(0.6f, 0.75f, 0.85f)));
			DoorParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 9, S * 10, 115), FVector(4, 3, 18), FLinearColor(0.75f, 0.6f, 0.2f)));
		}
		// the time clock by the doors: punch in when you're ready
		ClockParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 14, 260, 140), FVector(18, 36, 46), FLinearColor(0.55f, 0.5f, 0.38f)));
		ClockParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-RX + 23.5f, 260, 152), FVector(22, 22, 1), FLinearColor(0.9f, 0.88f, 0.8f), FRotator(0, 0, 90)));
		ClockParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 24, 260, 126), FVector(2, 14, 3), FLinearColor(0.05f, 0.05f, 0.05f)));
		ClockParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 8, 305, 140), FVector(6, 30, 60), FLinearColor(0.35f, 0.25f, 0.12f))); // the card rack
		// the case-selection table: a laptop (where) and a case folder (how hard) - the host's picks for the shift
		TexturedBlock(FVector(-420, -330, 72), FVector(90, 150, 6), Desk, TunedWoodSurface);
		for (float DX : { -40.f, 40.f }) for (float DY : { -68.f, 68.f }) TexturedBlock(FVector(-420 + DX, -330 + DY, 35), FVector(5, 5, 70), Desk * 0.7f, TunedWoodSurface);
		LaptopParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-428, -360, 76), FVector(24, 34, 2), FLinearColor(0.12f, 0.12f, 0.13f)));
		LaptopParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-441, -360, 88), FVector(1.5f, 34, 22), FLinearColor(0.12f, 0.12f, 0.13f)));
		LaptopParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-440.1f, -360, 88), FVector(0.2f, 31, 19), FLinearColor(0.02f, 0.06f, 0.12f)));
		UStaticMeshComponent* CaseFolder = Block(this, R, Blocks, EShape::Cube, FVector(-405, -295, 75.8f), FVector(24, 32, 1.5f), FLinearColor(0.75f, 0.6f, 0.32f), FRotator(0, 8, 0));
		ApplyPropSurface(CaseFolder, TEXT("/Game/Art/Materials/Textures/M_ManilaFolder_BaseColor.M_ManilaFolder_BaseColor"), FLinearColor::White, 24.f, 1.f);
		FolderParts.Add(CaseFolder);
		UStaticMeshComponent* FolderTab = Block(this, R, Blocks, EShape::Cube, FVector(-398, -295, 84), FVector(1, 26, 14), FLinearColor(0.85f, 0.82f, 0.72f));
		ApplyPropSurface(FolderTab, TEXT("/Game/Art/Materials/Textures/M_ManilaFolder_BaseColor.M_ManilaFolder_BaseColor"), FLinearColor::White, 24.f, 1.f);
		FolderParts.Add(FolderTab);
		return;
	}

	// ======== THE PRECINCT: the squad's base, the same whatever the case - where it gets solved ========
	// An open central bullpen feeds distinct west and east wings. The squad desks sit clear
	// of the dividers, while each room retains a wide doorway and visible open door leaf.
	PartitionAtX(-950.f, -1100.f, 1100.f, { -750.f, 750.f }); // records / captain wing
	PartitionAtY(0.f, -1500.f, -950.f, { -1200.f });          // records to captain's office
	PartitionAtX(350.f, -1100.f, 1100.f, { -750.f, 0.f, 750.f }); // evidence / interview / booking wing
	PartitionAtY(-350.f, 350.f, 1500.f, { 1100.f });           // evidence to interview suite
	PartitionAtY(350.f, 350.f, 1500.f, { 700.f });             // interview suite to booking
	PartitionAtY(0.f, 350.f, 1500.f, { 1200.f });              // two separate interview rooms
	PartitionAtX(1000.f, -1100.f, -350.f, { -750.f });         // processing lab to secure evidence store
	RoomSign(TEXT("RECORDS / ARCHIVES"), FVector(-938, -500, 255), 180, 17.f);
	RoomSign(TEXT("CAPTAIN'S OFFICE"), FVector(-938, 500, 255), 180, 17.f);
	RoomSign(TEXT("EVIDENCE LAB"), FVector(338, -500, 255), 180, 17.f);
	RoomSign(TEXT("INTERVIEW 1"), FVector(800, -338, 255), 90, 17.f);
	RoomSign(TEXT("INTERVIEW 2"), FVector(800, 338, 255), 270, 17.f);
	RoomSign(TEXT("BOOKING / HOLDING"), FVector(338, 800, 255), 180, 17.f);
	RoomSign(TEXT("RECORDS"), FVector(-1200, -12, 255), 90, 16.f);
	RoomSign(TEXT("CAPTAIN"), FVector(-1200, 12, 255), 270, 16.f);
	RoomSign(TEXT("EVIDENCE CONTROL"), FVector(988, -600, 255), 180, 15.f);
	RoomSign(TEXT("INTAKE"), FVector(700, 338, 255), 270, 16.f);
	// Captain's office: private desk, guest chair, and a low bookcase against the west wall.
	TexturedBlock(FVector(-1060, 760, 76), FVector(170, 90, 6), Desk, TunedWoodSurface);
	for (float DX : { -72.f, 72.f }) for (float DY : { -36.f, 36.f })
		TexturedBlock(FVector(-1060 + DX, 760 + DY, 37), FVector(6, 6, 74), Desk * 0.7f, TunedWoodSurface);
	Detail(FVector(-1140, 760, 48), FVector(42, 46, 9), FLinearColor(0.08f, 0.1f, 0.12f));
	Detail(FVector(-1156, 760, 75), FVector(8, 44, 48), FLinearColor(0.07f, 0.09f, 0.11f));
	Block(this, R, Blocks, EShape::Cube, FVector(-1370, 850, 108), FVector(48, 185, 216), FLinearColor(0.18f, 0.13f, 0.08f));
	for (int32 Shelf = 0; Shelf < 4; ++Shelf)
	{
		const float Z = 42.f + Shelf * 48.f;
		Detail(FVector(-1344, 850, Z), FVector(3, 172, 4), FLinearColor(0.33f, 0.23f, 0.13f));
		for (int32 Book = 0; Book < 8; ++Book)
			Detail(FVector(-1338, 778.f + Book * 20.f, Z + 17.f), FVector(10, 13, 28 + (Book % 3) * 4),
				FLinearColor(0.2f + (Book % 3) * 0.08f, 0.12f + (Book % 2) * 0.06f, 0.08f + (Book % 4) * 0.035f));
	}
	// Records room: three locked filing cabinets and two shelf runs keep the archive distinct from the bullpen.
	for (float CabinetY : { -900.f, -770.f, -640.f })
	{
		const FVector Cabinet(-1020.f, CabinetY, 0);
		const FLinearColor SteelBody(0.25f, 0.28f, 0.29f), SteelEdge(0.36f, 0.38f, 0.37f), Recess(0.17f, 0.19f, 0.19f);
		Block(this, R, Blocks, EShape::Cube, Cabinet, FVector(82, 56, 118), SteelBody);
		Detail(Cabinet + FVector(0, 0, 60), FVector(86, 60, 3), SteelEdge);
		for (int32 Drawer = 0; Drawer < 4; ++Drawer)
		{
			const float Z = 16.f + Drawer * 27.f;
			Detail(Cabinet + FVector(-42.f, 0, Z), FVector(2, 52, 25), SteelEdge);
			Detail(Cabinet + FVector(-43.2f, 0, Z), FVector(0.6f, 34, 1.4f), Recess);
			Detail(Cabinet + FVector(-44.f, 0, Z + 1.f), FVector(1.f, 10, 2.f), FLinearColor(0.58f, 0.53f, 0.39f));
			Detail(Cabinet + FVector(-44.f, -14.f, Z + 7.f), FVector(1.f, 11, 7.f), FLinearColor(0.68f, 0.66f, 0.57f));
		}
	}
	for (float ShelfY : { -930.f, -560.f })
	{
		Block(this, R, Blocks, EShape::Cube, FVector(-1390, ShelfY, 115), FVector(48, 180, 230), FLinearColor(0.21f, 0.15f, 0.1f));
		for (int32 Shelf = 0; Shelf < 4; ++Shelf)
		{
			const float Z = 45.f + Shelf * 48.f;
			Detail(FVector(-1364, ShelfY, Z), FVector(3, 168, 4), FLinearColor(0.34f, 0.25f, 0.14f));
			for (int32 File = 0; File < 5; ++File)
				Detail(FVector(-1357, ShelfY - 66.f + File * 31.f, Z + 18.f), FVector(14, 24, 31),
					(File % 2) ? FLinearColor(0.53f, 0.42f, 0.25f) : FLinearColor(0.22f, 0.3f, 0.35f));
		}
	}
	// Evidence processing and secure storage: stainless workbenches, tray racks, and tagged evidence boxes.
	for (float BenchX : { 760.f, 1260.f })
	{
		Block(this, R, Blocks, EShape::Cube, FVector(BenchX, -760, 82), FVector(210, 100, 6), FLinearColor(0.36f, 0.38f, 0.37f));
		for (float DX : { -88.f, 88.f }) for (float DY : { -38.f, 38.f })
			Detail(FVector(BenchX + DX, -760 + DY, 42), FVector(5, 5, 80), FLinearColor(0.25f, 0.27f, 0.28f));
		for (int32 Tray = 0; Tray < 3; ++Tray)
		{
			Detail(FVector(BenchX - 54.f + Tray * 52.f, -760, 87.f), FVector(40, 62, 3), FLinearColor(0.23f, 0.27f, 0.28f));
			Detail(FVector(BenchX - 54.f + Tray * 52.f, -760, 89.f), FVector(28, 44, 1.f), FLinearColor(0.63f, 0.57f, 0.43f));
		}
	}
	for (int32 BoxIdx = 0; BoxIdx < 4; ++BoxIdx)
	{
		const FVector EvidenceBox(1330.f, -930.f + BoxIdx * 112.f, 50.f);
		Detail(EvidenceBox, FVector(55, 68, 72), (BoxIdx % 2) ? FLinearColor(0.31f, 0.35f, 0.32f) : FLinearColor(0.45f, 0.36f, 0.22f));
		Detail(EvidenceBox + FVector(-28.f, 0, 8.f), FVector(1.f, 30.f, 12.f), FLinearColor(0.79f, 0.74f, 0.56f));
	}
	// Two interview rooms, each with a table, opposing chairs, a dark observation window, and a wall clock.
	for (float InterviewY : { -170.f, 180.f })
	{
		Block(this, R, Blocks, EShape::Cube, FVector(950, InterviewY, 82), FVector(160, 78, 5), FLinearColor(0.36f, 0.34f, 0.29f));
		for (float DX : { -66.f, 66.f }) for (float DY : { -28.f, 28.f })
			Detail(FVector(950 + DX, InterviewY + DY, 42), FVector(5, 5, 80), FLinearColor(0.24f, 0.25f, 0.25f));
		for (float ChairX : { 830.f, 1070.f })
		{
			Detail(FVector(ChairX, InterviewY, 47), FVector(42, 42, 9), FLinearColor(0.09f, 0.1f, 0.11f));
			Detail(FVector(ChairX + (ChairX < 950 ? -16.f : 16.f), InterviewY, 75), FVector(8, 42, 48), FLinearColor(0.07f, 0.08f, 0.09f));
		}
		Detail(FVector(1390, InterviewY, 225), FVector(4, 150, 64), FLinearColor(0.08f, 0.14f, 0.16f)); // observation glass
		Detail(FVector(1386, InterviewY, 225), FVector(2, 158, 72), FLinearColor(0.21f, 0.23f, 0.22f)); // dark surround
		Detail(FVector(1350, InterviewY, 250), FVector(8, 8, 8), FLinearColor(0.24f, 0.25f, 0.24f)); // simple wall clock
	}
	// the squad's desks, pushed together, each with a computer (the case archive, and the scene's security tapes)
	for (const FVector2D& DeskSpot : DeskSpots)
	{
		const float X = DeskSpot.X, Y = DeskSpot.Y;
		TexturedBlock(FVector(X, Y, 74), FVector(150, 90, 6), Desk, TunedWoodSurface);
		for (float DX : { -65.f, 65.f }) for (float DY : { -38.f, 38.f }) TexturedBlock(FVector(X + DX, Y + DY, 36), FVector(5, 5, 72), Desk * 0.7f, TunedWoodSurface);
		// Solid apron and shallow pencil drawers make each station read as a real desk.
		TexturedBlock(FVector(X - 35, Y, 62), FVector(6, 78, 18), Desk * 0.78f, TunedWoodSurface, false);
		for (int32 Drawer = 0; Drawer < 3; ++Drawer)
		{
			const float DrawerY = Y - 25.f + Drawer * 25.f;
			Detail(FVector(X - 38.5f, DrawerY, 64), FVector(1.5f, 21.f, 4.f), FLinearColor(0.31f, 0.18f, 0.085f));
			Detail(FVector(X - 39.6f, DrawerY, 64), FVector(1.f, 7.f, 1.2f), FLinearColor(0.42f, 0.36f, 0.22f));
		}
		// A few small finish breaks at the front edge suggest years of daily use.
		for (int32 Mark = 0; Mark < 5; ++Mark)
			Detail(FVector(X - 73.f + Mark * 28.f, Y - 45.6f, 77.05f), FVector(8.f + (Mark % 2) * 7.f, 0.35f, 0.12f), FLinearColor(0.36f, 0.23f, 0.12f), FRotator(0, 0, (Mark % 3) - 1));
		// Upholstered chair, steel column, five-spoke base, and low-profile casters.
		const FVector Chair(X - 132.f, Y, 0);
		Detail(Chair + FVector(0, 0, 48), FVector(44, 46, 10), FLinearColor(0.07f, 0.09f, 0.11f));
		Detail(Chair + FVector(-17, 0, 78), FVector(9, 45, 52), FLinearColor(0.06f, 0.08f, 0.1f), FRotator(0, 0, -4));
		Detail(Chair + FVector(0, 0, 24), FVector(4, 4, 36), FLinearColor(0.25f, 0.27f, 0.28f));
		for (int32 Spoke = 0; Spoke < 5; ++Spoke)
		{
			const float A = Spoke * 72.f;
			const FVector Dir(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0);
			Detail(Chair + FVector(Dir.X * 18, Dir.Y * 18, 8), FVector(30, 4, 3), FLinearColor(0.22f, 0.24f, 0.25f), FRotator(0, A, 0));
			Detail(Chair + FVector(Dir.X * 34, Dir.Y * 34, 5), FVector(7, 7, 7), FLinearColor(0.09f, 0.1f, 0.11f));
		}
		Block(this, R, Blocks, EShape::Cube, FVector(X - 10, Y + (Y > 0 ? 70.f : -70.f), 45), FVector(45, 45, 6), Steel);
		const FLinearColor Plastic(0.72f, 0.67f, 0.55f);
		const FVector Monitor(X + 30, Y, 90);
		const TCHAR* ComputerTexture = TEXT("/Game/Art/Materials/Textures/M_ComputerPlastic_BaseColor.M_ComputerPlastic_BaseColor");
		UStaticMeshComponent* MonitorShell = TexturedBlock(Monitor + FVector(-5, 0, 0), FVector(20, 31, 24), Plastic * 0.93f, nullptr);
		ApplyPropSurface(MonitorShell, ComputerTexture, FLinearColor::White, 45.f, 0.82f);
		ComputerParts.Add(MonitorShell); // deep CRT rear housing
		UStaticMeshComponent* FrontBezel = TexturedBlock(Monitor + FVector(6.f, 0, 0), FVector(12, 35, 27), Plastic * 0.97f, nullptr, false);
		ApplyPropSurface(FrontBezel, ComputerTexture, FLinearColor(0.97f, 0.96f, 0.91f), 45.f, 0.78f);
		ComputerParts.Add(FrontBezel); // projecting bezel, with visible depth
		UStaticMeshComponent* Bezel = TexturedBlock(Monitor + FVector(12.2f, 0, 0), FVector(1.2f, 29, 21), Plastic * 0.88f, nullptr, false);
		ApplyPropSurface(Bezel, ComputerTexture, FLinearColor(0.92f, 0.92f, 0.92f), 45.f, 0.82f);
		ComputerParts.Add(Bezel); // inset glass and thick outer frame
		ComputerParts.Add(Block(this, R, Blocks, EShape::Cube, Monitor + FVector(13.f, 0, 1), FVector(0.6f, 25, 17), FLinearColor(0.035f, 0.10f, 0.13f), FRotator(0, 0, 2), false));
		Detail(Monitor + FVector(13.36f, 0, -6.7f), FVector(0.25f, 23.8f, 1.1f), FLinearColor(0.025f, 0.12f, 0.16f)); // taskbar
		Detail(Monitor + FVector(13.38f, 2.5f, 2.3f), FVector(0.25f, 13.f, 7.f), FLinearColor(0.1f, 0.2f, 0.24f)); // open records window
		for (int32 Line = 0; Line < 3; ++Line)
			Detail(Monitor + FVector(13.55f, 1.2f, 4.2f - Line * 1.8f), FVector(0.18f, 9.f - Line * 1.5f, 0.22f), FLinearColor(0.34f, 0.43f, 0.42f));
		for (int32 Icon = 0; Icon < 3; ++Icon)
		{
			Detail(Monitor + FVector(13.5f, -8.8f, 5.8f - Icon * 5.3f), FVector(0.22f, 2.4f, 2.5f), FLinearColor(0.36f, 0.43f, 0.28f));
			Detail(Monitor + FVector(13.56f, -8.8f, 4.2f - Icon * 5.3f), FVector(0.18f, 1.6f, 0.45f), FLinearColor(0.64f, 0.68f, 0.56f));
		}
		// Brand plaque, power LED, and ventilation ribs anchor the silhouette in the 2005 era.
		Detail(Monitor + FVector(12.95f, 0, -10.9f), FVector(0.5f, 8.f, 1.5f), FLinearColor(0.33f, 0.3f, 0.24f));
		Detail(Monitor + FVector(13.f, 10.3f, -10.9f), FVector(0.7f, 1.2f, 1.2f), FLinearColor(0.35f, 0.55f, 0.29f));
		for (int32 Smudge = 0; Smudge < 3; ++Smudge)
			Block(this, R, Blocks, EShape::Sphere, Monitor + FVector(13.35f, 8.3f - Smudge * 1.7f, -12.3f + (Smudge % 2) * 0.65f),
				FVector(0.22f, 1.8f, 0.5f), FLinearColor(0.43f, 0.38f, 0.28f), FRotator::ZeroRotator, false); // finger wear around the controls
		for (int32 Vent = 0; Vent < 6; ++Vent)
			Detail(Monitor + FVector(-15.f, -11.f + Vent * 4.4f, -2.f), FVector(0.5f, 2.6f, 0.7f), FLinearColor(0.34f, 0.31f, 0.25f));
		UStaticMeshComponent* Pedestal = TexturedBlock(Monitor + FVector(0, 0, -16.5f), FVector(7, 8, 7), Plastic * 0.85f, nullptr);
		ApplyPropSurface(Pedestal, ComputerTexture, FLinearColor(0.85f, 0.85f, 0.85f), 45.f, 0.82f);
		ComputerParts.Add(Pedestal); // pedestal
		UStaticMeshComponent* Foot = TexturedBlock(Monitor + FVector(0, 0, -23), FVector(17, 20, 2), Plastic, nullptr);
		ApplyPropSurface(Foot, ComputerTexture, FLinearColor::White, 45.f, 0.82f);
		ComputerParts.Add(Foot); // weighted foot
		UStaticMeshComponent* Keyboard = TexturedBlock(FVector(X - 10, Y, 78), FVector(13, 30, 1.5f), FLinearColor(0.58f, 0.54f, 0.45f), nullptr, false);
		ApplyPropSurface(Keyboard, ComputerTexture, FLinearColor(0.82f, 0.78f, 0.68f), 45.f, 0.82f);
		ComputerParts.Add(Keyboard);
		for (int32 Row = 0; Row < 4; Row++) for (int32 Key = 0; Key < 10; Key++)
			Detail(FVector(X - 14.5f + Row * 3.3f, Y - 13.f + Key * 2.8f, 79.1f), FVector(2.25f, 2.f, 0.55f), (Row == 3 && Key == 9) ? FLinearColor(0.56f, 0.52f, 0.44f) : FLinearColor(0.75f, 0.71f, 0.61f));
		Detail(FVector(X - 7.f, Y, 79.1f), FVector(2.2f, 10.f, 0.5f), FLinearColor(0.75f, 0.71f, 0.61f)); // space bar
		// A wired two-button mouse sits beside the keyboard.
		Block(this, R, Blocks, EShape::Sphere, FVector(X - 8.f, Y - 40.f, 79.f), FVector(7, 9, 3.5f), FLinearColor(0.65f, 0.61f, 0.52f), FRotator::ZeroRotator, false);
		Detail(FVector(X - 8.f, Y - 40.f, 80.7f), FVector(0.6f, 0.7f, 1.2f), FLinearColor(0.2f, 0.19f, 0.16f));
		Detail(FVector(X - 8.f, Y - 34.f, 79.4f), FVector(0.45f, 12.f, 0.35f), FLinearColor(0.21f, 0.2f, 0.18f)); // mouse cable
		UStaticMeshComponent* Tower = TexturedBlock(FVector(X + 53, Y + 31, 53), FVector(14, 12, 34), Plastic, nullptr);
		ApplyPropSurface(Tower, ComputerTexture, FLinearColor::White, 45.f, 0.82f);
		ComputerParts.Add(Tower); // beige PC tower
		Detail(FVector(X + 60.3f, Y + 31.f, 61.f), FVector(0.3f, 6.f, 1.8f), FLinearColor(0.17f, 0.17f, 0.16f)); // optical drive slot
		Detail(FVector(X + 60.5f, Y + 29.f, 61.f), FVector(0.4f, 1.f, 0.7f), FLinearColor(0.45f, 0.56f, 0.35f)); // activity LED
		Detail(FVector(X + 60.4f, Y + 36.f, 45.f), FVector(0.3f, 2.f, 2.f), FLinearColor(0.21f, 0.21f, 0.19f)); // power button
		for (int32 Vent = 0; Vent < 5; ++Vent)
			Detail(FVector(X + 53.f, Y + 37.2f, 51.f + Vent * 2.2f), FVector(4.f, 0.3f, 0.45f), FLinearColor(0.3f, 0.29f, 0.25f)); // cooling vents
	}
	// HOLDING: a barred cell in the far corner where the van's prisoners wait to be booked
	const float CY0 = PY - 200.f; // the bars, facing the room
	const FLinearColor Bars(0.35f, 0.37f, 0.4f);
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 150, PY - 100, 1), FVector(300, 200, 2), FLinearColor(0.2f, 0.2f, 0.21f), FRotator::ZeroRotator, false);
	for (float X = PX - 295.f; X <= PX - 5.f; X += 20.f)
		if (FMath::Abs(X - (PX - 150.f)) > 50.f) Block(this, R, Blocks, EShape::Cylinder, FVector(X, CY0, 120), FVector(3, 3, 240), Bars); // leave a proper cell-door opening
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 198.f, CY0, 120.f), FVector(8, 10, 240), Steel, FRotator::ZeroRotator, false); // door jamb
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 150.f, CY0, 239.f), FVector(96, 10, 7), Steel, FRotator::ZeroRotator, false);
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 162.f, CY0 + 32.f, 120.f), FVector(78, 4, 220), Bars, FRotator(0, 68, 0), false); // cell door resting open
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 126.f, CY0 + 33.f, 120.f), FVector(2, 4, 16), FLinearColor(0.62f, 0.53f, 0.34f), FRotator(0, 68, 0), false); // latch
	for (float Y = CY0 + 10.f; Y <= PY - 10.f; Y += 20.f) Block(this, R, Blocks, EShape::Cylinder, FVector(PX - 300, Y, 120), FVector(3, 3, 240), Bars); // and the side
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 150, CY0, 238), FVector(300, 6, 6), Bars);
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 120, PY - 30, 22), FVector(160, 40, 6), FLinearColor(0.3f, 0.3f, 0.3f));                       // the bench
	for (int32 i = 0; i < 6; i++) Block(this, R, Blocks, EShape::Cube, FVector(PX - 220, PY - 4, 120 + i * 20), FVector(80, 2, 1.5f), FLinearColor(0.85f, 0.85f, 0.8f), FRotator::ZeroRotator, false); // height chart
	// the BOOKING COUNTER beside it: a ledger, an ink pad, a camera on a tripod
	const FVector Book(PX - 420, PY - 100, 0);
	CounterParts.Add(TexturedBlock(Book + FVector(0, 0, 50), FVector(70, 170, 100), Desk, TunedWoodSurface));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cube, Book + FVector(0, -20, 101.5f), FVector(30, 40, 3), FLinearColor(0.12f, 0.15f, 0.3f)));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cube, Book + FVector(0, 30, 101.2f), FVector(14, 18, 2.4f), FLinearColor(0.05f, 0.05f, 0.05f)));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cube, Book + FVector(10, 70, 145), FVector(18, 22, 14), FLinearColor(0.08f, 0.08f, 0.08f)));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cylinder, Book + FVector(10, 70, 115), FVector(3, 3, 50), Steel));
	// Give the booking camera a real lens stack, shade, and a short cable down its mount.
	CounterParts.Add(Block(this, R, Blocks, EShape::Cylinder, Book + FVector(20, 70, 145), FVector(5, 11, 11), FLinearColor(0.19f, 0.2f, 0.2f), FRotator(90, 0, 0), false));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cylinder, Book + FVector(23, 70, 145), FVector(1.6f, 7.5f, 7.5f), FLinearColor(0.54f, 0.48f, 0.34f), FRotator(90, 0, 0), false));
	CounterParts.Add(Block(this, R, Blocks, EShape::Sphere, Book + FVector(24, 70, 145), FVector(0.7f, 5.6f, 5.6f), FLinearColor(0.025f, 0.09f, 0.12f), FRotator::ZeroRotator, false));
	for (int32 Vent = 0; Vent < 4; ++Vent)
		CounterParts.Add(Block(this, R, Blocks, EShape::Cube, Book + FVector(5, 80 + Vent * 3, 145), FVector(8, 0.8f, 0.8f), FLinearColor(0.22f, 0.22f, 0.21f), FRotator::ZeroRotator, false));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cube, Book + FVector(8, 70, 88), FVector(1, 3, 28), FLinearColor(0.08f, 0.08f, 0.075f), FRotator::ZeroRotator, false)); // power lead
	// the CAPTAIN'S IN-TRAY: where a finished case goes
	const FVector Tray(PX - 250, -PY + 200, 0);
	TrayParts.Add(TexturedBlock(Tray + FVector(0, 0, 74), FVector(110, 120, 6), Desk, TunedWoodSurface));
	for (float DX : { -48.f, 48.f }) for (float DY : { -52.f, 52.f }) TexturedBlock(Tray + FVector(DX, DY, 36), FVector(5, 5, 72), Desk * 0.7f, TunedWoodSurface);
	TrayParts.Add(Block(this, R, Blocks, EShape::Cube, Tray + FVector(0, 0, 82), FVector(36, 46, 8), FLinearColor(0.4f, 0.3f, 0.15f)));
	TrayParts.Add(Block(this, R, Blocks, EShape::Cube, Tray + FVector(0, 0, 85), FVector(30, 40, 3), FLinearColor(0.75f, 0.6f, 0.32f)));
	// the coffee pot
	TexturedBlock(FVector(-PX + 100, PY - 60, 45), FVector(120, 60, 90), Desk, TunedWoodSurface);
	CoffeeParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-PX + 100, PY - 60, 105), FVector(16, 16, 22), FLinearColor(0.05f, 0.05f, 0.05f)));
	CoffeeParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-PX + 130, PY - 60, 95), FVector(9, 9, 10), FLinearColor(0.85f, 0.85f, 0.8f)));
	// the door back to ROLL CALL (the next shift), on the +X wall: once the case is in
	for (float S : { -1.f, 1.f })
	{
		ShiftParts.Add(TexturedBlock(FVector(PX - 4, -150 + S * 46, 120), FVector(6, 90, 240), FLinearColor(0.2f, 0.12f, 0.05f), TunedWoodSurface));
		ShiftParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(PX - 7.5f, -150 + S * 46, 170), FVector(1, 50, 60), FLinearColor(0.6f, 0.75f, 0.85f)));
	}
}

void AHUSquadRoom::BeginPlay()
{
	Super::BeginPlay();
	UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	auto Lights = [this](float W, float D)
	{
		const TArray<float> LightXs = bPrecinct ? TArray<float>{ -0.75f * W, -0.25f * W, 0.25f * W, 0.75f * W } : TArray<float>{ -0.5f * W, 0.f, 0.5f * W };
		const TArray<float> LightYs = bPrecinct ? TArray<float>{ -0.67f * D, 0.f, 0.67f * D } : TArray<float>{ -0.45f * D, 0.45f * D };
		for (float X : LightXs) for (float Y : LightYs)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(RootComponent);
			L->SetRelativeLocation(FVector(X, Y, RH - 50));
			L->SetIntensity(bPrecinct ? 3700.f : 4300.f);
			L->SetAttenuationRadius(bPrecinct ? 1060.f : 1080.f);
			L->SetLightColor(FLinearColor(1.0f, 0.94f, 0.82f)); // neutral aged fluorescent light
			L->RegisterComponent();
		}
	};
	if (!bPrecinct)
	{
		// ROLL CALL: a new shift. A case still open here was walked away from - it's dropped.
		if (C && C->bAwaitingCaptain) { C->bAwaitingCaptain = false; C->bCaseActive = false; }
		if (C) C->bBriefed = false;
		if (IsHost())
			if (const AGameStateBase* GS = GetWorld()->GetGameState())
				for (APlayerState* P : GS->PlayerArray) if (AHUPlayerState* HP = Cast<AHUPlayerState>(P)) HP->bReady = false;
		Lights(RX, RY);
		return;
	}
	// THE PRECINCT: back from a scene, the case waits for booking and the Captain (judged once it's submitted)
	if (C) { C->bAwaitingCaptain = C->bCaseActive; C->bBriefed = false; }
	BoardTitle->SetVisibility(false); BoardCode->SetVisibility(false); BoardRoster->SetVisibility(false);
	LaptopScreen->SetVisibility(false); FolderLabel->SetVisibility(false);
	DoorSign->SetRelativeLocation(FVector(PX - 8, -150, 285));
	DoorSign->SetRelativeRotation(FRotator(0, 180, 0));
	DoorSign->SetText(FText::FromString(TEXT("ROLL CALL  >  NEXT SHIFT")));
	// the CASE BOARD on the -Y wall, the MOTOR POOL door on the -X wall (back out to the scene). Each machine
	// spawns its own with the same name, so a squadmate's use of it finds this machine's copy.
	{
		FActorSpawnParameters SP;
		SP.Name = TEXT("HU_CaseBoard");
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GetWorld()->SpawnActor<AHUCaseBoard>(AHUCaseBoard::StaticClass(), FTransform(FRotator(0, 90, 0), FVector(-150, -PY, 0)) * GetActorTransform(), SP);
		SP.Name = TEXT("HU_MotorPoolDoor");
		GetWorld()->SpawnActor<AHUMotorPoolDoor>(AHUMotorPoolDoor::StaticClass(), FTransform(FRotator(0, 0, 0), FVector(-PX, 0, 0)) * GetActorTransform(), SP);
	}
	// the DESKS' case files: folders to pick up and read, one set per desk (same names on every machine)
	{
		int32 N = 0;
		for (const FVector2D& DeskSpot : DeskSpots)
		{
			const float X = DeskSpot.X, Y = DeskSpot.Y;
			FActorSpawnParameters SP;
			SP.Name = *FString::Printf(TEXT("HU_CaseDesk%d"), N);
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AHUCaseDesk* D = GetWorld()->SpawnActor<AHUCaseDesk>(AHUCaseDesk::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(X, Y, 0)) * GetActorTransform(), SP)) D->DeskIndex = N;
			N++;
		}
	}
	// the TAPE PLAYER behind the desk computers: plays the security tapes pulled at the scene (each player's own)
	{
		const FTransform T = GetActorTransform();
		if (AHUSecurityDesk* Deck = GetWorld()->SpawnActorDeferred<AHUSecurityDesk>(AHUSecurityDesk::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Deck->bAtPrecinct = true;
			Deck->FinishSpawning(T);
		}
	}
	Lights(PX, PY);
}

void AHUSquadRoom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPrecinct)
	{
		UpdateHolding();
		if (!bWelcomed)
			if (APlayerController* WPC = GetWorld()->GetFirstPlayerController())
				if (APawn* WP = WPC->GetPawn())
					if (UHUInteractComponent* WI = WP->FindComponentByClass<UHUInteractComponent>())
					{
						bWelcomed = true;
						const UHUCaseSubsystem* WC = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
						WI->Say(WC && WC->bAwaitingCaptain ? TEXT("Back at the precinct. Work the case board, watch the tapes at a desk, book whoever's in holding, then submit it at the Captain's in-tray. The motor pool takes you back out to the scene.")
							: TEXT("The precinct. Nothing open right now - roll call's through the door when you're ready for a shift."));
					}
		return;
	}
	const UHUSquadSubsystem* S = Squad();
	const FString Code = S && !S->Code.IsEmpty() ? S->Code : FString(TEXT("------"));
	BoardCode->SetText(FText::FromString(TEXT("CODE  ") + Code.Left(3) + TEXT(" ") + Code.Mid(3)));
	FString Roster = FString::Printf(TEXT("ON THE %s<br>"), S ? *S->NetworkName().ToUpper() : TEXT("NETWORK"));
	int32 N = 0;
	if (const AGameStateBase* GS = GetWorld()->GetGameState())
		for (const APlayerState* P : GS->PlayerArray)
			if (P)
			{
				const AHUPlayerState* HP = Cast<AHUPlayerState>(P);
				++N;
				Roster += FString::Printf(TEXT("%s  DET. %s%s<br>"), HP && HP->bReady ? TEXT("[READY]") : TEXT("[ - - ]"), *P->GetPlayerName().Replace(TEXT("_"), TEXT(" ")).ToUpper(), N == 1 ? TEXT("  (HOST)") : TEXT(""));
			}
	if (const UHUCaseSubsystem* CS = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>(); CS && !CS->LastVerdict.IsEmpty()) Roster += TEXT("<br>LAST SHIFT:  ") + CS->LastVerdict.Left(60) + TEXT("<br>");
	Roster += FString::Printf(TEXT("<br>CASE:  %s  ·  %s"), *PlaceLabel(), DifficultyName(DifficultyChoice));
	if (const UHUCaseSubsystem* CS3 = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>(); CS3 && CS3->bAwaitingCaptain)
		Roster += FString::Printf(TEXT("<br>LAST CASE OPEN — %d IN HOLDING, %d TO BOOK. SUBMIT IT TO THE CAPTAIN."), CS3->Arrested.Num(), CS3->UnbookedCount());
	Roster += Countdown >= 0 ? FString::Printf(TEXT("<br>EVERYONE'S IN — ROLL CALL IN %d"), Countdown) : FString::Printf(TEXT("<br>%d / 4  —  PUNCH IN AT THE TIME CLOCK (OR PRESS Y) WHEN READY"), N);
	BoardRoster->SetText(FText::FromString(Roster));

	LaptopScreen->SetText(FText::FromString(TEXT("LOCATION<br>") + PlaceLabel()));
	FolderLabel->SetText(FText::FromString(FString(DifficultyName(DifficultyChoice)).ToUpper()));
	// everyone's machine remembers the host's picks, so the briefing deals the same case to the whole squad
	if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>())
	{
		C->ChosenPlace = HUCaseGen::Places().IsValidIndex(PlaceChoice) ? HUCaseGen::Places()[PlaceChoice] : FString();
		C->ChosenDifficulty = DifficultyChoice >= 0 ? FString(DifficultyName(DifficultyChoice)).ToLower() : FString();
	}

	UpdateHolding();
	// Y readies you up from anywhere in the room
	if (!bKeysBound)
		if (APlayerController* LPC = GetWorld()->GetFirstPlayerController())
		{
			EnableInput(LPC);
			if (InputComponent) { InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &AHUSquadRoom::ToggleReady); bKeysBound = true; }
		}

	// the host watches for everyone being ready, then counts down and starts the shift
	if (IsHost() && !bStarting)
	{
		int32 Total = 0, Ready = 0;
		if (const AGameStateBase* GS = GetWorld()->GetGameState())
			for (const APlayerState* P : GS->PlayerArray) { Total++; if (const AHUPlayerState* HP = Cast<AHUPlayerState>(P)) if (HP->bReady) Ready++; }
		const UHUCaseSubsystem* CS2 = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		if (Total > 0 && Ready == Total && !(CS2 && CS2->bAwaitingCaptain)) // the last case has to be submitted first
		{
			if (Countdown < 0) { Countdown = 5; Tock = 1.f; }
			Tock -= DeltaSeconds;
			if (Tock <= 0.f) { Tock += 1.f; Countdown--; }
			if (Countdown <= 0) { Countdown = 0; bStarting = true; if (UHUSquadSubsystem* SQ = Squad()) SQ->StartShift(); }
		}
		else Countdown = -1;
	}

	// say hello once the player's here
	if (!bWelcomed)
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (APawn* P = PC->GetPawn())
				if (UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>())
				{
					bWelcomed = true;
					if (const UHUCaseSubsystem* CS = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>(); CS && !CS->LastVerdict.IsEmpty()) { I->Notify(TEXT("LAST SHIFT"), CS->LastVerdict, FLinearColor(0.95f, 0.76f, 0.32f)); }
					I->Say(IsHost() ? FString::Printf(TEXT("Your squad code is %s — friends enter it on their cell phone at the desk. Pick the case at the table by the doors, then everyone punches in at the time clock (or presses Y)."), *Code)
						: TEXT("Roll call. Punch in at the time clock by the doors (or press Y) when you're ready."));
				}
}

FText AHUSquadRoom::GetPrompt_Implementation(APawn* User) const
{
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return FText::GetEmpty();
	if (BoardParts.Contains(I->FocusPart)) return FText::FromString(TEXT("[E] Copy the squad code"));
	if (const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>())
	{
		if (CounterParts.Contains(I->FocusPart))
		{
			for (const FString& Id : C->Arrested)
				if (!C->IsBooked(Id))
				{
					FHUSuspect S; C->GetSuspect(Id, S);
					const int32 Step = C->BookingStep.FindRef(Id);
					const TCHAR* What[] = { TEXT("take the mugshot"), TEXT("roll the fingerprints"), TEXT("bag their property") };
					return FText::FromString(FString::Printf(TEXT("[E] Book %s — %s (%d/3)"), *S.Name, What[FMath::Clamp(Step, 0, 2)], Step + 1));
				}
			return FText::FromString(C->Arrested.Num() > 0 ? TEXT("Booking counter — everyone in holding is booked") : TEXT("Booking counter — nobody in holding"));
		}
		if (TrayParts.Contains(I->FocusPart))
		{
			if (!C->bAwaitingCaptain) return FText::FromString(TEXT("The Captain's in-tray — empty"));
			if (C->UnbookedCount() > 0) return FText::FromString(FString::Printf(TEXT("Book everyone in holding first (%d left)"), C->UnbookedCount()));
			return FText::FromString(FString::Printf(TEXT("[E] Submit the case to the Captain  (%d booked)"), C->Arrested.Num()));
		}
	}
	if (CoffeeParts.Contains(I->FocusPart)) return FText::FromString(TEXT("[E] Pour a coffee  (faster work for a while)"));
	if (ShiftParts.Contains(I->FocusPart))
	{
		const UHUCaseSubsystem* SC = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		if (SC && SC->bAwaitingCaptain) return FText::FromString(TEXT("Roll call — submit the case at the Captain's in-tray first"));
		return FText::FromString(IsHost() ? TEXT("[E] Go to roll call for the next shift") : TEXT("Roll call — the host takes the squad through"));
	}
	if (ComputerParts.Contains(I->FocusPart))
	{
		for (TActorIterator<AHUCaseDesk> It(GetWorld()); It; ++It)
			if (FVector::DistSquared(It->GetActorLocation(), I->FocusPart->GetComponentLocation()) < 120.f * 120.f && !It->CanUse(User))
				return FText::FromString(It->bOwnerOn ? FString::Printf(TEXT("[E] Look over Det. %s's shoulder"), *It->OwnerName()) : FString::Printf(TEXT("Det. %s's computer"), *It->OwnerName()));
		const UHUCaseSubsystem* TC = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		if (TC && TC->bTapesPulled) return FText::FromString(FString::Printf(TEXT("[E] Watch the security tapes (%d cameras)  ·  [Q] Case tools: records, alibis, profile"), TC->TapeCams()));
		return FText::FromString(TEXT("[E] Case tools: records, alibi check, profile  ·  [Q] Your service record"));
	}
	if (LaptopParts.Contains(I->FocusPart))
		return FText::FromString(IsHost() ? FString::Printf(TEXT("Location: %s  ·  [E] next  ·  [Q] previous"), *PlaceLabel()) : FString::Printf(TEXT("Location: %s  (the host picks)"), *PlaceLabel()));
	if (FolderParts.Contains(I->FocusPart))
		return FText::FromString(IsHost() ? FString::Printf(TEXT("Difficulty: %s  ·  [E] next  ·  [Q] previous"), DifficultyName(DifficultyChoice)) : FString::Printf(TEXT("Difficulty: %s  (the host picks)"), DifficultyName(DifficultyChoice)));
	if (DoorParts.Contains(I->FocusPart))
	{
		if (!IsHost()) return FText::FromString(TEXT("The doors open when everyone's punched in"));
		int32 N = 0; if (const AGameStateBase* GS = GetWorld()->GetGameState()) N = GS->PlayerArray.Num();
		return FText::FromString(FString::Printf(TEXT("[E] Start now without waiting (%d detective%s)"), N, N == 1 ? TEXT("") : TEXT("s")));
	}
	if (ClockParts.Contains(I->FocusPart))
	{
		const AHUPlayerState* PS = LocalState();
		return FText::FromString(PS && PS->bReady ? TEXT("[E] Punch out — not ready") : TEXT("[E] Punch in — ready for the shift"));
	}
	return FText::GetEmpty();
}

void AHUSquadRoom::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return;
	if (LaptopParts.Contains(I->FocusPart) || FolderParts.Contains(I->FocusPart))
	{
		if (!(IsHost() && User->IsLocallyControlled())) { if (User->IsLocallyControlled()) I->Say(TEXT("The host picks the case.")); return; }
		const int32 Dir = bSecondary ? -1 : 1;
		if (LaptopParts.Contains(I->FocusPart))
		{
			const int32 N = HUCaseGen::Places().Num() + 1; // + random
			PlaceChoice = ((PlaceChoice + 1 + Dir + N) % N) - 1;
			I->Say(TEXT("Location: ") + PlaceLabel());
		}
		else
		{
			DifficultyChoice = ((DifficultyChoice + 1 + Dir + 4) % 4) - 1;
			I->Say(FString(TEXT("Difficulty: ")) + DifficultyName(DifficultyChoice));
		}
		return;
	}
	if (bSecondary && !ComputerParts.Contains(I->FocusPart)) return; // (Q at a computer: the case tools)
	if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>())
	{
		if (CounterParts.Contains(I->FocusPart))
		{
			for (const FString& Id : C->Arrested)
				if (!C->IsBooked(Id))
				{
					FHUSuspect S; C->GetSuspect(Id, S);
					int32& Step = C->BookingStep.FindOrAdd(Id);
					const TCHAR* Done[] = { TEXT("*FLASH* Front, then profile, against the height chart."), TEXT("Ten fingers rolled in ink, one card for the file."), TEXT("Wallet, keys, belt and laces into a property bag, signed and sealed.") };
					I->PlayReach();
					I->Say(FString::Printf(TEXT("BOOKING %s:  %s"), *S.Name.ToUpper(), Done[FMath::Clamp(Step, 0, 2)]));
					if (++Step >= 3) I->Notify(TEXT("BOOKED"), FString::Printf(TEXT("%s is in the system"), *S.Name), FLinearColor(0.36f, 0.76f, 1.f));
					return;
				}
			return;
		}
		if (TrayParts.Contains(I->FocusPart))
		{
			if (!C->bAwaitingCaptain || C->UnbookedCount() > 0) return;
			I->PlayReach();
			C->ResolveArrests(); // judged, paid, closed
			I->Say(TEXT("THE CAPTAIN:  ") + C->LastVerdict);
			I->Notify(TEXT("CASE SUBMITTED"), C->LastVerdict, FLinearColor(0.95f, 0.76f, 0.32f));
			return;
		}
	}
	if (ClockParts.Contains(I->FocusPart)) { if (User->IsLocallyControlled()) ToggleReady(); return; } // only your own punch card
	if (ShiftParts.Contains(I->FocusPart))
	{
		const UHUCaseSubsystem* SC = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		if (SC && SC->bAwaitingCaptain) { if (User->IsLocallyControlled()) I->Say(TEXT("The case isn't in yet. Book the prisoners and submit it at the Captain's in-tray.")); return; }
		if (!(IsHost() && User->IsLocallyControlled())) return;
		// the whole squad goes (a hosted game travels everyone; alone it just opens the level)
		UWorld* W = GetWorld();
		if (W->GetNetMode() == NM_ListenServer || W->GetNetMode() == NM_DedicatedServer) W->ServerTravel(TEXT("/Game/Maps/L_Lobby"));
		else UGameplayStatics::OpenLevel(this, TEXT("L_Lobby"));
		return;
	}
	if (CoffeeParts.Contains(I->FocusPart))
	{
		const TCHAR* L[] = { TEXT("Burnt, bitter, hot. It does the job."), TEXT("Somebody made a fresh pot. A miracle."), TEXT("You top off the cup. Long night ahead.") };
		I->Say(L[FMath::RandRange(0, 2)]);
		if (AHUCharacter* Ch = Cast<AHUCharacter>(User)) { Ch->DrinkCoffee(); I->Notify(TEXT("COFFEE"), TEXT("Quicker hands, quicker feet - for a couple of minutes"), FLinearColor(0.85f, 0.6f, 0.35f)); }
		return;
	}
	if (ComputerParts.Contains(I->FocusPart))
	{
		// whose desk this computer is on: only they use it (a desk nobody has is free)
		AHUCaseDesk* Best = nullptr; float BestD = 1e12f;
		if (I->FocusPart)
			for (TActorIterator<AHUCaseDesk> It(GetWorld()); It; ++It)
			{
				const float Dist = FVector::DistSquared(It->GetActorLocation(), I->FocusPart->GetComponentLocation());
				if (Dist < BestD) { BestD = Dist; Best = *It; }
			}
		if (Best && !Best->CanUse(User))
		{
			// they're on it: look over their shoulder (their screen, live, read-only)
			if (Best->bOwnerOn) { if (!bSecondary) Best->OpenPeek(User); return; }
			if (User->IsLocallyControlled()) I->Say(FString::Printf(TEXT("That's Det. %s's computer. Use your own desk."), *Best->OwnerName()));
			return;
		}
		// the security tapes from the scene: E plays them (on this player's own screen)
		if (const UHUCaseSubsystem* TC = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>(); TC && TC->bTapesPulled && !bSecondary)
		{
			if (User->IsLocallyControlled())
				for (TActorIterator<AHUSecurityDesk> It(GetWorld()); It; ++It) if (It->bAtPrecinct) { It->OpenFor(User); break; }
			return;
		}
		// the case tools: this desk's terminal, on this player's own screen
		const UHUCaseSubsystem* TC2 = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		if ((TC2 && TC2->bTapesPulled) == bSecondary)
		{
			if (User->IsLocallyControlled() && Best) Best->OpenTerminal(User);
			return;
		}
		if (const UHUCaseSubsystem* C = TC2; C && User->IsLocallyControlled())
			I->Say(FString::Printf(TEXT("SERVICE RECORD — Det. %s: %d case%s closed. Pay on file: $%d."), C->DetectiveName.IsEmpty() ? TEXT("(no name)") : *C->DetectiveName, C->CasesSolved, C->CasesSolved == 1 ? TEXT("") : TEXT("s"), C->Cash));
		return;
	}
	UHUSquadSubsystem* S = Squad();
	if (BoardParts.Contains(I->FocusPart))
	{
		if (S && !S->Code.IsEmpty() && User->IsLocallyControlled()) { FPlatformApplicationMisc::ClipboardCopy(*S->Code); I->Say(FString::Printf(TEXT("Copied %s. Send it to your squad."), *S->Code)); }
		return;
	}
	if (DoorParts.Contains(I->FocusPart))
	{
		if (!(IsHost() && User->IsLocallyControlled())) { if (User->IsLocallyControlled()) I->Say(TEXT("The doors open when everyone's punched in at the time clock.")); return; }
		if (const UHUCaseSubsystem* CS4 = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>(); CS4 && CS4->bAwaitingCaptain) { I->Say(TEXT("The last case is still open. Book the prisoners and submit it to the Captain first.")); return; }
		if (S) S->StartShift();
	}
}

const TCHAR* AHUSquadRoom::DifficultyName(int32 I)
{
	switch (I) { case 0: return TEXT("Rookie"); case 1: return TEXT("Detective"); case 2: return TEXT("Veteran"); default: return TEXT("Random"); }
}

FString AHUSquadRoom::PlaceLabel() const
{
	return HUCaseGen::Places().IsValidIndex(PlaceChoice) ? HUCaseGen::Places()[PlaceChoice] : FString(TEXT("RANDOM — ANYWHERE IN THE CITY"));
}

void AHUSquadRoom::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHUSquadRoom, PlaceChoice);
	DOREPLIFETIME(AHUSquadRoom, DifficultyChoice);
	DOREPLIFETIME(AHUSquadRoom, Countdown);
}
void AHUSquadRoom::HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const
{
	for (const TArray<UPrimitiveComponent*>* G : { &BoardParts, &DoorParts, &LaptopParts, &FolderParts, &CoffeeParts, &ClockParts, &CounterParts, &TrayParts, &ShiftParts })
		if (G->Contains(Part)) { Out.Append(*G); return; }
	if (ComputerParts.Contains(Part)) Out.Add(Part); // just the one computer
}
AHUPlayerState* AHUSquadRoom::LocalState() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC ? PC->GetPlayerState<AHUPlayerState>() : nullptr;
}

void AHUSquadRoom::ToggleReady()
{
	AHUPlayerState* PS = LocalState();
	if (!PS) return;
	const bool bNow = !PS->bReady;
	PS->ServerSetReady(bNow);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (APawn* P = PC->GetPawn())
			if (UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>())
				I->Say(bNow ? TEXT("*KA-CHUNK* You punch in. Ready for the shift.") : TEXT("You punch back out. Not ready."));
}
// the prisoners in holding: a figure for each, a name card, BOOKED once they're done
void AHUSquadRoom::UpdateHolding()
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	TArray<FString> Want = C && C->bAwaitingCaptain ? C->Arrested : TArray<FString>();
	TArray<FString> Gone;
	for (const TPair<FString, USceneComponent*>& P : Prisoners) if (!Want.Contains(P.Key)) Gone.Add(P.Key);
	for (const FString& Id : Gone) { if (USceneComponent* G = Prisoners.FindRef(Id)) { TArray<USceneComponent*> Kids; G->GetChildrenComponents(true, Kids); for (USceneComponent* K : Kids) K->DestroyComponent(); G->DestroyComponent(); } Prisoners.Remove(Id); }
	for (int32 i = 0; i < Want.Num(); i++)
	{
		const FString& Id = Want[i];
		USceneComponent* G = Prisoners.FindRef(Id);
		if (!G)
		{
			G = NewObject<USceneComponent>(this);
			G->SetupAttachment(RootComponent);
			G->SetRelativeLocation(FVector(PX - 240 + (i % 3) * 80, PY - 80 - (i / 3) * 60, 0)); // in the holding cell
			G->SetRelativeRotation(FRotator(0, -90, 0)); // facing out through the bars
			G->RegisterComponent();
			TArray<UStaticMeshComponent*> Parts;
			const FLinearColor Coat = FLinearColor(0.15f + 0.05f * (i % 3), 0.13f, 0.12f + 0.04f * (i % 2));
			Block(this, G, Parts, EShape::Cylinder, FVector(0, 0, 45), FVector(40, 46, 90), FLinearColor(0.05f, 0.05f, 0.06f), FRotator::ZeroRotator, false);
			Block(this, G, Parts, EShape::Cylinder, FVector(0, 0, 125), FVector(52, 60, 76), Coat, FRotator::ZeroRotator, false);
			Block(this, G, Parts, EShape::Sphere, FVector(0, 0, 178), FVector(26, 24, 30), FLinearColor(0.58f, 0.42f, 0.33f), FRotator::ZeroRotator, false);
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
			T->SetupAttachment(G);
			T->SetRelativeLocation(FVector(0, 0, 210));
			T->SetRelativeRotation(FRotator(0, 180, 0));
			T->SetHorizontalAlignment(EHTA_Center);
			T->SetWorldSize(11.f);
			T->RegisterComponent();
			Prisoners.Add(Id, G);
		}
		TArray<USceneComponent*> Kids; G->GetChildrenComponents(false, Kids);
		for (USceneComponent* K : Kids)
			if (UTextRenderComponent* T = Cast<UTextRenderComponent>(K))
			{
				FHUSuspect S; if (C) C->GetSuspect(Id, S);
				const bool bBooked = C && C->IsBooked(Id);
				T->SetText(FText::FromString(S.Name.ToUpper() + (bBooked ? TEXT("<br>BOOKED") : TEXT("<br>AWAITING BOOKING"))));
				T->SetTextRenderColor(bBooked ? FColor(110, 200, 255) : FColor(255, 181, 71));
			}
	}
}
float AHUSquadRoom::UseTime(APawn* User, bool bSecondary) const
{
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!I || bSecondary) return 0.f;
	if (CoffeeParts.Contains(I->FocusPart)) return 1.f;
	if (CounterParts.Contains(I->FocusPart) && C && C->UnbookedCount() > 0) return 1.5f;
	if (TrayParts.Contains(I->FocusPart) && C && C->bAwaitingCaptain && C->UnbookedCount() == 0) return 1.f;
	return 0.f;
}
