#include "HUOffice.h"
#include "HUBuild.h"
#include "HUPlayerState.h"
#include "HUInteractComponent.h"
#include "HUUsables.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

using namespace HUBuild;

// ---- the catalogue: add options to these lists ----
struct FPaint { const TCHAR* Name; FLinearColor Color; };
static const FPaint Paints[] = {
	{ TEXT("Precinct cream"), FLinearColor(0.62f, 0.6f, 0.52f) }, { TEXT("Sage"), FLinearColor(0.42f, 0.5f, 0.4f) },
	{ TEXT("Navy"), FLinearColor(0.1f, 0.14f, 0.28f) },            { TEXT("Oxblood"), FLinearColor(0.32f, 0.08f, 0.07f) },
	{ TEXT("Charcoal"), FLinearColor(0.14f, 0.14f, 0.15f) },       { TEXT("Mustard"), FLinearColor(0.62f, 0.48f, 0.16f) },
	{ TEXT("Teal"), FLinearColor(0.1f, 0.34f, 0.34f) },            { TEXT("Wood panelling"), FLinearColor(0.3f, 0.17f, 0.08f) },
};
static const FPaint Carpets[] = {
	{ TEXT("Grey carpet"), FLinearColor(0.24f, 0.24f, 0.25f) },    { TEXT("Blue carpet"), FLinearColor(0.1f, 0.14f, 0.3f) },
	{ TEXT("Burgundy carpet"), FLinearColor(0.28f, 0.06f, 0.07f) },{ TEXT("Green carpet"), FLinearColor(0.12f, 0.22f, 0.13f) },
	{ TEXT("Oak boards"), FLinearColor(0.45f, 0.3f, 0.16f) },      { TEXT("Lino"), FLinearColor(0.38f, 0.38f, 0.34f) },
};
static const TCHAR* Posters[] = { TEXT("Nothing"), TEXT("The city at night"), TEXT("A WANTED poster"), TEXT("Your badge, framed"), TEXT("A pinned-up city map") };
// furniture: one bit each in FHUOfficeLook::Items
static const TCHAR* Items[] = { TEXT("Plant"), TEXT("Floor lamp"), TEXT("Bookshelf"), TEXT("Couch"), TEXT("Rug"), TEXT("Trophy shelf"),
	TEXT("Desk lamp"), TEXT("Coat rack"), TEXT("Filing cabinet"), TEXT("Fish tank") };
static constexpr int32 FixedRows = 3; // paint, carpet, poster

// the office, in its own frame: X 0..600 from the corridor wall (the door is at Y 90) to the back wall, Y 0..362
static const float OW = 600.f, OD = 362.5f, OH = 330.f, In = 8.5f;

int32 AHUOffice::RowCount() { return FixedRows + UE_ARRAY_COUNT(Items); }

FString AHUOffice::RowLabel(int32 Row, const FHUOfficeLook& L)
{
	switch (Row)
	{
	case 0: return FString::Printf(TEXT("Walls: %s"), Paints[L.Wall % UE_ARRAY_COUNT(Paints)].Name);
	case 1: return FString::Printf(TEXT("Floor: %s"), Carpets[L.Floor % UE_ARRAY_COUNT(Carpets)].Name);
	case 2: return FString::Printf(TEXT("On the wall: %s"), Posters[L.Poster % UE_ARRAY_COUNT(Posters)]);
	default:
	{
		const int32 I = Row - FixedRows;
		return FString::Printf(TEXT("%s: %s"), Items[I], (L.Items & (1 << I)) ? TEXT("yes") : TEXT("no"));
	}
	}
}

void AHUOffice::ChangeRow(int32 Row, FHUOfficeLook& L)
{
	switch (Row)
	{
	case 0: L.Wall = (L.Wall + 1) % UE_ARRAY_COUNT(Paints); break;
	case 1: L.Floor = (L.Floor + 1) % UE_ARRAY_COUNT(Carpets); break;
	case 2: L.Poster = (L.Poster + 1) % UE_ARRAY_COUNT(Posters); break;
	default: L.Items ^= 1 << (Row - FixedRows); break;
	}
}

FHUOfficeLook AHUOffice::LoadSaved()
{
	if (UGameplayStatics::DoesSaveGameExist(TEXT("HUOffice"), 0))
		if (const UHUOfficeSave* S = Cast<UHUOfficeSave>(UGameplayStatics::LoadGameFromSlot(TEXT("HUOffice"), 0))) if (S->bSet) return S->Look;
	FHUOfficeLook Def;
	Def.Items = (1 << 0) | (1 << 6); // a plant and a desk lamp to start
	return Def;
}

void AHUOffice::Save(const FHUOfficeLook& L)
{
	UHUOfficeSave* S = Cast<UHUOfficeSave>(UGameplayStatics::CreateSaveGameObject(UHUOfficeSave::StaticClass()));
	S->Look = L; S->bSet = true;
	UGameplayStatics::SaveGameToSlot(S, TEXT("HUOffice"), 0);
}

