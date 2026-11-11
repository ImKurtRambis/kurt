#include "HUCaseBoard.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

using namespace HUBuild;

// The board's own frame: X sticks out of the board into the room, Z is up, and U runs left to right as you
// face the board (that's local -Y). The cork spans U -330..330 and Z 65..315 above the floor.
static FVector At(float U, float Z, float Depth) { return FVector(Depth, -U, Z); }

// the suspects' coats at the scene (the same list as AHUCrimeScene), so the photo matches the person you met
static const FLinearColor SuspectCoats[] = { {0.25f, 0.18f, 0.1f}, {0.1f, 0.12f, 0.2f}, {0.3f, 0.05f, 0.05f}, {0.12f, 0.2f, 0.12f}, {0.35f, 0.3f, 0.2f}, {0.05f, 0.05f, 0.05f} };
static const FLinearColor Paper(0.86f, 0.83f, 0.72f), HeldPaper(1.f, 0.82f, 0.25f), Polaroid(0.9f, 0.89f, 0.85f), Red(0.6f, 0.02f, 0.02f);
static const FColor Ink(28, 24, 20), InkRed(150, 20, 20), Chalk(238, 232, 215);

// word-wrap for TextRender (which only breaks lines at <br>), cut to MaxLines
static FString Wrap(const FString& S, int32 Width, int32 MaxLines)
{
	TArray<FString> Words; S.ParseIntoArrayWS(Words);
	TArray<FString> Lines; FString Line;
	for (const FString& W : Words)
	{
		if (!Line.IsEmpty() && Line.Len() + 1 + W.Len() > Width) { Lines.Add(Line); Line.Reset(); }
		Line += (Line.IsEmpty() ? FString() : FString(TEXT(" "))) + W;
	}
	if (!Line.IsEmpty()) Lines.Add(Line);
	if (Lines.Num() > MaxLines) { Lines.SetNum(MaxLines); Lines.Last() = Lines.Last().Left(FMath::Max(1, Width - 3)) + TEXT("..."); }
	return FString::Join(Lines, TEXT("<br>"));
}

AHUCaseBoard::AHUCaseBoard()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	bReplicates = false; // every machine spawns its own, with the same name (see AHUSquadRoom::BeginPlay)
}

UHUCaseSubsystem* AHUCaseBoard::Cases() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
}

void AHUCaseBoard::BeginPlay()
{
	Super::BeginPlay();
	// the cork in a dark wooden frame, and a picture light over it
	Block(this, Root, Frame, EShape::Cube, At(0, 190, 0.5f), FVector(1, 680, 266), FLinearColor(0.1f, 0.06f, 0.03f));
	Block(this, Root, Frame, EShape::Cube, At(0, 190, 1.2f), FVector(1.5f, 660, 250), FLinearColor(0.42f, 0.29f, 0.16f));
	Block(this, Root, Frame, EShape::Cube, At(0, 63, 5), FVector(10, 680, 4), FLinearColor(0.1f, 0.06f, 0.03f)); // the ledge
	Block(this, Root, Frame, EShape::Cube, At(0, 326, 14), FVector(8, 420, 5), FLinearColor(0.15f, 0.15f, 0.14f), FRotator::ZeroRotator, false);
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetRelativeLocation(At(0, 290, 140));
	L->SetIntensity(1400.f);
	L->SetAttenuationRadius(600.f);
	L->SetLightColor(FLinearColor(1.f, 0.85f, 0.65f));
	L->RegisterComponent();

	// the usable cards, named so every machine's copy matches
	SceneCard = MakeCard(TEXT("BoardScene"));
	WeaponCard = MakeCard(TEXT("BoardWeapon"));
	for (int32 i = 0; i < MaxSuspects; i++) SusCards.Add(MakeCard(*FString::Printf(TEXT("BoardSuspect%d"), i)));
	for (int32 i = 0; i < MaxEvidence; i++) EvCards.Add(MakeCard(*FString::Printf(TEXT("BoardEvidence%d"), i)));

	if (FParse::Param(FCommandLine::Get(), TEXT("HUBoardTest"))) TestClock = 0.f;
	Rebuild();
}

UStaticMeshComponent* AHUCaseBoard::MakeCard(const TCHAR* Name)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, FName(Name));
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	C->SetupAttachment(Root);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		C->SetMaterial(0, UMaterialInstanceDynamic::Create(Base, C));
	C->RegisterComponent();
	C->SetVisibility(false);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	return C;
}

