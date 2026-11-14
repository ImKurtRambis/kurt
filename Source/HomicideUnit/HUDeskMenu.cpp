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
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HUProps.h"

#include "HUFilmCamera.h"
using namespace HUBuild;

static const float DeskZ = 75.f; // the desk top
// the office: the back wall's room-side face, and the window in it (centred on Y = 0)
static const float WallX = 120.f, WinHalf = 100.f, SillZ = 92.f, TopZ = 214.f;
static const float BlindZ = 158.f;   // the blinds hang from the top of the window down to here
static const float GlassX = WallX + 6.f;

// a self-lit material (Tools/CreateGlowMaterials.py) tinted Col - additive for rain - or, without the asset, a plain one
static UMaterialInstanceDynamic* GlowMat(UObject* Outer, FLinearColor Col, bool bAdd)
{
	static UMaterialInterface* Mats[2] = { nullptr, nullptr };
	static bool bTried[2] = { false, false };
	if (!bTried[bAdd])
	{
		bTried[bAdd] = true;
		const TCHAR* Path = bAdd ? TEXT("/Game/Art/Materials/M_HU_GlowAdd.M_HU_GlowAdd") : TEXT("/Game/Art/Materials/M_HU_Glow.M_HU_Glow");
		if (FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(Path)))) Mats[bAdd] = LoadObject<UMaterialInterface>(nullptr, Path);
		if (Mats[bAdd]) Mats[bAdd]->AddToRoot();
	}
	UMaterialInterface* Base = Mats[bAdd] ? Mats[bAdd] : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, Outer);
	D->SetVectorParameterValue(TEXT("Color"), Col);
	return D;
}

// many copies of one shape in one draw (rain, lit windows); its instances are placed by the caller
static UInstancedStaticMeshComponent* Instanced(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, bool bSphere, UMaterialInterface* Mat)
{
	UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(Owner);
	I->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, bSphere ? TEXT("/Engine/BasicShapes/Sphere.Sphere") : TEXT("/Engine/BasicShapes/Cube.Cube")));
	I->SetupAttachment(Parent);
	I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	I->SetCastShadow(false);
	I->SetMaterial(0, Mat);
	I->RegisterComponent();
	Out.Add(I);
	return I;
}

// a furniture prop at its own proportions: Height tall (0 = its modelled size), standing on Base (its bottom-centre), turned Yaw
static UStaticMeshComponent* PropAt(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, const TCHAR* Name, FVector Base, float Height, float Yaw, bool bCollide = true)
{
	UStaticMesh* M = PropMesh(Name);
	if (!M) return nullptr;
	const FVector Ext = M->GetBoundingBox().GetSize();
	const FVector Size = Height > 0.f ? Ext * (Height / FMath::Max(1.f, (float)Ext.Z)) : Ext;
	return Prop(Owner, Parent, Out, Name, Base + FVector(0, 0, Size.Z / 2), Size, FRotator(0, Yaw, 0), bCollide);
}

