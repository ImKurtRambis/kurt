#include "HUCaseDesk.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "HUProfiles.h"
#include "HUPlayerState.h"
#include "HUPhoneData.h"
#include "HUSecurityDesk.h"
#include "GameFramework/GameStateBase.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/SlateApplication.h"

using namespace HUBuild;

static const FLinearColor FolderColors[] = {
	FLinearColor(0.78f, 0.66f, 0.42f), // manila
	FLinearColor(0.42f, 0.55f, 0.72f), // blue
	FLinearColor(0.72f, 0.36f, 0.3f),  // red
	FLinearColor(0.48f, 0.62f, 0.45f), // green
	FLinearColor(0.55f, 0.55f, 0.58f), // grey
};

// ---- PRECINCTOS: the desktop shortcuts. Add, remove or reorder them here. ----
// Tool = which case tool the window shows: 0 records, 1 alibi check, 2 offender profile, 3 tape flags, 4 case status.
struct FDeskApp { const TCHAR* Name; const TCHAR* Glyph; FLinearColor Color; int32 Tool; };
static const FDeskApp DeskApps[] = {
	{ TEXT("Records Search"),   TEXT("R"), FLinearColor(0.93f, 0.68f, 0.18f), 0 },
	{ TEXT("Alibi Check"),      TEXT("A"), FLinearColor(0.3f, 0.66f, 0.3f),   1 },
	{ TEXT("Offender Profile"), TEXT("P"), FLinearColor(0.62f, 0.38f, 0.82f), 2 },
	{ TEXT("Tape Flags"),       TEXT("T"), FLinearColor(0.25f, 0.5f, 0.88f),  3 },
	{ TEXT("Case Status"),      TEXT("C"), FLinearColor(0.84f, 0.32f, 0.26f), 4 },
	{ TEXT("Suspect Database"), TEXT("D"), FLinearColor(0.2f, 0.6f, 0.62f),   5 },
	{ TEXT("Call Log"),         TEXT("L"), FLinearColor(0.35f, 0.75f, 0.4f),  6 },
	{ TEXT("Text Messages"),    TEXT("M"), FLinearColor(0.3f, 0.55f, 0.95f),  7 },
	{ TEXT("Web History"),      TEXT("W"), FLinearColor(0.95f, 0.5f, 0.15f),  8 },
	{ TEXT("Phone Photos"),     TEXT("I"), FLinearColor(0.85f, 0.35f, 0.6f),  9 },
	{ TEXT("Cell Pings"),       TEXT("G"), FLinearColor(0.55f, 0.55f, 0.6f),  10 },
	{ TEXT("Security Footage"), TEXT("V"), FLinearColor(0.15f, 0.15f, 0.18f), 11 },
};
static constexpr int32 FootageTool = 11;
const TCHAR* AHUCaseDesk::OSName = TEXT("PrecinctOS");
int32 AHUCaseDesk::AppCount() { return UE_ARRAY_COUNT(DeskApps); }
FString AHUCaseDesk::AppName(int32 A) { return A >= 0 && A < AppCount() ? FString(DeskApps[A].Name) : FString(); }
FString AHUCaseDesk::AppGlyph(int32 A) { return A >= 0 && A < AppCount() ? FString(DeskApps[A].Glyph) : FString(); }
FLinearColor AHUCaseDesk::AppColor(int32 A) { return A >= 0 && A < AppCount() ? DeskApps[A].Color : FLinearColor::Gray; }

const TCHAR* AHUCaseDesk::FolderName(int32 K)
{
	switch (K) { case 0: return TEXT("SCENE REPORT"); case 1: return TEXT("EVIDENCE LOG"); case 2: return TEXT("SUSPECT FILES"); case 3: return TEXT("AUTOPSY"); case 4: return TEXT("TAPE NOTES"); default: return TEXT("CASE FILE"); }
}

AHUCaseDesk::AHUCaseDesk()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FolderF(TEXT("/Game/Art/Materials/Props/M_HU_PropWorldAlignedSurface.M_HU_PropWorldAlignedSurface"));
	FolderSurface = FolderF.Object;
}

UHUCaseSubsystem* AHUCaseDesk::Cases() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
}