void AHUCaseBoard::PlaceCard(UStaticMeshComponent* C, float U, float Z, float W, float H, FLinearColor Color, float Depth)
{
	if (!C) return;
	C->SetRelativeLocation(At(U, Z, Depth));
	C->SetRelativeScale3D(FVector(1.f, W, H) / 100.f);
	if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0))) M->SetVectorParameterValue(TEXT("Color"), Color);
	C->SetVisibility(true);
	C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// a red pushpin at the top
	Block(this, Root, Deco, EShape::Sphere, At(U, Z + H / 2 - 3, Depth + 1.5f), FVector(2.4f, 2.4f, 2.4f), Red, FRotator::ZeroRotator, false);
}

UTextRenderComponent* AHUCaseBoard::Write(const FString& S, float U, float Z, float Size, FColor Color, float Depth)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetupAttachment(Root);
	T->SetRelativeLocation(At(U, Z, Depth));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Color);
	T->SetText(FText::FromString(S));
	T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	T->RegisterComponent();
	Texts.Add(T);
	return T;
}

// red string from A to B (board-local points)
void AHUCaseBoard::Thread(FVector A, FVector B)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < 1.f) return;
	Block(this, Root, Deco, EShape::Cylinder, (A + B) / 2, FVector(0.5f, 0.5f, Len), FLinearColor(0.75f, 0.03f, 0.03f), FRotationMatrix::MakeFromZ(D / Len).Rotator(), false);
}

TArray<FString> AHUCaseBoard::Bagged() const
{
	TArray<FString> Out;
	if (const UHUCaseSubsystem* C = Cases())
		for (const FHUClue& K : C->Current.Clues)
			if (C->HasFound(K.Id) && Out.Num() < MaxEvidence) Out.Add(K.Id);
	return Out;
}

FString AHUCaseBoard::Signature() const
{
	const UHUCaseSubsystem* C = Cases();
	if (!C) return FString();
	FString S = FString::Printf(TEXT("%d|%s|%d%d|%s|"), C->Current.Number, *C->Current.Title, C->bCaseActive, C->bAwaitingCaptain, *C->LastVerdict.Left(24));
	S += FString::Join(Bagged(), TEXT(",")) + TEXT("|");
	for (const TPair<FString, FString>& P : C->BoardPins) S += P.Key + TEXT(">") + P.Value + TEXT(",");
	for (const TPair<FString, uint8>& P : C->BoardMarks) S += FString::Printf(TEXT("%s=%d,"), *P.Key, P.Value);
	for (const TPair<TWeakObjectPtr<APawn>, FString>& P : Holding) S += TEXT("^") + P.Value;
	return S;
}

void AHUCaseBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (auto It = Holding.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent(); // they left
	if (Signature() != Shown) Rebuild();
	if (TestClock >= 0.f) TestStep(DeltaSeconds);
}

