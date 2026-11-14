// THE PRECINCT, laid out as a working station (AHUSquadRoom::BuildStation). Sizes are real-world, in cm.
// Plan (X runs from the street door at -X to the back at +X; Y north is +):
//
//   Y +1600 ┌──────────────┬───────┬─┬─────────┬──────────┬──────────────────┐
//           │              │CAPTAIN│ │ OFFICE 4│          │                  │
//           │   LOCKER     │       │B│ OFFICE 3│ (storage)│   (storage)      │
//           │    ROOM      ├───────┤ │ OFFICE 2├──────────┤──────────────────│
//           │              │ BREAK │ │ OFFICE 1│ EVIDENCE │ INTERVIEW 1 | 2  │
//   ┌───────┤              │ ROOM  │ │         │          │                  │
//   │ LOBBY ├──────────────┴───────┘ └─────────┴──────────┴──────────────────┤
//   │ front    MAIN CORRIDOR (3 m wide)                                ROLL CALL >
//   │ desk  ├────────────────────────┬────────────┬──────────────────────────┤
//   └───────┤        BULLPEN         │  BOOKING   │        RECORDS           │
//           │  (case board, upgrade  ├────────────┤                          │
//           │   board, case wall)    │  HOLDING   │                          │
//   Y -1600 └────────────────────────┴────────────┴──────────────────────────┘
//         X -2400  -1600            0           900                       2400
// Corridor B runs north from the main corridor (X -150..150); the four personal offices open off it.
#include "HUSquadRoom.h"
#include "HUBuild.h"
#include "HUProps.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"

using namespace HUBuild;

namespace HUStation
{
	// the walls and doors
	const float H = 330.f;      // ceiling (the case board is tall)
	const float T = 15.f;       // wall thickness
	const float DoorH = 220.f;
	// the personal offices: X 150..750, a 362 cm slice of Y each (office k: Y0 + k * W)
	const float OfficeX0 = 150.f, OfficeX1 = 750.f, OfficeY0 = 150.f, OfficeW = 362.5f;
}
using namespace HUStation;

// where each room's ceiling light hangs
static const FVector2D StationLights[] = {
	{ -1300, 0 }, { -500, 0 }, { 300, 0 }, { 1100, 0 }, { 1900, 0 }, { 0, 650 }, { 0, 1300 },               // corridors
	{ -2000, 0 },                                                                                            // lobby
	{ -1225, 500 }, { -1225, 1200 }, { -500, 520 }, { -500, 1250 },                                          // lockers, break room, captain
	{ 450, 331 }, { 450, 693 }, { 450, 1056 }, { 450, 1418 },                                                // offices
	{ 1125, 520 }, { 1725, 375 }, { 2175, 375 },                                                             // evidence, interviews
	{ -1200, -550 }, { -400, -550 }, { -1200, -1200 }, { -400, -1200 },                                      // bullpen
	{ 450, -520 }, { 450, -1250 }, { 1300, -600 }, { 2000, -600 }, { 1650, -1250 },                          // booking, holding, records
};

FVector2D AHUSquadRoom::OfficeDesk(int32 K) { return FVector2D(520.f, OfficeY0 + OfficeW * (K + 0.5f)); }
FVector AHUSquadRoom::OfficeOrigin(int32 K) { return FVector(OfficeX0, OfficeY0 + OfficeW * K, 0.f); }

void AHUSquadRoom::StationLightsUp()
{
	for (const FVector2D& P : StationLights)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetRelativeLocation(FVector(P.X, P.Y, H - 40));
		L->SetIntensity(4500.f);
		L->SetAttenuationRadius(800.f);
		L->SetCastShadows(false);
		L->SetLightColor(FLinearColor(1.f, 0.92f, 0.8f));
		L->RegisterComponent();
	}
}