// the folders lie fanned out on the near half of the desk (the desk's centre is this actor; the computer is at +X)
void AHUCaseDesk::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks);
	for (UTextRenderComponent* T : FolderLabels) if (T) T->DestroyComponent();
	FolderParts.Reset(); FolderLabels.Reset();
	for (int32 K = 0; K < FolderCount; K++)
	{
		const FVector At(-42 + (K % 2) * 6, -44 + K * 22, 78.f + K * 0.6f);
		const FRotator Turn(0, -8 + K * 4, 0);
		UStaticMeshComponent* F = Block(this, Root, Blocks, EShape::Cube, At, FVector(30, 21, 1.4f), FolderColors[K], Turn);
		if (F) F->SetMaterial(0, HUBuild::PropSurface(this, FolderSurface,
			TEXT("/Game/Art/Materials/Textures/M_ManilaFolder_BaseColor.M_ManilaFolder_BaseColor"), FolderColors[K] * 1.35f, 24.f, 1.f));
		FolderParts.Add(F);
		// The file is in active use: a ruled report sheet and a small index tab peek from its back edge.
		const FVector PaperAt = At + FVector(0, -8.3f, 0.92f);
		UStaticMeshComponent* Paper = Block(this, Root, Blocks, EShape::Cube, PaperAt, FVector(25, 7.2f, 0.22f),
			(K % 2 == 0) ? FLinearColor(0.78f, 0.76f, 0.67f) : FLinearColor(0.84f, 0.82f, 0.73f), Turn);
		if (Paper) Paper->SetMaterial(0, HUBuild::PropSurface(this, FolderSurface,
			TEXT("/Game/Art/Materials/Textures/M_ManilaFolder_BaseColor.M_ManilaFolder_BaseColor"), FLinearColor(1.22f, 1.2f, 1.08f), 30.f, 1.05f));
		for (int32 Line = 0; Line < 4; ++Line)
		{
			const float Length = 10.f + ((K + Line) % 3) * 3.5f;
			UStaticMeshComponent* Rule = Block(this, Root, Blocks, EShape::Cube,
				PaperAt + FVector(-2.f, 1.f + Line * 1.1f, 0.16f), FVector(0.18f, Length, 0.035f), FLinearColor(0.22f, 0.3f, 0.34f), Turn, false);
			(void)Rule;
		}
		Block(this, Root, Blocks, EShape::Cube, PaperAt + FVector(5.4f, -0.4f, 0.17f), FVector(0.35f, 1.6f, 0.08f),
			(K % 2 == 0) ? FLinearColor(0.47f, 0.16f, 0.12f) : FLinearColor(0.18f, 0.3f, 0.34f), Turn, false); // file stamp
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
		T->SetupAttachment(Root);
		T->SetRelativeLocation(At + FVector(0, 0, 1.6f));
		T->SetRelativeRotation(FRotator(90, 0, 0) + FRotator(0, Turn.Yaw + 90, 0));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(2.6f);
		T->SetTextRenderColor(FColor(30, 26, 20));
		T->SetText(FText::FromString(FolderName(K)));
		T->RegisterComponent();
		FolderLabels.Add(T);
	}
	// the name plate on the front edge, facing the chair
	Block(this, Root, Blocks, EShape::Cube, FVector(-72, 0, 81), FVector(4, 46, 8), FLinearColor(0.12f, 0.1f, 0.08f), FRotator(0, 0, 0), false);
	if (NamePlate) NamePlate->DestroyComponent();
	NamePlate = NewObject<UTextRenderComponent>(this);
	NamePlate->SetupAttachment(Root);
	NamePlate->SetRelativeLocation(FVector(-74.5f, 0, 81));
	NamePlate->SetRelativeRotation(FRotator(0, 180, 0));
	NamePlate->SetHorizontalAlignment(EHTA_Center);
	NamePlate->SetVerticalAlignment(EVRTA_TextCenter);
	NamePlate->SetWorldSize(4.5f);
	NamePlate->SetTextRenderColor(FColor(235, 200, 110));
	NamePlate->RegisterComponent();
}

AHUPlayerState* AHUCaseDesk::OwnerState() const
{
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
		for (APlayerState* P : GS->PlayerArray) if (AHUPlayerState* HP = Cast<AHUPlayerState>(P)) if (HP->Desk == DeskIndex) return HP;
	return nullptr;
}

bool AHUCaseDesk::CanUse(const APawn* User) const
{
	const AHUPlayerState* O = OwnerState();
	return !O || (User && User->GetPlayerState() == O);
}

FString AHUCaseDesk::OwnerName() const
{
	const AHUPlayerState* O = OwnerState();
	return O ? O->GetPlayerName().Replace(TEXT("_"), TEXT(" ")) : FString();
}

// the host hands out desks: anyone without one gets the lowest free desk (kept while they're here)
void AHUCaseDesk::AssignDesks()
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS) return;
	int32 DeskCount = 0;
	for (TActorIterator<AHUCaseDesk> It(GetWorld()); It; ++It) DeskCount++;
	TSet<int32> Taken;
	for (APlayerState* P : GS->PlayerArray) if (const AHUPlayerState* HP = Cast<AHUPlayerState>(P)) if (HP->Desk >= 0) Taken.Add(HP->Desk);
	for (APlayerState* P : GS->PlayerArray)
		if (AHUPlayerState* HP = Cast<AHUPlayerState>(P); HP && HP->Desk < 0)
			for (int32 D = 0; D < DeskCount; D++) if (!Taken.Contains(D)) { HP->Desk = D; Taken.Add(D); break; }
}

