#include "HUSquadRoom.h"
#include "HUSquadBoards.h"
#include "HUProgress.h"
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
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

using namespace HUBuild;

// the room: X from -700 to 700, Y from -500 to 500. The board is on the +X wall, the roll-call door on the -X wall.
static const float RX = 700.f, RY = 500.f, RH = 340.f;
// the precinct (the base, a level of its own): X from -PX to PX, Y from -PY to PY
static const float PX = 1400.f, PY = 1000.f;
// the squad's four desks (desk centres; each faces +X, its chair on the -X side)
static const FVector2D DeskSpots[] = { FVector2D(-700, -380), FVector2D(-250, -380), FVector2D(-700, 380), FVector2D(-250, 380) };

// roll call's benches: four rows facing the chalkboard, split by an aisle down the middle
static const float BenchRows[] = { 430.f, 300.f, 170.f, 40.f };
static const float BenchMidY = 235.f, BenchLen = 290.f, BenchSeatZ = 47.f;

// words made while building the room (OnConstruction can't use CreateDefaultSubobject). Text faces local +X and
// runs toward local -Y; EHTA_Left anchors its first letter, lines go down from Pos.
static UTextRenderComponent* RoomText(AActor* Owner, USceneComponent* Root, TArray<UTextRenderComponent*>& Out, FVector Pos, float Yaw, float Size, FColor Col, const FString& Text, EHorizTextAligment H = EHTA_Center)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(Owner);
	T->SetupAttachment(Root);
	T->SetRelativeLocation(Pos);
	T->SetRelativeRotation(FRotator(0, Yaw, 0));
	T->SetHorizontalAlignment(H);
	T->SetVerticalAlignment(EVRTA_TextTop);
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Col);
	T->SetText(FText::FromString(Text));
	T->RegisterComponent();
	Out.Add(T);
	return T;
}

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
	// text on the board faces into the room (-X)
	BoardTitle = MakeText(this, Root, TEXT("BoardTitle"), FVector(RX - 8, 0, 270), 180, 24, FColor(235, 235, 225));
	BoardCode = MakeText(this, Root, TEXT("BoardCode"), FVector(RX - 8, 0, 235), 180, 46, FColor(255, 225, 120));
	// the roster hangs down from under the code, small enough that a full squad still fits on the slate
	BoardRoster = MakeText(this, Root, TEXT("BoardRoster"), FVector(RX - 8, 0, 206), 180, 11, FColor(225, 230, 235));
	BoardRoster->SetVerticalAlignment(EVRTA_TextTop);
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
	Clear(Blocks); BoardParts.Reset(); DoorParts.Reset(); LaptopParts.Reset(); FolderParts.Reset(); ComputerParts.Reset(); ChairParts.Reset(); CoffeeParts.Reset(); ClockParts.Reset(); CounterParts.Reset(); TrayParts.Reset(); ShiftParts.Reset(); BenchParts.Reset();
	for (UTextRenderComponent* T : RoomTexts) if (T) T->DestroyComponent();
	RoomTexts.Reset(); RosterNames = nullptr;
	USceneComponent* R = RootComponent;
	const FLinearColor Floor(0.16f, 0.12f, 0.08f), Wall(0.32f, 0.36f, 0.33f), Trim(0.12f, 0.08f, 0.05f), Desk(0.25f, 0.14f, 0.06f), Steel(0.3f, 0.32f, 0.34f);
	const float W = bPrecinct ? PX : RX, D = bPrecinct ? PY : RY;
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, -5), FVector(W * 2, D * 2, 10), Floor);
	Block(this, R, Blocks, EShape::Cube, FVector(0, 0, RH + 5), FVector(W * 2, D * 2, 10), Wall * 0.6f);
	for (float S : { -1.f, 1.f })
	{
		Block(this, R, Blocks, EShape::Cube, FVector(S * (W + 5), 0, RH / 2), FVector(10, D * 2, RH), Wall);
		Block(this, R, Blocks, EShape::Cube, FVector(0, S * (D + 5), RH / 2), FVector(W * 2, 10, RH), Wall);
		Block(this, R, Blocks, EShape::Cube, FVector(0, S * (D - 2), 45), FVector(W * 2, 4, 90), Trim);
	}
	for (float X = -W + 250.f; X <= W - 250.f; X += (W - 250.f) / 2.f) for (float Y : { -D * 0.45f, D * 0.45f }) Block(this, R, Blocks, EShape::Cube, FVector(X, Y, RH - 4), FVector(140, 30, 4), FLinearColor(3, 3, 2.7f), FRotator::ZeroRotator, false);

	if (!bPrecinct)
	{
		// ======== ROLL CALL: just where the squad clocks in for the shift ========
		// the chalkboard: the squad code and who's punched in
		// (the frame sits behind the slate, and the chalk text 2 cm in front of it - a frame in front hid the board)
		BoardParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(RX - 5, 0, 200), FVector(2, 560, 190), FLinearColor(0.03f, 0.06f, 0.04f)));
		BoardParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(RX - 2, 0, 200), FVector(4, 580, 210), Trim));
		BoardParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(RX - 12, 0, 96), FVector(14, 560, 4), Trim));
		// the roll-call door to the briefing: double doors with a glass panel
		for (float S : { -1.f, 1.f })
		{
			DoorParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 4, S * 46, 120), FVector(6, 90, 240), FLinearColor(0.2f, 0.12f, 0.05f)));
			DoorParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 7.5f, S * 46, 170), FVector(1, 50, 60), FLinearColor(0.6f, 0.75f, 0.85f)));
			DoorParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 9, S * 10, 115), FVector(4, 3, 18), FLinearColor(0.75f, 0.6f, 0.2f)));
		}
		// the time clock by the doors: punch in when you're ready
		ClockParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 14, 260, 140), FVector(18, 36, 46), FLinearColor(0.55f, 0.5f, 0.38f)));
		ClockParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-RX + 23.5f, 260, 152), FVector(22, 22, 1), FLinearColor(0.9f, 0.88f, 0.8f), FRotator(0, 0, 90)));
		ClockParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 24, 260, 126), FVector(2, 14, 3), FLinearColor(0.05f, 0.05f, 0.05f)));
		ClockParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-RX + 8, 305, 140), FVector(6, 30, 60), FLinearColor(0.35f, 0.25f, 0.12f))); // the card rack
		// the case-selection table: a laptop (where) and a case folder (how hard) - the host's picks for the shift
		Block(this, R, Blocks, EShape::Cube, FVector(-420, -330, 72), FVector(90, 150, 6), Desk);
		for (float DX : { -40.f, 40.f }) for (float DY : { -68.f, 68.f }) Block(this, R, Blocks, EShape::Cube, FVector(-420 + DX, -330 + DY, 35), FVector(5, 5, 70), Desk * 0.7f);
		LaptopParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-428, -360, 76), FVector(24, 34, 2), FLinearColor(0.12f, 0.12f, 0.13f)));
		LaptopParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-441, -360, 88), FVector(1.5f, 34, 22), FLinearColor(0.12f, 0.12f, 0.13f)));
		LaptopParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-440.1f, -360, 88), FVector(0.2f, 31, 19), FLinearColor(0.02f, 0.06f, 0.12f)));
		FolderParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-405, -295, 75.8f), FVector(24, 32, 1.5f), FLinearColor(0.75f, 0.6f, 0.32f), FRotator(0, 8, 0)));
		FolderParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(-398, -295, 84), FVector(1, 26, 14), FLinearColor(0.85f, 0.82f, 0.72f)));

		// ---- the room a shift starts in: benches facing the board, lockers, the duty roster, the notice boards ----
		const FLinearColor Bench(0.28f, 0.17f, 0.08f), LockerGreen(0.2f, 0.26f, 0.22f), Paper(0.86f, 0.84f, 0.76f), Cork(0.5f, 0.34f, 0.18f), Brass(0.75f, 0.6f, 0.2f);
		const FColor Ink(30, 30, 35), Red(150, 25, 20);
		// BENCHES: four rows, an aisle down the middle from the doors to the board (E sits, facing the board)
		for (float BX : BenchRows)
			for (float S : { -1.f, 1.f })
			{
				const float BY = S * BenchMidY;
				BenchParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(BX, BY, BenchSeatZ - 2.5f), FVector(42, BenchLen, 5), Bench));
				BenchParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(BX - 19, BY, 75), FVector(4, BenchLen, 26), Bench));
				for (float DY : { -BenchLen / 2 + 15.f, 0.f, BenchLen / 2 - 15.f }) Block(this, R, Blocks, EShape::Cube, FVector(BX, BY + DY, 21), FVector(34, 5, 42), Steel * 0.6f);
				for (float DY : { -BenchLen / 2 + 15.f, BenchLen / 2 - 15.f }) Block(this, R, Blocks, EShape::Cube, FVector(BX - 19, BY + DY, 58), FVector(4, 5, 30), Steel * 0.6f, FRotator::ZeroRotator, false);
			}
		// the sergeant's lectern, front corner beside the board, facing the benches
		const FVector Lec(585, -390, 0);
		Block(this, R, Blocks, EShape::Cube, Lec + FVector(0, 0, 52), FVector(40, 56, 104), Desk);
		Block(this, R, Blocks, EShape::Cube, Lec + FVector(2, 0, 108), FVector(48, 62, 5), Desk * 1.2f, FRotator(-12, 0, 0));
		Block(this, R, Blocks, EShape::Cube, Lec + FVector(-20.5f, 0, 72), FVector(1, 34, 34), Brass, FRotator::ZeroRotator, false);
		Block(this, R, Blocks, EShape::Cylinder, Lec + FVector(-12, 14, 124), FVector(1.5f, 1.5f, 24), Steel * 0.4f, FRotator(0, 0, 0), false);
		Block(this, R, Blocks, EShape::Sphere, Lec + FVector(-14, 14, 137), FVector(4, 4, 6), FLinearColor(0.04f, 0.04f, 0.04f), FRotator::ZeroRotator, false);
		RoomText(this, R, RoomTexts, Lec + FVector(-21.2f, 0, 84), 180, 5.5f, FColor(40, 28, 8), TEXT("METRO PD<br>NIGHT WATCH"));

		// LOCKERS: a bank of twelve along the +Y wall, one standing open
		for (int32 i = 0; i < 12; i++)
		{
			const float LX = -575.f + i * 50.f;
			const FLinearColor LC = LockerGreen * (i % 3 == 0 ? 0.9f : 1.f);
			Block(this, R, Blocks, EShape::Cube, FVector(LX, 475, 5), FVector(50, 44, 10), Trim);
			Block(this, R, Blocks, EShape::Cube, FVector(LX, 475, 100), FVector(50, 46, 180), LC);
			if (i == 4)
			{
				// left open: the dark inside, a coat on the hook, the door swung out on its hinge
				Block(this, R, Blocks, EShape::Cube, FVector(LX, 451.4f, 100), FVector(44, 1, 172), FLinearColor(0.02f, 0.02f, 0.02f), FRotator::ZeroRotator, false);
				Block(this, R, Blocks, EShape::Cube, FVector(LX, 449, 120), FVector(30, 4, 70), FLinearColor(0.16f, 0.13f, 0.1f), FRotator::ZeroRotator, false);
				const float A = FMath::DegreesToRadians(75.f), HX = LX - 23.f;
				Block(this, R, Blocks, EShape::Cube, FVector(HX + 23.f * FMath::Cos(A), 451.f - 23.f * FMath::Sin(A), 100), FVector(46, 1.5f, 174), LC * 1.1f, FRotator(0, -75, 0));
				continue;
			}
			Block(this, R, Blocks, EShape::Cube, FVector(LX, 451.5f, 100), FVector(46, 1, 174), LC * 1.1f, FRotator::ZeroRotator, false);
			for (int32 k = 0; k < 3; k++) Block(this, R, Blocks, EShape::Cube, FVector(LX, 450.8f, 168 + k * 6), FVector(24, 0.6f, 2), FLinearColor(0.03f, 0.03f, 0.03f), FRotator::ZeroRotator, false);
			Block(this, R, Blocks, EShape::Cube, FVector(LX + 16, 450.5f, 105), FVector(3, 2, 14), Steel * 1.4f, FRotator::ZeroRotator, false);
			Block(this, R, Blocks, EShape::Cube, FVector(LX, 450.8f, 150), FVector(22, 0.5f, 6), Paper, FRotator::ZeroRotator, false);
		}

		// DUTY ROSTER on the back wall, across the doors from the time clock: tonight's watch - and the squad
		const FVector Ros(-700, -300, 0);
		Block(this, R, Blocks, EShape::Cube, Ros + FVector(1.5f, 0, 190), FVector(3, 300, 160), Trim, FRotator::ZeroRotator, false);
		Block(this, R, Blocks, EShape::Cube, Ros + FVector(3.5f, 0, 190), FVector(1, 288, 148), FLinearColor(0.85f, 0.86f, 0.82f), FRotator::ZeroRotator, false);
		Block(this, R, Blocks, EShape::Cube, Ros + FVector(4.2f, 0, 254), FVector(0.4f, 288, 18), FLinearColor(0.06f, 0.1f, 0.22f), FRotator::ZeroRotator, false);
		RoomText(this, R, RoomTexts, Ros + FVector(4.6f, 0, 260), 0, 8.f, FColor(235, 235, 225), TEXT("DUTY ROSTER  ·  NIGHT WATCH"));
		RoomText(this, R, RoomTexts, Ros + FVector(4.6f, 132, 238), 0, 7.f, Ink,
			TEXT("WATCH COMMANDER<br>DESK<br>CAR 7<br>CAR 12<br>FOOT BEAT 4<br>HOMICIDE"), EHTA_Left);
		RosterNames = RoomText(this, R, RoomTexts, Ros + FVector(4.6f, 30, 238), 0, 7.f, Ink,
			TEXT("LT. HARGROVE<br>SGT. KOWALSKI<br>OFC. DELGADO, OFC. PRUITT<br>OFC. MORROW, OFC. TATE<br>OFC. LINDSAY<br>-"), EHTA_Left);

		// NOTICE BOARDS on the -Y wall: cork, papers pinned up crooked, a few worth reading
		// BodyDrop: how far below the paper's top the small print starts (posters put it under the photo;
		// TextRender drops blank lines, so <br> can't push it down)
		struct FNote { float DX, Z, W, H; FLinearColor Col; const TCHAR* Head; FColor HeadCol; const TCHAR* Body; float Tilt; float BodyDrop = 0.f; };
		auto NoticeBoard = [&](float CX, const TArray<FNote>& Notes)
		{
			Block(this, R, Blocks, EShape::Cube, FVector(CX, -497, 185), FVector(250, 4, 150), Trim, FRotator::ZeroRotator, false);
			Block(this, R, Blocks, EShape::Cube, FVector(CX, -494.5f, 185), FVector(238, 1, 138), Cork, FRotator::ZeroRotator, false);
			for (const FNote& N : Notes)
			{
				const float X = CX + N.DX, Top = N.Z + N.H / 2;
				Block(this, R, Blocks, EShape::Cube, FVector(X, -493.8f, N.Z), FVector(N.W, 0.4f, N.H), N.Col, FRotator(N.Tilt, 0, 0), false);
				Block(this, R, Blocks, EShape::Sphere, FVector(X, -493.2f, Top - 3), FVector(1.6f, 1.6f, 1.6f), FLinearColor(0.7f, 0.08f, 0.06f), FRotator::ZeroRotator, false);
				if (N.Head) RoomText(this, R, RoomTexts, FVector(X, -493.4f, Top - 6), 90, FMath::Min(9.f, N.W / 7.f), N.HeadCol, N.Head);
				if (N.Body) RoomText(this, R, RoomTexts, FVector(X, -493.4f, Top - (N.BodyDrop > 0.f ? N.BodyDrop : N.Head ? 17.f : 7.f)), 90, 3.4f, Ink, N.Body);
			}
		};
		NoticeBoard(-170.f, {
			{ -70, 195, 60, 80, Paper, TEXT("WANTED"), Red, TEXT("ARMED ROBBERY<br>REWARD $500"), 0, 40 },
			{ 20, 215, 70, 62, FLinearColor(0.92f, 0.92f, 0.88f), TEXT("MEMO"), Ink, TEXT("ALL PERSONNEL<br>Overtime slips go to the<br>desk sergeant by Friday.<br>No exceptions.<br>- Lt. Hargrove"), 0 },
			{ 85, 150, 50, 50, FLinearColor(0.92f, 0.8f, 0.35f), nullptr, Ink, TEXT("PBA BOWLING<br>THURSDAY 8 PM<br>STARLITE LANES<br>bring your<br>own shoes"), 0 },
			{ -5, 148, 44, 28, Paper, nullptr, Ink, TEXT("COFFEE FUND<br>$2 A WEEK<br>- PAY UP -"), 0 },
			{ 75, 232, 30, 20, FLinearColor(0.9f, 0.6f, 0.62f), nullptr, Ink, nullptr, 8 },
			{ -95, 140, 34, 24, FLinearColor(0.6f, 0.72f, 0.88f), nullptr, Ink, nullptr, -6 } });
		Block(this, R, Blocks, EShape::Cube, FVector(-240, -493.4f, 209), FVector(24, 0.3f, 20), FLinearColor(0.35f, 0.35f, 0.37f), FRotator::ZeroRotator, false); // the wanted man's photo
		NoticeBoard(230.f, {
			{ -70, 200, 60, 80, Paper, TEXT("MISSING"), FColor(20, 40, 110), TEXT("HAVE YOU SEEN HER?<br>CALL METRO PD"), 0, 38 },
			{ 25, 210, 75, 68, FLinearColor(0.92f, 0.92f, 0.88f), TEXT("BOLO"), Red, TEXT("Dark green sedan,<br>partial plate 4K2.<br>Driver armed.<br>Do not approach alone."), 0 },
			{ 90, 150, 46, 40, FLinearColor(0.85f, 0.9f, 0.82f), nullptr, Ink, TEXT("BAG IT.<br>TAG IT.<br>SIGN IT.<br>- Evidence Unit"), 3 },
			{ -10, 142, 52, 34, Paper, nullptr, Ink, TEXT("SHIFT BIDS<br>DUE MONDAY<br>see Sgt. Kowalski"), 0 },
			{ -100, 135, 28, 20, FLinearColor(0.92f, 0.8f, 0.35f), nullptr, Ink, nullptr, -9 } });
		Block(this, R, Blocks, EShape::Cube, FVector(160, -493.4f, 214), FVector(22, 0.3f, 18), FLinearColor(0.42f, 0.38f, 0.34f), FRotator::ZeroRotator, false); // her photo
		return;
	}

	// ======== THE PRECINCT: the squad's base, the same whatever the case - where it gets solved ========
	// the squad's desks, pushed together, each with a computer (the case archive, and the scene's security tapes)
	for (const FVector2D& DS : DeskSpots)
	{
		const float X = DS.X, Y = DS.Y;
		// the desk: 150 deep, 150 wide, top at 77 - the folders fan across the near half, the computer at the back
		Block(this, R, Blocks, EShape::Cube, FVector(X, Y, 74), FVector(150, 150, 6), Desk);
		for (float DX : { -68.f, 68.f }) for (float DY : { -68.f, 68.f }) Block(this, R, Blocks, EShape::Cube, FVector(X + DX, Y + DY, 36), FVector(5, 5, 72), Desk * 0.7f);
		Block(this, R, Blocks, EShape::Cube, FVector(X + 15, Y + 55, 50), FVector(100, 34, 44), Desk * 0.8f); // a drawer pedestal, out of the knees' way
		// the computer at the back: a beige box, the monitor on it facing the chair, a keyboard in front
		ComputerParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(X + 45, Y + 10, 83), FVector(40, 46, 12), FLinearColor(0.6f, 0.57f, 0.48f)));
		ComputerParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(X + 45, Y + 10, 108), FVector(36, 44, 38), FLinearColor(0.62f, 0.59f, 0.5f)));
		ComputerParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(X + 26.8f, Y + 10, 109), FVector(1, 36, 28), FLinearColor(0.05f, 0.12f, 0.25f)));
		ComputerParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(X + 5, Y + 10, 78), FVector(16, 44, 2), FLinearColor(0.55f, 0.53f, 0.46f)));
		// the chair, pulled up to the -X side (E sits)
		ChairParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(X - 115, Y, 46), FVector(48, 48, 8), Steel));
		ChairParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(X - 138, Y, 78), FVector(5, 46, 56), Steel));
		Block(this, R, Blocks, EShape::Cylinder, FVector(X - 115, Y, 21), FVector(6, 6, 42), Steel * 0.6f);
		Block(this, R, Blocks, EShape::Cube, FVector(X - 115, Y, 3), FVector(50, 50, 6), Steel * 0.6f);
	}
	// HOLDING: a barred cell in the far corner where the van's prisoners wait to be booked
	const float CY0 = PY - 200.f; // the bars, facing the room
	const FLinearColor Bars(0.35f, 0.37f, 0.4f);
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 150, PY - 100, 1), FVector(300, 200, 2), FLinearColor(0.2f, 0.2f, 0.21f), FRotator::ZeroRotator, false);
	for (float X = PX - 295.f; X <= PX - 5.f; X += 20.f) Block(this, R, Blocks, EShape::Cylinder, FVector(X, CY0, 120), FVector(3, 3, 240), Bars);   // the bars along the front
	for (float Y = CY0 + 10.f; Y <= PY - 10.f; Y += 20.f) Block(this, R, Blocks, EShape::Cylinder, FVector(PX - 300, Y, 120), FVector(3, 3, 240), Bars); // and the side
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 150, CY0, 238), FVector(300, 6, 6), Bars);
	Block(this, R, Blocks, EShape::Cube, FVector(PX - 120, PY - 30, 22), FVector(160, 40, 6), FLinearColor(0.3f, 0.3f, 0.3f));                       // the bench
	for (int32 i = 0; i < 6; i++) Block(this, R, Blocks, EShape::Cube, FVector(PX - 220, PY - 4, 120 + i * 20), FVector(80, 2, 1.5f), FLinearColor(0.85f, 0.85f, 0.8f), FRotator::ZeroRotator, false); // height chart
	// the BOOKING COUNTER beside it: a ledger, an ink pad, a camera on a tripod
	const FVector Book(PX - 420, PY - 100, 0);
	CounterParts.Add(Block(this, R, Blocks, EShape::Cube, Book + FVector(0, 0, 50), FVector(70, 170, 100), Desk));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cube, Book + FVector(0, -20, 101.5f), FVector(30, 40, 3), FLinearColor(0.12f, 0.15f, 0.3f)));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cube, Book + FVector(0, 30, 101.2f), FVector(14, 18, 2.4f), FLinearColor(0.05f, 0.05f, 0.05f)));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cube, Book + FVector(10, 70, 145), FVector(18, 22, 14), FLinearColor(0.08f, 0.08f, 0.08f)));
	CounterParts.Add(Block(this, R, Blocks, EShape::Cylinder, Book + FVector(10, 70, 115), FVector(3, 3, 50), Steel));
	// the CAPTAIN'S IN-TRAY: where a finished case goes
	const FVector Tray(PX - 250, -PY + 200, 0);
	TrayParts.Add(Block(this, R, Blocks, EShape::Cube, Tray + FVector(0, 0, 74), FVector(110, 120, 6), Desk));
	for (float DX : { -48.f, 48.f }) for (float DY : { -52.f, 52.f }) Block(this, R, Blocks, EShape::Cube, Tray + FVector(DX, DY, 36), FVector(5, 5, 72), Desk * 0.7f);
	TrayParts.Add(Block(this, R, Blocks, EShape::Cube, Tray + FVector(0, 0, 82), FVector(36, 46, 8), FLinearColor(0.4f, 0.3f, 0.15f)));
	TrayParts.Add(Block(this, R, Blocks, EShape::Cube, Tray + FVector(0, 0, 85), FVector(30, 40, 3), FLinearColor(0.75f, 0.6f, 0.32f)));
	// the coffee pot
	Block(this, R, Blocks, EShape::Cube, FVector(-PX + 100, PY - 60, 45), FVector(120, 60, 90), Desk);
	CoffeeParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-PX + 100, PY - 60, 105), FVector(16, 16, 22), FLinearColor(0.05f, 0.05f, 0.05f)));
	CoffeeParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-PX + 130, PY - 60, 95), FVector(9, 9, 10), FLinearColor(0.85f, 0.85f, 0.8f)));
	// the door back to ROLL CALL (the next shift), on the +X wall: once the case is in
	for (float S : { -1.f, 1.f })
	{
		ShiftParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(PX - 4, -150 + S * 46, 120), FVector(6, 90, 240), FLinearColor(0.2f, 0.12f, 0.05f)));
		ShiftParts.Add(Block(this, R, Blocks, EShape::Cube, FVector(PX - 7.5f, -150 + S * 46, 170), FVector(1, 50, 60), FLinearColor(0.6f, 0.75f, 0.85f)));
	}
}