AHUOffice::AHUOffice()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AHUOffice::BeginPlay()
{
	Super::BeginPlay();
	// the catalogue, on the wall by the door (inside), and the name by the door (outside)
	CatalogueParts.Add(Block(this, Root, Fixed, EShape::Cube, FVector(In + 1, 250, 150), FVector(2, 120, 140), FLinearColor(0.12f, 0.1f, 0.08f)));
	CatalogueParts.Add(Block(this, Root, Fixed, EShape::Cube, FVector(In + 2.2f, 250, 150), FVector(0.5f, 112, 132), FLinearColor(0.88f, 0.86f, 0.78f)));
	CatalogueText = NewObject<UTextRenderComponent>(this);
	CatalogueText->SetupAttachment(Root);
	CatalogueText->SetRelativeLocation(FVector(In + 2.8f, 250, 212));
	CatalogueText->SetHorizontalAlignment(EHTA_Center);
	CatalogueText->SetVerticalAlignment(EVRTA_TextTop);
	CatalogueText->SetWorldSize(6.f);
	CatalogueText->SetTextRenderColor(FColor(30, 26, 22));
	CatalogueText->RegisterComponent();
	DoorName = NewObject<UTextRenderComponent>(this);
	DoorName->SetupAttachment(Root);
	DoorName->SetRelativeLocation(FVector(-9.5f, 90, 205));
	DoorName->SetRelativeRotation(FRotator(0, 180, 0));
	DoorName->SetHorizontalAlignment(EHTA_Center);
	DoorName->SetVerticalAlignment(EVRTA_TextCenter);
	DoorName->SetWorldSize(9.f);
	DoorName->SetTextRenderColor(FColor(235, 200, 110));
	DoorName->RegisterComponent();
}

AHUPlayerState* AHUOffice::OwnerState() const
{
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
		for (APlayerState* P : GS->PlayerArray) if (AHUPlayerState* HP = Cast<AHUPlayerState>(P)) if (HP->Desk == Index) return HP;
	return nullptr;
}

bool AHUOffice::IsOwner(const APawn* P) const
{
	const AHUPlayerState* O = OwnerState();
	return O && P && P->GetPlayerState() == O;
}

void AHUOffice::HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const
{
	if (CatalogueParts.Contains(Part)) Out.Append(CatalogueParts);
}

FText AHUOffice::GetPrompt_Implementation(APawn* User) const
{
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I || !CatalogueParts.Contains(I->FocusPart)) return FText::GetEmpty();
	const AHUPlayerState* O = OwnerState();
	if (!IsOwner(User)) return FText::FromString(O ? FString::Printf(TEXT("Det. %s's office  -  only they redecorate it"), *O->GetPlayerName().Replace(TEXT("_"), TEXT(" "))) : FString(TEXT("A vacant office")));
	return FText::FromString(FString::Printf(TEXT("[E] Next line  ·  [Q] Change it  -  %s"), *RowLabel(Row, O->Office)));
}

// runs on every machine; only the owner's own machine changes anything (and tells the squad through the player state)
void AHUOffice::Interact_Implementation(APawn* User, bool bSecondary)
{
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I || !CatalogueParts.Contains(I->FocusPart) || !User->IsLocallyControlled()) return;
	AHUPlayerState* O = OwnerState();
	if (!IsOwner(User) || !O) return;
	if (!bSecondary) { Row = (Row + 1) % RowCount(); RefreshText(O->Office); return; }
	FHUOfficeLook L = O->Office;
	ChangeRow(Row, L);
	Save(L);
	O->Office = L;          // at once here...
	O->ServerSetOffice(L);  // ...and for the squad
	Dress(L);
}

void AHUOffice::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AHUPlayerState* O = OwnerState();
	// your own office: put your saved look up the first time you're given it
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (O && PC && PC->PlayerState == O && !bSentSaved)
	{
		bSentSaved = true;
		const FHUOfficeLook L = LoadSaved();
		if (L != O->Office) { O->Office = L; O->ServerSetOffice(L); }
	}
	const FHUOfficeLook Want = O ? O->Office : FHUOfficeLook();
	if (!bBuiltOnce || Want != Built) Dress(Want);
	if (DoorName)
	{
		const FString N = O ? TEXT("DET. ") + O->GetPlayerName().Replace(TEXT("_"), TEXT(" ")).ToUpper() : FString(TEXT("VACANT"));
		if (DoorName->Text.ToString() != N) DoorName->SetText(FText::FromString(N));
	}
}