int32 AHUCaseDesk::FolderOf(const UPrimitiveComponent* Part) const
{
	for (int32 K = 0; K < FolderParts.Num(); K++) if (FolderParts[K] && (FolderParts[K] == Part || (Part && Part->GetAttachParent() == FolderParts[K]))) return K;
	return -1;
}

void AHUCaseDesk::HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const
{
	const int32 K = FolderOf(Part);
	if (K >= 0) Out.Add(FolderParts[K]);
}

void AHUCaseDesk::SetOnDesk(int32 K, bool bOn)
{
	if (!FolderParts.IsValidIndex(K)) return;
	FolderParts[K]->SetVisibility(bOn, true);
	FolderParts[K]->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (FolderLabels.IsValidIndex(K)) FolderLabels[K]->SetVisibility(bOn);
}

static FString WhoIs(const APawn* P)
{
	return P && P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName().Replace(TEXT("_"), TEXT(" ")) : FString(TEXT("A partner"));
}

FText AHUCaseDesk::GetPrompt_Implementation(APawn* User) const
{
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	const int32 K = I ? FolderOf(I->FocusPart) : -1;
	if (K < 0) return FText::GetEmpty();
	if (Holder[K].IsValid()) return FText::FromString(FString::Printf(TEXT("%s has the %s"), *WhoIs(Holder[K].Get()), FolderName(K)));
	if (!CanUse(User)) return FText::FromString(FString::Printf(TEXT("%s  —  Det. %s's desk"), FolderName(K), *OwnerName()));
	const UHUCaseSubsystem* C = Cases();
	if (!C || !C->bCaseActive) return FText::FromString(FString::Printf(TEXT("%s  —  empty, no case open"), FolderName(K)));
	return FText::FromString(FString::Printf(TEXT("[E] Pick up the %s"), FolderName(K)));
}

// runs on every machine (the squad replays each use): the folder leaves the desk for everyone
void AHUCaseDesk::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	const int32 K = I ? FolderOf(I->FocusPart) : -1;
	if (K < 0) return;
	if (bSecondary)
	{
		// putting it back (sent when the reader closes)
		if (Holder[K].Get() == User) { Holder[K] = nullptr; SetOnDesk(K, true); }
		return;
	}
	if (!CanUse(User))
	{
		if (User->IsLocallyControlled()) I->Say(FString::Printf(TEXT("That's Det. %s's desk. Yours has your name on it."), *OwnerName()));
		return;
	}
	const UHUCaseSubsystem* C = Cases();
	if (Holder[K].IsValid() || !C || !C->bCaseActive)
	{
		if (User->IsLocallyControlled() && !Holder[K].IsValid()) I->Say(TEXT("Nothing in it. There's no case open."));
		return;
	}
	Holder[K] = User;
	SetOnDesk(K, false);
	I->PlayReach();
	if (User->IsLocallyControlled()) Open(User, false, K);
}

void AHUCaseDesk::OpenTerminal(APawn* User)
{
	if (User && User->IsLocallyControlled()) Open(User, true, -1);
}

void AHUCaseDesk::Open(APawn* User, bool bTerm, int32 K)
{
	APlayerController* PC = User ? Cast<APlayerController>(User->GetController()) : nullptr;
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!PC || !I || bReading) return;
	Viewer = User;
	bReading = true; bTerminal = bTerm; Folder = K; Page = 0; Scroll = 0;
	bPeek = false; Active = -1; OpenWins.Reset(); Minimised.Reset(); Sel = FMath::Clamp(Sel, 0, AppCount() - 1);
	Rebuild();
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	I->Reading = this;
	I->Say(bTerm ? FString::Printf(TEXT("%s boots up with a chime. Your desktop."), OSName) : FString::Printf(TEXT("You flip open the %s."), *FString(FolderName(K)).ToLower()));
	EnableInput(PC);
	if (InputComponent && InputComponent->KeyBindings.Num() == 0)
	{
		InputComponent->BindKey(EKeys::D, IE_Pressed, this, &AHUCaseDesk::KeyRight);
		InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AHUCaseDesk::KeyRight);
		InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AHUCaseDesk::KeyLeft);
		InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AHUCaseDesk::KeyLeft);
		InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AHUCaseDesk::KeyDown);
		InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AHUCaseDesk::KeyDown);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AHUCaseDesk::KeyDown);
		InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AHUCaseDesk::KeyUp);
		InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AHUCaseDesk::KeyUp);
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AHUCaseDesk::KeyUp);
		InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AHUCaseDesk::KeyQ);
		InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AHUCaseDesk::KeyE);
		InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AHUCaseDesk::KeyE);
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AHUCaseDesk::KeyM);
		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AHUCaseDesk::KeyTab);
			InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AHUCaseDesk::KeyF);
			InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AHUCaseDesk::MouseClick);
	}
		// the computer takes the mouse: a free cursor over the screen (folders keep the keys)
		if (bTerm)
		{
			FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = true;
		}
	if (bTerm && CanUse(User)) Broadcast(); // (someone looking over a shoulder never speaks for the screen)
}