void AHUSquadRoom::BuildStation()
{
	USceneComponent* R = RootComponent;
	const FLinearColor Paint(0.62f, 0.6f, 0.52f), Lino(0.33f, 0.34f, 0.31f), Desk(0.25f, 0.14f, 0.06f), Steel(0.3f, 0.32f, 0.34f),
		Dark(0.08f, 0.08f, 0.09f), Paper(0.86f, 0.84f, 0.76f), Glass(0.55f, 0.7f, 0.8f), Wood(0.36f, 0.22f, 0.1f), Green(0.2f, 0.26f, 0.22f);
	auto B = [&](FVector P, FVector S, FLinearColor C, bool bCollide = true) { return Block(this, R, Blocks, EShape::Cube, P, S, C, FRotator::ZeroRotator, bCollide); };
	auto Cyl = [&](FVector P, FVector S, FLinearColor C, bool bCollide = true) { return Block(this, R, Blocks, EShape::Cylinder, P, S, C, FRotator::ZeroRotator, bCollide); };
	// a wall along X (at Y = Fixed) or along Y (at X = Fixed), from A0 to A1, with doorways (centre, width)
	auto Wall = [&](bool bAlongX, float Fixed, float A0, float A1, std::initializer_list<FVector2D> Doors, float DH = DoorH)
	{
		TArray<FVector2D> Gaps(Doors);
		Gaps.Sort([](const FVector2D& L, const FVector2D& Rr) { return L.X < Rr.X; });
		auto Seg = [&](float From, float To, float Z0, float Z1)
		{
			if (To - From < 1.f || Z1 - Z0 < 1.f) return;
			const float M = (From + To) / 2, Len = To - From;
			B(bAlongX ? FVector(M, Fixed, (Z0 + Z1) / 2) : FVector(Fixed, M, (Z0 + Z1) / 2), bAlongX ? FVector(Len, T, Z1 - Z0) : FVector(T, Len, Z1 - Z0), Paint);
			// a dark skirting board along the bottom, both faces
			if (Z0 < 1.f) B(bAlongX ? FVector(M, Fixed, 6) : FVector(Fixed, M, 6), bAlongX ? FVector(Len, T + 2, 12) : FVector(T + 2, Len, 12), Desk * 0.6f, false);
		};
		float At = A0;
		for (const FVector2D& D : Gaps)
		{
			Seg(At, D.X - D.Y / 2, 0, H);
			Seg(D.X - D.Y / 2, D.X + D.Y / 2, DH, H); // over the door
			// the door frame
			for (float S : { -1.f, 1.f }) B(bAlongX ? FVector(D.X + S * D.Y / 2, Fixed, DH / 2) : FVector(Fixed, D.X + S * D.Y / 2, DH / 2), bAlongX ? FVector(6, T + 4, DH) : FVector(T + 4, 6, DH), Wood, false);
			At = D.X + D.Y / 2;
		}
		Seg(At, A1, 0, H);
	};
	// a room-name sign over a door: Yaw is the way it faces
	auto Sign = [&](FVector P, float Yaw, const FString& Text)
	{
		UTextRenderComponent* S = NewObject<UTextRenderComponent>(this);
		S->SetupAttachment(R);
		S->SetRelativeLocation(P);
		S->SetRelativeRotation(FRotator(0, Yaw, 0));
		S->SetHorizontalAlignment(EHTA_Center);
		S->SetVerticalAlignment(EVRTA_TextCenter);
		S->SetWorldSize(14.f);
		S->SetTextRenderColor(FColor(240, 236, 220));
		S->SetText(FText::FromString(Text));
		S->RegisterComponent();
		RoomTexts.Add(S);
		// the plate behind the letters (long along the wall it hangs on)
		const FVector Back = P - FRotator(0, Yaw, 0).Vector() * 1.5f;
		const float Len = FMath::Max(140.f, Text.Len() * 11.f);
		const bool bFacesY = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(Yaw))) > 0.5f;
		B(Back, bFacesY ? FVector(Len, 2, 26) : FVector(2, Len, 26), FLinearColor(0.05f, 0.12f, 0.22f), false);
	};
	// a floor tile for a room (the slab under it all is concrete)
	auto Floor = [&](float X0, float X1, float Y0, float Y1, FLinearColor C) { B(FVector((X0 + X1) / 2, (Y0 + Y1) / 2, 0.6f), FVector(X1 - X0, Y1 - Y0, 1.2f), C, false); };
	auto Table = [&](FVector P, FVector S, FLinearColor C)
	{
		B(P + FVector(0, 0, S.Z - 3), FVector(S.X, S.Y, 6), C);
		for (float DX : { -1.f, 1.f }) for (float DY : { -1.f, 1.f }) B(P + FVector(DX * (S.X / 2 - 6), DY * (S.Y / 2 - 6), (S.Z - 6) / 2), FVector(5, 5, S.Z - 6), C * 0.7f);
	};
	// a chair facing away from its back (HUProps: a real one when the art is in, Kind = StackChair or OfficeChair)
	auto Chair = [&](FVector P, float BackDX, float BackDY, const TCHAR* Kind = TEXT("StackChair"))
	{
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-BackDY, -BackDX));
		if (Prop(this, R, Blocks, Kind, P + FVector(0, 0, 50), FVector(0, 0, 100), FRotator(0, Yaw, 0))) return;
		B(P + FVector(0, 0, 46), FVector(46, 46, 7), Steel);
		B(P + FVector(BackDX, BackDY, 76), BackDX != 0 ? FVector(5, 44, 52) : FVector(44, 5, 52), Steel);
		for (float DX : { -18.f, 18.f }) for (float DY : { -18.f, 18.f }) B(P + FVector(DX, DY, 21), FVector(3, 3, 42), Steel * 0.6f, false);
	};
	auto Locker = [&](FVector P, float FaceYaw)
	{
		if (Prop(this, R, Blocks, TEXT("Locker"), P + FVector(0, 0, 95), FVector(45, 38, 190), FRotator(0, FaceYaw, 0))) return;
		const bool bX = FMath::Abs(FaceYaw) < 1.f || FMath::Abs(FaceYaw - 180.f) < 1.f;
		B(P + FVector(0, 0, 95), bX ? FVector(45, 38, 190) : FVector(38, 45, 190), Green);
		const FVector F = FRotator(0, FaceYaw, 0).Vector();
		for (float Z : { 160.f, 168.f, 176.f }) B(P + F * 23 + FVector(0, 0, Z), bX ? FVector(1, 24, 2) : FVector(24, 1, 2), Dark, false);
	};
	auto Shelf = [&](FVector P, bool bAlongX, int32 Seed)
	{
		const FVector S = bAlongX ? FVector(240, 50, 0) : FVector(50, 240, 0);
		for (float DX : { -1.f, 1.f }) B(P + (bAlongX ? FVector(DX * 118, 0, 110) : FVector(0, DX * 118, 110)), bAlongX ? FVector(4, 50, 220) : FVector(50, 4, 220), Steel);
		FRandomStream Rng(Seed);
		for (int32 L = 0; L < 4; L++)
		{
			const float Z = 20 + L * 55;
			B(P + FVector(0, 0, Z), FVector(S.X, S.Y, 3), Steel, false);
			for (int32 k = 0; k < 4; k++)
			{
				if (Rng.FRand() < 0.2f) continue;
				const float U = -90 + k * 60 + Rng.FRandRange(-6, 6);
				B(P + (bAlongX ? FVector(U, 0, Z + 18) : FVector(0, U, Z + 18)), FVector(40, 36, 32), FLinearColor(0.6f, 0.47f, 0.3f) * Rng.FRandRange(0.8f, 1.1f), false);
			}
		}
	};

	// ---- the slab, the ceiling ----
	B(FVector(0, 0, -5), FVector(4800, 3200, 10), FLinearColor(0.2f, 0.2f, 0.19f));
	B(FVector(0, 0, H + 5), FVector(4800, 3200, 10), Paint * 0.75f);
	for (const FVector2D& P : StationLights) B(FVector(P.X, P.Y, H - 2), FVector(120, 30, 4), FLinearColor(3, 3, 2.7f), false); // the light panels

	// ---- the floors ----
	Floor(-1600, 2400, -150, 150, Lino);                           // main corridor
	Floor(-150, 150, 150, 1600, Lino);                             // corridor B
	Floor(-2400, -1600, -700, 700, FLinearColor(0.42f, 0.38f, 0.32f)); // lobby terrazzo
	Floor(-1600, -850, 150, 1600, FLinearColor(0.5f, 0.5f, 0.48f));    // lockers (tiles)
	Floor(-850, -150, 150, 900, FLinearColor(0.4f, 0.34f, 0.26f));     // break room
	Floor(-850, -150, 900, 1600, FLinearColor(0.25f, 0.08f, 0.07f));   // captain (red carpet)
	Floor(750, 1500, 150, 900, Lino * 0.8f);                       // evidence
	Floor(1500, 2400, 150, 600, FLinearColor(0.28f, 0.28f, 0.27f));    // interviews
	Floor(-1600, 0, -1600, -150, FLinearColor(0.2f, 0.22f, 0.26f));    // bullpen carpet
	Floor(0, 900, -900, -150, Lino * 0.9f);                        // booking
	Floor(0, 900, -1600, -900, FLinearColor(0.3f, 0.3f, 0.3f));        // holding
	Floor(900, 2400, -1600, -150, FLinearColor(0.3f, 0.27f, 0.22f));   // records
	// (the offices' carpets are their owners' choice: AHUOffice)

	// ---- the walls ----
	// the outside
	Wall(true, 1600 + T / 2, -1600, 2400, {});
	Wall(true, -1600 - T / 2, -1600, 2400, {});
	Wall(false, 2400 + T / 2, -1600, 1600, { FVector2D(0, 190) });          // the roll-call door
	Wall(false, -2400 - T / 2, -700, 700, { FVector2D(0, 200) }, 250.f);    // the street door (the motor pool door's frame)
	Wall(true, 700, -2400, -1600, {});
	Wall(true, -700, -2400, -1600, {});
	Wall(false, -1600, 150, 1600, {});                                      // locker room's west wall (and the lobby's)
	Wall(false, -1600, -1600, -150, {});                                    // the bullpen's
	// the main corridor's north side: locker room, break room | offices' corridor | evidence, interview 1, 2
	Wall(true, 150, -1600, -150, { FVector2D(-1225, 110), FVector2D(-500, 110) });
	Wall(true, 150, 150, 2400, { FVector2D(1125, 110), FVector2D(1725, 110), FVector2D(2175, 110) });
	// and its south side: the bullpen (two wide openings), booking, records
	Wall(true, -150, -1600, 2400, { FVector2D(-1100, 220), FVector2D(-400, 220), FVector2D(450, 160), FVector2D(1650, 120) });
	// the north-west block
	Wall(false, -850, 150, 1600, {});
	Wall(true, 900, -850, -150, {});
	Wall(false, -150, 150, 1600, { FVector2D(1250, 110) });                 // corridor B's west side: the Captain's door
	// corridor B's east side: the four offices' doors (near the main corridor end of each)
	{
		const float D0 = OfficeY0 + 90.f;
		Wall(false, OfficeX0, OfficeY0, 1600, { FVector2D(D0, 100), FVector2D(D0 + OfficeW, 100), FVector2D(D0 + OfficeW * 2, 100), FVector2D(D0 + OfficeW * 3, 100) });
		for (int32 k = 1; k < 4; k++) Wall(true, OfficeY0 + OfficeW * k, OfficeX0, OfficeX1, {});
		Wall(false, OfficeX1, OfficeY0, 1600, {});
		for (int32 k = 0; k < 4; k++) Sign(FVector(OfficeX0 - 9, D0 + OfficeW * k, 250), 180, FString::Printf(TEXT("OFFICE %d"), k + 1));
	}
	// evidence, the interview rooms, the storage behind
	Wall(true, 900, 750, 1500, {});
	Wall(false, 1500, 150, 900, {});
	Wall(true, 600, 1500, 2400, {});
	Wall(false, 1950, 150, 600, {});
	// booking, holding, records
	Wall(false, 0, -1600, -150, {});
	Wall(false, 900, -1600, -150, {});
	Wall(true, -900, 0, 900, { FVector2D(450, 120) });

	// ---- the signs over the doors ----
	Sign(FVector(-1225, 141, 250), -90, TEXT("LOCKER ROOM"));
	Sign(FVector(-500, 141, 250), -90, TEXT("BREAK ROOM"));
	Sign(FVector(1125, 141, 250), -90, TEXT("EVIDENCE"));
	Sign(FVector(1725, 141, 250), -90, TEXT("INTERVIEW 1"));
	Sign(FVector(2175, 141, 250), -90, TEXT("INTERVIEW 2"));
	Sign(FVector(-750, -141, 250), 90, TEXT("BULLPEN"));
	Sign(FVector(450, -141, 250), 90, TEXT("BOOKING"));
	Sign(FVector(1650, -141, 250), 90, TEXT("RECORDS"));
	Sign(FVector(-141, 1250, 250), 0, TEXT("CAPTAIN"));
	Sign(FVector(0, 141, 280), -90, TEXT("OFFICES  ^"));
	Sign(FVector(2385, 0, 290), 180, TEXT("ROLL CALL"));

	// ======== THE LOBBY: the public's side of the front desk ========
	{
		// the front desk, an L along the north side, a glass screen over it
		B(FVector(-2000, 420, 55), FVector(500, 70, 110), Wood);
		B(FVector(-1785, 520, 55), FVector(70, 130, 110), Wood);
		B(FVector(-2000, 420, 112), FVector(520, 80, 5), Wood * 1.3f);
		B(FVector(-2000, 400, 170), FVector(500, 2, 110), Glass * FLinearColor(1, 1, 1, 0.3f), false);
		B(FVector(-2000, 520, 74), FVector(400, 60, 6), Desk); // the desk sergeant's side
		Chair(FVector(-2050, 580, 0), 0, 22, TEXT("OfficeChair"));
		if (!Prop(this, R, Blocks, TEXT("CRTMonitor"), FVector(-1950, 520, 96), FVector(0, 0, 38), FRotator(0, 90, 0), false)) // the desk's computer
			B(FVector(-1950, 520, 92), FVector(40, 30, 30), FLinearColor(0.6f, 0.57f, 0.48f), false);
		Prop(this, R, Blocks, TEXT("DeskPhone"), FVector(-2090, 520, 83), FVector(0, 0, 12), FRotator(0, 90, 0), false);
		Sign(FVector(-2000, 384, 145), -90, TEXT("FRONT DESK  ·  ALL VISITORS REPORT HERE"));
		// benches for the public along the south wall, a plant, the city seal on the back wall
		for (float X : { -2250.f, -1950.f })
		{
			if (Prop(this, R, Blocks, TEXT("RollCallBench"), FVector(X, -641, 44), FVector(47, 240, 88), FRotator(0, 90, 0))) continue;
			B(FVector(X, -640, 45), FVector(240, 45, 6), Wood);
			B(FVector(X, -662, 75), FVector(240, 5, 50), Wood);
			for (float DX : { -100.f, 100.f }) B(FVector(X + DX, -640, 21), FVector(5, 40, 42), Steel);
		}
		Cyl(FVector(-1700, -620, 25), FVector(40, 40, 50), FLinearColor(0.3f, 0.2f, 0.14f));
		Block(this, R, Blocks, EShape::Sphere, FVector(-1700, -620, 90), FVector(70, 70, 90), FLinearColor(0.12f, 0.3f, 0.12f), FRotator::ZeroRotator, false);
		Block(this, R, Blocks, EShape::Cylinder, FVector(-2392, 380, 200), FVector(110, 110, 3), FLinearColor(0.72f, 0.6f, 0.25f), FRotator(0, 0, 90), false);
		RoomTextAt(FVector(-2389, 380, 205), 0, 12.f, FColor(40, 30, 10), TEXT("METRO PD<br>14TH PRECINCT"));
	}

	// ======== THE LOCKER ROOM ========
	for (int32 i = 0; i < 18; i++) Locker(FVector(-1575, 230 + i * 48, 0), 0);          // a bank along the west wall
	for (int32 i = 0; i < 10; i++) Locker(FVector(-875, 600 + i * 48, 0), 180);         // and the east wall
	for (float Y : { 500.f, 1000.f })
	{
		if (Prop(this, R, Blocks, TEXT("BenchLong"), FVector(-1225, Y, 23), FVector(40, 420, 46))) continue;
		B(FVector(-1225, Y, 42), FVector(60, 420, 6), Wood);
		for (float DY : { -180.f, 0.f, 180.f }) B(FVector(-1225, Y + DY, 20), FVector(40, 6, 40), Steel);
	}
	// the showers at the far end: tiles, three heads
	B(FVector(-1225, 1590, 110), FVector(700, 4, 220), FLinearColor(0.75f, 0.78f, 0.78f), false);
	for (float X : { -1450.f, -1225.f, -1000.f }) Cyl(FVector(X, 1575, 200), FVector(8, 8, 20), Steel, false);
	Floor(-1600, -850, 1400, 1600, FLinearColor(0.7f, 0.72f, 0.72f));

	// ======== THE BREAK ROOM ========
	{
		Table(FVector(-500, 500, 0), FVector(160, 100, 76), FLinearColor(0.7f, 0.68f, 0.62f));
		Chair(FVector(-560, 420, 0), 0, -22); Chair(FVector(-440, 420, 0), 0, -22); Chair(FVector(-560, 580, 0), 0, 22); Chair(FVector(-440, 580, 0), 0, 22);
		// the counter along the north wall: the coffee, a microwave, the sink
		B(FVector(-500, 860, 45), FVector(600, 60, 90), FLinearColor(0.55f, 0.5f, 0.42f));
		B(FVector(-500, 860, 92), FVector(610, 66, 4), FLinearColor(0.75f, 0.72f, 0.65f));
		CoffeeParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-650, 855, 108), FVector(16, 16, 26), Dark));
		CoffeeParts.Add(Block(this, R, Blocks, EShape::Cylinder, FVector(-615, 855, 99), FVector(9, 9, 10), FLinearColor(0.85f, 0.85f, 0.8f)));
		CoffeeParts.Add(B(FVector(-650, 870, 125), FVector(30, 26, 60), Dark * 1.5f));
		B(FVector(-450, 860, 112), FVector(50, 36, 30), FLinearColor(0.85f, 0.85f, 0.82f), false); // microwave
		B(FVector(-300, 855, 93), FVector(50, 40, 2), Steel, false);                             // sink
		// the fridge, the vending machine, the TV
		B(FVector(-805, 820, 90), FVector(70, 70, 180), FLinearColor(0.85f, 0.86f, 0.84f));
		B(FVector(-805, 250, 95), FVector(70, 90, 190), FLinearColor(0.6f, 0.1f, 0.08f));
		B(FVector(-769, 250, 120), FVector(2, 60, 100), Glass, false);
		if (!Prop(this, R, Blocks, TEXT("CRTTV"), FVector(-192, 500, 190), FVector(0, 0, 46), FRotator(0, 180, 0), false)) // the TV, up on its shelf
			B(FVector(-170, 500, 190), FVector(6, 110, 65), Dark, false);
	}

	// ======== THE CAPTAIN'S OFFICE ========
	{
		const FVector Tray(-500, 1250, 0);
		TrayParts.Add(B(Tray + FVector(0, 0, 74), FVector(110, 200, 6), Wood));
		for (float DX : { -48.f, 48.f }) for (float DY : { -92.f, 92.f }) B(Tray + FVector(DX, DY, 36), FVector(5, 5, 72), Wood * 0.7f);
		B(Tray + FVector(-50, 0, 40), FVector(6, 190, 66), Wood * 0.8f); // the modesty panel, facing the door
		TrayParts.Add(B(Tray + FVector(0, -40, 82), FVector(36, 46, 8), FLinearColor(0.4f, 0.3f, 0.15f)));
		TrayParts.Add(B(Tray + FVector(0, -40, 85), FVector(30, 40, 3), FLinearColor(0.75f, 0.6f, 0.32f)));
		Sign(FVector(-545, 1250, 60), 180, TEXT("THE CAPTAIN"));
		// the big chair behind it, the flags, the bookcase, the couch for bad news
		Chair(FVector(-410, 1250, 0), 24, 0, TEXT("OfficeChair"));
		for (float Y : { 1520.f, 1560.f })
		{
			Cyl(FVector(-200, Y, 110), FVector(3, 3, 220), FLinearColor(0.75f, 0.6f, 0.2f), false);
			B(FVector(-200, Y + 0, 170), FVector(2, 30, 50), Y < 1540.f ? FLinearColor(0.1f, 0.15f, 0.45f) : FLinearColor(0.6f, 0.1f, 0.1f), false);
		}
		B(FVector(-830, 1250, 110), FVector(30, 300, 220), Wood);
		for (int32 L = 0; L < 4; L++) B(FVector(-812, 1250, 40 + L * 50), FVector(8, 280, 30), FLinearColor(0.4f, 0.1f, 0.08f) * (0.7f + 0.1f * L), false);
		B(FVector(-500, 960, 40), FVector(200, 70, 40), FLinearColor(0.3f, 0.18f, 0.1f));
		B(FVector(-500, 925, 70), FVector(200, 12, 50), FLinearColor(0.3f, 0.18f, 0.1f));
		for (int32 i = 0; i < 3; i++) B(FVector(-250 - i * 120, 1592, 180), FVector(80, 2, 60), FLinearColor(0.72f, 0.6f, 0.25f), false); // the plaques
	}

	// ======== EVIDENCE: the cage counter, the shelving ========
	{
		// the cage: bars across the room with a counter window by the door
		for (float X = 760.f; X <= 1490.f; X += 18.f) if (FMath::Abs(X - 1125.f) > 60.f) Cyl(FVector(X, 330, 115), FVector(2.5f, 2.5f, 230), Steel);
		B(FVector(1125, 330, 100), FVector(120, 50, 6), Steel);
		B(FVector(1125, 330, 225), FVector(740, 4, 10), Steel);
		Sign(FVector(1125, 300, 150), -90, TEXT("EVIDENCE  ·  SIGN IN / SIGN OUT"));
		for (int32 i = 0; i < 3; i++) Shelf(FVector(1125, 470 + i * 140, 0), true, 30 + i);
		B(FVector(1450, 230, 50), FVector(60, 80, 100), Steel); // the property lockers
	}

	// ======== THE INTERVIEW ROOMS ========
	for (float X : { 1725.f, 2175.f })
	{
		Table(FVector(X, 380, 0), FVector(120, 80, 76), Steel * 1.2f);
		Chair(FVector(X - 30, 310, 0), 0, -22); Chair(FVector(X + 30, 310, 0), 0, -22); // the detectives
		Chair(FVector(X, 455, 0), 0, 22);                                              // the suspect, back to the mirror
		B(FVector(X, 455 + 30, 74), FVector(16, 4, 8), Steel * 0.5f, false);            // the cuff ring
		B(FVector(X, 594, 160), FVector(300, 2, 120), FLinearColor(0.05f, 0.06f, 0.08f), false); // the one-way mirror
		B(FVector(X + 200, 590, 300), FVector(14, 14, 10), Dark, false);                // the camera
		Block(this, R, Blocks, EShape::Sphere, FVector(X + 200, 584, 297), FVector(5, 5, 5), FLinearColor(3, 0.1f, 0.1f), FRotator::ZeroRotator, false);
	}

	// ======== THE BULLPEN: the squad's open floor ========
	{
		// eight desks in two rows of four, nose to nose, each with an old computer
		for (int32 row = 0; row < 2; row++)
			for (int32 i = 0; i < 4; i++)
				for (float S : { -1.f, 1.f })
				{
					const FVector P(-1350 + i * 300, -650 - row * 450 + S * 60, 0);
					const FRotator Faces(0, -S * 90, 0); // toward the detective in the chair
					if (!Prop(this, R, Blocks, TEXT("Desk"), P + FVector(0, 0, 38), FVector(110, 150, 76), Faces)) Table(P, FVector(150, 110, 76), Desk);
					if (!Prop(this, R, Blocks, TEXT("CRTMonitor"), P + FVector(0, S * 30, 95), FVector(0, 0, 38), Faces, false))
						B(P + FVector(0, S * 30, 98), FVector(40, 36, 34), FLinearColor(0.6f, 0.57f, 0.48f), false);
					if (!Prop(this, R, Blocks, TEXT("Keyboard"), P + FVector(0, -S * 10, 78), FVector(16, 44, 0), Faces, false))
						B(P + FVector(0, -S * 10, 78), FVector(44, 16, 2), FLinearColor(0.55f, 0.53f, 0.46f), false);
					Prop(this, R, Blocks, TEXT("DeskPhone"), P + FVector(52, S * 28, 82), FVector(0, 0, 12), Faces, false);
					Chair(P + FVector(0, -S * 90, 0), 0, -S * 22, TEXT("OfficeChair"));
					for (int32 k = 0; k < 3; k++) B(P + FVector(-50 + k * 6, S * 20, 80 + k * 1.2f), FVector(24, 32, 1.2f), Paper, false);
				}
		// the water cooler, the printer, a whiteboard on the east wall
		if (!Prop(this, R, Blocks, TEXT("WaterCooler"), FVector(-60, -300, 73.5f), FVector(0, 0, 147), FRotator(0, 180, 0)))
		{
			Cyl(FVector(-60, -300, 55), FVector(30, 30, 110), FLinearColor(0.85f, 0.85f, 0.85f));
			Cyl(FVector(-60, -300, 130), FVector(26, 26, 40), FLinearColor(0.4f, 0.6f, 0.9f), false);
		}
		Prop(this, R, Blocks, TEXT("WallClock"), FVector(-T / 2 - 2.4f, -1250, 240), FVector(0, 0, 32), FRotator(0, 180, 0), false); // over the whiteboard's shoulder
		B(FVector(-60, -1500, 50), FVector(70, 60, 100), FLinearColor(0.75f, 0.74f, 0.7f));
		B(FVector(-9, -900, 160), FVector(2, 300, 120), FLinearColor(0.95f, 0.95f, 0.93f), false);
		// (the case board, the upgrades board and the case wall are their own actors, hung in BeginPlay)
	}

	// ======== BOOKING ========
	{
		const FVector Book(450, -500, 0);
		CounterParts.Add(B(Book + FVector(0, 0, 50), FVector(170, 70, 100), Desk));
		CounterParts.Add(B(Book + FVector(-20, 0, 101.5f), FVector(40, 30, 3), FLinearColor(0.12f, 0.15f, 0.3f)));
		CounterParts.Add(B(Book + FVector(30, 0, 101.2f), FVector(18, 14, 2.4f), Dark));
		CounterParts.Add(B(Book + FVector(70, -140, 145), FVector(22, 18, 14), Dark));
		CounterParts.Add(Cyl(Book + FVector(70, -140, 115), FVector(3, 3, 50), Steel));
		Sign(FVector(450, -540, 150), 90, TEXT("BOOKING"));
		// the height chart and the mugshot wall, the fingerprint station
		for (int32 i = 0; i < 6; i++) B(FVector(150, -886, 120 + i * 20), FVector(80, 2, 1.5f), FLinearColor(0.85f, 0.85f, 0.8f), false);
		B(FVector(150, -887, 160), FVector(110, 1, 150), FLinearColor(0.6f, 0.62f, 0.66f), false);
		B(FVector(800, -800, 50), FVector(80, 60, 100), Steel);
	}

	// ======== HOLDING: three cells behind bars, a walkway in front ========
	{
		const float BarsY = -1150.f;
		for (float X = 10.f; X <= 890.f; X += 18.f) Cyl(FVector(X, BarsY, 115), FVector(3, 3, 230), FLinearColor(0.35f, 0.37f, 0.4f));
		B(FVector(450, BarsY, 228), FVector(900, 8, 8), FLinearColor(0.35f, 0.37f, 0.4f));
		for (float X : { 300.f, 600.f }) B(FVector(X, -1375, H / 2), FVector(T, 450, H), Paint * 0.8f);
		for (int32 c = 0; c < 3; c++)
		{
			B(FVector(150 + c * 300, -1560, 22), FVector(240, 50, 6), FLinearColor(0.3f, 0.3f, 0.3f)); // the bench
			B(FVector(60 + c * 300, -1530, 25), FVector(45, 45, 50), Steel, false);                    // the toilet
		}
	}

	// ======== RECORDS ========
	for (int32 row = 0; row < 3; row++)
		for (int32 i = 0; i < 6; i++)
		{
			const FVector P(1100 + i * 110, -500 - row * 350, 0);
			if (Prop(this, R, Blocks, TEXT("FilingCabinet"), P + FVector(0, 0, 70), FVector(70, 50, 140), FRotator(0, 90, 0))) continue; // drawers to +Y
			B(P + FVector(0, 0, 70), FVector(50, 70, 140), FLinearColor(0.42f, 0.44f, 0.4f));
			for (int32 d = 0; d < 4; d++) B(P + FVector(0, 36, 20 + d * 34), FVector(30, 1, 3), Dark, false);
		}
	B(FVector(2250, -300, 55), FVector(90, 70, 110), FLinearColor(0.72f, 0.71f, 0.66f)); // the copier
	Table(FVector(2100, -1350, 0), FVector(200, 100, 76), Wood);

	// ======== THE CORRIDORS: the odds and ends ========
	B(FVector(-1000, 142, 150), FVector(200, 2, 110), FLinearColor(0.5f, 0.34f, 0.18f), false); // a notice board
	for (int32 i = 0; i < 6; i++) B(FVector(-1070 + i * 28, 140, 130 + (i % 2) * 40), FVector(22, 1, 28), Paper, false);
	Cyl(FVector(1400, -138, 60), FVector(16, 16, 50), FLinearColor(0.75f, 0.08f, 0.05f), false);  // a fire extinguisher
	Cyl(FVector(130, 1400, 60), FVector(16, 16, 50), FLinearColor(0.75f, 0.08f, 0.05f), false);

	// ---- the doors you use ----
	// back to ROLL CALL for the next shift: double doors at the end of the main corridor
	for (float S : { -1.f, 1.f })
	{
		ShiftParts.Add(B(FVector(2400 - 4, S * 46, 110), FVector(6, 90, 220), FLinearColor(0.2f, 0.12f, 0.05f)));
		ShiftParts.Add(B(FVector(2400 - 7.5f, S * 46, 160), FVector(1, 50, 60), FLinearColor(0.6f, 0.75f, 0.85f)));
	}

	// ---- the personal offices' desks: each with its computer and chair (AHUOffice dresses the rest) ----
	for (int32 k = 0; k < 4; k++)
	{
		const FVector2D DS = OfficeDesk(k);
		const float X = DS.X, Y = DS.Y;
		B(FVector(X, Y, 74), FVector(150, 150, 6), Desk);
		for (float DX : { -68.f, 68.f }) for (float DY : { -68.f, 68.f }) B(FVector(X + DX, Y + DY, 36), FVector(5, 5, 72), Desk * 0.7f);
		B(FVector(X + 15, Y + 55, 50), FVector(100, 34, 44), Desk * 0.8f);
		ComputerParts.Add(B(FVector(X + 45, Y + 10, 83), FVector(40, 46, 12), FLinearColor(0.6f, 0.57f, 0.48f)));
		ComputerParts.Add(B(FVector(X + 45, Y + 10, 108), FVector(36, 44, 38), FLinearColor(0.62f, 0.59f, 0.5f)));
		ComputerParts.Add(B(FVector(X + 26.8f, Y + 10, 109), FVector(1, 36, 28), FLinearColor(0.05f, 0.12f, 0.25f)));
		ComputerParts.Add(B(FVector(X + 5, Y + 10, 78), FVector(16, 44, 2), FLinearColor(0.55f, 0.53f, 0.46f)));
		ChairParts.Add(B(FVector(X - 115, Y, 46), FVector(48, 48, 8), Steel));
		ChairParts.Add(B(FVector(X - 138, Y, 78), FVector(5, 46, 56), Steel));
		B(FVector(X - 115, Y, 21), FVector(6, 6, 42), Steel * 0.6f);
		B(FVector(X - 115, Y, 3), FVector(50, 50, 6), Steel * 0.6f);
	}
}

UTextRenderComponent* AHUSquadRoom::RoomTextAt(FVector Pos, float Yaw, float Size, FColor Col, const FString& Text)
{
	UTextRenderComponent* T2 = NewObject<UTextRenderComponent>(this);
	T2->SetupAttachment(RootComponent);
	T2->SetRelativeLocation(Pos);
	T2->SetRelativeRotation(FRotator(0, Yaw, 0));
	T2->SetHorizontalAlignment(EHTA_Center);
	T2->SetVerticalAlignment(EVRTA_TextCenter);
	T2->SetWorldSize(Size);
	T2->SetTextRenderColor(Col);
	T2->SetText(FText::FromString(Text));
	T2->RegisterComponent();
	RoomTexts.Add(T2);
	return T2;
}
