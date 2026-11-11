#include "HUCrimeScene.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUEvidence.h"
#include "HULadder.h"
#include "HUPushable.h"
#include "HUSuspectActor.h"
#include "HUVictim.h"
#include "HUCruiser.h"
#include "HUOfficer.h"
#include "HUPoliceVan.h"
#include "HUSecurityDesk.h"
#include "HUCoffee.h"
#include "HUInteractComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

// ---------- one theme per kind of place ----------
struct FHUTheme
{
	const TCHAR* Key;      // matched against the case's place name
	const TCHAR* Loft;     // up the stairs
	const TCHAR* High;     // up the ladder
	const TCHAR* Crawl;    // the crawlspace
	const TCHAR* Back;     // the room beyond it
	const TCHAR* Push;     // the heavy furniture
	FVector PushSize;
	FLinearColor PushColor, Floor, Wall, Trim;
	float Height;
};

static const FHUTheme Themes[] = {
	{ TEXT("DINER"),     TEXT("STOREROOM LOFT"),      TEXT("TOP OF THE WALK-IN"),   TEXT("GREASE VENT"),          TEXT("KITCHEN"),         TEXT("Booth Table"),      FVector(180, 90, 75),  FLinearColor(0.55f, 0.06f, 0.05f), FLinearColor(0.7f, 0.7f, 0.66f),  FLinearColor(0.6f, 0.55f, 0.4f),  FLinearColor(0.5f, 0.05f, 0.04f), 450 },
	{ TEXT("WAREHOUSE"), TEXT("MEZZANINE"),           TEXT("PALLET RACK"),          TEXT("CONVEYOR CRAWLWAY"),    TEXT("LOADING DOCK"),    TEXT("Shipping Crate"),   FVector(130, 130, 110), FLinearColor(0.4f, 0.26f, 0.12f), FLinearColor(0.32f, 0.32f, 0.3f), FLinearColor(0.42f, 0.44f, 0.46f), FLinearColor(0.7f, 0.55f, 0.05f), 650 },
	{ TEXT("PIER"),      TEXT("NET LOFT"),            TEXT("STACKED CONTAINERS"),   TEXT("UNDER THE BOARDWALK"),  TEXT("BOATHOUSE"),       TEXT("Fish Crate"),       FVector(110, 80, 70),  FLinearColor(0.3f, 0.35f, 0.4f),  FLinearColor(0.3f, 0.22f, 0.14f), FLinearColor(0.25f, 0.3f, 0.33f), FLinearColor(0.15f, 0.12f, 0.1f), 500 },
	{ TEXT("HOTEL"),     TEXT("MEZZANINE"),           TEXT("LUGGAGE SHELF"),        TEXT("SERVICE DUCT"),         TEXT("LAUNDRY"),         TEXT("Steamer Trunk"),    FVector(110, 70, 70),  FLinearColor(0.35f, 0.2f, 0.1f),  FLinearColor(0.35f, 0.08f, 0.08f), FLinearColor(0.75f, 0.68f, 0.52f), FLinearColor(0.6f, 0.45f, 0.15f), 600 },
	{ TEXT("LOUNGE"),    TEXT("VIP BALCONY"),         TEXT("BACK BAR SHELF"),       TEXT("AIR DUCT"),             TEXT("STOCKROOM"),       TEXT("Velvet Couch"),     FVector(220, 90, 80),  FLinearColor(0.35f, 0.03f, 0.08f), FLinearColor(0.08f, 0.05f, 0.05f), FLinearColor(0.2f, 0.06f, 0.1f),  FLinearColor(0.6f, 0.45f, 0.15f), 480 },
	{ TEXT("STABLES"),   TEXT("HAYLOFT"),             TEXT("FEED BINS"),            TEXT("DRAINAGE CULVERT"),     TEXT("TACK ROOM"),       TEXT("Hay Bale"),         FVector(120, 70, 60),  FLinearColor(0.75f, 0.6f, 0.25f), FLinearColor(0.4f, 0.3f, 0.15f),  FLinearColor(0.35f, 0.2f, 0.1f),  FLinearColor(0.2f, 0.12f, 0.06f), 550 },
	{ TEXT("SALON"),     TEXT("OFFICE LOFT"),         TEXT("SUPPLY CABINETS"),      TEXT("AIR DUCT"),             TEXT("SUPPLY ROOM"),     TEXT("Supply Trolley"),   FVector(100, 70, 90),  FLinearColor(0.7f, 0.7f, 0.72f),  FLinearColor(0.85f, 0.85f, 0.85f), FLinearColor(0.75f, 0.5f, 0.55f), FLinearColor(0.15f, 0.15f, 0.15f), 420 },
	{ TEXT("THEATRE"),   TEXT("BALCONY"),             TEXT("LIGHTING RIG"),         TEXT("UNDER THE STAGE"),      TEXT("BACKSTAGE"),       TEXT("Prop Trunk"),       FVector(120, 70, 70),  FLinearColor(0.15f, 0.1f, 0.06f), FLinearColor(0.25f, 0.04f, 0.04f), FLinearColor(0.12f, 0.05f, 0.05f), FLinearColor(0.6f, 0.45f, 0.15f), 650 },
	{ TEXT("HOSPITAL"),  TEXT("OBSERVATION GALLERY"), TEXT("SUPPLY LOCKERS"),       TEXT("SERVICE DUCT"),         TEXT("RECORDS ROOM"),    TEXT("Gurney"),           FVector(200, 80, 90),  FLinearColor(0.8f, 0.82f, 0.85f), FLinearColor(0.6f, 0.65f, 0.62f), FLinearColor(0.7f, 0.78f, 0.75f), FLinearColor(0.3f, 0.45f, 0.45f), 450 },
	{ TEXT("BOWLING"),   TEXT("SCOREKEEPER'S LOFT"),  TEXT("BALL RACKS"),           TEXT("PINSETTER CRAWLWAY"),   TEXT("PINSETTER ROOM"),  TEXT("Ball Return Cart"), FVector(120, 80, 80),  FLinearColor(0.1f, 0.2f, 0.5f),   FLinearColor(0.6f, 0.45f, 0.25f), FLinearColor(0.15f, 0.2f, 0.35f), FLinearColor(0.7f, 0.1f, 0.1f), 480 },
	{ TEXT("BANK"),      TEXT("MANAGER'S GALLERY"),   TEXT("DEPOSIT BOX STACKS"),   TEXT("VAULT VENT"),           TEXT("VAULT CORRIDOR"),  TEXT("Filing Cabinet"),   FVector(70, 70, 140),  FLinearColor(0.3f, 0.32f, 0.3f),  FLinearColor(0.7f, 0.68f, 0.62f), FLinearColor(0.55f, 0.52f, 0.45f), FLinearColor(0.15f, 0.25f, 0.18f), 650 },
	{ TEXT("BALLROOM"),  TEXT("ORCHESTRA BALCONY"),   TEXT("STACKED CHAIRS"),       TEXT("DUMBWAITER SHAFT"),     TEXT("SERVICE KITCHEN"), TEXT("Banquet Table"),    FVector(200, 100, 75), FLinearColor(0.9f, 0.88f, 0.82f), FLinearColor(0.4f, 0.25f, 0.12f), FLinearColor(0.7f, 0.62f, 0.45f), FLinearColor(0.7f, 0.55f, 0.15f), 650 },
	{ TEXT("LODGE"),     TEXT("TROPHY GALLERY"),      TEXT("GUN CABINETS"),         TEXT("CRAWLSPACE"),           TEXT("MEETING ROOM"),    TEXT("Bookcase"),         FVector(150, 50, 180), FLinearColor(0.3f, 0.17f, 0.07f), FLinearColor(0.3f, 0.2f, 0.1f),  FLinearColor(0.3f, 0.22f, 0.14f), FLinearColor(0.15f, 0.25f, 0.12f), 520 },
};
static const FHUTheme Fallback = { TEXT(""), TEXT("LOFT"), TEXT("STORAGE SHELVES"), TEXT("CRAWLSPACE"), TEXT("BACK ROOM"), TEXT("Crate"), FVector(120, 120, 100), FLinearColor(0.4f, 0.26f, 0.12f), FLinearColor(0.35f, 0.33f, 0.3f), FLinearColor(0.45f, 0.45f, 0.43f), FLinearColor(0.2f, 0.2f, 0.2f), 500 };

static const FHUTheme& ThemeFor(const FString& Place)
{
	for (const FHUTheme& T : Themes) if (Place.ToUpper().Contains(T.Key)) return T;
	return Fallback;
}

static const float TunnelLen = 500.f, TunnelW = 150.f, TunnelH = 110.f, BackW = 600.f, BackH = 260.f;

// ---------- the map generator ----------
// Every case gets its own floor plan from its MapSeed: a footprint, split into rooms (a binary split, one door
// per split so every room is reachable), then features dropped into the rooms that can take them.
// Layout styles: an open HALL, a warren of ROOMS, a WAREHOUSE of rack aisles, a GALLERY with balconies.
namespace
{
	struct FR // a rectangle on the floor plan (local cm)
	{
		float X0 = 0, Y0 = 0, X1 = 0, Y1 = 0;
		float W() const { return X1 - X0; } float D() const { return Y1 - Y0; }
		float CX() const { return (X0 + X1) / 2; } float CY() const { return (Y0 + Y1) / 2; }
		bool Overlaps(const FR& O, float Pad = 0) const { return X0 - Pad < O.X1 && X1 + Pad > O.X0 && Y0 - Pad < O.Y1 && Y1 + Pad > O.Y0; }
		bool Contains(float X, float Y, float Pad = 0) const { return X > X0 + Pad && X < X1 - Pad && Y > Y0 + Pad && Y < Y1 - Pad; }
	};
	struct FSplit { bool bAtX; float At, From, To; };  // a wall: at X=At (running along Y) or at Y=At (running along X)
	enum class EStyle : uint8 { Hall, Rooms, Racks, Gallery };
	enum EArea : uint8 { ALoft, AHigh, ANarrow, ABack, ADark, AOpen, ACount };
	struct FArea { FR Rect; float Z = 0; EArea Kind = AOpen; };

	const TCHAR* StyleName(EStyle S)
	{
		switch (S) { case EStyle::Hall: return TEXT("an open hall"); case EStyle::Rooms: return TEXT("a warren of small rooms"); case EStyle::Racks: return TEXT("aisles of tall racks"); default: return TEXT("balconies on both sides"); }
	}

	void SplitRooms(FR R, int32 Want, float MinSize, FRandomStream& Rng, TArray<FR>& Rooms, TArray<FSplit>& Splits)
	{
		const bool bCanX = R.W() >= MinSize * 2, bCanY = R.D() >= MinSize * 2;
		if (Want <= 1 || (!bCanX && !bCanY)) { Rooms.Add(R); return; }
		const bool bAtX = bCanX && (!bCanY || R.W() >= R.D() || Rng.FRand() < 0.3f);
		const float Lo = bAtX ? R.X0 : R.Y0, Hi = bAtX ? R.X1 : R.Y1;
		const float At = FMath::GridSnap(FMath::Clamp(Lo + (Hi - Lo) * Rng.FRandRange(0.35f, 0.65f), Lo + MinSize, Hi - MinSize), 50.f);
		Splits.Add({ bAtX, At, bAtX ? R.Y0 : R.X0, bAtX ? R.Y1 : R.X1 });
		FR A = R, B = R;
		if (bAtX) { A.X1 = At; B.X0 = At; } else { A.Y1 = At; B.Y0 = At; }
		const int32 WA = FMath::Max(1, Want / 2 + (Rng.FRand() < 0.5f ? Want % 2 : 0));
		SplitRooms(A, WA, MinSize, Rng, Rooms, Splits);
		SplitRooms(B, FMath::Max(1, Want - WA), MinSize, Rng, Rooms, Splits);
	}
}