AHUDeskMenu::AHUDeskMenu()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	// you, sitting at the desk, looking down at it
	Seat = CreateDefaultSubobject<UCameraComponent>(TEXT("Seat"));
	Seat->SetupAttachment(Root);
	// sitting back in the chair: the desk below, the window and the rain beyond it
	Seat->SetRelativeLocation(FVector(-150, 0, 168));
	Seat->SetRelativeRotation(FRotator(-19.5f, 0, 0));
	Seat->SetFieldOfView(70.f);
	// the desk lamp: a warm cone from under the shade (placed with the lamp in OnConstruction), and a soft fill
	LampSpot = CreateDefaultSubobject<USpotLightComponent>(TEXT("LampSpot"));
	LampSpot->SetupAttachment(Root);
	LampSpot->SetIntensity(7000.f);
	LampSpot->SetAttenuationRadius(260.f);
	LampSpot->SetInnerConeAngle(20.f);
	LampSpot->SetOuterConeAngle(46.f);
	LampSpot->SetSourceRadius(2.f);
	LampSpot->SetLightColor(FLinearColor(1.f, 0.7f, 0.4f));
	Lamp = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Root);
	Lamp->SetRelativeLocation(FVector(10, -40, 125));
	Lamp->SetIntensity(380.f);
	Lamp->SetAttenuationRadius(380.f);
	Lamp->SetCastShadows(false);
	Lamp->SetLightColor(FLinearColor(1.f, 0.72f, 0.45f));
	// a streetlight high outside, shining in through the blinds: the slats lay cold stripes across the desk
	Street = CreateDefaultSubobject<USpotLightComponent>(TEXT("Street"));
	Street->SetupAttachment(Root);
	Street->SetRelativeLocation(FVector(700, 200, 760));
	Street->SetRelativeRotation((FVector(-10, -20, 75) - FVector(700, 200, 760)).Rotation());
	Street->SetIntensity(400000.f);
	Street->SetAttenuationRadius(2200.f);
	Street->SetInnerConeAngle(7.f);
	Street->SetOuterConeAngle(13.f);
	Street->SetLightColor(FLinearColor(0.55f, 0.68f, 1.f));
	// the neon sign's red, just outside the glass, and the green of the computer screen
	Neon = CreateDefaultSubobject<UPointLightComponent>(TEXT("Neon"));
	Neon->SetupAttachment(Root);
	Neon->SetRelativeLocation(FVector(185, 110, 150));
	Neon->SetIntensity(700.f);
	Neon->SetAttenuationRadius(420.f);
	Neon->SetCastShadows(false);
	Neon->SetLightColor(FLinearColor(1.f, 0.1f, 0.07f));
	ScreenGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("ScreenGlow"));
	ScreenGlow->SetupAttachment(Root);
	ScreenGlow->SetRelativeLocation(FVector(0, 52, 100));
	ScreenGlow->SetIntensity(140.f);
	ScreenGlow->SetAttenuationRadius(130.f);
	ScreenGlow->SetCastShadows(false);
	ScreenGlow->SetLightColor(FLinearColor(0.45f, 1.f, 0.65f));
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
	for (UTextRenderComponent* T : Labels) if (T) T->DestroyComponent();
	Labels.Reset();
	ItemOf.Reset(); NeonMats.Reset(); SkyMats.Reset(); SkyBase.Reset();
	RainFall = RainDrops = nullptr; NeonText = nullptr;
	USceneComponent* R = RootComponent;
	const EHUDeskItem None = EHUDeskItem::Count;
	const FRotator Flat = FRotator::ZeroRotator;
	auto Mark = [&](UStaticMeshComponent* M, EHUDeskItem It) { if (M && It != None) ItemOf.Add(M, (int32)It); return M; };
	auto Part = [&](EHUDeskItem It, EShape S, FVector P, FVector Size, FLinearColor Col, FRotator Rot = FRotator::ZeroRotator, EHUSurface Surf = EHUSurface::None)
	{
		return Mark(Block(this, R, Blocks, S, P, Size, Col, Rot, true, Surf), It);
	};
	// decoration that's never clicked and doesn't need to block the mouse
	auto Deco = [&](EShape S, FVector P, FVector Size, FLinearColor Col, FRotator Rot = FRotator::ZeroRotator, EHUSurface Surf = EHUSurface::None)
	{
		return Block(this, R, Blocks, S, P, Size, Col, Rot, false, Surf);
	};
	// self-lit (lit windows, neon, the screen, a bulb): no shadows, they are the light
	auto Glow = [&](FVector P, FVector Size, FLinearColor Col, FRotator Rot = FRotator::ZeroRotator, EShape S = EShape::Cube, bool bAdd = false)
	{
		UStaticMeshComponent* M = Block(this, R, Blocks, S, P, Size, FLinearColor::Black, Rot, false);
		if (M) { M->SetMaterial(0, GlowMat(M, Col, bAdd)); M->SetCastShadow(false); }
		return M;
	};
	// print: TextRender faces +X, so Yaw 180 reads from the chair; Pitch 90 lays it flat on the desk
	auto Print = [&](FVector P, FRotator Rot, float Size, FColor Col, const TCHAR* Str)
	{
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
		T->SetupAttachment(R);
		T->SetRelativeLocation(P);
		T->SetRelativeRotation(Rot);
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(Size);
		T->SetTextRenderColor(Col);
		T->SetText(FText::FromString(Str));
		T->RegisterComponent();
		Labels.Add(T);
		return T;
	};

	// ---------- the office: a wood floor, plaster over dark panelling, the window in the back wall ----------
	const FLinearColor Plaster(0.07f, 0.08f, 0.066f), Panelling(0.05f, 0.022f, 0.01f), Trim(0.03f, 0.014f, 0.008f);
	Part(None, EShape::Cube, FVector(0, 0, -5), FVector(700, 600, 10), FLinearColor(0.06f, 0.03f, 0.015f), Flat, EHUSurface::WoodFloor);
	const float WT = 12.f, WallH = 300.f;
	auto Wall = [&](float Y0, float Y1, float Z0, float Z1)
	{
		if (Y1 - Y0 > 0.5f && Z1 - Z0 > 0.5f) Part(None, EShape::Cube, FVector(WallX + WT / 2, (Y0 + Y1) / 2, (Z0 + Z1) / 2), FVector(WT, Y1 - Y0, Z1 - Z0), Plaster, Flat, EHUSurface::Plaster);
	};
	Wall(-320, -WinHalf, 0, WallH); Wall(WinHalf, 320, 0, WallH); Wall(-WinHalf, WinHalf, 0, SillZ); Wall(-WinHalf, WinHalf, TopZ, WallH);
	for (float S : { -1.f, 1.f })
	{
		Part(None, EShape::Cube, FVector(-60, S * 215, WallH / 2), FVector(370, 10, WallH), Plaster * 0.85f, Flat, EHUSurface::Plaster);
		Deco(EShape::Cube, FVector(-60, S * 209, 45), FVector(370, 2, 90), Panelling, Flat, EHUSurface::Walnut);
		Deco(EShape::Cube, FVector(-60, S * 207.5f, 90), FVector(370, 4, 4), Trim);
	}
	Deco(EShape::Cube, FVector(WallX - 1, 0, 45), FVector(2, 420, 90), Panelling, Flat, EHUSurface::Walnut);   // panelling to the chair rail
	Deco(EShape::Cube, FVector(WallX - 2.5f, 0, 90), FVector(4, 420, 4), Trim);                              // the chair rail

	// the window: a dark painted frame, a cross of mullions in the glass, a deep sill
	const FLinearColor Frame(0.05f, 0.046f, 0.04f);
	Deco(EShape::Cube, FVector(WallX - 1.5f, 0, TopZ + 3), FVector(5, WinHalf * 2 + 12, 6), Frame);
	for (float S : { -1.f, 1.f }) Deco(EShape::Cube, FVector(WallX - 1.5f, S * (WinHalf + 3), (SillZ + TopZ) / 2), FVector(5, 6, TopZ - SillZ + 12), Frame);
	Deco(EShape::Cube, FVector(WallX - 4, 0, SillZ - 2), FVector(22, WinHalf * 2 + 16, 4), Frame * 1.4f);
	Deco(EShape::Cube, FVector(GlassX, 0, (SillZ + TopZ) / 2), FVector(4, 4, TopZ - SillZ), Frame);
	Deco(EShape::Cube, FVector(GlassX, 0, 150), FVector(4, WinHalf * 2, 4), Frame);
	Glow(FVector(GlassX + 1.f, 0, (SillZ + TopZ) / 2), FVector(0.2f, WinHalf * 2, TopZ - SillZ), FLinearColor(0.006f, 0.007f, 0.009f), Flat, EShape::Cube, true); // the glass's faint sheen

	// venetian blinds let down to BlindZ, the slats tilted open toward the street (they stripe the streetlight)
	const FLinearColor Slat(0.7f, 0.66f, 0.56f);
	const float BX = WallX - 5.f, BW = WinHalf * 2 - 4.f;
	Deco(EShape::Cube, FVector(BX, 0, TopZ - 3), FVector(5, BW + 4, 4), Slat * 0.75f);              // head rail
	for (float Z = TopZ - 7.f; Z > BlindZ + 2.f; Z -= 3.2f) Deco(EShape::Cube, FVector(BX, 0, Z), FVector(4.8f, BW, 0.4f), Slat, FRotator(38, 0, 0));
	Deco(EShape::Cube, FVector(BX, 0, BlindZ), FVector(5, BW, 1.4f), Slat * 0.8f);                  // bottom rail
	for (float Y : { -70.f, 0.f, 70.f }) for (float D : { -2.6f, 2.6f })                          // ladder strings
		Deco(EShape::Cube, FVector(BX + D, Y, (TopZ + BlindZ) / 2), FVector(0.15f, 0.15f, TopZ - BlindZ), Slat * 0.6f);
	Deco(EShape::Cube, FVector(BX - 3, WinHalf - 10, (BlindZ + TopZ) / 2 - 22), FVector(0.3f, 0.3f, TopZ - BlindZ + 44), Slat * 0.6f); // pull cord
	Deco(EShape::Cylinder, FVector(BX - 3, WinHalf - 10, BlindZ - 23), FVector(1.4f, 1.4f, 4), Slat * 0.5f);

	// ---------- the city across the street: three rows of buildings, lit windows, a sky lit from below ----------
	FRandomStream City(1957);
	UInstancedStaticMeshComponent* Lit[3] = {
		Instanced(this, R, Blocks, false, GlowMat(this, FLinearColor(1.2f, 0.72f, 0.28f), false)),  // lamps behind curtains
		Instanced(this, R, Blocks, false, GlowMat(this, FLinearColor(0.32f, 0.48f, 0.75f), false)), // office tubes, a TV
		Instanced(this, R, Blocks, false, GlowMat(this, FLinearColor(0.2f, 0.11f, 0.04f), false)) }; // a low lamp somewhere inside
	// the squad room is high up: from the chair you look down through the window, so the city lies below eye level -
	// rooftops across the street, a skyline further off, cut out against the haze of its own lights
	struct FRow { float X, Depth, Y0, Y1, MinTop, MaxTop, FloorH, Bay, Chance; FLinearColor Col; };
	const FRow Rows[] = {
		{ 2000.f, 600.f, -2100.f, 2100.f, -520.f, -120.f, 290.f, 80.f, 0.6f, FLinearColor(0.018f, 0.016f, 0.017f) },
		{ 2900.f, 800.f, -2900.f, 2900.f, -720.f, -150.f, 290.f, 85.f, 0.58f, FLinearColor(0.016f, 0.016f, 0.02f) },
		{ 4000.f, 1000.f, -4000.f, 4000.f, -960.f, -250.f, 290.f, 90.f, 0.55f, FLinearColor(0.014f, 0.014f, 0.021f) },
		{ 5600.f, 1400.f, -5800.f, 5800.f, -1300.f, -350.f, 290.f, 100.f, 0.5f, FLinearColor(0.012f, 0.013f, 0.022f) } };
	const float StreetZ = -1900.f; // the street, six floors down
	for (const FRow& Rw : Rows)
		for (float Y = Rw.Y0; Y < Rw.Y1;)
		{
			const float W = City.FRandRange(220.f, 560.f), Top = City.FRandRange(Rw.MinTop, Rw.MaxTop), X = Rw.X + City.FRandRange(-120.f, 160.f);
			// facades are self-lit just above black: dark shapes cut out of the glowing sky
			Glow(FVector(X + Rw.Depth / 2, Y + W / 2, (StreetZ + Top) / 2), FVector(Rw.Depth, W - 20.f, Top - StreetZ), Rw.Col * City.FRandRange(0.15f, 0.3f));
			if (City.FRand() < 0.3f) // a water tower on the roof
			{
				const FVector T(X + Rw.Depth / 2, Y + W * City.FRandRange(0.3f, 0.7f), Top);
				Glow(T + FVector(0, 0, 150), FVector(150, 150, 170), Rw.Col * 0.2f, Flat, EShape::Cylinder);
				Glow(T + FVector(0, 0, 32), FVector(120, 120, 64), Rw.Col * 0.15f);
			}
			else if (Rw.X > 3500.f && City.FRand() < 0.25f) // a red warning light on a far roof
				Glow(FVector(X, Y + W / 2, Top + 30.f), FVector(22, 22, 22), FLinearColor(2.5f, 0.12f, 0.06f), Flat, EShape::Sphere);
			for (float Z = Top - Rw.FloorH * 0.3f; Z > StreetZ + 200.f; Z -= Rw.FloorH)
				for (float B = Y + 60.f; B < Y + W - 80.f; B += Rw.Bay)
				{
					const float Roll = City.FRand();
					if (Roll > Rw.Chance) continue;
					const int32 Kind = Roll < Rw.Chance * 0.62f ? 0 : Roll < Rw.Chance * 0.85f ? 1 : 2;
					if (Lit[Kind]) Lit[Kind]->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X - 2.f, B + Rw.Bay * 0.3f, Z), FVector(0.04f, Rw.Bay * 0.38f / 100.f, Rw.FloorH * 0.3f / 100.f)));
				}
			Y += W;
		}
	// beyond the skyline: the haze of the city's lights low down, darker toward the sky (lightning lights it - TickWeather)
	const FLinearColor SkyCols[] = { FLinearColor(0.3f, 0.17f, 0.16f), FLinearColor(0.13f, 0.1f, 0.15f), FLinearColor(0.05f, 0.05f, 0.085f) };
	const float Bands[] = { -4000.f, -600.f, 600.f, 7000.f };
	for (int32 i = 0; i < 3; i++)
		if (UStaticMeshComponent* S = Glow(FVector(9000, 0, (Bands[i] + Bands[i + 1]) / 2), FVector(10, 32000, Bands[i + 1] - Bands[i]), SkyCols[i]))
		{
			SkyMats.Add(Cast<UMaterialInstanceDynamic>(S->GetMaterial(0)));
			SkyBase.Add(SkyCols[i]);
		}
	// a hotel's neon sign on the building across the street: H-O-T-E-L bent from red tube, in a frame of it
	NeonColor = FLinearColor(3.f, 0.25f, 0.15f);
	const FVector SignAt(1600, 430, -150);
	auto Tube = [&](FVector C, float W, float H)
	{
		if (UStaticMeshComponent* T = Glow(C, FVector(5, FMath::Max(W, 5.f), FMath::Max(H, 5.f)), NeonColor)) NeonMats.Add(Cast<UMaterialInstanceDynamic>(T->GetMaterial(0)));
	};
	Deco(EShape::Cube, SignAt + FVector(10, 0, 0), FVector(12, 90, 450), FLinearColor(0.02f, 0.01f, 0.01f));
	for (float S : { -1.f, 1.f }) { Tube(SignAt + FVector(0, S * 40, 0), 5, 440); Tube(SignAt + FVector(0, 0, S * 220), 85, 5); }
	const TCHAR* Word = TEXT("HOTEL");
	for (int32 i = 0; i < 5; i++)
	{
		const FVector L = SignAt + FVector(0, 0, 160.f - i * 80.f); // the letter's centre; -Y is its left, seen from the window
		const float HW = 22.f, HH = 29.f;
		auto Up = [&](float Y) { Tube(L + FVector(0, Y, 0), 5, HH * 2); };
		auto Across = [&](float Z, float W, float Y) { Tube(L + FVector(0, Y, Z), W, 5); };
		switch (Word[i])
		{
		case 'H': Up(-HW); Up(HW); Across(0, HW * 2, 0); break;
		case 'O': Up(-HW); Up(HW); Across(HH, HW * 2, 0); Across(-HH, HW * 2, 0); break;
		case 'T': Across(HH, HW * 2, 0); Up(0); break;
		case 'E': Up(-HW); Across(HH, HW * 2, 0); Across(0, HW * 1.4f, -HW * 0.3f); Across(-HH, HW * 2, 0); break;
		case 'L': Up(-HW); Across(-HH, HW * 2, 0); break;
		default: break;
		}
	}

	// rain falling past the window, and drops on the glass (their instances are made and moved at play: TickWeather)
	RainFall = Instanced(this, R, Blocks, false, GlowMat(this, FLinearColor(0.08f, 0.09f, 0.11f), true));
	RainDrops = Instanced(this, R, Blocks, true, GlowMat(this, FLinearColor(0.13f, 0.15f, 0.19f), true));

	// ---------- the furniture round the walls ----------
	// a filing cabinet in the corner left of the window, a coat stand with your coat and hat on the right
	if (!PropAt(this, R, Blocks, TEXT("FilingCabinet"), FVector(WallX - 36, -158, 0), 132.f, 180.f))
		Part(None, EShape::Cube, FVector(WallX - 36, -158, 66), FVector(70, 46, 132), FLinearColor(0.2f, 0.21f, 0.2f), Flat, EHUSurface::PaintedMetal);
	{
		const FVector C(WallX - 32, 142, 0);
		const FLinearColor Brass(0.25f, 0.18f, 0.07f), Coat(0.16f, 0.12f, 0.08f);
		Deco(EShape::Cylinder, C + FVector(0, 0, 1.5f), FVector(46, 46, 3), Trim);
		Deco(EShape::Cylinder, C + FVector(0, 0, 90), FVector(4, 4, 180), Trim * 1.4f, Flat, EHUSurface::Walnut);
		for (float A : { 0.f, 120.f, 240.f }) Deco(EShape::Cube, C + FRotator(0, A, 0).RotateVector(FVector(7, 0, 172)), FVector(14, 1.2f, 1.2f), Brass, FRotator(-25, A, 0));
		Deco(EShape::Cube, C + FVector(-9, 0, 122), FVector(14, 44, 96), Coat, FRotator(-4, 0, 0), EHUSurface::Fabric);    // the trench coat
		Deco(EShape::Cube, C + FVector(-9, 0, 162), FVector(16, 30, 14), Coat * 0.9f, FRotator(-4, 0, 0), EHUSurface::Fabric); // its collar
		Deco(EShape::Cylinder, C + FVector(4, 0, 181), FVector(34, 34, 1.2f), FLinearColor(0.03f, 0.025f, 0.022f), FRotator(0, 0, 8), EHUSurface::Fabric); // the fedora's brim
		Deco(EShape::Cylinder, C + FVector(4, 0, 186), FVector(19, 19, 11), FLinearColor(0.03f, 0.025f, 0.022f), FRotator(0, 0, 8), EHUSurface::Fabric);
		Deco(EShape::Cylinder, C + FVector(4, 0, 182.2f), FVector(19.4f, 19.4f, 2.4f), FLinearColor(0.08f, 0.02f, 0.015f), FRotator(0, 0, 8)); // hatband
	}

	// ---------- the desk: a steel precinct desk (or the old wooden one) under a green blotter ----------
	const FLinearColor Wood(0.2f, 0.1f, 0.04f);
	if (!Prop(this, R, Blocks, TEXT("Desk"), FVector(0, 0, DeskZ / 2), FVector(92, 184, DeskZ), FRotator(0, 180, 0)))
	{
		Part(None, EShape::Cube, FVector(0, 0, DeskZ - 3), FVector(92, 184, 6), Wood, Flat, EHUSurface::Walnut);
		for (float X : { -40.f, 40.f }) for (float Y : { -85.f, 85.f }) Part(None, EShape::Cube, FVector(X, Y, (DeskZ - 6) / 2), FVector(6, 6, DeskZ - 6), Wood * 0.7f, Flat, EHUSurface::Walnut);
	}
	Part(None, EShape::Cube, FVector(-2, 0, DeskZ + 0.25f), FVector(58, 100, 0.5f), FLinearColor(0.035f, 0.075f, 0.045f), Flat, EHUSurface::Fabric);
	for (float Y : { -48.f, 48.f }) Deco(EShape::Cube, FVector(-2, Y, DeskZ + 0.55f), FVector(58, 6, 0.4f), FLinearColor(0.05f, 0.02f, 0.01f), Flat, EHUSurface::Leather);

	// the lamp at the back left, leaning over the desk (or the old green banker's lamp); the light comes from its bulb
	FVector Bulb(19, -77, DeskZ + 47); // inside the arm lamp's shade (measured off a screenshot)
	if (!PropAt(this, R, Blocks, TEXT("DeskLamp"), FVector(34, -80, DeskZ), 50.f, 135.f, false))
	{
		Part(None, EShape::Cylinder, FVector(22, -70, DeskZ + 1), FVector(14, 14, 2), FLinearColor(0.15f, 0.25f, 0.12f));
		Part(None, EShape::Cylinder, FVector(22, -70, DeskZ + 22), FVector(1.5f, 1.5f, 42), FLinearColor(0.6f, 0.5f, 0.2f));
		Part(None, EShape::Cylinder, FVector(22, -62, DeskZ + 42), FVector(18, 9, 9), FLinearColor(0.1f, 0.3f, 0.12f), FRotator(0, 0, 90));
		Bulb = FVector(22, -62, DeskZ + 38);
		Glow(Bulb, FVector(3.5f, 3.5f, 3.5f), FLinearColor(6.f, 4.2f, 2.2f), Flat, EShape::Sphere); // the banker's lamp shows its bulb
	}
	LampSpot->SetRelativeLocation(Bulb);
	LampSpot->SetRelativeRotation((FVector(-2, -22, DeskZ) - Bulb).Rotation());
	Lamp->SetRelativeLocation(Bulb + FVector(-10, 10, 14));

	// CASE FILES: a stack of manila folders with their tabs, the top one stamped
	const FLinearColor Manila(0.62f, 0.48f, 0.24f);
	for (int32 i = 0; i < 4; i++)
	{
		const FRotator Rt(0, -6 + i * 4, 0);
		const FVector C(0, -5, DeskZ + 0.9f + i * 1.7f);
		Part(EHUDeskItem::CaseFiles, EShape::Cube, C, FVector(30, 24, 1.5f), Manila, Rt, EHUSurface::Fabric);
		Part(EHUDeskItem::CaseFiles, EShape::Cube, C + Rt.RotateVector(FVector(15.5f, -7.f + i * 4.5f, 0)), FVector(2.5f, 7, 1.2f), Manila * 0.9f, Rt);
	}
	Part(EHUDeskItem::CaseFiles, EShape::Cube, FVector(-4, -5, DeskZ + 7.6f), FVector(8, 16, 0.2f), FLinearColor(0.5f, 0.04f, 0.03f), FRotator(0, 6, 0));
	Print(FVector(-4, -5, DeskZ + 7.75f), FRotator(90, 186, 0), 2.6f, FColor(235, 225, 205), TEXT("HOMICIDE"));
	ItemPos[(int32)EHUDeskItem::CaseFiles] = FVector(0, -5, DeskZ);

	// THE COMPUTER: a beige CRT on the precinct login, and its keyboard
	if (!Mark(PropAt(this, R, Blocks, TEXT("CRTMonitor"), FVector(32, 52, DeskZ), 40.f, 180.f), EHUDeskItem::Computer))
		Part(EHUDeskItem::Computer, EShape::Cube, FVector(30, 52, DeskZ + 17), FVector(36, 38, 32), FLinearColor(0.62f, 0.58f, 0.48f));
	Mark(Glow(FVector(11.1f, 52, DeskZ + 23.f), FVector(0.4f, 31, 24), FLinearColor(0.02f, 0.07f, 0.04f)), EHUDeskItem::Computer);
	Print(FVector(10.6f, 52, DeskZ + 26.f), FRotator(0, 180, 0), 2.4f, FColor(110, 255, 150), TEXT("PRECINCT OS<br> <br>LOGIN: _"));
	ScreenGlow->SetRelativeLocation(FVector(0, 52, DeskZ + 24));
	if (!Mark(PropAt(this, R, Blocks, TEXT("Keyboard"), FVector(-6, 48, DeskZ), 0.f, 180.f), EHUDeskItem::Computer))
		Part(EHUDeskItem::Computer, EShape::Cube, FVector(-8, 46, DeskZ + 1), FVector(13, 38, 2), FLinearColor(0.55f, 0.52f, 0.44f));
	ItemPos[(int32)EHUDeskItem::Computer] = FVector(20, 50, DeskZ);

	// DESK PHONE: the precinct's black desk set
	if (!Mark(PropAt(this, R, Blocks, TEXT("DeskPhone"), FVector(6, -44, DeskZ), 0.f, 195.f), EHUDeskItem::DeskPhone))
	{
		Part(EHUDeskItem::DeskPhone, EShape::Cube, FVector(4, -46, DeskZ + 3.5f), FVector(18, 20, 7), FLinearColor(0.02f, 0.02f, 0.02f));
		Part(EHUDeskItem::DeskPhone, EShape::Cylinder, FVector(4, -46, DeskZ + 8.5f), FVector(3.5f, 3.5f, 22), FLinearColor(0.03f, 0.03f, 0.03f), FRotator(0, 0, 90));
	}
	ItemPos[(int32)EHUDeskItem::DeskPhone] = FVector(4, -46, DeskZ);

	// YOUR CELL PHONE: a flip phone, its little screen lit
	Part(EHUDeskItem::CellPhone, EShape::Cube, FVector(-26, 26, DeskZ + 1), FVector(9, 5, 2), FLinearColor(0.08f, 0.08f, 0.1f), FRotator(0, 20, 0));
	Mark(Glow(FVector(-26.5f, 26, DeskZ + 2.1f), FVector(3, 3, 0.2f), FLinearColor(0.25f, 0.9f, 0.5f), FRotator(0, 20, 0)), EHUDeskItem::CellPhone);
	ItemPos[(int32)EHUDeskItem::CellPhone] = FVector(-26, 26, DeskZ);

	// POLICE MANUAL: a thick navy binder, its rings showing at the spine
	const FRotator ManRot(0, 12, 0);
	const FVector ManAt(-12, -74, DeskZ);
	Part(EHUDeskItem::Manual, EShape::Cube, ManAt + FVector(0, 0, 2.5f), FVector(24, 18, 5), FLinearColor(0.035f, 0.06f, 0.17f), ManRot, EHUSurface::Leather);
	Part(EHUDeskItem::Manual, EShape::Cube, ManAt + ManRot.RotateVector(FVector(0, 0, 0)) + FVector(0, 0, 5.1f), FVector(10, 8, 0.2f), FLinearColor(0.7f, 0.6f, 0.2f), ManRot);
	Print(ManAt + FVector(0, 0, 5.3f), FRotator(90, 180 + 12, 0), 1.3f, FColor(40, 30, 10), TEXT("POLICE<br>MANUAL"));
	for (float Y : { -5.f, 0.f, 5.f }) Deco(EShape::Cylinder, ManAt + ManRot.RotateVector(FVector(0, -9.2f, 0)) + ManRot.RotateVector(FVector(Y, 0, 0)) + FVector(0, 0, 2.5f), FVector(2.2f, 2.2f, 0.6f), FLinearColor(0.5f, 0.5f, 0.52f), FRotator(0, 12, 90));
	ItemPos[(int32)EHUDeskItem::Manual] = ManAt;

	// THE PAPER: the late edition, folded, its headline up
	Part(EHUDeskItem::Paper, EShape::Cube, FVector(-22, -32, DeskZ + 0.6f), FVector(28, 20, 1.2f), FLinearColor(0.62f, 0.6f, 0.53f), FRotator(0, 18, 0), EHUSurface::Fabric);
	Print(FVector(-27.5f, -33.5f, DeskZ + 1.3f), FRotator(90, 198, 0), 2.2f, FColor(25, 22, 20), TEXT("THE EVENING LEDGER"));
	Print(FVector(-22.f, -32.f, DeskZ + 1.3f), FRotator(90, 198, 0), 3.0f, FColor(15, 12, 10), TEXT("BODY FOUND<br>IN HOTEL"));
	ItemPos[(int32)EHUDeskItem::Paper] = FVector(-22, -32, DeskZ);

	// the rest of a long night: cold coffee, a full ashtray with one still burning, a cup of pens
	const FLinearColor Mug(0.58f, 0.56f, 0.5f);
	Deco(EShape::Cylinder, FVector(-4, 80, DeskZ + 5), FVector(8.5f, 8.5f, 10), Mug);
	Deco(EShape::Cylinder, FVector(-4, 80, DeskZ + 9.4f), FVector(7.4f, 7.4f, 0.4f), FLinearColor(0.03f, 0.015f, 0.008f));
	Deco(EShape::Cube, FVector(-4, 85, DeskZ + 5), FVector(1.4f, 3, 6), Mug);
	Deco(EShape::Cylinder, FVector(-34, -62, DeskZ + 0.8f), FVector(11, 11, 1.6f), FLinearColor(0.12f, 0.13f, 0.13f), Flat, EHUSurface::Glass);
	for (int32 i = 0; i < 3; i++) Deco(EShape::Cylinder, FVector(-34 + i * 1.6f, -63.5f + i * 1.4f, DeskZ + 1.9f), FVector(0.8f, 0.8f, 3.2f), FLinearColor(0.55f, 0.5f, 0.42f), FRotator(0, i * 50.f, 85));
	Deco(EShape::Cylinder, FVector(-31, -58, DeskZ + 2.2f), FVector(0.8f, 0.8f, 6.5f), FLinearColor(0.7f, 0.68f, 0.62f), FRotator(0, 30, 80));
	Glow(FVector(-29.4f, -55.2f, DeskZ + 2.6f), FVector(0.9f, 0.9f, 0.9f), FLinearColor(4.f, 0.9f, 0.15f), Flat, EShape::Sphere); // the cigarette's tip
	Deco(EShape::Cylinder, FVector(42, -58, DeskZ + 5), FVector(7, 7, 10), FLinearColor(0.04f, 0.04f, 0.045f), Flat, EHUSurface::PaintedMetal);
	for (int32 i = 0; i < 3; i++) Deco(EShape::Cylinder, FVector(42 + (i - 1) * 1.2f, -58 + (i - 1) * 1.5f, DeskZ + 11), FVector(0.9f, 0.9f, 14), i == 1 ? FLinearColor(0.5f, 0.04f, 0.03f) : FLinearColor(0.03f, 0.04f, 0.12f), FRotator((i - 1) * 8.f, 0, (i - 1) * 6.f));

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
	// the rain: streaks falling past the window, drops beading and running on the glass
	Weather.Initialize((int32)FPlatformTime::Cycles());
	StreetBase = Street->Intensity;
	if (RainFall)
	{
		Falling.SetNum(340);
		for (FVector& Pt : Falling) Pt = FVector(Weather.FRandRange(GlassX + 20.f, 760.f), Weather.FRandRange(-330.f, 330.f), Weather.FRandRange(-320.f, 440.f));
		for (int32 i = 0; i < Falling.Num(); i++) RainFall->AddInstance(FTransform::Identity);
	}
	if (RainDrops)
	{
		Drops.SetNum(130);
		for (FDrop& D : Drops) ResetDrop(D, true);
		for (int32 i = 0; i < Drops.Num(); i++) RainDrops->AddInstance(FTransform::Identity);
	}
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
	// and the camera's focus follows it there (the window and the city stay soft behind)
	if (UHUFilmCamera* Film = GetWorld()->GetSubsystem<UHUFilmCamera>())
		Film->FocusOn(FVector::Dist(Seat->GetComponentLocation(), GetActorTransform().TransformPosition(ItemPos[Pick] + FVector(0, 0, 6))));
	const FString Shown = bTyping && TypingField == 0 ? Typed + (FMath::Fmod(GetWorld()->GetTimeSeconds(), 1.0) < 0.5 ? TEXT("_") : TEXT(" ")) : Name();
	PlateText->SetText(FText::FromString(Shown.IsEmpty() ? TEXT("— YOUR NAME —") : (TEXT("DET. ") + Shown).ToUpper()));
	TickWeather(DeltaSeconds);
}