void AHUSquadRoom::BeginPlay()
{
	Super::BeginPlay();
	UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	auto Lights = [this](float W, float D)
	{
		for (float X = -W + 250.f; X <= W - 250.f; X += (W - 250.f) / 2.f) for (float Y : { -D * 0.45f, D * 0.45f })
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(RootComponent);
			L->SetRelativeLocation(FVector(X, Y, RH - 50));
			L->SetIntensity(6000.f);
			L->SetAttenuationRadius(1000.f);
			L->SetLightColor(FLinearColor(1.f, 0.9f, 0.75f));
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
		// the UPGRADES board beside it, and the CASE WALL across the room (see HUProgress)
		SP.Name = TEXT("HU_UpgradeBoard");
		GetWorld()->SpawnActor<AHUUpgradeBoard>(AHUUpgradeBoard::StaticClass(), FTransform(FRotator(0, 90, 0), FVector(250, -PY, 0)) * GetActorTransform(), SP);
		SP.Name = TEXT("HU_CaseWall");
		GetWorld()->SpawnActor<AHUCaseWall>(AHUCaseWall::StaticClass(), FTransform(FRotator(0, -90, 0), FVector(-250, PY, 0)) * GetActorTransform(), SP);
		SP.Name = TEXT("HU_MotorPoolDoor");
		GetWorld()->SpawnActor<AHUMotorPoolDoor>(AHUMotorPoolDoor::StaticClass(), FTransform(FRotator(0, 0, 0), FVector(-PX, 0, 0)) * GetActorTransform(), SP);
	}
	// the DESKS' case files: folders to pick up and read, one set per desk (same names on every machine)
	{
		int32 N = 0;
		for (const FVector2D& DS : DeskSpots)
		{
			const float X = DS.X, Y = DS.Y;
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
	RollCallShots(DeltaSeconds);
	// the duty roster has the squad on it: homicide, tonight's detectives (two names a line)
	if (RosterNames)
	{
		FString Dets;
		int32 K = 0;
		if (const AGameStateBase* GS = GetWorld()->GetGameState())
			for (const APlayerState* P : GS->PlayerArray)
				if (P)
				{
					Dets += FString::Printf(TEXT("%sDET. %s"), K == 0 ? TEXT("") : (K % 2 ? TEXT(", ") : TEXT("<br>")), *P->GetPlayerName().Replace(TEXT("_"), TEXT(" ")).ToUpper());
					++K;
				}
		RosterNames->SetText(FText::FromString(TEXT("LT. HARGROVE<br>SGT. KOWALSKI<br>OFC. DELGADO, OFC. PRUITT<br>OFC. MORROW, OFC. TATE<br>OFC. LINDSAY<br>") + (K ? Dets : FString(TEXT("-")))));
	}

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

// -HURollCallShots on L_Lobby: walk the camera round roll call, take a screenshot at each stop, sit on a bench,
// then quit. The shots land in Saved/Screenshots.
void AHUSquadRoom::RollCallShots(float DeltaSeconds)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("HURollCallShots"))) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) return;
	struct FStop { FVector At; float Yaw, Pitch; const TCHAR* Name; };
	static const FStop Stops[] = {
		{ FVector(-640, 0, 110), 0, -6, TEXT("RollCall_FromDoors") },
		{ FVector(600, 330, 110), 169, -8, TEXT("RollCall_Lockers") },
		{ FVector(30, -120, 105), -90, 0, TEXT("RollCall_NoticeBoards") },
		{ FVector(-520, -300, 110), 180, 0, TEXT("RollCall_DutyRoster") },
		{ FVector(-170, -330, 120), -90, 0, TEXT("RollCall_NoticeA") },
		{ FVector(230, -340, 120), -90, 0, TEXT("RollCall_NoticeB") },
	};
	const float Was = ShotClock;
	ShotClock += DeltaSeconds;
	auto Passed = [&](float T) { return Was < T && ShotClock >= T; };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Stops); i++)
	{
		if (Passed(2.f + i * 2.f))
		{
			P->SetActorLocation(GetActorTransform().TransformPosition(Stops[i].At), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(FRotator(Stops[i].Pitch, GetActorRotation().Yaw + Stops[i].Yaw, 0));
		}
		if (Passed(3.f + i * 2.f)) FScreenshotRequest::RequestScreenshot(Stops[i].Name, false, false);
	}
	const float SitAt = 2.f + UE_ARRAY_COUNT(Stops) * 2.f;
	if (Passed(SitAt) && BenchParts.Num() > 2)
		if (UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>())
		{
			P->SetActorLocation(GetActorTransform().TransformPosition(FVector(375, 200, 100)), false, nullptr, ETeleportType::TeleportPhysics);
			I->FocusPart = BenchParts[3]; // the front row, +Y side
			UE_LOG(LogTemp, Log, TEXT("HURollCallShots: prompt '%s'"), *IHUInteractable::Execute_GetPrompt(this, P).ToString());
			IHUInteractable::Execute_Interact(this, P, false);
		}
	if (Passed(SitAt + 1.5f)) FScreenshotRequest::RequestScreenshot(TEXT("RollCall_Seated"), false, false);
	if (Passed(SitAt + 3.f)) FPlatformMisc::RequestExit(false);
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
	if (ChairParts.Contains(I->FocusPart) || BenchParts.Contains(I->FocusPart)) { const AHUCharacter* Ch = Cast<AHUCharacter>(User); return FText::FromString(Ch && Ch->bSeated ? TEXT("[E] Stand up") : TEXT("[E] Sit down")); }
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
		return FText::FromString(TEXT("[E] Log on: records, suspects, phones, security footage  ·  [Q] Your service record"));
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
			// the big places (the hotel) open once someone in the squad is a full Detective
			const bool bBigOk = HUProgress::BestRank(GetWorld()) >= HUProgress::BigPlaceRank;
			for (int32 Tries = 0; Tries < N; Tries++)
			{
				PlaceChoice = ((PlaceChoice + 1 + Dir + N) % N) - 1;
				if (bBigOk || !PlaceLabel().Contains(TEXT("HOTEL"))) break;
			}
			I->Say(TEXT("Location: ") + PlaceLabel() + (bBigOk ? FString() : FString::Printf(TEXT("   (the hotel opens at %s)"), HUProgress::RankName(HUProgress::BigPlaceRank))));
		}
		else
		{
			// Veteran opens once someone in the squad reaches Detective II
			const bool bVetOk = HUProgress::BestRank(GetWorld()) >= HUProgress::VeteranRank;
			DifficultyChoice = ((DifficultyChoice + 1 + Dir + 4) % 4) - 1;
			if (DifficultyChoice == 2 && !bVetOk) DifficultyChoice = ((DifficultyChoice + 1 + Dir + 4) % 4) - 1;
			I->Say(FString(TEXT("Difficulty: ")) + DifficultyName(DifficultyChoice) + (bVetOk ? FString() : FString::Printf(TEXT("   (Veteran opens at %s)"), HUProgress::RankName(HUProgress::VeteranRank))));
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
			I->Say(TEXT("THE CAPTAIN:  ") + C->LastVerdict + (C->LastReveal.Num() ? TEXT("  The whole story's in the file now - any desk computer.") : TEXT("")));
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
	if (BenchParts.Contains(I->FocusPart))
	{
		// a roll-call bench: sit where you're standing along it, facing the board (every machine seats them)
		if (AHUCharacter* Ch = Cast<AHUCharacter>(User))
		{
			if (Ch->bSeated) Ch->StandUp();
			else
			{
				const int32 k = BenchParts.IndexOfByKey(I->FocusPart) / 2; // seat + back per bench, two benches per row
				const float BX = BenchRows[FMath::Clamp(k / 2, 0, 3)], BY = (k % 2 ? 1.f : -1.f) * BenchMidY;
				const float Along = GetActorTransform().InverseTransformPosition(User->GetActorLocation()).Y;
				const float Y = FMath::Clamp(Along, BY - BenchLen / 2 + 30.f, BY + BenchLen / 2 - 30.f);
				Ch->SitDown(GetActorTransform().TransformPosition(FVector(BX + 4, Y, BenchSeatZ)), GetActorRotation().Yaw);
			}
		}
		return;
	}
	if (ChairParts.Contains(I->FocusPart))
	{
		// every machine seats them (the squad sees it); WASD or E stands back up
		if (AHUCharacter* Ch = Cast<AHUCharacter>(User))
		{
			if (Ch->bSeated) Ch->StandUp();
			else
			{
				const int32 k = ChairParts.IndexOfByKey(I->FocusPart) / 2;
				const FVector2D DS = DeskSpots[FMath::Clamp(k, 0, 3)];
				Ch->SitDown(GetActorTransform().TransformPosition(FVector(DS.X - 112, DS.Y, 50)), GetActorRotation().Yaw);
			}
		}
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
		// E: log on (the tapes are the Security Footage icon on the desktop); Q: your service record
		const UHUCaseSubsystem* TC2 = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		if (!bSecondary)
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
	for (const TArray<UPrimitiveComponent*>* G : { &BoardParts, &DoorParts, &LaptopParts, &FolderParts, &ChairParts, &CoffeeParts, &ClockParts, &CounterParts, &TrayParts, &ShiftParts })
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