AHUCrimeScene::AHUCrimeScene()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Preview = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Preview"));
	Preview->SetupAttachment(Root);
	Preview->SetRelativeLocation(FVector(0, 0, 150));
	Preview->SetHorizontalAlignment(EHTA_Center);
	Preview->SetWorldSize(40.f);
	Preview->SetTextRenderColor(FColor(255, 181, 71));
	Preview->SetText(FText::FromString(TEXT("CRIME SCENE — builds from the case when you press Play")));
	Preview->SetHiddenInGame(true);
}

UStaticMeshComponent* AHUCrimeScene::Box(FVector Pos, FVector Size, FLinearColor Color, UMaterialInterface* Override, bool bCollide)
{
	UStaticMeshComponent* C = HUBuild::Block(this, RootComponent, Blocks, HUBuild::EShape::Cube, Pos, Size, Color, FRotator::ZeroRotator, bCollide);
	if (C && Override) C->SetMaterial(0, Override);
	return C;
}

void AHUCrimeScene::Sign(const FString& Text, FVector Pos, float Yaw, float Size, FColor Color)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetupAttachment(RootComponent);
	T->SetRelativeLocation(Pos);
	T->SetRelativeRotation(FRotator(0, Yaw, 0));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Color);
	T->SetText(FText::FromString(Text));
	T->RegisterComponent();
}