void AHUOffice::RefreshText(const FHUOfficeLook& L)
{
	if (!CatalogueText) return;
	const AHUPlayerState* O = OwnerState();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const bool bMine = O && PC && PC->PlayerState == O;
	FString S = TEXT("DECORATING<br>");
	for (int32 r = 0; r < RowCount(); r++) S += (bMine && r == Row ? TEXT("> ") : TEXT("   ")) + RowLabel(r, L) + TEXT("<br>");
	CatalogueText->SetText(FText::FromString(S));
}

// dress the room: paint, carpet, the poster, the furniture
void AHUOffice::Dress(const FHUOfficeLook& L)
{
	Built = L; bBuiltOnce = true;
	Clear(Decor);
	auto B = [&](FVector P, FVector S, FLinearColor C, bool bCollide = true, EHUSurface Sf = EHUSurface::None) { return Block(this, Root, Decor, EShape::Cube, P, S, C, FRotator::ZeroRotator, bCollide, Sf); };
	auto Cyl = [&](FVector P, FVector S, FLinearColor C, bool bCollide = true) { return Block(this, Root, Decor, EShape::Cylinder, P, S, C, FRotator::ZeroRotator, bCollide); };
	auto Ball = [&](FVector P, FVector S, FLinearColor C) { return Block(this, Root, Decor, EShape::Sphere, P, S, C, FRotator::ZeroRotator, false); };
	const FLinearColor Paint = Paints[L.Wall % UE_ARRAY_COUNT(Paints)].Color, Carpet = Carpets[L.Floor % UE_ARRAY_COUNT(Carpets)].Color;
	const FLinearColor Wood(0.32f, 0.19f, 0.09f), Steel(0.3f, 0.32f, 0.34f), Brass(0.75f, 0.6f, 0.2f);
	// the paint: a skin over each wall, inside (the door wall in pieces around the doorway)
	// (painted plaster and a woven carpet, tinted to exactly the colours the owner picked - HUSurfaces.h)
	B(FVector(OW - In, OD / 2, OH / 2), FVector(1, OD - 2 * In, OH), Paint, false, EHUSurface::Plaster);
	B(FVector(OW / 2, In, OH / 2), FVector(OW - 2 * In, 1, OH), Paint, false, EHUSurface::Plaster);
	B(FVector(OW / 2, OD - In, OH / 2), FVector(OW - 2 * In, 1, OH), Paint, false, EHUSurface::Plaster);
	B(FVector(In, (In + 40) / 2, OH / 2), FVector(1, 40 - In, OH), Paint, false, EHUSurface::Plaster);
	B(FVector(In, (140 + OD - In) / 2, OH / 2), FVector(1, OD - In - 140, OH), Paint, false, EHUSurface::Plaster);
	B(FVector(In, 90, (220 + OH) / 2), FVector(1, 100, OH - 220), Paint, false, EHUSurface::Plaster);
	B(FVector(OW / 2, OD / 2, 1.4f), FVector(OW - 2 * In, OD - 2 * In, 1.2f), Carpet, false, EHUSurface::Fabric);
	// what hangs on the wall over the couch side
	switch (L.Poster % UE_ARRAY_COUNT(Posters))
	{
	case 1: // the city at night
		B(FVector(160, OD - In - 1.5f, 175), FVector(150, 2, 90), FLinearColor(0.03f, 0.04f, 0.1f), false);
		for (int32 i = 0; i < 9; i++) B(FVector(95 + i * 15, OD - In - 2.8f, 140 + (i * 37 % 40)), FVector(10, 1, 10 + (i * 53 % 50)), FLinearColor(0.1f, 0.1f, 0.14f), false);
		for (int32 i = 0; i < 14; i++) B(FVector(95 + (i * 41 % 130), OD - In - 3.2f, 145 + (i * 29 % 45)), FVector(2, 1, 2), FLinearColor(2.f, 1.6f, 0.6f), false);
		break;
	case 2: // WANTED
		B(FVector(160, OD - In - 1.5f, 175), FVector(70, 2, 95), FLinearColor(0.85f, 0.78f, 0.6f), false);
		B(FVector(160, OD - In - 2.8f, 170), FVector(40, 1, 45), FLinearColor(0.25f, 0.2f, 0.16f), false);
		break;
	case 3: // the badge, framed
		B(FVector(160, OD - In - 1.5f, 175), FVector(60, 2, 70), Wood, false);
		B(FVector(160, OD - In - 2.8f, 175), FVector(50, 1, 60), FLinearColor(0.05f, 0.08f, 0.2f), false);
		Block(this, Root, Decor, EShape::Cylinder, FVector(160, OD - In - 3.6f, 178), FVector(22, 22, 1), Brass, FRotator(0, 0, 90), false);
		break;
	case 4: // the city map, pins in it
		B(FVector(160, OD - In - 1.5f, 175), FVector(160, 2, 110), FLinearColor(0.82f, 0.8f, 0.68f), false);
		for (int32 i = 0; i < 8; i++) Ball(FVector(100 + (i * 37 % 120), OD - In - 3.f, 135 + (i * 23 % 80)), FVector(3, 3, 3), FLinearColor(0.8f, 0.05f, 0.05f));
		break;
	default: break;
	}
	auto Has = [&](int32 Bit) { return (L.Items & (1 << Bit)) != 0; };
	if (Has(0)) { Cyl(FVector(35, OD - 30, 22), FVector(34, 34, 44), FLinearColor(0.35f, 0.22f, 0.14f)); Ball(FVector(35, OD - 30, 85), FVector(60, 60, 80), FLinearColor(0.12f, 0.32f, 0.12f)); }
	if (Has(1)) { Cyl(FVector(OW - 40, 35, 80), FVector(4, 4, 160), Brass, false); HUUsables::Mark(Cyl(FVector(OW - 40, 35, 165), FVector(36, 36, 26), FLinearColor(0.9f, 0.85f, 0.7f), false), EHUUse::Lamp, FString(), 180.f); Cyl(FVector(OW - 40, 35, 2), FVector(30, 30, 4), Brass); }
	if (Has(2)) // a bookshelf on the back wall
	{
		B(FVector(OW - 30, 300, 100), FVector(40, 100, 200), Wood);
		for (int32 s = 0; s < 4; s++) for (int32 b = 0; b < 7; b++)
			B(FVector(OW - 38, 262 + b * 12, 22 + s * 48), FVector(24, 9, 30 + (b * 7 % 10)), FLinearColor(0.15f + (b % 3) * 0.15f, 0.08f + (s % 2) * 0.1f, 0.06f + (b % 2) * 0.1f), false);
	}
	if (Has(3)) // a couch along the north wall, by the door
	{
		HUUsables::Mark(B(FVector(160, OD - 45, 25), FVector(180, 70, 40), FLinearColor(0.3f, 0.16f, 0.1f)), EHUUse::Bench, FString(), 999.f, 1.f); // sit (facing the way you look)
		B(FVector(160, OD - 75, 60), FVector(180, 14, 50), FLinearColor(0.3f, 0.16f, 0.1f));
		for (float S : { -1.f, 1.f }) B(FVector(160 + S * 92, OD - 45, 40), FVector(16, 70, 40), FLinearColor(0.26f, 0.14f, 0.08f));
	}
	if (Has(4)) B(FVector(160, OD - 140, 2.4f), FVector(200, 150, 1), FLinearColor(0.45f, 0.12f, 0.1f), false);
	if (Has(5)) // trophies on a wall shelf
	{
		B(FVector(380, In + 12, 165), FVector(150, 22, 3), Wood, false);
		for (int32 t = 0; t < 4; t++) { Cyl(FVector(320 + t * 40, In + 12, 175), FVector(8, 8, 18 + t * 3), Brass, false); Ball(FVector(320 + t * 40, In + 12, 190 + t * 3), FVector(10, 10, 8), Brass); }
	}
	if (Has(6)) // a green banker's lamp on the desk
	{
		Cyl(FVector(330, 245, 79), FVector(14, 14, 3), Brass, false);
		Cyl(FVector(330, 245, 90), FVector(2, 2, 22), Brass, false);
		HUUsables::Mark(Block(this, Root, Decor, EShape::Cylinder, FVector(330, 245, 101), FVector(10, 26, 8), FLinearColor(0.05f, 0.35f, 0.15f), FRotator(0, 90, 90), false), EHUUse::Lamp, FString(), 90.f); // (its front: the chair's side)
	}
	if (Has(7)) { Cyl(FVector(40, 30, 90), FVector(4, 4, 180), Wood); for (float A : { 0.f, 120.f, 240.f }) B(FVector(40, 30, 175) + FRotator(0, A, 0).Vector() * 12, FVector(16, 3, 3), Wood, false); }
	if (Has(8)) { HUUsables::Mark(B(FVector(OW - 35, 60, 66), FVector(50, 60, 132), Steel), EHUUse::Files, FString(), 180.f); /* drawers on the room side */ for (int32 d = 0; d < 4; d++) B(FVector(OW - 61, 60, 20 + d * 32), FVector(1, 24, 3), FLinearColor(0.08f, 0.08f, 0.09f), false); }
	if (Has(9)) // a fish tank on a stand by the couch
	{
		B(FVector(290, OD - 40, 40), FVector(90, 45, 80), Wood);
		B(FVector(290, OD - 40, 105), FVector(86, 41, 50), FLinearColor(0.2f, 0.45f, 0.65f), false);
		for (int32 f = 0; f < 3; f++) Ball(FVector(265 + f * 22, OD - 40, 95 + f * 10), FVector(7, 3, 5), FLinearColor(1.f, 0.45f, 0.1f));
	}
	RefreshText(L);
}