void AHUCaseDesk::OpenPeek(APawn* User)
{
	if (!User || !User->IsLocallyControlled() || !bOwnerOn) return;
	Open(User, true, -1);
	if (!bReading) return;
	bPeek = true;
	Active = OwnerApp;
	if (Active >= 0) OpenWins.Add(Active);
	Rebuild();
	if (UHUInteractComponent* I = User->FindComponentByClass<UHUInteractComponent>()) I->Say(FString::Printf(TEXT("You lean over Det. %s's shoulder."), *OwnerName()));
}

// ---- PrecinctOS keys (the folders keep their own: A / D pages, W / S scroll, E / Q put it down) ----
void AHUCaseDesk::KeyLeft()
{
	if (!bReading) return;
	if (!bTerminal) { PrevPage(); return; }
	if (bPeek) return;
	if (IsOnDesktop()) { Sel = (Sel + AppCount() - 1) % AppCount(); return; }
	const int32 i = OpenWins.Find(Active);
	if (OpenWins.Num() > 1) { Active = OpenWins[(i + OpenWins.Num() - 1) % OpenWins.Num()]; Minimised.Remove(Active); Scroll = 0; Rebuild(); Broadcast(); }
}
void AHUCaseDesk::KeyRight()
{
	if (!bReading) return;
	if (!bTerminal) { NextPage(); return; }
	if (bPeek) return;
	if (IsOnDesktop()) { Sel = (Sel + 1) % AppCount(); return; }
	KeyTab();
}
void AHUCaseDesk::KeyUp()
{
	if (!bReading) return;
	if (bTerminal && !bPeek && IsOnDesktop()) { Sel = (Sel + AppCount() - 1) % AppCount(); return; }
	ScrollUp();
}
void AHUCaseDesk::KeyDown()
{
	if (!bReading) return;
	if (bTerminal && !bPeek && IsOnDesktop()) { Sel = (Sel + 1) % AppCount(); return; }
	ScrollDown();
}
void AHUCaseDesk::KeyE()
{
	if (!bReading) return;
	if (!bTerminal) { Close(); return; }
	if (!bPeek && IsOnDesktop()) OpenApp(Sel);
}
void AHUCaseDesk::KeyQ()
{
	if (!bReading) return;
	if (!bTerminal || bPeek || IsOnDesktop()) { Close(); return; }
	CloseWindow();
}
void AHUCaseDesk::KeyM()
{
	if (!bReading || !bTerminal || bPeek || IsOnDesktop()) return;
	Minimised.Add(Active);
	Active = -1;
	Broadcast();
}
void AHUCaseDesk::KeyTab()
{
	if (!bReading || !bTerminal || bPeek || OpenWins.Num() == 0) return;
	const int32 i = OpenWins.Find(Active);
	Active = OpenWins[(i + 1) % OpenWins.Num()];
	Minimised.Remove(Active);
	Scroll = 0;
	Rebuild();
	Broadcast();
}
// F: send the window in front to the squad
void AHUCaseDesk::KeyF()
{
	if (!bReading || !bTerminal || bPeek || IsOnDesktop()) return;
	if (UHUInteractComponent* I = Viewer.IsValid() ? Viewer->FindComponentByClass<UHUInteractComponent>() : nullptr) I->SendDeskShare(DeskIndex, Active);
}

// a click: whatever the screen drew under the cursor (icons: click picks, double-click opens)
void AHUCaseDesk::MouseClick()
{
	if (!bReading || !bTerminal || !FSlateApplication::IsInitialized()) return;
	const FVector2D At = FSlateApplication::Get().GetCursorPos();
	const FHit* Hit = nullptr;
	for (int32 i = Hits.Num() - 1; i >= 0; i--) // topmost drawn last
		if (At.X >= Hits[i].Min.X && At.Y >= Hits[i].Min.Y && At.X <= Hits[i].Max.X && At.Y <= Hits[i].Max.Y) { Hit = &Hits[i]; break; }
	if (!Hit) return;
	const FHit H = *Hit;
	if (bPeek) { if (H.Kind == EHit::ScrollUp) ScrollUp(); else if (H.Kind == EHit::ScrollDown) ScrollDown(); else if (H.Kind == EHit::Close || H.Kind == EHit::Start) Close(); return; }
	const double Now = FPlatformTime::Seconds();
	switch (H.Kind)
	{
	case EHit::Icon:
		if (H.Arg == LastClickIcon && Now - LastClickT < 0.45) { OpenApp(H.Arg); LastClickIcon = -1; }
		else { Sel = H.Arg; LastClickIcon = H.Arg; LastClickT = Now; }
		return;
	case EHit::Close: CloseWindow(); return;
	case EHit::Minimise: KeyM(); return;
	case EHit::Task:
		if (H.Arg == Active && !IsOnDesktop()) KeyM();
		else { Active = H.Arg; Minimised.Remove(Active); Scroll = 0; Rebuild(); Broadcast(); }
		return;
	case EHit::Start: Close(); return;
	case EHit::ScrollUp: ScrollUp(); return;
	case EHit::ScrollDown: ScrollDown(); return;
	}
}