// ---------- the night outside ----------
// a drop on the glass: it beads (holding still), then runs, catching now and then, until it reaches the sill
void AHUDeskMenu::ResetDrop(FDrop& D, bool bAnywhere)
{
	D.P = FVector(GlassX + 2.2f, Weather.FRandRange(-WinHalf + 3.f, WinHalf - 3.f), bAnywhere ? Weather.FRandRange(SillZ + 3.f, TopZ - 3.f) : Weather.FRandRange(SillZ + 40.f, TopZ - 3.f));
	D.Size = Weather.FRandRange(0.5f, 1.4f);
	D.Len = D.Size;
	D.Speed = 0.f;
	D.Hold = Weather.FRandRange(0.5f, 14.f);
}

void AHUDeskMenu::TickWeather(float Dt)
{
	Dt = FMath::Min(Dt, 0.1f);
	// rain past the window, slanted by the wind, starting again at the top when it reaches the street
	if (RainFall && Falling.Num() == RainFall->GetInstanceCount())
	{
		static const FVector Vel(0, -120, -980);
		static const FRotator Slant = FRotationMatrix::MakeFromZ(Vel.GetSafeNormal()).Rotator();
		TArray<FTransform> T;
		T.Reserve(Falling.Num());
		for (FVector& Pt : Falling)
		{
			Pt += Vel * Dt;
			if (Pt.Z < -320.f) Pt = FVector(Weather.FRandRange(GlassX + 20.f, 760.f), Weather.FRandRange(-330.f, 330.f), 440.f + Weather.FRandRange(0.f, 80.f));
			if (Pt.Y < -340.f) Pt.Y += 680.f;
			T.Add(FTransform(Slant, Pt, FVector(0.005f, 0.005f, 0.28f)));
		}
		RainFall->BatchUpdateInstancesTransforms(0, T, false, true, true);
	}
	// drops on the glass
	if (RainDrops && Drops.Num() == RainDrops->GetInstanceCount())
	{
		TArray<FTransform> T;
		T.Reserve(Drops.Num());
		for (FDrop& D : Drops)
		{
			if (D.Speed <= 0.f)
			{
				D.Hold -= Dt;
				if (D.Hold <= 0.f) D.Speed = Weather.FRandRange(6.f, 20.f);
			}
			else
			{
				D.P.Z -= D.Speed * Dt;
				D.P.Y += FMath::Sin(D.P.Z * 0.4f + D.Size * 9.f) * 1.5f * Dt;
				D.Len = FMath::Min(D.Len + Dt * 4.f, D.Size * 7.f); // a running drop draws its trail out behind it
				if (Weather.FRand() < Dt * 0.5f) { D.Speed = 0.f; D.Hold = Weather.FRandRange(0.2f, 2.f); } // it catches on the glass
				if (D.P.Z < SillZ + 1.5f) ResetDrop(D, false);
			}
			T.Add(FTransform(FRotator::ZeroRotator, D.P + FVector(0, 0, (D.Len - D.Size) * 0.5f), FVector(0.002f, D.Size / 100.f, D.Len / 100.f)));
		}
		RainDrops->BatchUpdateInstancesTransforms(0, T, false, true, true);
	}
	// the neon: steady, then a stutter now and then
	NextFlicker -= Dt;
	if (NextFlicker <= 0.f) { NeonOff = Weather.FRandRange(0.1f, 0.45f); NextFlicker = Weather.FRandRange(2.f, 8.f); }
	const bool bOn = NeonOff <= 0.f || FMath::Fmod(NeonOff * 17.f, 1.f) > 0.5f;
	NeonOff -= Dt;
	Neon->SetIntensity(bOn ? 700.f : 80.f);
	for (UMaterialInstanceDynamic* M : NeonMats) if (M) M->SetVectorParameterValue(TEXT("Color"), bOn ? NeonColor : NeonColor * 0.06f);
	if (NeonText) NeonText->SetVisibility(bOn);
	// lightning every so often: two quick flashes in the sky, and through the blinds
	NextFlash -= Dt;
	if (NextFlash <= 0.f) { Flash = 0.f; NextFlash = Weather.FRandRange(16.f, 40.f); }
	float Bright = 0.f;
	if (Flash >= 0.f)
	{
		Flash += Dt;
		Bright = Flash < 0.07f ? 1.f : Flash < 0.15f ? 0.12f : Flash < 0.22f ? 0.75f : FMath::Max(0.f, 0.4f - (Flash - 0.22f) * 1.2f);
		if (Flash > 0.6f) { Flash = -1.f; Bright = 0.f; }
	}
	for (int32 i = 0; i < SkyMats.Num(); i++)
		if (SkyMats[i]) SkyMats[i]->SetVectorParameterValue(TEXT("Color"), SkyBase[i] + FLinearColor(0.55f, 0.6f, 0.8f) * Bright * (i == 0 ? 0.5f : 1.f));
	Street->SetIntensity(StreetBase * (1.f + Bright * 3.f));
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