void AHUCaseBoard::Rebuild()
{
	Shown = Signature();
	Clear(Deco);
	for (UTextRenderComponent* T : Texts) if (T) T->DestroyComponent();
	Texts.Reset();
	for (UStaticMeshComponent* Cd : SusCards) { Cd->SetVisibility(false); Cd->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	for (UStaticMeshComponent* Cd : EvCards) { Cd->SetVisibility(false); Cd->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	SceneCard->SetVisibility(false); SceneCard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponCard->SetVisibility(false); WeaponCard->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const UHUCaseSubsystem* C = Cases();
	if (!C || C->Current.Suspects.Num() == 0)
	{
		Write(TEXT("CASE BOARD"), 0, 230, 16, Chalk);
		Write(TEXT("NOTHING ON IT YET.<br>IT FILLS IN WHEN YOU CATCH A CASE - BRING THE EVIDENCE BACK HERE AND WORK IT."), 0, 185, 6, FColor(230, 210, 150));
		return;
	}
	const FHUCase& K = C->Current;

	// ---- the header ----
	// (the title already carries the case number)
	Write(Wrap(K.Title.Contains(TEXT("#")) ? K.Title.ToUpper() : FString::Printf(TEXT("CASE #%d  -  %s"), K.Number, *K.Title.ToUpper()), 64, 1), 0, 302, 9, Chalk);
	FString Status = !C->bCaseActive ? TEXT("CLOSED  -  ") + C->LastVerdict
		: C->bAwaitingCaptain ? FString(TEXT("BACK FROM THE SCENE  -  WORK IT OUT, BOOK THEM, THEN THE CAPTAIN'S IN-TRAY"))
		: FString(TEXT("OPEN  -  BRING THE EVIDENCE BACK AND WORK IT HERE"));
	Write(Wrap(Status.ToUpper(), 90, 1), 0, 288, 5.5f, FColor(255, 196, 90));

	// ---- the scene and the weapon, top left ----
	PlaceCard(SceneCard, -255, 225, 130, 100, Paper);
	Write(TEXT("THE SCENE"), -255, 265, 7, InkRed);
	FString Scene = Wrap(K.Place.ToUpper(), 22, 2) + TEXT("<br>") + Wrap(K.Crime, 22, 2);
	if (!K.Victim.IsEmpty()) Scene += TEXT("<br>VICTIM: ") + Wrap(K.Victim, 16, 1);
	if (!K.Time.IsEmpty()) Scene += TEXT("<br>TIME: ") + Wrap(K.Time, 16, 1);
	Write(Scene, -255, 220, 5.5f, Ink);
	PlaceCard(WeaponCard, -255, 118, 130, 80, Paper);
	Write(TEXT("MURDER WEAPON"), -255, 147, 6, InkRed);
	Write(Wrap(K.Weapon.IsEmpty() ? FString(TEXT("UNKNOWN")) : K.Weapon.ToUpper(), 20, 3), -255, 116, 5.5f, Ink);

	// ---- the suspects: a mugshot each, name and role under it, their pinned evidence below ----
	const TArray<FString> Ev = Bagged();
	const int32 N = FMath::Min(K.Suspects.Num(), MaxSuspects);
	const float Left = -175.f, Width = 495.f, Step = Width / FMath::Max(N, 1);
	for (int32 i = 0; i < N; i++)
	{
		const FHUSuspect& S = K.Suspects[i];
		const float U = Left + Step * (i + 0.5f);
		const float Sc = FMath::Min(1.f, (Step - 6.f) / 60.f); // seven or eight suspects: smaller photos
		auto PZ = [Sc](float Z) { return 247.f + (Z - 247.f) * Sc; }; // photo heights, scaled about the card's middle
		PlaceCard(SusCards[i], U, 247, 60 * Sc, 70 * Sc, Polaroid);
		// the photo: grey wall with height lines, coat and shoulders, a head, the booking placard
		Block(this, Root, Deco, EShape::Cube, At(U, PZ(254), 2.3f), FVector(0.5f, 50 * Sc, 50 * Sc), FLinearColor(0.33f, 0.35f, 0.38f), FRotator::ZeroRotator, false);
		for (float Z : { 238.f, 250.f, 262.f, 274.f }) Block(this, Root, Deco, EShape::Cube, At(U, PZ(Z), 2.7f), FVector(0.3f, 50 * Sc, 0.6f), FLinearColor(0.75f, 0.75f, 0.72f), FRotator::ZeroRotator, false);
		Block(this, Root, Deco, EShape::Cube, At(U, PZ(238), 4), FVector(3, 38 * Sc, 18 * Sc), SuspectCoats[i % UE_ARRAY_COUNT(SuspectCoats)], FRotator::ZeroRotator, false);
		Block(this, Root, Deco, EShape::Cube, At(U, PZ(248), 4), FVector(3, 8 * Sc, 6 * Sc), FLinearColor(0.6f, 0.42f, 0.32f), FRotator::ZeroRotator, false);
		Block(this, Root, Deco, EShape::Sphere, At(U, PZ(259), 4.5f), FVector(6, 15 * Sc, 18 * Sc), FLinearColor(0.6f, 0.42f, 0.32f), FRotator::ZeroRotator, false);
		Block(this, Root, Deco, EShape::Cube, At(U, PZ(236), 6), FVector(0.5f, 20 * Sc, 7 * Sc), FLinearColor(0.03f, 0.03f, 0.03f), FRotator::ZeroRotator, false);
		Write(FString::Printf(TEXT("No. %d"), i + 1), U, PZ(236), 3.4f * Sc, Chalk, 6.6f);
		Write(Wrap(S.Name.ToUpper(), Sc < 1.f ? 14 : 20, 2), U, 203, 4.6f * FMath::Max(Sc, 0.85f), Chalk);
		Write(Wrap(S.Role, 24, 1), U, 193, 3.6f, FColor(200, 195, 180));
		// the mark, stamped across the photo in front of everything on it
		const uint8 Mark = C->BoardMarks.FindRef(S.Id);
		if (Mark == 1) { UTextRenderComponent* T = Write(TEXT("PRIME<br>SUSPECT"), U, PZ(244), 7.5f * Sc, FColor(225, 15, 15), 11.f); T->SetRelativeRotation(FRotator(0, 0, -14)); }
		if (Mark == 2) { UTextRenderComponent* T = Write(TEXT("CLEARED"), U, PZ(244), 7.5f * Sc, FColor(20, 70, 200), 11.f); T->SetRelativeRotation(FRotator(0, 0, -14)); }

		int32 Row = 0, More = 0;
		for (int32 e = 0; e < Ev.Num(); e++)
		{
			if (C->BoardPins.FindRef(Ev[e]) != S.Id) continue;
			if (Row >= 3) { More++; continue; }
			const float Z = 178.f - Row * 21.f;
			bool bHeld = false; for (const TPair<TWeakObjectPtr<APawn>, FString>& P : Holding) bHeld |= P.Value == Ev[e];
			PlaceCard(EvCards[e], U, Z, FMath::Min(58.f, Step - 4.f), 17, bHeld ? HeldPaper : Paper, bHeld ? 6.f : 1.5f);
			FHUClue Clue; C->GetClue(Ev[e], Clue);
			Write(Wrap(Clue.Name, 16, 2), U, Z - 1, 3.6f, Ink, bHeld ? 7.2f : 2.6f);
			Thread(At(U - 22 * Sc, PZ(213), 3.2f), At(U - 22 * Sc, Z + 6, 3.2f));
			Row++;
		}
		if (More > 0) Write(FString::Printf(TEXT("+%d MORE"), More), U, 115, 3.6f, Chalk);
	}

	// ---- loose evidence along the bottom ----
	TArray<int32> Loose;
	for (int32 e = 0; e < Ev.Num(); e++)
	{
		const FString* Pin = C->BoardPins.Find(Ev[e]);
		bool bPinnedHere = false; for (int32 i = 0; i < N; i++) bPinnedHere |= Pin && *Pin == K.Suspects[i].Id;
		if (!bPinnedHere) Loose.Add(e);
	}
	Write(Ev.Num() == 0 ? FString(TEXT("NO EVIDENCE BAGGED YET  -  IT GOES UP HERE WHEN YOU BRING IT BACK FROM THE SCENE"))
		: FString(TEXT("LOOSE EVIDENCE   [E] TAKE A CARD   [E] ON A PHOTO TO PIN IT")), Left + Width / 2, 122, 4, FColor(230, 210, 150));
	const float LStep = Width / 8.f;
	for (int32 j = 0; j < Loose.Num(); j++)
	{
		const int32 e = Loose[j];
		if (j >= 16) { Write(FString::Printf(TEXT("+%d MORE"), Loose.Num() - 16), Left + Width - 20, 122, 3.6f, Chalk); break; }
		const float U = Left + LStep * (j % 8 + 0.5f), Z = j < 8 ? 104.f : 82.f;
		bool bHeld = false; for (const TPair<TWeakObjectPtr<APawn>, FString>& P : Holding) bHeld |= P.Value == Ev[e];
		PlaceCard(EvCards[e], U, Z, 58, 18, bHeld ? HeldPaper : Paper, bHeld ? 6.f : 1.5f);
		FHUClue Clue; C->GetClue(Ev[e], Clue);
		Write(Wrap(Clue.Name, 16, 2), U, Z - 1, 3.6f, Ink, bHeld ? 7.2f : 2.6f);
	}
}

FText AHUCaseBoard::GetPrompt_Implementation(APawn* User) const
{
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	const UHUCaseSubsystem* C = Cases();
	if (!I || !C) return FText::GetEmpty();
	const FString* Held = Holding.Find(User);
	const UPrimitiveComponent* Part = I->FocusPart;
	FHUClue HeldClue; if (Held) C->GetClue(*Held, HeldClue);
	if (Part == SceneCard) return FText::FromString(TEXT("The scene  ·  [E] Read the brief"));
	if (Part == WeaponCard) return FText::FromString(TEXT("The murder weapon  ·  [E] Read it"));
	const int32 Si = SuspectSlot(Part);
	if (C->Current.Suspects.IsValidIndex(Si))
	{
		const FHUSuspect& S = C->Current.Suspects[Si];
		if (Held) return FText::FromString(FString::Printf(TEXT("[E] Pin the %s to %s"), *HeldClue.Name, *S.Name));
		static const TCHAR* Next[] = { TEXT("mark as PRIME SUSPECT"), TEXT("mark as CLEARED"), TEXT("take the mark off") };
		return FText::FromString(FString::Printf(TEXT("%s  ·  [E] %s  ·  [Q] Their file"), *S.Name, Next[FMath::Clamp<int32>(C->BoardMarks.FindRef(S.Id), 0, 2)]));
	}
	const TArray<FString> Ev = Bagged();
	const int32 Ei = EvidenceSlot(Part);
	if (Ev.IsValidIndex(Ei))
	{
		FHUClue Clue; C->GetClue(Ev[Ei], Clue);
		if (Held && *Held == Ev[Ei]) return FText::FromString(FString::Printf(TEXT("[E] Put the %s back with the loose evidence  ·  [Q] Read it"), *Clue.Name));
		return FText::FromString(FString::Printf(TEXT("%s  ·  [E] Take the card  ·  [Q] Read it"), *Clue.Name));
	}
	if (Held) return FText::FromString(FString::Printf(TEXT("[E] Put the %s back on the board"), *HeldClue.Name));
	return FText::FromString(TEXT("The case board"));
}

void AHUCaseBoard::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	UHUCaseSubsystem* C = Cases();
	if (!I || !C || C->Current.Suspects.Num() == 0) return;
	const FHUCase& K = C->Current;
	const UPrimitiveComponent* Part = I->FocusPart;
	const FString* HeldPtr = Holding.Find(User);
	const FString Held = HeldPtr ? *HeldPtr : FString();
	FHUClue HeldClue; if (!Held.IsEmpty()) C->GetClue(Held, HeldClue);

	if (Part == SceneCard) { I->Say(FString::Printf(TEXT("THE SCENE — %s. %s"), *K.Place, *K.Brief)); return; }
	if (Part == WeaponCard) { I->Say(FString::Printf(TEXT("MURDER WEAPON — %s."), K.Weapon.IsEmpty() ? TEXT("unknown") : *K.Weapon)); return; }

	const int32 Si = SuspectSlot(Part);
	if (K.Suspects.IsValidIndex(Si))
	{
		const FHUSuspect& S = K.Suspects[Si];
		if (bSecondary)
		{
			FString Pinned;
			for (const FString& Id : Bagged()) if (C->BoardPins.FindRef(Id) == S.Id) { FHUClue Cl; C->GetClue(Id, Cl); Pinned += (Pinned.IsEmpty() ? TEXT("") : TEXT(", ")) + Cl.Name; }
			const FString* Rec = C->SuspectRecord.Find(S.Id);
			I->Say(FString::Printf(TEXT("%s — %s. RECORD: %s. ON THE BOARD: %s."), *S.Name.ToUpper(), *S.Role, Rec ? **Rec : TEXT("none on file"), Pinned.IsEmpty() ? TEXT("nothing pinned yet") : *Pinned));
			return;
		}
		if (!Held.IsEmpty())
		{
			C->BoardPins.Add(Held, S.Id);
			Holding.Remove(User);
			I->PlayReach();
			I->Say(FString::Printf(TEXT("You pin the %s under %s's photo and run red string to it."), *HeldClue.Name, *S.Name));
			return;
		}
		uint8& Mark = C->BoardMarks.FindOrAdd(S.Id);
		Mark = (Mark + 1) % 3;
		I->PlayReach();
		I->Say(Mark == 1 ? FString::Printf(TEXT("You write PRIME SUSPECT across %s's photo."), *S.Name)
			: Mark == 2 ? FString::Printf(TEXT("You mark %s CLEARED."), *S.Name)
			: FString::Printf(TEXT("You wipe the mark off %s's photo."), *S.Name));
		return;
	}

	const TArray<FString> Ev = Bagged();
	const int32 Ei = EvidenceSlot(Part);
	if (Ev.IsValidIndex(Ei))
	{
		FHUClue Clue; C->GetClue(Ev[Ei], Clue);
		if (bSecondary) { I->Say(FString::Printf(TEXT("%s — %s"), *Clue.Name.ToUpper(), *Clue.Detail)); return; }
		if (Held == Ev[Ei])
		{
			C->BoardPins.Remove(Ev[Ei]);
			Holding.Remove(User);
			I->Say(FString::Printf(TEXT("You put the %s back with the loose evidence."), *Clue.Name));
			return;
		}
		Holding.Add(User, Ev[Ei]);
		I->PlayReach();
		I->Say(FString::Printf(TEXT("You take the %s card off the board. Now [E] on a suspect's photo to pin it to them."), *Clue.Name));
		return;
	}
	// anywhere else on the cork: put the card back where it was
	if (!Held.IsEmpty()) { Holding.Remove(User); I->Say(FString::Printf(TEXT("You stick the %s back where it was."), *HeldClue.Name)); }
}

// ---------- -HUBoardTest: work the board by script and take two screenshots, then quit ----------
void AHUCaseBoard::TestStep(float DeltaSeconds)
{
	const float Was = TestClock;
	TestClock += DeltaSeconds;
	auto Passed = [&](float T) { return Was < T && TestClock >= T; };
	UHUCaseSubsystem* C = Cases();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	UHUInteractComponent* I = P ? P->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!C) return;
	if (Passed(0.5f))
	{
		// deal a case and bag evidence without paying out (the save stays as it was)
		C->StartCase(-1);
		for (int32 e = 0; e < FMath::Min(9, C->Current.Clues.Num()); e++) C->Found.AddUnique(C->Current.Clues[e].Id);
		C->bAwaitingCaptain = true;
		UE_LOG(LogTemp, Log, TEXT("HUBoardTest: case %d, %d suspects, %d bagged"), C->Current.Number, C->Current.Suspects.Num(), C->Found.Num());
	}
	auto Use = [&](UPrimitiveComponent* Part, bool bQ)
	{
		if (!I || !P) return;
		I->Focus = this; I->FocusPart = Part;
		IHUInteractable::Execute_Interact(this, P, bQ);
		UE_LOG(LogTemp, Log, TEXT("HUBoardTest: used %s -> %s"), *Part->GetName(), *IHUInteractable::Execute_GetPrompt(this, P).ToString());
	};
	if (Passed(1.5f) && I)
	{
		Use(EvCards[0], false); Use(SusCards[0], false);   // two cards on suspect 1
		Use(EvCards[1], false); Use(SusCards[0], false);
		Use(EvCards[2], false); Use(SusCards[1], false);   // one on suspect 2
		Use(SusCards[0], false);                           // suspect 1: prime
		Use(SusCards[2], false); Use(SusCards[2], false);  // suspect 3: cleared
		Use(EvCards[4], false);                            // a card in hand
		UE_LOG(LogTemp, Log, TEXT("HUBoardTest: pins %d, marks %d, holding %d"), C->BoardPins.Num(), C->BoardMarks.Num(), Holding.Num());
	}
	auto Stand = [&](float Back, float Side, float Pitch)
	{
		if (!P || !PC) return;
		const FVector Out = GetActorForwardVector(), Right = -GetActorRightVector();
		P->SetActorLocation(GetActorLocation() + Out * Back + Right * Side + FVector(0, 0, 95));
		PC->SetControlRotation(FRotator(Pitch, (-Out).Rotation().Yaw, 0));
	};
	if (Passed(2.5f)) Stand(330, 0, 0);
	if (Passed(3.5f)) FScreenshotRequest::RequestScreenshot(TEXT("CaseBoard_Wide"), false, false);
	if (Passed(4.5f)) Stand(160, 60, 18);
	if (Passed(5.5f)) FScreenshotRequest::RequestScreenshot(TEXT("CaseBoard_Close"), false, false);
	if (Passed(6.5f)) Stand(160, -190, 12);
	if (Passed(7.5f)) FScreenshotRequest::RequestScreenshot(TEXT("CaseBoard_Left"), false, false);
	if (Passed(9.f)) FPlatformMisc::RequestExit(false);
}