void AHUCaseDesk::OpenApp(int32 A)
{
	if (A < 0 || A >= AppCount()) return;
	// the tapes play in their own viewer: log off and slot them in
	if (DeskApps[A].Tool == FootageTool)
	{
		const UHUCaseSubsystem* C = Cases();
		APawn* User = Viewer.Get();
		if (!C || !C->bTapesPulled || !User)
		{
			OpenWins.AddUnique(A); Minimised.Remove(A); Active = A; Scroll = 0; Rebuild(); Broadcast();
			return;
		}
		Close();
		for (TActorIterator<AHUSecurityDesk> It(GetWorld()); It; ++It) if (It->bAtPrecinct) { It->OpenFor(User); break; }
		return;
	}
	OpenWins.AddUnique(A);
	Minimised.Remove(A);
	SharedFrom.Remove(A);
	Active = A; Scroll = 0;
	Rebuild();
	Broadcast();
}

void AHUCaseDesk::CloseWindow()
{
	OpenWins.Remove(Active);
	Minimised.Remove(Active);
	Active = -1;
	for (int32 i = OpenWins.Num() - 1; i >= 0; i--) if (!Minimised.Contains(OpenWins[i])) { Active = OpenWins[i]; break; }
	Scroll = 0;
	Rebuild();
	Broadcast();
}

// tell the squad what this screen shows (only when it changes)
void AHUCaseDesk::Broadcast()
{
	if (bPeek) return;
	const bool bOn = bReading && bTerminal;
	const int32 App = bOn && !IsOnDesktop() ? Active : -1;
	if (bOn == bSentOn && App == SentApp) return;
	if (UHUInteractComponent* I = Viewer.IsValid() ? Viewer->FindComponentByClass<UHUInteractComponent>() : nullptr)
	{
		bSentOn = bOn; SentApp = App;
		I->SendDeskView(DeskIndex, bOn, App);
	}
}

void AHUCaseDesk::Close()
{
	APawn* User = Viewer.Get();
	const int32 K = Folder;
	const bool bWasFolder = !bTerminal;
	if (bTerminal && !bPeek) { bReading = false; Broadcast(); } // the screen goes dark for anyone looking on
	bPeek = false;
	bReading = false; Folder = -1; Viewer = nullptr;
	if (APlayerController* PC = User ? Cast<APlayerController>(User->GetController()) : nullptr)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		DisableInput(PC);
		if (PC->bShowMouseCursor) { PC->bShowMouseCursor = false; PC->SetInputMode(FInputModeGameOnly()); }
	}
	Hits.Reset();
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (I) I->Reading = nullptr;
	// put the folder back on the desk, for the whole squad
	if (bWasFolder && I && FolderParts.IsValidIndex(K)) { I->SendUseOn(this, FolderParts[K], true); I->Say(TEXT("You drop the folder back on the desk.")); }
}

void AHUCaseDesk::CloseKey() { if (bReading) Close(); }
void AHUCaseDesk::NextPage() { if (!bReading) return; Rebuild(); Page = FMath::Min(Page + 1, Pages.Num() - 1); Scroll = 0; }
void AHUCaseDesk::PrevPage() { if (!bReading) return; Rebuild(); Page = FMath::Max(Page - 1, 0); Scroll = 0; }
void AHUCaseDesk::ScrollDown() { if (bReading && Pages.IsValidIndex(Page)) Scroll = FMath::Min(Scroll + 3, Pages[Page].Lines.Num() * 3); } // (lines wrap into rows: the screen clamps)
void AHUCaseDesk::ScrollUp() { if (bReading) Scroll = FMath::Max(Scroll - 3, 0); }

void AHUCaseDesk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DeskIndex == 0 && HasAuthority()) AssignDesks(); // the host's job (desk 0 does it for all of them)
	if (NamePlate)
	{
		const FString Name = OwnerName();
		const FString Want = Name.IsEmpty() ? FString(TEXT("OPEN DESK")) : TEXT("DET. ") + Name.ToUpper();
		if (NamePlate->Text.ToString() != Want) NamePlate->SetText(FText::FromString(Want));
	}
	// a squadmate who left (or fell out of the game) drops what they were holding back on the desk
	for (int32 K = 0; K < FolderCount; K++)
		if (!Holder[K].IsValid() && FolderParts.IsValidIndex(K) && !FolderParts[K]->IsVisible() && !(bReading && Folder == K)) SetOnDesk(K, true);
	if (bReading && !Viewer.IsValid()) Close();
	// looking over someone's shoulder: follow their screen, and step back when they log off
	if (bReading && bPeek)
	{
		if (!bOwnerOn) { if (UHUInteractComponent* I = Viewer->FindComponentByClass<UHUInteractComponent>()) I->Say(TEXT("They log off. You step back.")); Close(); }
		else if (Active != OwnerApp) { Active = OwnerApp; OpenWins.Reset(); if (Active >= 0) OpenWins.Add(Active); Scroll = 0; Rebuild(); }
		else Rebuild();
	}
}

