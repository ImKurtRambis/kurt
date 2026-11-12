#include "HUCaseDesk.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "HUProfiles.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

using namespace HUBuild;

static const FLinearColor FolderColors[] = {
	FLinearColor(0.78f, 0.66f, 0.42f), // manila
	FLinearColor(0.42f, 0.55f, 0.72f), // blue
	FLinearColor(0.72f, 0.36f, 0.3f),  // red
	FLinearColor(0.48f, 0.62f, 0.45f), // green
	FLinearColor(0.55f, 0.55f, 0.58f), // grey
};

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
		FolderParts.Add(F);
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
	Rebuild();
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	I->Reading = this;
	I->Say(bTerm ? TEXT("The terminal hums awake. Green letters on black.") : FString::Printf(TEXT("You flip open the %s."), *FString(FolderName(K)).ToLower()));
	EnableInput(PC);
	if (InputComponent && InputComponent->KeyBindings.Num() == 0)
	{
		InputComponent->BindKey(EKeys::D, IE_Pressed, this, &AHUCaseDesk::NextPage);
		InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AHUCaseDesk::NextPage);
		InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AHUCaseDesk::PrevPage);
		InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AHUCaseDesk::PrevPage);
		InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AHUCaseDesk::ScrollDown);
		InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AHUCaseDesk::ScrollDown);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AHUCaseDesk::ScrollDown);
		InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AHUCaseDesk::ScrollUp);
		InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AHUCaseDesk::ScrollUp);
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AHUCaseDesk::ScrollUp);
		InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AHUCaseDesk::CloseKey);
		InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AHUCaseDesk::CloseKey);
	}
}

void AHUCaseDesk::Close()
{
	APawn* User = Viewer.Get();
	const int32 K = Folder;
	const bool bWasFolder = !bTerminal;
	bReading = false; Folder = -1; Viewer = nullptr;
	if (APlayerController* PC = User ? Cast<APlayerController>(User->GetController()) : nullptr)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		DisableInput(PC);
	}
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (I) I->Reading = nullptr;
	// put the folder back on the desk, for the whole squad
	if (bWasFolder && I && FolderParts.IsValidIndex(K)) { I->SendUseOn(this, FolderParts[K], true); I->Say(TEXT("You drop the folder back on the desk.")); }
}

void AHUCaseDesk::CloseKey() { if (bReading) Close(); }
void AHUCaseDesk::NextPage() { if (!bReading) return; Rebuild(); Page = FMath::Min(Page + 1, Pages.Num() - 1); Scroll = 0; }
void AHUCaseDesk::PrevPage() { if (!bReading) return; Rebuild(); Page = FMath::Max(Page - 1, 0); Scroll = 0; }
void AHUCaseDesk::ScrollDown() { if (bReading && Pages.IsValidIndex(Page)) Scroll = FMath::Min(Scroll + 3, FMath::Max(0, Pages[Page].Lines.Num() - 4)); }
void AHUCaseDesk::ScrollUp() { if (bReading) Scroll = FMath::Max(Scroll - 3, 0); }

void AHUCaseDesk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// a squadmate who left (or fell out of the game) drops what they were holding back on the desk
	for (int32 K = 0; K < FolderCount; K++)
		if (!Holder[K].IsValid() && FolderParts.IsValidIndex(K) && !FolderParts[K]->IsVisible() && !(bReading && Folder == K)) SetOnDesk(K, true);
	if (bReading && !Viewer.IsValid()) Close();
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