void AHUCrimeScene::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void AHUCrimeScene::Build()
{
	SpawnCounts.Reset();
	UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!Cases) return;
	if (ForceCase >= 0 || !Cases->bCaseActive) { Cases->StartCase(ForceCase); Cases->bBriefed = true; }
	const FHUCase& C = Cases->Current;
	const FHUTheme& T = ThemeFor(C.Place);
	PlaceName = C.Place; LoftName = T.Loft; HighName = T.High; CrawlName = T.Crawl; BackName = T.Back;
	using namespace HUBuild;
	const FColor Gold(232, 200, 106);

	// the case's map seed decides the floor plan; the place decides the look and leans the style
	FRandomStream Rng(C.MapSeed != 0 ? C.MapSeed : C.Number * 7919 + GetTypeHash(C.Place));
	const FString Key = T.Key;
	float StyleW[4] = { 1, 1, 1, 1 };
	if (Key == TEXT("WAREHOUSE") || Key == TEXT("PIER") || Key == TEXT("STABLES")) StyleW[(int32)EStyle::Racks] += 2.5f;
	if (Key == TEXT("HOTEL") || Key == TEXT("HOSPITAL") || Key == TEXT("BANK") || Key == TEXT("SALON")) StyleW[(int32)EStyle::Rooms] += 2.5f;
	if (Key == TEXT("THEATRE") || Key == TEXT("BALLROOM") || Key == TEXT("LODGE")) StyleW[(int32)EStyle::Gallery] += 2.5f;
	if (Key == TEXT("DINER") || Key == TEXT("LOUNGE") || Key == TEXT("BOWLING")) StyleW[(int32)EStyle::Hall] += 2.5f;
	float Roll = Rng.FRand() * (StyleW[0] + StyleW[1] + StyleW[2] + StyleW[3]);
	EStyle Style = EStyle::Hall;
	for (int32 i = 0; i < 4; i++) { Roll -= StyleW[i]; if (Roll <= 0) { Style = (EStyle)i; break; } }
	LayoutName = StyleName(Style);
	// HAND-BUILT MAPS: a fixed building (see further down) instead of a generated one. Only what's in it changes.
	const bool bHotel = Key == TEXT("HOTEL");

	// footprint and height
	const float W = bHotel ? 2800.f : FMath::GridSnap(Rng.FRandRange(1900.f, 3100.f), 100.f), D = bHotel ? 2000.f : FMath::GridSnap(Rng.FRandRange(1500.f, 2300.f), 100.f);
	const FR Plan = { -W / 2, -D / 2, W / 2, D / 2 };
	const float H = bHotel ? 400.f : FMath::Clamp(T.Height + Rng.FRandRange(-60.f, 80.f), 420.f, 700.f);
	const float LoftZ = FMath::Clamp(H - 230.f, 220.f, 360.f), HighZ = FMath::Clamp(H - 150.f, 260.f, 400.f);

	// rooms
	TArray<FR> Rooms; TArray<FSplit> Splits;
	const int32 WantRooms = Style == EStyle::Hall ? Rng.RandRange(1, 2) : Style == EStyle::Rooms ? Rng.RandRange(4, 7) : Style == EStyle::Racks ? Rng.RandRange(2, 3) : Rng.RandRange(2, 3);
	if (!bHotel) SplitRooms(Plan, WantRooms, Style == EStyle::Rooms ? 650.f : 900.f, Rng, Rooms, Splits);

	TArray<FR> Solid;  // things you can't stand in (for placing people, furniture and evidence)
	TArray<FR> Keep;   // doorways and the like, kept clear of features
	auto AddSolid = [&Solid](float X0, float Y0, float X1, float Y1) { Solid.Add({ FMath::Min(X0, X1), FMath::Min(Y0, Y1), FMath::Max(X0, X1), FMath::Max(Y0, Y1) }); };

	// ---- floor, ceiling, outer walls (entrance on -X, crawlspace opening on +X) ----
	const float EntY = bHotel ? 0.f : FMath::GridSnap(Rng.FRandRange(Plan.Y0 + 250.f, Plan.Y1 - 250.f), 50.f);
	const float TunY = FMath::GridSnap(Rng.FRandRange(Plan.Y0 + 300.f, Plan.Y1 - 300.f), 50.f);
	if (!bHotel)
	{
		Box(FVector(0, 0, -5), FVector(W, D, 10), T.Floor, FloorMaterial);
		Box(FVector(0, 0, H + 5), FVector(W, D, 10), T.Wall * 0.5f, WallMaterial);
		WallWithDoor(this, RootComponent, Blocks, true, FVector(Plan.X0 - 5, 0, H / 2), D, H, 10, EntY, 200, 260, T.Wall);
		WallWithDoor(this, RootComponent, Blocks, true, FVector(Plan.X1 + 5, 0, H / 2), D, H, 10, TunY, TunnelW, TunnelH, T.Wall);
		for (float Side : { -1.f, 1.f }) Box(FVector(0, Side * (D / 2 + 5), H / 2), FVector(W, 10, H), T.Wall, WallMaterial);
		for (float Side : { -1.f, 1.f }) Box(FVector(0, Side * (D / 2 - 2), 40), FVector(W, 4, 80), T.Trim, WoodMaterial, false);
		Keep.Add({ Plan.X1 - 300, TunY - 200, Plan.X1, TunY + 200 });
		Sign(C.Place.ToUpper(), FVector(Plan.X0 + 4, EntY, 300), 0, 30, Gold);
	}
	Keep.Add({ Plan.X0, EntY - 200, Plan.X0 + 300, EntY + 200 });
	Box(FVector(Plan.X0 + 30, EntY - 60, 100), FVector(3, 120, 6), FLinearColor(1, 0.8f, 0.02f), nullptr, false); // broken police tape
	// ---- OUTSIDE: the street in front of the building - where you arrive, and how you leave ----
	{
		const float X0 = Plan.X0, YA = Plan.Y0 - 700.f, YB = Plan.Y1 + 700.f, Len = YB - YA, MidY = (YA + YB) / 2;
		const FLinearColor Sidewalk(0.3f, 0.3f, 0.29f), Road(0.04f, 0.04f, 0.045f), Curb(0.45f, 0.45f, 0.43f), Line(0.7f, 0.6f, 0.2f), Pole(0.12f, 0.13f, 0.14f);
		Box(FVector(X0 - 225, MidY, -5), FVector(450, Len, 10), Sidewalk);                                   // sidewalk out front
		Box(FVector(X0 - 452, MidY, 2), FVector(6, Len, 14), Curb);                                          // curb
		Box(FVector(X0 - 900, MidY, -6), FVector(900, Len, 10), Road);                                       // the street
		for (float Y = YA + 150; Y < YB; Y += 500) Box(FVector(X0 - 900, Y, -0.8f), FVector(10, 250, 1), Line, nullptr, false); // centre line
		Box(FVector(X0 - 1500, MidY, -5), FVector(300, Len, 10), Sidewalk);                                  // far sidewalk
		Box(FVector(X0 - 1660, MidY, 300), FVector(20, Len, 600), T.Wall * 0.35f, WallMaterial);            // buildings across the street
		for (float Y = YA + 300; Y < YB; Y += 520) Box(FVector(X0 - 1648, Y, 260), FVector(4, 120, 160), FLinearColor(1.6f, 1.2f, 0.5f) * (FMath::Fmod(Y, 3.f) < 1.f ? 0.15f : 1.f), nullptr, false); // their windows
		// the front of this building: an awning and the name over the door
		Box(FVector(X0 - 60, EntY, 285), FVector(120, 340, 8), T.Trim * 1.2f);
		Sign(C.Place.ToUpper(), FVector(X0 - 12, EntY, 330), 180, 34, Gold);
		// streetlights along the sidewalk
		auto Lamp = [this](FVector Pos, float I, float R, FLinearColor Col)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(RootComponent); L->SetRelativeLocation(Pos); L->SetIntensity(I); L->SetAttenuationRadius(R); L->SetLightColor(Col); L->RegisterComponent();
		};
		for (float Y = YA + 250; Y < YB; Y += 750)
		{
			HUBuild::Block(this, RootComponent, Blocks, HUBuild::EShape::Cylinder, FVector(X0 - 400, Y, 210), FVector(10, 10, 420), Pole);
			Box(FVector(X0 - 450, Y, 418), FVector(110, 8, 8), Pole);
			Box(FVector(X0 - 500, Y, 410), FVector(34, 22, 10), FLinearColor(3.f, 2.6f, 1.6f), nullptr, false);
			Lamp(FVector(X0 - 500, Y, 395), 7000.f, 1100.f, FLinearColor(1.f, 0.82f, 0.55f));
		}
		// police tape around the front door, with a gap to walk through
		auto Tape = [&](FVector A, FVector B)
		{
			const FVector D = B - A;
			HUBuild::Block(this, RootComponent, Blocks, HUBuild::EShape::Cube, (A + B) / 2 + FVector(0, 0, 95), FVector(D.Size2D(), 2, 6), FLinearColor(1.f, 0.8f, 0.02f), FRotator(0, D.Rotation().Yaw, 0), false);
		};
		const float TX = X0 - 330.f;
		for (const FVector& Post : { FVector(TX, EntY - 480, 0), FVector(TX, EntY + 480, 0), FVector(TX, EntY - 130, 0), FVector(TX, EntY + 130, 0) })
			Box(Post + FVector(0, 0, 50), FVector(6, 6, 100), FLinearColor(0.6f, 0.4f, 0.1f));
		Tape(FVector(X0, EntY - 480, 0), FVector(TX, EntY - 480, 0)); Tape(FVector(TX, EntY - 480, 0), FVector(TX, EntY - 130, 0));
		Tape(FVector(TX, EntY + 130, 0), FVector(TX, EntY + 480, 0)); Tape(FVector(TX, EntY + 480, 0), FVector(X0, EntY + 480, 0));
		// the cars at the curb: yours (it takes the squad back), and patrol with their lights going
		auto Car = [&](FVector At, float Yaw, bool bMine)
		{
			const FTransform CT(FRotator(0, Yaw, 0), At);
			AHUCruiser* Cr = SpawnNamed<AHUCruiser>(CT * GetActorTransform());
			if (!Cr) return;
			Cr->bPlayerCar = bMine;
			Cr->SceneLevel = TEXT("/Game/Maps/L_Precinct"); // back to base: the precinct
			Cr->DestinationLabel = TEXT("the precinct");
			Cr->FinishSpawning(CT * GetActorTransform());
		};
		Car(FVector(X0 - 600, EntY + 120, 0), 90.f, true);
		Car(FVector(X0 - 600, EntY - 520, 0), 90.f, false);
		Car(FVector(X0 - 1200, EntY + 260, 0), -90.f, false);
		// the prisoner van, back doors toward the sidewalk
		{
			const FTransform VT(FRotator(0, 90, 0), FVector(X0 - 640, EntY + 820, 0));
			if (AHUPoliceVan* Van = SpawnNamed<AHUPoliceVan>(VT * GetActorTransform())) Van->FinishSpawning(VT * GetActorTransform());
		}
		// the coffee patrol brought, on a folding table by the tape
		{
			const FTransform KT(FRotator(0, 90, 0), FVector(TX - 140, EntY + 560, 0));
			if (AHUCoffee* K = SpawnNamed<AHUCoffee>(KT * GetActorTransform())) K->FinishSpawning(KT * GetActorTransform());
		}
		// officers holding the tape
		int32 Var = 0;
		for (const FVector& At : { FVector(TX - 40, EntY - 175, 0), FVector(TX - 40, EntY + 175, 0), FVector(X0 - 860, EntY - 380, 0) })
		{
			const FTransform OT(FRotator(0, 180, 0), At);
			AHUOfficer* O = SpawnNamed<AHUOfficer>(OT * GetActorTransform());
			if (O) { O->Variant = Var++; O->FinishSpawning(OT * GetActorTransform()); }
		}
		// you arrive out here, by your car, facing the door
		PlayerStart = FVector(X0 - 470, EntY + 260, 100);
	}

	// ---- inner walls: one door per wall (two on long walls), kept away from the outer walls' features ----
	for (const FSplit& S : Splits)
	{
		const float Len = S.To - S.From;
		TArray<float> DoorsAt;
		const float Margin = Len > 1100.f ? 450.f : Len / 2 - 1.f;
		DoorsAt.Add(Rng.FRandRange(S.From + Margin, S.To - Margin));
		if (Len > 1600.f && Rng.FRand() < 0.5f) { const float A = Rng.FRandRange(S.From + 300.f, S.To - 300.f); if (FMath::Abs(A - DoorsAt[0]) > 400.f) DoorsAt.Add(A); }
		DoorsAt.Sort();
		const FLinearColor InnerWall = T.Wall * 0.85f;
		float Cursor = S.From;
		auto Seg = [&](float A, float B, float Z0, float Z1)
		{
			if (B - A < 1.f) return;
			const float M = (A + B) / 2;
			if (S.bAtX) Box(FVector(S.At, M, (Z0 + Z1) / 2), FVector(12, B - A, Z1 - Z0), InnerWall, WallMaterial);
			else Box(FVector(M, S.At, (Z0 + Z1) / 2), FVector(B - A, 12, Z1 - Z0), InnerWall, WallMaterial);
		};
		for (float Dc : DoorsAt)
		{
			Seg(Cursor, Dc - 90, 0, H);
			Seg(Dc - 90, Dc + 90, 250, H); // over the door
			Cursor = Dc + 90;
			if (S.bAtX) Keep.Add({ S.At - 200, Dc - 120, S.At + 200, Dc + 120 }); else Keep.Add({ Dc - 120, S.At - 200, Dc + 120, S.At + 200 });
		}
		Seg(Cursor, S.To, 0, H);
		if (S.bAtX) AddSolid(S.At - 10, S.From, S.At + 10, S.To); else AddSolid(S.From, S.At - 10, S.To, S.At + 10);
	}

	TArray<FArea> Areas;
	TArray<bool> RoomUsed; RoomUsed.Init(false, Rooms.Num());
	auto FreeRect = [&](const FR& R) { for (const FR& S : Solid) if (S.Overlaps(R)) return false; for (const FR& K : Keep) if (K.Overlaps(R)) return false; return true; };
	auto TouchesSide = [&](const FR& R, int32 Side) { return Side < 0 ? FMath::IsNearlyEqual(R.Y0, Plan.Y0) : FMath::IsNearlyEqual(R.Y1, Plan.Y1); };

	// ---- LOFTS: stairs up to a balcony along an outer wall (galleries get one each side) ----
	auto BuildLoft = [&](int32 RoomIdx, int32 Side) -> bool
	{
		const FR& Rm = Rooms[RoomIdx];
		if (Rm.W() < 1100.f || !TouchesSide(Rm, Side)) return false;
		const float WallY = Side < 0 ? Plan.Y0 : Plan.Y1, In = Side < 0 ? 1.f : -1.f;
		const float SX0 = Rm.X0 + 60.f, SX1 = SX0 + 592.f, LX0 = SX1, LX1 = Rm.X1 - 20.f;
		const FR Strip = { Rm.X0, FMath::Min(WallY, WallY + In * 360.f), Rm.X1, FMath::Max(WallY, WallY + In * 360.f) };
		if (!FreeRect({ SX0, Strip.Y0, LX1, Strip.Y1 })) return false;
		const float StepRise = LoftZ / 16.f;
		for (int32 i = 0; i < 16; i++)
		{
			const float Top = (i + 1) * StepRise;
			Box(FVector(SX0 + 37.f * (i + 0.5f), WallY + In * 175.f, Top / 2), FVector(37, 200, Top), T.Trim * 1.6f, WoodMaterial);
		}
		AddSolid(SX0, WallY + In * 60.f, SX1, WallY + In * 290.f);
		const float LY = WallY + In * 175.f;
		Box(FVector((LX0 + LX1) / 2, LY, LoftZ - 10), FVector(LX1 - LX0, 350, 20), T.Trim * 1.6f, WoodMaterial);
		Box(FVector((LX0 + LX1) / 2, WallY + In * 350.f, LoftZ + 50), FVector(LX1 - LX0, 5, 100), T.Trim * 0.4f); // railing
		for (float Y : { WallY + In * 37.f, WallY + In * 320.f }) Box(FVector(LX0, Y, LoftZ + 50), FVector(5, 60, 100), T.Trim * 0.4f);
		for (float X = LX0; X <= LX1; X += FMath::Max(300.f, (LX1 - LX0) / 3)) { Box(FVector(X, WallY + In * 340.f, (LoftZ - 20) / 2), FVector(20, 20, LoftZ - 20), T.Trim); AddSolid(X - 15, WallY + In * 330.f, X + 15, WallY + In * 350.f); }
		Areas.Add({ { LX0 + 60, FMath::Min(WallY + In * 30.f, WallY + In * 320.f), LX1 - 40, FMath::Max(WallY + In * 30.f, WallY + In * 320.f) }, LoftZ, ALoft });
		Sign(T.Loft, FVector(SX0 + 40, WallY + In * 320.f, LoftZ + 150), In > 0 ? 90.f : -90.f, 18, Gold);
		Keep.Add({ SX0 - 150, FMath::Min(WallY, WallY + In * 320.f), SX0, FMath::Max(WallY, WallY + In * 320.f) }); // the foot of the stairs
		return true;
	};
	{
		TArray<int32> Order; for (int32 i = 0; i < Rooms.Num(); i++) Order.Add(i);
		for (int32 i = Order.Num() - 1; i > 0; i--) Order.Swap(i, Rng.RandRange(0, i));
		const int32 WantLofts = Style == EStyle::Gallery ? 2 : (Style == EStyle::Racks ? Rng.RandRange(0, 1) : 1);
		int32 Made = 0;
		const int32 FirstSide = Rng.FRand() < 0.5f ? -1 : 1;
		for (int32 Sd : { FirstSide, -FirstSide })
		{
			if (Made >= WantLofts) break;
			for (int32 i : Order) if (!RoomUsed[i] && BuildLoft(i, Sd)) { RoomUsed[i] = true; Made++; break; }
		}
	}

	// ---- RACKS: rows of tall racks with narrow aisles between (warehouse style) ----
	auto BuildRacks = [&](int32 RoomIdx) -> bool
	{
		const FR& Rm = Rooms[RoomIdx];
		if (Rm.W() < 1000.f || Rm.D() < 800.f) return false;
		const float RX0 = Rm.X0 + 260.f, RX1 = Rm.X1 - 260.f, RH = FMath::Min(H - 100.f, 330.f);
		int32 Rows = 0;
		for (float Y = Rm.Y0 + 200.f; Y + 110.f < Rm.Y1 - 200.f; Y += 250.f)
		{
			const FR Rack = { RX0, Y, RX1, Y + 110.f };
			if (!FreeRect(Rack)) continue;
			Box(FVector(Rack.CX(), Rack.CY(), RH / 2), FVector(Rack.W(), 110, RH), T.Trim * 1.2f, WoodMaterial);
			for (float Z = 70.f; Z < RH; Z += 70.f) for (float Sd : { -1.f, 1.f }) Box(FVector(Rack.CX(), Rack.CY() + Sd * 56.f, Z), FVector(Rack.W() - 10, 3, 5), T.Trim * 0.5f, nullptr, false);
			AddSolid(Rack.X0, Rack.Y0, Rack.X1, Rack.Y1);
			if (Rows > 0) Areas.Add({ { RX0 + 60, Y - 135, RX1 - 60, Y - 5 }, 0, ANarrow }); // the aisle before this rack
			if (Rows == 0)
			{
				// a ladder up the first rack: its top is a hiding place
				const FTransform LT(FRotator(0, 180, 0), FVector(RX0 - 5, Rack.CY(), 0));
				AHULadder* L = SpawnNamed<AHULadder>(LT * GetActorTransform());
				if (L) { L->Height = RH; L->LadderMesh = LadderMesh; L->FinishSpawning(LT * GetActorTransform()); }
				Areas.Add({ { RX0 + 60, Rack.Y0 + 20, RX1 - 40, Rack.Y1 - 20 }, RH, AHigh });
				Keep.Add({ RX0 - 150, Rack.Y0 - 20, RX0, Rack.Y1 + 20 });
				Sign(T.High, FVector(RX0 - 4, Rack.CY(), RH + 50), 180, 16, Gold);
			}
			Rows++;
		}
		return Rows >= 2;
	};

	// ---- a TALL UNIT along an outer wall, with a ladder up it ----
	auto BuildTallUnit = [&](int32 RoomIdx, int32 Side) -> bool
	{
		const FR& Rm = Rooms[RoomIdx];
		if (Rm.W() < 900.f || !TouchesSide(Rm, Side)) return false;
		const float WallY = Side < 0 ? Plan.Y0 : Plan.Y1, In = Side < 0 ? 1.f : -1.f;
		const float Len = FMath::Min(650.f, Rm.W() - 400.f), UX1 = Rm.X1 - 40.f, UX0 = UX1 - Len;
		const FR U = { UX0 - 160.f, FMath::Min(WallY, WallY + In * 220.f), UX1, FMath::Max(WallY, WallY + In * 220.f) };
		if (!FreeRect(U)) return false;
		Box(FVector((UX0 + UX1) / 2, WallY + In * 110.f, HighZ / 2), FVector(Len, 220, HighZ), T.Trim * 1.2f, WoodMaterial);
		for (float Z = 80.f; Z < HighZ; Z += 80.f) Box(FVector((UX0 + UX1) / 2, WallY + In * 222.f, Z), FVector(Len - 10, 4, 6), T.Trim * 0.6f, nullptr, false);
		AddSolid(UX0, U.Y0, UX1, U.Y1);
		const FTransform LT(FRotator(0, 180, 0), FVector(UX0 - 5, WallY + In * 110.f, 0));
		AHULadder* L = SpawnNamed<AHULadder>(LT * GetActorTransform());
		if (L) { L->Height = HighZ; L->LadderMesh = LadderMesh; L->FinishSpawning(LT * GetActorTransform()); }
		Areas.Add({ { UX0 + 60, FMath::Min(WallY + In * 30.f, WallY + In * 200.f), UX1 - 40, FMath::Max(WallY + In * 30.f, WallY + In * 200.f) }, HighZ, AHigh });
		Keep.Add({ UX0 - 160, U.Y0, UX0, U.Y1 });
		Sign(T.High, FVector(UX0 - 4, WallY + In * 110.f, HighZ + 50), 180, 16, Gold);
		return true;
	};

	// ---- a NARROW PASSAGE between two partitions, dead-ending ----
	auto BuildPassage = [&](int32 RoomIdx) -> bool
	{
		const FR& Rm = Rooms[RoomIdx];
		for (int32 Try = 0; Try < 12; Try++)
		{
			const float Len = Rng.FRandRange(600.f, 900.f);
			if (Rm.W() < Len + 400.f || Rm.D() < 600.f) return false;
			const float X0 = Rng.FRandRange(Rm.X0 + 200.f, Rm.X1 - 200.f - Len), Y = Rng.FRandRange(Rm.Y0 + 250.f, Rm.Y1 - 250.f);
			const FR P = { X0, Y - 85.f, X0 + Len, Y + 85.f };
			if (!FreeRect({ P.X0 - 150, P.Y0 - 60, P.X1 + 60, P.Y1 + 60 })) continue;
			const bool bOpenLeft = Rng.FRand() < 0.5f;
			for (float Sd : { -1.f, 1.f }) Box(FVector(P.CX(), Y + Sd * 75.f, 150), FVector(Len, 20, 300), T.Wall * 0.8f, WallMaterial);
			Box(FVector(bOpenLeft ? P.X1 : P.X0, Y, 150), FVector(20, 170, 300), T.Wall * 0.8f, WallMaterial);
			AddSolid(P.X0, P.Y0, P.X1, Y - 60); AddSolid(P.X0, Y + 60, P.X1, P.Y1);
			Areas.Add({ { P.X0 + 60, Y - 18, P.X1 - 60, Y + 18 }, 0, ANarrow });
			Keep.Add({ bOpenLeft ? P.X0 - 150 : P.X1, Y - 100, bOpenLeft ? P.X0 : P.X1 + 150, Y + 100 });
			Sign(TEXT("TIGHT SQUEEZE"), FVector(bOpenLeft ? P.X0 - 4 : P.X1 + 4, Y, 260), bOpenLeft ? 180.f : 0.f, 12, FColor(200, 200, 200));
			return true;
		}
		return false;
	};

	{
		TArray<int32> Order; for (int32 i = 0; i < Rooms.Num(); i++) Order.Add(i);
		for (int32 i = Order.Num() - 1; i > 0; i--) Order.Swap(i, Rng.RandRange(0, i));
		bool bHigh = false, bPass = false;
		if (Style == EStyle::Racks) for (int32 i : Order) if (!RoomUsed[i] && BuildRacks(i)) { RoomUsed[i] = true; bHigh = bPass = true; break; }
		for (int32 i : Order) if (!bHigh) for (int32 Sd : { -1, 1 }) if (!bHigh && BuildTallUnit(i, Sd)) { bHigh = true; RoomUsed[i] = true; }
		for (int32 i : Order) if (!bPass && BuildPassage(i)) bPass = true;
		if (Style != EStyle::Racks) for (int32 i : Order) if (BuildPassage(i)) break; // a second one if there's room
	}

	// ---- the CRAWLSPACE through the +X wall, and the dark back room beyond (generated places only) ----
	if (!bHotel)
	{
	const float TX0 = Plan.X1 + 10, TX1 = TX0 + TunnelLen, BackX = TX1 + BackW / 2;
	Box(FVector((TX0 + TX1) / 2, TunY, -5), FVector(TunnelLen, TunnelW + 20, 10), T.Floor * 0.6f, FloorMaterial);
	Box(FVector((TX0 + TX1) / 2, TunY, TunnelH + 5), FVector(TunnelLen, TunnelW + 20, 10), T.Wall * 0.4f);
	for (float Side : { -1.f, 1.f }) Box(FVector((TX0 + TX1) / 2, TunY + Side * (TunnelW / 2 + 5), TunnelH / 2), FVector(TunnelLen, 10, TunnelH), T.Wall * 0.4f);
	Box(FVector(BackX, TunY, -5), FVector(BackW, BackW, 10), T.Floor * 0.8f, FloorMaterial);
	Box(FVector(BackX, TunY, BackH + 5), FVector(BackW, BackW, 10), T.Wall * 0.4f);
	WallWithDoor(this, RootComponent, Blocks, true, FVector(TX1 + 5, TunY, BackH / 2), BackW, BackH, 10, 0, TunnelW, TunnelH, T.Wall * 0.7f);
	Box(FVector(TX1 + BackW + 5, TunY, BackH / 2), FVector(10, BackW, BackH), T.Wall * 0.7f, WallMaterial);
	for (float Side : { -1.f, 1.f }) Box(FVector(BackX, TunY + Side * (BackW / 2 + 5), BackH / 2), FVector(BackW, 10, BackH), T.Wall * 0.7f, WallMaterial);
	Sign(T.Crawl, FVector(Plan.X1 - 4, TunY, TunnelH + 30), 180, 16, Gold);
	Sign(T.Back, FVector(TX1 + BackW, TunY, 200), 180, 22, Gold);
	Areas.Add({ { TX1 + 80, TunY - 240, TX1 + BackW - 60, TunY + 240 }, 0, ABack });
	CrawlZone = NewObject<UBoxComponent>(this);
	CrawlZone->SetupAttachment(RootComponent);
	CrawlZone->SetRelativeLocation(FVector((Plan.X1 - 50 + TX1 + 60) / 2, TunY, 60));
	CrawlZone->SetBoxExtent(FVector((TX1 + 60 - (Plan.X1 - 50)) / 2, TunnelW / 2 + 15, 60));
	CrawlZone->SetCollisionProfileName(TEXT("Trigger"));
	CrawlZone->SetGenerateOverlapEvents(true);
	CrawlZone->RegisterComponent();
	CrawlZone->OnComponentBeginOverlap.AddDynamic(this, &AHUCrimeScene::OnCrawlEnter);
	CrawlZone->OnComponentEndOverlap.AddDynamic(this, &AHUCrimeScene::OnCrawlLeave);
	}

	// ---- lights: one per room, except a room whose bulb is out (evidence in there needs a flashlight) ----
	auto Light = [this](FVector Pos, float I, float R, FLinearColor Col)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetRelativeLocation(Pos);
		L->SetIntensity(I);
		L->SetAttenuationRadius(R);
		L->SetLightColor(Col);
		L->RegisterComponent();
	};
	int32 DarkRoom = -1;
	if (Rooms.Num() >= 3 && Rng.FRand() < 0.6f)
		for (int32 Try = 0; Try < 6 && DarkRoom < 0; Try++) { const int32 i = Rng.RandRange(0, Rooms.Num() - 1); if (!Rooms[i].Contains(PlayerStart.X, PlayerStart.Y)) DarkRoom = i; }
	for (int32 i = 0; i < Rooms.Num(); i++)
	{
		const FR& Rm = Rooms[i];
		if (i == DarkRoom) { Areas.Add({ { Rm.X0 + 80, Rm.Y0 + 80, Rm.X1 - 80, Rm.Y1 - 80 }, 0, ADark }); continue; }
		const float Big = FMath::Max(Rm.W(), Rm.D());
		Light(FVector(Rm.CX(), Rm.CY(), H - 60), 4000.f + Big * 3.f, Big * 0.9f + 400.f, FLinearColor(1.f, 0.9f, 0.75f));
		Areas.Add({ { Rm.X0 + 80, Rm.Y0 + 80, Rm.X1 - 80, Rm.Y1 - 80 }, 0, AOpen });
	}
	for (const FArea& A : Areas) if (A.Kind == ALoft) Light(FVector(A.Rect.CX(), A.Rect.CY(), A.Z + 180), 2500.f, 800.f, FLinearColor(1.f, 0.85f, 0.65f));

	// =====================================================================================================
	// THE HOTEL - a fixed building you get to know case by case (where things end up still changes).
	// Offenders here: stalker, crime of passion, poisoner (HUProfiles::ForPlace).
	//   GROUND (z 0): the LOBBY (front desk, the stairs up), the BAR, SERVICE (staff only - the stairs down)
	//   BASEMENT (z -400): LAUNDRY & BOILER ROOM, no lights
	//   2ND / 3RD FLOORS (z 400 / 800): a corridor, five guest rooms, the stair landing at the west end
	//   OUTSIDE: the street out front, the ALLEY out back, the FIRE ESCAPE up the back wall, the ROOF (z 1200)
	// =====================================================================================================
	if (bHotel)
	{
		LayoutName = TEXT("the hotel - lobby and bar, two guest floors, a basement, the fire escape and the roof");
		const float X0 = Plan.X0, X1 = Plan.X1, Y0 = Plan.Y0, Y1 = Plan.Y1, FH = 400.f, Th = 12.f;
		const FLinearColor Carpet = T.Floor, Paper = T.Wall, Wood = T.Trim, Brick(0.28f, 0.14f, 0.1f), Concrete(0.22f, 0.22f, 0.21f), Steel(0.18f, 0.19f, 0.2f);
		const FColor SignCol(232, 200, 106);
		// a wall at X=At (bAtX) or Y=At, From..To, standing on Z0, with doorways (2.4 m high) centred on Doors
		auto Wall = [&](bool bAtX, float At, float From, float To, float Z0, TArray<float> Doors, FLinearColor Col, float DoorW = 120.f)
		{
			Doors.Sort();
			auto Seg = [&](float A, float B, float Za, float Zb)
			{
				if (B - A < 1.f) return;
				const float M = (A + B) / 2;
				if (bAtX) Box(FVector(At, M, Z0 + (Za + Zb) / 2), FVector(Th, B - A, Zb - Za), Col, WallMaterial);
				else Box(FVector(M, At, Z0 + (Za + Zb) / 2), FVector(B - A, Th, Zb - Za), Col, WallMaterial);
			};
			float Cur = From;
			for (float Dc : Doors)
			{
				Seg(Cur, Dc - DoorW / 2, 0, FH);
				Seg(Dc - DoorW / 2, Dc + DoorW / 2, 240, FH);
				Cur = Dc + DoorW / 2;
				if (FMath::Abs(Z0) < 1.f) { if (bAtX) Keep.Add({ At - 160, Dc - 100, At + 160, Dc + 100 }); else Keep.Add({ Dc - 100, At - 160, Dc + 100, At + 160 }); }
			}
			Seg(Cur, To, 0, FH);
			if (FMath::Abs(Z0) < 1.f) { if (bAtX) AddSolid(At - 10, From, At + 10, To); else AddSolid(From, At - 10, To, At + 10); }
		};
		// a floor slab whose top is at Z, with one stairwell hole (or none)
		auto Slab = [&](float Z, FR Hole, FLinearColor Col)
		{
			auto Piece = [&](float A0, float B0, float A1, float B1) { if (A1 - A0 > 1.f && B1 - B0 > 1.f) Box(FVector((A0 + A1) / 2, (B0 + B1) / 2, Z - 10), FVector(A1 - A0, B1 - B0, 20), Col, FloorMaterial); };
			if (Hole.W() <= 0.f) { Piece(X0, Y0, X1, Y1); return; }
			Piece(X0, Y0, X1, Hole.Y0); Piece(X0, Hole.Y1, X1, Y1); Piece(X0, Hole.Y0, Hole.X0, Hole.Y1); Piece(Hole.X1, Hole.Y0, X1, Hole.Y1);
		};
		// a flight of 16 steps, 2 m wide, climbing one storey along X from XFrom (Dir +1 / -1)
		auto Flight = [&](float XFrom, float Y, float Z0, float Dir)
		{
			for (int32 i = 0; i < 16; i++)
			{
				const float Top = (i + 1) * FH / 16.f;
				Box(FVector(XFrom + Dir * 37.f * (i + 0.5f), Y, Z0 + Top / 2), FVector(37, 200, Top), Wood * 1.4f, WoodMaterial);
			}
			const float XA = FMath::Min(XFrom, XFrom + Dir * 592.f), XB = FMath::Max(XFrom, XFrom + Dir * 592.f);
			if (Z0 < 1.f) AddSolid(XA, Y - 100, XB, Y + 100);
		};
		auto Lamp = [this](FVector Pos, float I, float R, FLinearColor Col)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(RootComponent); L->SetRelativeLocation(Pos); L->SetIntensity(I); L->SetAttenuationRadius(R); L->SetLightColor(Col); L->RegisterComponent();
		};
		const FLinearColor Warm(1.f, 0.85f, 0.65f);

		// ---- the stairs: lobby -> 2nd (south wall), 2nd -> 3rd (doubling back), service -> basement ----
		const FR HoleG2 = { -900, -1000, -308, -770 }, Hole23 = { -900, -745, -308, -515 }, HoleB = { 708, -1000, 1300, -770 };
		Flight(-900, -880, 0, 1);     // lobby up to the 2nd floor landing
		Flight(-308, -630, FH, -1);   // 2nd floor landing up to the 3rd
		Flight(708, -880, -FH, 1);    // basement up to the service room

		// ---- slabs: ground (hole for the basement stairs), 2nd, 3rd, the roof ----
		Slab(0, HoleB, Carpet);
		Slab(FH, HoleG2, Carpet * 0.9f);
		Slab(2 * FH, Hole23, Carpet * 0.8f);
		Slab(3 * FH, FR(), Concrete);
		// the basement: under the service room
		Box(FVector(800, -500, -FH - 10), FVector(1200, 1000, 20), Concrete, FloorMaterial);
		Wall(true, 200, -1000, 0, -FH, {}, Concrete * 0.8f); Wall(true, 1400, -1000, 0, -FH, {}, Concrete * 0.8f);
		Wall(false, -1000, 200, 1400, -FH, {}, Concrete * 0.8f); Wall(false, 0, 200, 1400, -FH, {}, Concrete * 0.8f);
		Box(FVector(330, -200, -FH + 110), FVector(160, 160, 220), Steel); AddSolid(250, -280, 410, -120);                 // the boiler
		for (int32 k = 0; k < 3; k++) { Box(FVector(560 + k * 110, -80, -FH + 50), FVector(90, 80, 100), FLinearColor(0.75f, 0.75f, 0.72f)); } // washers
		AddSolid(510, -120, 840, -40);
		Sign(TEXT("LAUNDRY"), FVector(800, -15, -120), 90, 18, SignCol);

		// ---- outside walls, three storeys: the front door on the street, the back door to the alley ----
		for (int32 F = 0; F < 3; F++)
		{
			const float Z = F * FH;
			Wall(true, X0, Y0, Y1, Z, F == 0 ? TArray<float>{ 0.f } : TArray<float>{}, Paper * 0.9f, 220.f);
			Wall(true, X1, Y0, Y1, Z, F == 0 ? TArray<float>{ 500.f } : TArray<float>{ 0.f }, Brick);
			Wall(false, Y0, X0, X1, Z, {}, Brick); Wall(false, Y1, X0, X1, Z, {}, Brick);
			// lit windows on the street side
			if (F > 0) for (float Y : { -700.f, -250.f, 250.f, 700.f }) Box(FVector(X0 - 8, Y, Z + 200), FVector(4, 140, 160), FLinearColor(1.6f, 1.2f, 0.55f) * (FMath::Abs(Y) > 500.f ? 0.25f : 1.f), nullptr, false);
		}
		// the roof: a parapet all round, with a gap where the fire escape ladder comes up
		{
			const float RZ = 3 * FH;
			Box(FVector(X0 + 6, 0, RZ + 50), FVector(12, Y1 - Y0, 100), Brick);
			Box(FVector(0, Y0 + 6, RZ + 50), FVector(X1 - X0, 12, 100), Brick);
			Box(FVector(0, Y1 - 6, RZ + 50), FVector(X1 - X0, 12, 100), Brick);
			Box(FVector(X1 - 6, (Y0 + 240) / 2, RZ + 50), FVector(12, 240 - Y0, 100), Brick);
			Box(FVector(X1 - 6, (360 + Y1) / 2, RZ + 50), FVector(12, Y1 - 360, 100), Brick);
			Box(FVector(-600, 500, RZ + 120), FVector(240, 240, 240), Concrete * 1.2f);  // the water tank's housing
			Box(FVector(500, -400, RZ + 60), FVector(300, 160, 120), Steel);             // air-con unit
			Sign(C.Place.ToUpper(), FVector(X0 - 14, 0, RZ + 120), 180, 60, SignCol);    // the big sign over the street
		}

		// ---- GROUND: the lobby, the bar, the service room ----
		Wall(true, 200, Y0, Y1, 0, { -500.f, 500.f }, Paper);
		Wall(false, 0, 200, X1, 0, { 450.f }, Paper);
		Box(FVector(-300, 600, 55), FVector(100, 500, 110), Wood * 1.3f, WoodMaterial); AddSolid(-350, 350, -250, 850);  // the front desk
		Box(FVector(-200, 600, 160), FVector(10, 420, 200), Wood * 0.6f, WoodMaterial);                                    // key boxes behind it
		Box(FVector(850, 680, 55), FVector(700, 80, 110), Wood * 1.2f, WoodMaterial); AddSolid(500, 640, 1200, 720);      // the bar
		Box(FVector(850, 960, 120), FVector(700, 60, 240), Wood * 0.7f, WoodMaterial); AddSolid(500, 930, 1200, 990);     // bottles
		Box(FVector(1100, -300, 45), FVector(260, 120, 90), Steel); AddSolid(970, -360, 1230, -240);                       // service: a steel worktable
		Sign(TEXT("LOBBY"), FVector(-1380, 600, 300), 0, 20, SignCol);
		Sign(TEXT("BAR"), FVector(206, 500, 300), 0, 20, SignCol);
		Sign(TEXT("STAFF ONLY"), FVector(206, -500, 290), 0, 14, SignCol);
		Sign(TEXT("FLOORS 2 - 3"), FVector(-920, -770, 230), 0, 14, SignCol);

		// ---- GUEST FLOORS: corridor down the middle, three rooms north, two south-east, the stair landing ----
		const FR North[] = { { X0, 150, -467, Y1 }, { -467, 150, 467, Y1 }, { 467, 150, X1, Y1 } };
		const FR South[] = { { 0, Y0, 700, -150 }, { 700, Y0, X1, -150 } };
		const int32 DarkGuest = Rng.RandRange(0, 9); // one guest room with the bulb out
		int32 GuestN = 0;
		for (int32 F = 1; F <= 2; F++)
		{
			const float Z = F * FH;
			Wall(false, 150, X0, X1, Z, { -930.f, 0.f, 930.f }, Paper);
			Wall(false, -150, 0, X1, Z, { 350.f, 1050.f }, Paper);
			Wall(true, -467, 150, Y1, Z, {}, Paper); Wall(true, 467, 150, Y1, Z, {}, Paper);
			Wall(true, 0, Y0, -150, Z, {}, Paper); Wall(true, 700, Y0, -150, Z, {}, Paper);
			Sign(FString::Printf(TEXT("FLOOR %d"), F + 1), FVector(-1380, -500, Z + 280), 0, 20, SignCol);
			// the corridor: a runner and lamps
			Box(FVector(0, 0, Z + 1), FVector(2700, 160, 2), FLinearColor(0.3f, 0.05f, 0.05f), nullptr, false);
			for (float X : { -900.f, 0.f, 900.f }) Lamp(FVector(X, 0, Z + FH - 50), 2200.f, 700.f, Warm);
			Areas.Add({ { X0 + 30, -120, X1 - 30, 120 }, Z, ANarrow });
			// the stair landing (west of the stairs)
			Areas.Add({ { X0 + 30, Y0 + 30, -930, -180 }, Z, AOpen });
			Lamp(FVector(-700, -600, Z + FH - 50), 2500.f, 900.f, Warm);
			// the rooms: a bed against the outside wall, a nightstand, a number on the door
			auto Guest = [&](const FR& Rm, bool bNorth)
			{
				const float BedY = bNorth ? Rm.Y1 - 110 : Rm.Y0 + 110;
				Box(FVector(Rm.CX(), BedY, Z + 28), FVector(170, 200, 56), FLinearColor(0.7f, 0.68f, 0.6f));
				Box(FVector(Rm.CX(), bNorth ? Rm.Y1 - 12 : Rm.Y0 + 12, Z + 80), FVector(180, 12, 160), Wood * 0.8f, WoodMaterial);
				Box(FVector(Rm.CX() + 140, BedY + (bNorth ? 60 : -60), Z + 30), FVector(50, 50, 60), Wood, WoodMaterial);
				const int32 Num = (F + 1) * 100 + (++GuestN);
				Sign(FString::FromInt(Num), FVector(Rm.CX() + 90, bNorth ? 144 : -144, Z + 230), bNorth ? -90.f : 90.f, 14, SignCol);
				const FR A = bNorth ? FR{ Rm.X0 + 30, Rm.Y0 + 30, Rm.X1 - 30, Rm.Y1 - 240 } : FR{ Rm.X0 + 30, Rm.Y0 + 240, Rm.X1 - 30, Rm.Y1 - 30 };
				const bool bDark = (F - 1) * 5 + GuestN - 1 == DarkGuest;
				Areas.Add({ A, Z, bDark ? ADark : AOpen });
				if (!bDark) Lamp(FVector(Rm.CX(), Rm.CY(), Z + FH - 60), 5000.f, 1000.f, Warm);
			};
			GuestN = 0;
			for (const FR& Rm : North) Guest(Rm, true);
			for (const FR& Rm : South) Guest(Rm, false);
		}

		// ---- THE ALLEY out back, and the FIRE ESCAPE up the back wall ----
		const float AX1 = X1 + 600;
		Box(FVector((X1 + AX1) / 2, 0, -5), FVector(600, Y1 - Y0, 10), Concrete * 0.8f, FloorMaterial);
		Box(FVector(AX1 + 6, 0, 250), FVector(12, Y1 - Y0, 500), Brick * 0.7f, WallMaterial);
		for (float Y : { Y0, Y1 }) Box(FVector((X1 + AX1) / 2, Y, 250), FVector(600, 12, 500), Brick * 0.7f, WallMaterial);
		Box(FVector(AX1 - 80, -500, 70), FVector(120, 220, 140), FLinearColor(0.1f, 0.25f, 0.12f)); AddSolid(AX1 - 140, -610, AX1 - 20, -390); // the dumpster
		Sign(TEXT("FIRE ESCAPE"), FVector(X1 + 8, 300, 280), 0, 16, SignCol);
		Lamp(FVector(X1 + 60, 500, 330), 1800.f, 900.f, FLinearColor(1.f, 0.75f, 0.45f));   // the light over the back door
		Areas.Add({ { X1 + 80, Y0 + 80, AX1 - 80, Y1 - 80 }, 0, AOpen });
		for (int32 F = 1; F <= 2; F++)
		{
			const float Z = F * FH;
			Box(FVector(X1 + 80, 100, Z - 5), FVector(160, 700, 10), Steel);                                              // the landing
			// the railing, with gaps where the ladders meet it (Y 300: from the alley, Y -100: on up)
			for (const FVector2D& R : { FVector2D(-250, -160), FVector2D(-40, 240), FVector2D(360, 450) })
				Box(FVector(X1 + 158, (R.X + R.Y) / 2, Z + 50), FVector(4, R.Y - R.X, 100), Steel);
			Areas.Add({ { X1 + 20, -230, X1 + 140, 420 }, Z, AHigh });
		}
		auto Ladder = [&](FVector At, float Yaw, float Height)
		{
			const FTransform LT(FRotator(0, Yaw, 0), At);
			AHULadder* L = SpawnNamed<AHULadder>(LT * GetActorTransform());
			if (L) { L->Height = Height; L->LadderMesh = LadderMesh; L->FinishSpawning(LT * GetActorTransform()); }
		};
		Ladder(FVector(X1 + 165, 300, 0), 0.f, FH);          // alley up to the 2nd floor landing
		Ladder(FVector(X1 + 165, -100, FH), 0.f, FH);        // 2nd floor landing up to the 3rd
		Ladder(FVector(X1 + 8, 300, 2 * FH), 0.f, FH);       // 3rd floor landing up onto the roof
		Keep.Add({ X1 + 160, 200, X1 + 300, 400 });

		// ---- the roof, the basement ----
		Areas.Add({ { X0 + 40, Y0 + 40, X1 - 40, Y1 - 40 }, 3 * FH, AOpen });
		Lamp(FVector(0, 0, 3 * FH + 600), 1500.f, 2400.f, FLinearColor(0.55f, 0.65f, 1.f)); // moonlight
		Areas.Add({ { 230, -970, 1370, -30 }, -FH, ADark });

		// ---- GROUND lights and areas ----
		Rooms = { { X0, Y0, 200, Y1 }, { 200, 0, X1, Y1 }, { 200, Y0, X1, 0 } }; // lobby, bar, service (the cameras use these)
		for (const FR& Rm : Rooms)
		{
			// a chandelier every 7 m or so: the lobby's big
			const int32 NX = FMath::Max(1, FMath::RoundToInt(Rm.W() / 700.f)), NY = FMath::Max(1, FMath::RoundToInt(Rm.D() / 700.f));
			for (int32 a = 0; a < NX; a++) for (int32 b = 0; b < NY; b++)
				Lamp(FVector(Rm.X0 + Rm.W() * (a + 0.5f) / NX, Rm.Y0 + Rm.D() * (b + 0.5f) / NY, FH - 60), 6000.f, 900.f, Warm);
			Areas.Add({ { Rm.X0 + 80, Rm.Y0 + 80, Rm.X1 - 80, Rm.Y1 - 80 }, 0, AOpen });
		}
	}

	// ---- spot pickers ----
	TArray<FVector> Taken; // things already placed (local)
	auto FloorFree = [&](float X, float Y, float R)
	{
		for (const FR& S : Solid) if (S.Contains(X, Y, -R)) return false;
		for (const FR& K : Keep) if (K.Contains(X, Y)) return false;
		for (const FVector& V : Taken) if (FMath::Abs(V.Z) < 50.f && FVector::Dist2D(V, FVector(X, Y, 0)) < R) return false;
		return true;
	};
	auto SpotIn = [&](const FArea& A, float R, FRandomStream& S, FVector& Out) -> bool
	{
		for (int32 Try = 0; Try < 40; Try++)
		{
			const float X = S.FRandRange(A.Rect.X0, A.Rect.X1), Y = S.FRandRange(A.Rect.Y0, A.Rect.Y1);
			if (A.Z < 1.f && !FloorFree(X, Y, R)) continue;
			bool bClear = true;
			for (const FVector& V : Taken) if (FMath::Abs(V.Z - A.Z) < 50.f && FVector::Dist2D(V, FVector(X, Y, 0)) < R) { bClear = false; break; }
			if (!bClear) continue;
			Out = FVector(X, Y, A.Z);
			return true;
		}
		return false;
	};
	auto OpenSpot = [&](float R, FRandomStream& S, FVector& Out) -> bool
	{
		TArray<int32> Open; for (int32 i = 0; i < Areas.Num(); i++) if (Areas[i].Kind == AOpen) Open.Add(i);
		for (int32 Try = 0; Try < 8 && Open.Num(); Try++) if (SpotIn(Areas[Open[S.RandRange(0, Open.Num() - 1)]], R, S, Out)) return true;
		return false;
	};

	// ---- the victim: the body where it fell (or, if they lived or vanished, what's left of them) ----
	AHUVictim* Victim = nullptr;
	FVector VictimAt = FVector::ZeroVector;
	{
		const EHUVictimKind VK = C.Crime == TEXT("assault") || C.Crime == TEXT("robbery") ? EHUVictimKind::Outline
			: C.Crime == TEXT("missing") ? EHUVictimKind::Belongings : C.Crime == TEXT("arson") ? EHUVictimKind::Scorch : EHUVictimKind::Body;
		if (OpenSpot(VK == EHUVictimKind::Scorch ? 260.f : 220.f, Rng, VictimAt))
		{
			Taken.Add(VictimAt);
			const FTransform VT(FRotator(0, Rng.FRandRange(0.f, 360.f), 0), VictimAt);
			Victim = SpawnNamed<AHUVictim>(VT * GetActorTransform());
			if (Victim)
			{
				Victim->Kind = VK;
				const FLinearColor Coats2[] = { {0.18f, 0.14f, 0.1f}, {0.1f, 0.12f, 0.18f}, {0.25f, 0.06f, 0.06f}, {0.3f, 0.28f, 0.22f} };
				Victim->Coat = Coats2[Rng.RandRange(0, 3)];
				Victim->FinishSpawning(VT * GetActorTransform());
			}
		}
	}

	TArray<FVector> SusAt; TArray<FLinearColor> SusCoat; TArray<FString> SusId; // for the security tapes
	// ---- the suspects, spread through the lit rooms, each facing the middle of their room ----
	const FLinearColor Coats[] = { {0.25f, 0.18f, 0.1f}, {0.1f, 0.12f, 0.2f}, {0.3f, 0.05f, 0.05f}, {0.12f, 0.2f, 0.12f}, {0.35f, 0.3f, 0.2f}, {0.05f, 0.05f, 0.05f} };
	for (int32 i = 0; i < C.Suspects.Num(); i++)
	{
		FVector At;
		if (!OpenSpot(170.f, Rng, At)) continue;
		Taken.Add(At);
		FVector Mid = At; for (const FR& Rm : Rooms) if (Rm.Contains(At.X, At.Y)) Mid = FVector(Rm.CX(), Rm.CY(), 0);
		const FTransform ST(FRotator(0, (Mid - At).IsNearlyZero() ? Rng.FRandRange(0.f, 360.f) : (Mid - At).Rotation().Yaw, 0), At);
		AHUSuspectActor* Su = SpawnNamed<AHUSuspectActor>(ST * GetActorTransform());
		if (!Su) continue;
		Su->SuspectId = C.Suspects[i].Id; Su->Coat = Coats[i % UE_ARRAY_COUNT(Coats)];
		SusAt.Add(At); SusCoat.Add(Su->Coat); SusId.Add(C.Suspects[i].Id);
		Su->FinishSpawning(ST * GetActorTransform());
	}

	// From here on it's random every time you play: the map is the case's, but where things ended up isn't.
	FRandomStream Loot(Cases->SquadSeed != 0 ? (C.MapSeed ^ Cases->SquadSeed) : FMath::Rand()); // a squad all sees the same scene
	int32 DeadEndCount, HiddenCount, LiesNeeded;
	Cases->GetRules(DeadEndCount, HiddenCount, LiesNeeded);

	// ---- heavy furniture ----
	TArray<AHUPushable*> Pushables;
	const int32 FurnitureCount = FMath::Max(4, HiddenCount + 2) + Rooms.Num() / 2;
	for (int32 i = 0; i < FurnitureCount; i++)
	{
		FVector At;
		if (!OpenSpot(200.f, Loot, At)) continue;
		Taken.Add(At);
		const FTransform PT(FRotator(0, Loot.FRandRange(-25.f, 25.f) + (Loot.FRand() < 0.5f ? 90.f : 0.f), 0), At + FVector(0, 0, T.PushSize.Z / 2));
		AHUPushable* Pu = SpawnNamed<AHUPushable>(PT * GetActorTransform());
		if (!Pu) continue;
		Pu->Label = T.Push; Pu->SizeCm = T.PushSize; Pu->Color = T.PushColor; Pu->Mesh = FurnitureMesh;
		Pu->FinishSpawning(PT * GetActorTransform());
		Pushables.Add(Pu);
	}

	// ---- which evidence goes in: every piece that proves a lie, plus some dead ends ----
	TArray<FString> Key2 = Cases->GetKeyEvidence(), Dead = Cases->GetDeadEnds();
	auto Shuffle = [&Loot](TArray<FString>& A) { for (int32 i = A.Num() - 1; i > 0; i--) A.Swap(i, Loot.RandRange(0, i)); };
	Shuffle(Key2); Shuffle(Dead);
	if (Dead.Num() > DeadEndCount) Dead.SetNum(DeadEndCount);
	TArray<FString> All = Key2; All.Append(Dead);
	Shuffle(All);
	TSet<FString> Under;
	for (int32 i = 0; i < All.Num() && Under.Num() < FMath::Min(HiddenCount, Pushables.Num()); i++) Under.Add(All[i]);

	// ---- where each goes: key evidence leans to the hard places, dead ends to the open floor ----
	int32 Used[ACount] = { 0 };
	const int32 Cap[ACount] = { 3, 2, 2, 3, 3, 99 };
	const float KeyW[ACount] = { 3, 2, 1.5f, 3, 2, 2 }, DeadW[ACount] = { 1, 0.5f, 0, 1, 0.5f, 4 }, FreeW[ACount] = { 3, 2, 1.5f, 0, 0, 2 };
	bool bHas[ACount] = { false }; for (const FArea& A : Areas) bHas[A.Kind] = true;
	auto PickKind = [&](const float* Wt) -> EArea
	{
		float Sum = 0; for (int32 a = 0; a < ACount; a++) if (bHas[a] && Used[a] < Cap[a]) Sum += Wt[a];
		float Rr = Loot.FRand() * Sum;
		for (int32 a = 0; a < ACount; a++) { if (!bHas[a] || Used[a] >= Cap[a]) continue; Rr -= Wt[a]; if (Rr <= 0) return (EArea)a; }
		return AOpen;
	};
	auto SpotOfKind = [&](EArea Kind, FVector& Out) -> bool
	{
		TArray<int32> Of; for (int32 i = 0; i < Areas.Num(); i++) if (Areas[i].Kind == Kind) Of.Add(i);
		for (int32 Try = 0; Try < 6 && Of.Num(); Try++) if (SpotIn(Areas[Of[Loot.RandRange(0, Of.Num() - 1)]], 90.f, Loot, Out)) return true;
		return false;
	};

	// a TUCKED spot in a floor area: where people don't look. Corners, hard against a wall, on the far side of
	// furniture from the middle of the room, or wedged beside a built-in fixture - instead of the open floor.
	auto TuckedSpot = [&](const FArea& A, FVector& Out) -> bool
	{
		// ground-floor areas sit 80 cm in from the walls; the hand-built upper floors and basement are drawn to the walls already
		const bool bGround = FMath::Abs(A.Z) < 1.f;
		const float Pad = bGround ? 80.f : 0.f;
		const FR Room{ A.Rect.X0 - Pad, A.Rect.Y0 - Pad, A.Rect.X1 + Pad, A.Rect.Y1 + Pad };
		const float Gap = 28.f; // from a wall: right up against it
		TArray<FVector2D> Cand;
		for (float X : { Room.X0 + Gap, Room.X1 - Gap }) for (float Y : { Room.Y0 + Gap, Room.Y1 - Gap }) Cand.Add(FVector2D(X, Y));
		for (int32 k = 0; k < 8; k++)
		{
			const float U = Loot.FRandRange(0.1f, 0.9f);
			switch (Loot.RandRange(0, 3))
			{
			case 0: Cand.Add(FVector2D(FMath::Lerp(Room.X0, Room.X1, U), Room.Y0 + Gap)); break;
			case 1: Cand.Add(FVector2D(FMath::Lerp(Room.X0, Room.X1, U), Room.Y1 - Gap)); break;
			case 2: Cand.Add(FVector2D(Room.X0 + Gap, FMath::Lerp(Room.Y0, Room.Y1, U))); break;
			default: Cand.Add(FVector2D(Room.X1 - Gap, FMath::Lerp(Room.Y0, Room.Y1, U))); break;
			}
		}
		for (AHUPushable* Pu : Pushables)
		{
			if (!Pu) continue;
			const FVector L = GetActorTransform().InverseTransformPosition(Pu->GetActorLocation());
			if (!Room.Contains(L.X, L.Y) || FMath::Abs(L.Z - Pu->SizeCm.Z / 2 - A.Z) > 50.f) continue; // (on this floor)
			FVector2D Away(L.X - Room.CX(), L.Y - Room.CY());
			Away = Away.IsNearlyZero() ? FVector2D(1, 0) : Away.GetSafeNormal();
			const float Reach = FMath::Max(Pu->SizeCm.X, Pu->SizeCm.Y) * 0.5f + 30.f;
			Cand.Add(FVector2D(L.X, L.Y) + Away * Reach);
			Cand.Add(FVector2D(L.X, L.Y) + FVector2D(-Away.Y, Away.X) * Reach); // or round the side of it
		}
		for (const FR& S : Solid)
		{
			if (!bGround || !Room.Contains(S.CX(), S.CY(), -100.f)) continue; // fixtures are only known on the ground floor
			const float U = Loot.FRandRange(0.15f, 0.85f);
			Cand.Add(FVector2D(FMath::Lerp(S.X0, S.X1, U), Loot.FRand() < 0.5f ? S.Y0 - 24.f : S.Y1 + 24.f));
			Cand.Add(FVector2D(Loot.FRand() < 0.5f ? S.X0 - 24.f : S.X1 + 24.f, FMath::Lerp(S.Y0, S.Y1, U)));
		}
		for (int32 i = Cand.Num() - 1; i > 0; i--) Cand.Swap(i, Loot.RandRange(0, i));
		for (const FVector2D& P : Cand)
		{
			if (!Room.Contains(P.X, P.Y, Gap - 1.f) || !FloorFree(P.X, P.Y, 12.f)) continue;
			bool bClear = true;
			for (const FVector& V : Taken) if (FMath::Abs(V.Z - A.Z) < 50.f && FVector::Dist2D(V, FVector(P.X, P.Y, 0)) < 70.f) { bClear = false; break; }
			if (!bClear) continue;
			Out = FVector(P.X, P.Y, A.Z);
			return true;
		}
		return false;
	};
	// how often a floor piece is tucked away instead of lying in the open (dead ends half as often: the easy finds are decoys)
	const float TuckChance = Cases->PlayDifficulty == TEXT("rookie") ? 0.5f : Cases->PlayDifficulty == TEXT("veteran") ? 0.95f : 0.8f;

	const float PlainChance = Cases->PlayDifficulty == TEXT("rookie") ? 0.7f : Cases->PlayDifficulty == TEXT("veteran") ? 0.25f : 0.45f;
	int32 FreeKeyLeft = LiesNeeded + 1; // always findable with no tools, so a case can be closed with an empty kit
	for (const FString& Id : All) if (Under.Contains(Id) && Key2.Contains(Id)) FreeKeyLeft--;

	// one piece of real evidence is on the victim (the coat pocket, the bag) - search it with Q
	FString PocketId;
	if (Victim && (Victim->Kind == EHUVictimKind::Body || Victim->Kind == EHUVictimKind::Belongings))
		for (const FString& Id : Key2) if (!Under.Contains(Id)) { PocketId = Id; if (FreeKeyLeft > 0) FreeKeyLeft--; break; }

	Cases->InScene.Reset();
	int32 Marker = 0, NextCover = 0;
	for (const FString& Id : All)
	{
		FVector At;
		AHUPushable* Cover = nullptr;
		EHUConceal Conceal = EHUConceal::Plain;
		const bool bPocket = Id == PocketId;
		if (bPocket) At = VictimAt;
		else if (Under.Contains(Id) && Pushables.IsValidIndex(NextCover))
		{
			Cover = Pushables[NextCover++];
			At = GetActorTransform().InverseTransformPosition(Cover->GetActorLocation()) * FVector(1, 1, 0);
		}
		else
		{
			const bool bFree = Key2.Contains(Id) && FreeKeyLeft > 0;
			EArea Kind = PickKind(bFree ? FreeW : Key2.Contains(Id) ? KeyW : DeadW);
			bool bPlaced = false;
			if ((Kind == AOpen || Kind == ADark) && Loot.FRand() < (Key2.Contains(Id) ? TuckChance : TuckChance * 0.5f))
			{
				TArray<int32> Of; for (int32 i = 0; i < Areas.Num(); i++) if (Areas[i].Kind == Kind) Of.Add(i);
				for (int32 Try = 0; Try < 4 && Of.Num() && !bPlaced; Try++) bPlaced = TuckedSpot(Areas[Of[Loot.RandRange(0, Of.Num() - 1)]], At);
			}
			if (!bPlaced && !SpotOfKind(Kind, At)) { Kind = AOpen; if (!OpenSpot(90.f, Loot, At)) continue; }
			Used[Kind]++;
			if (bFree) FreeKeyLeft--;
			else if (Kind == ABack || Kind == ADark) Conceal = EHUConceal::Dark;
			else if (Loot.FRand() > PlainChance)
			{
				const EHUConceal Hard[] = { EHUConceal::Trace, EHUConceal::Prints, EHUConceal::Locked };
				Conceal = Hard[Loot.RandRange(0, 2)];
			}
		}
		Taken.Add(At);
		const FTransform CT(FRotator(0, Loot.FRandRange(0.f, 360.f), 0), At);
		AHUEvidence* K = SpawnNamed<AHUEvidence>(CT * GetActorTransform());
		if (!K) continue;
		K->ClueId = Id; K->MarkerNumber = ++Marker; K->Mesh = EvidenceMesh; K->Conceal = Conceal;
		K->FinishSpawning(CT * GetActorTransform());
		if (Cover) { K->Hide(); Cover->Hidden.Add(K); }
		if (bPocket && Victim) { K->Hide(); Victim->Pocket = K; }
		Cases->InScene.Add(Id);
	}

	// ---- SECURITY CAMERAS and the desk where the tapes are reviewed ----
	{
		FRandomStream Cr(C.MapSeed ^ 0x5EC);
		auto RoomOf = [&](const FVector& P) -> int32 { for (int32 i = 0; i < Rooms.Num(); i++) if (Rooms[i].Contains(P.X, P.Y)) return i; return 0; };
		// cameras: high in a corner of the entrance room, the victim's room (half the time - otherwise it's too easy), and one more
		TArray<int32> CamRooms;
		CamRooms.Add(RoomOf(PlayerStart.X < Plan.X0 ? FVector(Plan.X0 + 150, EntY, 0) : PlayerStart));
		const int32 VictimRoom = RoomOf(VictimAt);
		CamRooms.AddUnique(VictimRoom); // the one that matters is always there
		for (int32 Try = 0; Try < 6 && CamRooms.Num() < 3; Try++) CamRooms.AddUnique(Cr.RandRange(0, Rooms.Num() - 1));
		TArray<FTransform> Cams; TArray<FString> CamNames; TArray<float> CamFloor;
		for (int32 n = 0; n < CamRooms.Num(); n++)
		{
			const FR& Rm = Rooms[CamRooms[n]];
			const float CX = Cr.FRand() < 0.5f ? Rm.X0 + 30 : Rm.X1 - 30, CY = Cr.FRand() < 0.5f ? Rm.Y0 + 30 : Rm.Y1 - 30;
			const float FloorZ = CamRooms[n] == VictimRoom ? VictimAt.Z : 0.f; // (in a building with floors: on the victim's)
			CamFloor.Add(FloorZ);
			const FVector At(CX, CY, FloorZ + H - 45);
			const FVector Look = FVector(Rm.CX(), Rm.CY(), FloorZ + 60) - At;
			// the camera on the wall: a box, a lens, a red light
			const FRotator R = Look.Rotation();
			HUBuild::Block(this, RootComponent, Blocks, HUBuild::EShape::Cube, At, FVector(26, 12, 12), FLinearColor(0.75f, 0.75f, 0.72f), R, false);
			HUBuild::Block(this, RootComponent, Blocks, HUBuild::EShape::Cylinder, At + R.Vector() * 14, FVector(8, 8, 4), FLinearColor(0.02f, 0.02f, 0.02f), R + FRotator(90, 0, 0), false);
			HUBuild::Block(this, RootComponent, Blocks, HUBuild::EShape::Cube, At + FVector(0, 0, 8), FVector(3, 3, 3), FLinearColor(3.f, 0.05f, 0.05f), FRotator::ZeroRotator, false);
			Cams.Add(FTransform(R, At) * GetActorTransform());
			CamNames.Add(FString::Printf(TEXT("CAM %d  ·  %s"), n + 1, CamRooms[n] == VictimRoom ? TEXT("WHERE IT HAPPENED") : n == 0 ? TEXT("FRONT") : TEXT("BACK ROOMS")));
		}

		// THE NIGHT ON TAPE: the half hour before it happened, and just after. Scripted from the case:
		//   the culprit with the victim early on (talking), another suspect around them (a red herring), strangers
		//   passing through, the victim moving to where it happened, the culprit closing in - and, on the camera in
		//   that room, the culprit right there with them (THE clip: flag it and their lie goes on the record).
		//   A robbery / fire / disappearance: the culprit where it happened just before; a missing person walks out
		//   with them. Sometimes the recorder was switched off around the crime.
		int32 Hr = 10, Mn = 0; { FString Hs, Ms; if (C.Time.Split(TEXT(":"), &Hs, &Ms)) { Hr = FCString::Atoi(*Hs); Mn = FCString::Atoi(*Ms.Left(2)); } }
		auto Clock = [&](int32 Off) { int32 Mins = Hr * 60 + Mn + Off; Mins = (Mins + 24 * 60) % (24 * 60); const int32 H12 = (Mins / 60) % 12 == 0 ? 12 : (Mins / 60) % 12; return FString::Printf(TEXT("%d:%02d PM"), H12, Mins % 60); };
		const int32 Offsets[] = { -32, -28, -24, -20, -16, -12, -8, -5, -3, -1, 2, 6 };
		const int32 NM = UE_ARRAY_COUNT(Offsets), CatchAt = 8; // Offsets[8] == -3
		const int32 Killer = SusId.IndexOfByKey(C.Solution.Killer);
		const int32 Decoy = SusAt.Num() > 1 ? (Killer + 1 + Cr.RandRange(0, SusAt.Num() - 2)) % SusAt.Num() : -1;
		const EHUProfile Prof = (EHUProfile)C.Profile;
		const bool bCut = Prof == EHUProfile::ContractKiller || Prof == EHUProfile::Enforcer || Prof == EHUProfile::ConArtist || Cr.FRand() < 0.25f;
		const bool bHomicide = C.Crime == TEXT("homicide"), bMissing = C.Crime == TEXT("missing"), bArson = C.Crime == TEXT("arson");
		auto RoomSpot = [&](int32 R) { const FR& Rm = Rooms[R]; return FVector(Cr.FRandRange(Rm.X0 + 80, Rm.X1 - 80), Cr.FRandRange(Rm.Y0 + 80, Rm.Y1 - 80), 0); };
		TArray<FVector> Early; // where each suspect was earlier in the night
		for (int32 i = 0; i < SusAt.Num(); i++) Early.Add(RoomSpot(Cr.RandRange(0, Rooms.Num() - 1)));
		const FVector Meet = RoomSpot(CamRooms.Num() > 1 ? CamRooms[1] : CamRooms[0]); // where the culprit and the victim talked (on camera)
		const FVector Door(Plan.X0 + 150, EntY, 0);
		// two strangers walking through
		TArray<FVector> SA, SB;
		for (int32 s = 0; s < 2; s++) { SA.Add(RoomSpot(Cr.RandRange(0, Rooms.Num() - 1))); SB.Add(RoomSpot(Cr.RandRange(0, Rooms.Num() - 1))); }
		const int32 NS = SusAt.Num(), NP = NS + 2 + 1; // suspects, strangers, the victim
		TArray<TArray<FVector>> Local; Local.SetNum(NM);   // local positions per moment (zero = not there)
		TArray<FHUFootageFrame> Frames;
		for (int32 k = 0; k < NM; k++)
		{
			FHUFootageFrame F;
			const int32 Off = Offsets[k];
			F.Clock = Clock(Off);
			F.bAfter = Off >= 0;
			F.bLost = bCut && (Off == -1 || Off == 2);
			TArray<FVector>& P = Local[k]; P.Init(FVector::ZeroVector, NP);
			const float Drift = FMath::Clamp((Off + 32) / 38.f, 0.f, 1.f);
			for (int32 i = 0; i < NS; i++) P[i] = FMath::Lerp(Early[i], SusAt[i], Drift);
			// the victim: at the meeting spot, then where it happened (gone after, unless they're lying there)
			FVector V = Off <= -24 ? Meet : Off <= -12 ? FMath::Lerp(Meet, VictimAt, (Off + 24) / 12.f) : VictimAt;
			if (bArson && Off >= -8) V = FVector::ZeroVector; // they'd gone by the time it was set
			if (Off >= 0) V = FVector::ZeroVector;
			if (Killer >= 0)
			{
				FVector& K = P[Killer];
				if (Off == -32 || Off == -28) K = Meet + FVector(85, 30, 0);             // talking with the victim
				if (Off == -8) K = FMath::Lerp(K, VictimAt, 0.5f);                       // heading that way
				if (Off == -5) K = FMath::Lerp(K, VictimAt, 0.85f);
				if (Off == -3 || Off == -1) K = VictimAt + FVector(70, 40, 0);         // right there
				if (Off == 2) K = FMath::Lerp(VictimAt, SusAt[Killer], 0.5f);           // walking away
				if (bMissing && (Off == -1)) { K = FMath::Lerp(VictimAt, Door, 0.6f); V = K + FVector(-60, 40, 0); } // leaving together
			}
			if (Decoy >= 0 && (Off == -24 || Off == -20)) P[Decoy] = (Off == -24 ? Meet : FMath::Lerp(Meet, VictimAt, 0.4f)) + FVector(-90, -20, 0); // around the victim too
			for (int32 s = 0; s < 2; s++) if (Off <= -5 && (Off + 32) / 4 % 2 == s) P[NS + s] = FMath::Lerp(SA[s], SB[s], Cr.FRand());
			P[NS + 2] = V;
			for (const FVector& L : P) F.People.Add(L.IsNearlyZero() ? FVector::ZeroVector : GetActorTransform().TransformPosition(L));
			Frames.Add(F);
		}
		// what a detective writes down for each camera at each moment (shown when a moment's flagged)
		TArray<FString> Notes;
		FString CatchSuspect; int32 CatchFrame = -1;
		auto NameOf = [&](int32 i) { FHUSuspect S; return SusId.IsValidIndex(i) && Cases->GetSuspect(SusId[i], S) ? S.Name : FString(TEXT("someone")); };
		for (int32 c = 0; c < CamRooms.Num(); c++)
			for (int32 k = 0; k < NM; k++)
			{
				const FR& Rm = Rooms[CamRooms[c]];
				auto Seen = [&](const FVector& L) { return !L.IsNearlyZero() && Rm.Contains(L.X, L.Y) && FMath::Abs(L.Z - CamFloor[c]) < 150.f; };
				TArray<FString> Who;
				for (int32 i = 0; i < NS; i++) if (Seen(Local[k][i])) Who.Add(NameOf(i));
				int32 Strangers = 0; for (int32 s = 0; s < 2; s++) if (Seen(Local[k][NS + s])) Strangers++;
				const bool bVic = Seen(Local[k][NS + 2]);
				FString N;
				if (Who.Num() || Strangers || bVic)
				{
					TArray<FString> Parts = Who;
					if (bVic) Parts.Add(TEXT("the victim"));
					if (Strangers) Parts.Add(Strangers == 1 ? TEXT("a stranger in a hat") : TEXT("two strangers"));
					N = FString::Printf(TEXT("%s, %s — %s."), *Frames[k].Clock, *CamNames[c], *FString::Join(Parts, TEXT(", ")));
					if (Killer >= 0 && Seen(Local[k][Killer]) && bVic && FVector::Dist2D(Local[k][Killer], Local[k][NS + 2]) < 200.f) N += FString::Printf(TEXT(" %s and the victim, close together."), *NameOf(Killer));
				}
				Notes.Add(N);
				if (k == CatchAt && Killer >= 0 && Seen(Local[k][Killer]) && !Frames[k].bLost) { CatchFrame = c * NM + k; CatchSuspect = SusId[Killer]; }
			}

		// the desk itself, somewhere on the floor
		FVector DeskAt;
		if (OpenSpot(180.f, Cr, DeskAt))
		{
			Taken.Add(DeskAt);
			const FTransform DT(FRotator(0, Cr.FRandRange(0.f, 360.f), 0), DeskAt);
			if (AHUSecurityDesk* Desk = SpawnNamed<AHUSecurityDesk>(DT * GetActorTransform()))
			{
				Desk->Cameras = Cams; Desk->CameraNames = CamNames; Desk->Frames = Frames; Desk->Coats = SusCoat; Desk->VictimActor = Victim;
				Desk->Notes = Notes; Desk->CatchFrame = CatchFrame; Desk->CatchSuspect = CatchSuspect;
				Desk->FinishSpawning(DT * GetActorTransform());
			}
		}
	}
	Arrival = FString::Printf(TEXT("%s — %s, %d rooms. %d pieces of evidence here, %d of them prove a lie. You need %d lies on the record. Your car's at the curb when the squad's ready to leave."),
		*C.Place, *LayoutName, Rooms.Num(), Cases->InScene.Num(), Key2.Num(), LiesNeeded);
}