// ---------------- what's written in the file ----------------
void AHUCaseDesk::Rebuild()
{
	Pages.Reset();
	const UHUCaseSubsystem* C = Cases();
	if (!C || !C->bCaseActive)
	{
		Pages.Add({ TEXT("NO OPEN CASE"), { TEXT("Nothing's been assigned. Roll call deals the next case.") } });
		return;
	}
	const FHUCase& Case = C->Current;
	auto SuspectName = [&](const FString& Id) { FHUSuspect S; return C->GetSuspect(Id, S) ? S.Name : Id; };
	auto ClueName = [&](const FString& Id) { FHUClue Cl; return C->GetClue(Id, Cl) ? Cl.Name : Id; };
	auto ProfileOf = [&](const FString& Id) { const EHUProfile* P = C->SuspectProfile.Find(Id); return P && *P != EHUProfile::None ? FString(HUProfiles::Name(*P)) : FString(TEXT("nothing on file")); };
	auto TapeLines = [&]()
	{
		TArray<FString> L;
		if (!C->bTapesPulled) { L.Add(TEXT("No tapes pulled. The building's recorder is at the scene - hold E on it to pull them.")); return L; }
		L.Add(FString::Printf(TEXT("Tapes pulled: %d cameras, %d moments. Watch them on any desk computer (E)."), C->TapeCams(), C->TapeMoments()));
		if (C->bTapeCaught) L.Add(TEXT("CAUGHT ON TAPE: one clip puts the culprit where they swore they weren't. That lie is on the record."));
		L.Add(TEXT(""));
		int32 N = 0;
		if (const APawn* V = Viewer.Get())
			if (const UHUInteractComponent* I = V->FindComponentByClass<UHUInteractComponent>())
				for (const FString& P : I->Pages()) if (P.StartsWith(TEXT("ON THE TAPE"))) { L.Add(TEXT("- ") + P.Mid(14)); N++; }
		if (N == 0) L.Add(TEXT("Nothing flagged yet. Press E on a moment while watching to write it down here."));
		return L;
	};

	if (bTerminal)
	{
		// RECORDS
		{
			FPage P{ TEXT("RECORDS SEARCH"), {} };
			for (const FHUSuspect& S : Case.Suspects)
			{
				P.Lines.Add(FString::Printf(TEXT("> QUERY %s"), *S.Name.ToUpper()));
				P.Lines.Add(FString::Printf(TEXT("  %s"), *S.Role));
				const FString Rec = C->SuspectRecord.FindRef(S.Id);
				P.Lines.Add(TEXT("  RECORD: ") + (Rec.IsEmpty() ? FString(TEXT("clean - no priors on file")) : Rec));
				P.Lines.Add(TEXT("  FITS: ") + ProfileOf(S.Id));
				P.Lines.Add(TEXT(""));
			}
			Pages.Add(P);
		}
		// ALIBI CHECK: every statement against the bagged evidence
		{
			FPage P{ TEXT("ALIBI CROSS-CHECK"), { TEXT("Every statement, run against the evidence you've bagged.") , TEXT("") } };
			for (const FHUSuspect& S : Case.Suspects)
			{
				P.Lines.Add(FString::Printf(TEXT("> %s"), *S.Name.ToUpper()));
				for (int32 i = 0; i < S.Statements.Num(); i++)
				{
					const FHUStatement& St = S.Statements[i];
					P.Lines.Add(FString::Printf(TEXT("  \"%s\""), *St.Text));
					if (C->IsBroken(S.Id, i)) { P.Lines.Add(TEXT("     [BROKEN] on the record as a lie")); continue; }
					FString Hit;
					if (!St.Truth) for (const FString& E : St.Evidence) if (C->HasFound(E)) { Hit = ClueName(E); break; }
					P.Lines.Add(Hit.IsEmpty() ? FString(TEXT("     [ -- ] nothing bagged contradicts this")) : FString::Printf(TEXT("     [CONFLICT] %s - put it to them in the interview room"), *Hit));
				}
				P.Lines.Add(TEXT(""));
			}
			Pages.Add(P);
		}
		// PROFILE
		{
			FPage P{ TEXT("OFFENDER PROFILE"), {} };
			const FString Leads = C->DescribeProfileLeads();
			P.Lines.Add(Leads.IsEmpty() ? FString(TEXT("Not enough bagged to profile the offender yet.")) : Leads);
			P.Lines.Add(TEXT(""));
			P.Lines.Add(TEXT("Suspects whose records fit:"));
			for (const FHUSuspect& S : Case.Suspects) P.Lines.Add(FString::Printf(TEXT("  %s - %s"), *S.Name, *ProfileOf(S.Id)));
			P.Lines.Add(TEXT(""));
			P.Lines.Add(TEXT("Careful: some evidence is planted to point the wrong way."));
			Pages.Add(P);
		}
		Pages.Add({ TEXT("TAPE FLAGS"), TapeLines() });
		// CASE STATUS
		{
			FPage P{ TEXT("CASE STATUS"), {} };
			int32 Bagged = 0, Placed = 0, KeyB = 0, KeyP = 0;
			C->GetEvidenceCount(Bagged, Placed, KeyB, KeyP);
			P.Lines.Add(FString::Printf(TEXT("CASE #%d  %s"), Case.Number, *Case.Title));
			P.Lines.Add(FString::Printf(TEXT("Evidence bagged: %d of %d at the scene"), Bagged, Placed));
			P.Lines.Add(FString::Printf(TEXT("Lies on the record: %d   (needed to charge: %d)"), C->Breaks, Case.Solution.RequiredBreaks));
			P.Lines.Add(C->CanAccuse() ? FString(TEXT("ENOUGH TO CHARGE. Make the arrest.")) : FString(TEXT("Not enough to charge yet. Break more of their stories.")));
			P.Lines.Add(TEXT(""));
			P.Lines.Add(FString::Printf(TEXT("In holding: %d   still to book: %d"), C->Arrested.Num(), C->UnbookedCount()));
			for (const FString& Id : C->Arrested) P.Lines.Add(FString::Printf(TEXT("  %s - %s"), *SuspectName(Id), C->IsBooked(Id) ? TEXT("booked") : TEXT("awaiting booking")));
			if (C->bAwaitingCaptain) P.Lines.Add(C->UnbookedCount() > 0 ? FString(TEXT("Book everyone, then submit at the Captain's in-tray.")) : FString(TEXT("Ready for the Captain's in-tray.")));
			Pages.Add(P);
		}
		// the suspect database and, with a warrant, each suspect's phone (one section per suspect)
		{
			const TCHAR* Heads[] = { TEXT("SUSPECT DATABASE"), TEXT("CALL LOG"), TEXT("TEXT MESSAGES"), TEXT("WEB HISTORY"), TEXT("PHONE PHOTOS"), TEXT("CELL TOWER PINGS") };
			for (int32 App = 0; App < 6; App++)
			{
				FPage P{ Heads[App], {} };
				if (App == 0) P.Lines.Add(TEXT("DMV, records and phone company, cross-referenced."));
				else P.Lines.Add(FString::Printf(TEXT("From the phone company and the handset. Crime at %s - read around it."), *Case.Time));
				P.Lines.Add(TEXT(""));
				for (const FHUSuspect& S : Case.Suspects)
				{
					P.Lines.Add(FString::Printf(TEXT("==== %s  (%s) ===="), *S.Name.ToUpper(), *S.Role));
					for (const FString& Ln : HUPhone::Lines(C, S, (HUPhone::EApp)App)) P.Lines.Add(TEXT("  ") + Ln);
					P.Lines.Add(TEXT(""));
				}
				Pages.Add(P);
			}
			FPage F{ TEXT("SECURITY FOOTAGE"), {} };
			F.Lines.Add(C->bTapesPulled ? FString(TEXT("Tapes loaded. Open this icon again to play them.")) : FString(TEXT("NO TAPE IN THE DECK. Pull the tapes from the building's recorder at the scene (hold E on it).")));
			Pages.Add(F);
		}
		// the window in front shows its tool's page (Pages holds every tool, in Tool order, until here)
		{
			const int32 Tool = Active >= 0 && Active < AppCount() ? DeskApps[Active].Tool : -1;
			TArray<FPage> All = MoveTemp(Pages);
			Pages.Reset();
			if (All.IsValidIndex(Tool)) Pages.Add(All[Tool]);
		}
		return;
	}

	switch (Folder)
	{
	case 0: // SCENE REPORT
	{
		FPage P{ FString::Printf(TEXT("CASE #%d - %s"), Case.Number, *Case.Title), {} };
		P.Lines.Add(TEXT("CRIME: ") + Case.Crime);
		P.Lines.Add(TEXT("LOCATION: ") + Case.Place);
		P.Lines.Add(TEXT("TIME: ") + Case.Time);
		P.Lines.Add(TEXT("VICTIM: ") + Case.Victim);
		P.Lines.Add(TEXT(""));
		P.Lines.Add(Case.Brief);
		Pages.Add(P);
		if (Case.VictimRows.Num() > 0) { FPage V{ Case.VictimHeader.IsEmpty() ? FString(TEXT("THE VICTIM")) : Case.VictimHeader, Case.VictimRows }; Pages.Add(V); }
		break;
	}
	case 1: // EVIDENCE LOG
	{
		FPage P{ TEXT("EVIDENCE LOG"), {} };
		int32 Bagged = 0, Placed = 0, KeyB = 0, KeyP = 0;
		C->GetEvidenceCount(Bagged, Placed, KeyB, KeyP);
		P.Lines.Add(FString::Printf(TEXT("%d of %d items bagged at the scene."), Bagged, Placed));
		P.Lines.Add(TEXT(""));
		for (const FString& Id : C->Found)
		{
			FHUClue Cl;
			if (!C->GetClue(Id, Cl)) continue;
			P.Lines.Add(FString::Printf(TEXT("#%s  %s"), *Cl.Id.ToUpper(), *Cl.Name.ToUpper()));
			P.Lines.Add(TEXT("   ") + Cl.Detail);
			if (const FString* Sign = C->ClueSign.Find(Id)) if (!Sign->IsEmpty()) P.Lines.Add(TEXT("   Lab: ") + *Sign);
			if (const FString* Pin = C->BoardPins.Find(Id)) P.Lines.Add(TEXT("   Pinned on the board under ") + SuspectName(*Pin));
			P.Lines.Add(TEXT(""));
		}
		if (C->Found.Num() == 0) P.Lines.Add(TEXT("Nothing logged. Bag evidence at the scene."));
		Pages.Add(P);
		break;
	}
	case 2: // SUSPECT FILES: a page each
		for (const FHUSuspect& S : Case.Suspects)
		{
			FPage P{ S.Name.ToUpper(), {} };
			P.Lines.Add(S.Role);
			const uint8 Mark = C->BoardMarks.FindRef(S.Id);
			if (Mark == 1) P.Lines.Add(TEXT("MARKED ON THE BOARD: PRIME SUSPECT")); else if (Mark == 2) P.Lines.Add(TEXT("MARKED ON THE BOARD: CLEARED"));
			if (C->Arrested.Contains(S.Id)) P.Lines.Add(TEXT("IN CUSTODY"));
			P.Lines.Add(TEXT(""));
			P.Lines.Add(TEXT("\"") + S.Intro + TEXT("\""));
			const FString Rec = C->SuspectRecord.FindRef(S.Id);
			P.Lines.Add(TEXT(""));
			P.Lines.Add(TEXT("PRIORS: ") + (Rec.IsEmpty() ? FString(TEXT("none")) : Rec));
			P.Lines.Add(FString::Printf(TEXT("Interview pressure: %d%%%s"), FMath::RoundToInt(C->GetPressure(S.Id)), C->Lawyered.Contains(S.Id) ? TEXT("  (lawyered up)") : TEXT("")));
			P.Lines.Add(TEXT(""));
			P.Lines.Add(TEXT("STATEMENTS:"));
			for (int32 i = 0; i < S.Statements.Num(); i++)
				P.Lines.Add(FString::Printf(TEXT("%d. \"%s\"%s"), i + 1, *S.Statements[i].Text, C->IsBroken(S.Id, i) ? TEXT("   - LIE, on the record") : TEXT("")));
			Pages.Add(P);
		}
		if (Case.Suspects.Num() == 0) Pages.Add({ TEXT("SUSPECT FILES"), { TEXT("No persons of interest yet.") } });
		break;
	case 3: // AUTOPSY
	{
		FPage P{ TEXT("MEDICAL EXAMINER'S REPORT"), {} };
		P.Lines.Add(TEXT("DECEDENT: ") + Case.Victim);
		P.Lines.Add(TEXT("ESTIMATED TIME OF DEATH: ") + Case.Time);
		P.Lines.Add(TEXT("CAUSE / WEAPON: ") + Case.Weapon);
		P.Lines.Add(TEXT(""));
		int32 Signs = 0;
		for (const TPair<EHUProfile, int32>& B : C->BodyLeads)
			if (B.Value > 0) { P.Lines.Add(FString::Printf(TEXT("Findings on the body consistent with a %s (%d sign%s)."), HUProfiles::Name(B.Key), B.Value, B.Value == 1 ? TEXT("") : TEXT("s"))); Signs++; }
		if (Signs == 0) P.Lines.Add(TEXT("Nothing noted from the body yet. Examine the victim at the scene."));
		P.Lines.Add(TEXT(""));
		const FString Leads = C->DescribeProfileLeads();
		if (!Leads.IsEmpty()) { P.Lines.Add(TEXT("ALL LEADS SO FAR:")); P.Lines.Add(Leads); }
		Pages.Add(P);
		break;
	}
	default: // TAPE NOTES
		Pages.Add({ TEXT("SECURITY TAPE NOTES"), TapeLines() });
		break;
	}
}