void AHUCrimeScene::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPlayerReady) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn) return;
	bPlayerReady = true;
	UHUInteractComponent* I = Pawn->FindComponentByClass<UHUInteractComponent>();
	if (!I) { I = NewObject<UHUInteractComponent>(Pawn, TEXT("HUInteract")); I->RegisterComponent(); }
	I->Subtitle = FText::FromString(Arrival);
	if (ACharacter* Ch = Cast<ACharacter>(Pawn))
	{
		UCharacterMovementComponent* M = Ch->GetCharacterMovement();
		M->GetNavAgentPropertiesRef().bCanCrouch = true;
		M->SetCrouchedHalfHeight(44.f); // low enough for the crawlspace
	}
	if (bPlacePlayer)
	{
		// the host places the whole squad by the car (standing in a line); alone, it's just you
		int32 i = 0;
		if (GetNetMode() != NM_Client)
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++i)
				if (APlayerController* Each = It->Get()) if (APawn* EP = Each->GetPawn())
				{
					EP->SetActorLocation(GetActorTransform().TransformPosition(PlayerStart + FVector(0, -90.f * i, 0)), false, nullptr, ETeleportType::TeleportPhysics);
					Each->ClientSetRotation(GetActorRotation());
				}
		PC->SetControlRotation(GetActorRotation());
	}
	SetActorTickEnabled(false);
}

void AHUCrimeScene::OnCrawlEnter(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Body, bool bSweep, const FHitResult& Hit)
{
	if (ACharacter* Ch = Cast<ACharacter>(Other)) Ch->Crouch();
}

void AHUCrimeScene::OnCrawlLeave(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Body)
{
	if (ACharacter* Ch = Cast<ACharacter>(Other)) Ch->UnCrouch();
}