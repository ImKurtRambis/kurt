#include "HUSquadRun.h"
#include "HUSquadSubsystem.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "HUCharacter.h"
#include "HUPlayerState.h"
#include "HUBriefingLaptop.h"
#include "HUCruiser.h"
#include "HUMotorPoolDoor.h"
#include "HUPoliceVan.h"
#include "HUSuspectActor.h"
#include "HUEvidence.h"
#include "HUSquadRoom.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

static FString RunRole()
{
	FString V;
	FParse::Value(FCommandLine::Get(), TEXT("HUSquadRun="), V);
	return V.ToLower();
}

bool UHUSquadRun::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return !RunRole().IsEmpty() && Super::ShouldCreateSubsystem(Outer);
#endif
}

#if !UE_BUILD_SHIPPING

static double NowS() { return FPlatformTime::Seconds(); }

template <class T> static T* First(UWorld* W, TFunctionRef<bool(T*)> Pred)
{
	if (W) for (TActorIterator<T> It(W); It; ++It) if (Pred(*It)) return *It;
	return nullptr;
}

// a private list of parts on an actor (the squad room's booking counter, the Captain's tray), by its property name
static TArray<UPrimitiveComponent*> PartsOf(AActor* A, const TCHAR* Prop)
{
	TArray<UPrimitiveComponent*> Out;
	FArrayProperty* P = A ? FindFProperty<FArrayProperty>(A->GetClass(), Prop) : nullptr;
	FObjectProperty* Inner = P ? CastField<FObjectProperty>(P->Inner) : nullptr;
	if (!Inner) return Out;
	FScriptArrayHelper H(P, P->ContainerPtrToValuePtr<void>(A));
	for (int32 i = 0; i < H.Num(); i++)
		if (UPrimitiveComponent* C = Cast<UPrimitiveComponent>(Inner->GetObjectPropertyValue(H.GetRawPtr(i)))) Out.Add(C);
	return Out;
}

void UHUSquadRun::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bHost = RunRole() == TEXT("host");
	if (!FParse::Value(FCommandLine::Get(), TEXT("HUSquadRunDir="), Dir)) Dir = FPaths::ProjectSavedDir() / TEXT("HUSquadRun");
	IFileManager::Get().MakeDirectory(*Dir, true);
	// a player's frame rate: headless copies otherwise run at hundreds of frames a second, and the movement they send
	// the host piles up faster than it's answered
	if (IConsoleVariable* Fps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) Fps->Set(60.f);
	Start = StepStart = NowS();
	TickH = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UHUSquadRun::Tick), 0.f);
	// leaving a level: a hard load (the desk, joining) or the squad's seamless travel (everything after roll call)
	LeaveH = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UHUSquadRun::OnLeaveMap);
	SeamlessH = FWorldDelegates::OnSeamlessTravelStart.AddWeakLambda(this, [this](UWorld*, const FString& To) { OnLeaveMap(To); });
	Say(FString::Printf(TEXT("playing a squad shift as the %s (notes in %s)"), bHost ? TEXT("host") : TEXT("partner"), *Dir));
}

void UHUSquadRun::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickH);
	FCoreUObjectDelegates::PreLoadMap.Remove(LeaveH);
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessH);
	Super::Deinitialize();
}

// ---------- helpers ----------
UWorld* UHUSquadRun::World() const { return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr; }
APlayerController* UHUSquadRun::PC() const { return GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController(World()) : nullptr; }
APawn* UHUSquadRun::Me() const { APlayerController* P = PC(); return P ? P->GetPawn() : nullptr; }
UHUInteractComponent* UHUSquadRun::Hands() const { APawn* P = Me(); return P ? P->FindComponentByClass<UHUInteractComponent>() : nullptr; }
UHUCaseSubsystem* UHUSquadRun::Case() const { return GetGameInstance()->GetSubsystem<UHUCaseSubsystem>(); }

TArray<APawn*> UHUSquadRun::Squad() const
{
	TArray<APawn*> Out;
	if (const AGameStateBase* GS = World() ? World()->GetGameState() : nullptr)
		for (const APlayerState* PS : GS->PlayerArray) if (PS && PS->GetPawn()) Out.Add(PS->GetPawn());
	return Out;
}

APawn* UHUSquadRun::Partner() const
{
	for (APawn* P : Squad()) if (P != Me()) return P;
	return nullptr;
}

void UHUSquadRun::Say(const FString& What) const
{
	UE_LOG(LogTemp, Display, TEXT("HUSquadRun %s [%.1f s]: %s"), bHost ? TEXT("host") : TEXT("partner"), NowS() - Start, *What);
}

void UHUSquadRun::Check(bool bOk, const FString& What) const
{
	UE_LOG(LogTemp, Display, TEXT("HUSquadRun CHECK %s %s: %s"), bOk ? TEXT("ok") : TEXT("FAIL"), bHost ? TEXT("host") : TEXT("partner"), *What);
}

void UHUSquadRun::Flag(const FString& Name) const { FFileHelper::SaveStringToFile(Name, *(Dir / (Name + TEXT(".flag")))); }
bool UHUSquadRun::HasFlag(const FString& Name) const { return IFileManager::Get().FileExists(*(Dir / (Name + TEXT(".flag")))); }
void UHUSquadRun::After(double Seconds) { NotBefore = NowS() + Seconds; }

void UHUSquadRun::Done(const FString& Why)
{
	Say(TEXT("done: ") + Why);
	bQuit = true;
	After(3.0);
}

// the host moves a detective (its own pawn, or a partner's - the partner's PC is told too)
void UHUSquadRun::Teleport(APawn* P, FVector At, float Yaw) const
{
	if (!P) return;
	FCollisionQueryParams Q(TEXT("HUSquadRun"), false);
	for (APawn* S : Squad()) Q.AddIgnoredActor(S);
	FHitResult Hit;
	if (World()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 150), At - FVector(0, 0, 400), ECC_Visibility, Q)) At.Z = Hit.ImpactPoint.Z + 100.f;
	else At.Z += 100.f;
	if (AHUCharacter* Ch = Cast<AHUCharacter>(P)) Ch->StandUp();
	P->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	if (APlayerController* C = Cast<APlayerController>(P->GetController()))
	{
		C->SetControlRotation(FRotator(0, Yaw, 0));
		if (!C->IsLocalController()) C->ClientSetLocation(At, FRotator(0, Yaw, 0));
	}
}

static FString Sorted(TArray<FString> A) { A.Sort(); return FString::Join(A, TEXT(",")); }

void UHUSquadRun::Digest(const FString& Stage) const
{
	const UHUCaseSubsystem* C = Case();
	if (!C) return;
	TArray<FString> Book, Pres;
	for (const TPair<FString, int32>& KV : C->BookingStep) Book.Add(FString::Printf(TEXT("%s=%d"), *KV.Key, KV.Value));
	for (const TPair<FString, float>& KV : C->Pressure) Pres.Add(FString::Printf(TEXT("%s=%.1f"), *KV.Key, KV.Value));
	FString Verdict = C->LastVerdict;
	int32 Cut;
	if (Verdict.FindChar(TEXT('.'), Cut)) Verdict.LeftInline(Cut); // the rest (pay, XP, promotions) is each detective's own
	UE_LOG(LogTemp, Display, TEXT("HUSquadRun DIGEST %s | case '%s' killer %s active %d briefed %d | found [%s] | breaks %d confessed '%s' | arrested [%s] | booked [%s] awaiting %d | phones [%s] | pressure [%s] | verdict '%s'"),
		*Stage, *C->Current.Title, *C->Current.Solution.Killer, C->bCaseActive, C->bBriefed, *Sorted(C->Found), C->Breaks, *C->Confessed,
		*Sorted(C->Arrested), *Sorted(Book), C->bAwaitingCaptain, *Sorted(C->SeizedPhones), *Sorted(Pres), *Verdict);
}

void UHUSquadRun::OnLeaveMap(const FString& NextMap)
{
	// once per level (a seamless trip passes through the transition level: that one isn't ours)
	if (!Map.IsEmpty() && Map != Digested && Map.StartsWith(TEXT("L_"))) { Digest(TEXT("leaving ") + Map); Digested = Map; }
}

// ---------- the run ----------
bool UHUSquadRun::Tick(float Dt)
{
	UWorld* W = World();
	if (!W) return true;
	const double Now = NowS();
	if (bQuit)
	{
		if (Now >= NotBefore) { bQuit = false; FPlatformMisc::RequestExit(false, TEXT("HUSquadRun")); }
		return true;
	}
	const FString Here = W->GetMapName();
	if (Here != Map)
	{
		Say(FString::Printf(TEXT("in %s (%d in the squad)"), *Here, Squad().Num()));
		Map = Here; MapStart = StepStart = Now; Step = 0; LastStep = 0; Tries = 0; Cursor = 0;
		After(3.0); // let the level settle (the scene builds itself on every PC)
		return true;
	}
	if (Now < NotBefore) return true;
	if (Now - Start > 1200.0) { Check(false, TEXT("the shift took more than 20 minutes")); Done(TEXT("out of time")); return true; }
	if (Now - MapStart > 300.0) { Check(false, FString::Printf(TEXT("stuck in %s at step %d"), *Map, Step)); Done(TEXT("stuck")); return true; }
	if (Step != LastStep) { LastStep = Step; StepStart = Now; }
	if (Map == TEXT("L_Menu")) Menu();
	else if (Map == TEXT("L_Lobby")) Lobby();
	else if (Map == TEXT("L_Briefing")) Briefing();
	else if (Map == TEXT("L_Scene")) Scene();
	else if (Map == TEXT("L_Precinct")) Precinct();
	return true;
}

// the desk: host a squad, or join it by the code the host left
void UHUSquadRun::Menu()
{
	UHUSquadSubsystem* SQ = GetGameInstance()->GetSubsystem<UHUSquadSubsystem>();
	if (bHost)
	{
		if (Step == 0) { Say(TEXT("hosting a squad")); SQ->HostSquad(); Next(2.0); return; }
		After(1.0); // the squad room is loading
		return;
	}
	if (Step == 0)
	{
		FString Code;
		if (!FFileHelper::LoadFileToString(Code, *(Dir / TEXT("code.txt")))) { After(1.0); return; }
		Code.TrimStartAndEndInline();
		Say(TEXT("joining squad ") + Code + TEXT(" by its code"));
		SQ->JoinByCode(Code);
		Next(1.0);
		return;
	}
	if (SQ->State == EHUSquadState::Failed)
	{
		Say(TEXT("couldn't join: ") + SQ->Status);
		if (++Tries >= 5) { Check(false, TEXT("joining by code: ") + SQ->Status); Done(TEXT("couldn't join")); return; }
		Step = 0;
		After(3.0);
		return;
	}
	After(0.5);
}

// roll call: both punch in, and the squad room counts down and starts the shift
void UHUSquadRun::Lobby()
{
	UHUSquadSubsystem* SQ = GetGameInstance()->GetSubsystem<UHUSquadSubsystem>();
	if (Step == 0)
	{
		if (bHost)
		{
			FFileHelper::SaveStringToFile(SQ->Code, *(Dir / TEXT("code.txt")));
			Say(TEXT("squad code ") + SQ->Code + TEXT(" left for the partner"));
		}
		Next(0.5);
		return;
	}
	if (Step == 1)
	{
		AHUPlayerState* PS = PC() ? PC()->GetPlayerState<AHUPlayerState>() : nullptr;
		if (Squad().Num() < 2 || !Me() || !PS) { After(0.5); return; }
		Check(true, FString::Printf(TEXT("both detectives are in roll call (squad %s)"), *SQ->Code));
		PS->ServerSetReady(true);
		Say(TEXT("punched in"));
		Next(1.0);
		return;
	}
	After(1.0); // the countdown, then the host's squad room starts the shift
}

// the briefing: the partner clicks through the deck, stands and walks, tries the car early; the host drives
void UHUSquadRun::Briefing()
{
	UHUCaseSubsystem* C = Case();
	UHUInteractComponent* I = Hands();
	if (!I || Squad().Num() < 2) { After(0.5); return; }
	AHUBriefingLaptop* Laptop = First<AHUBriefingLaptop>(World(), [](AHUBriefingLaptop*) { return true; });
	AHUCruiser* Car = First<AHUCruiser>(World(), [](AHUCruiser* K) { return K->bPlayerCar && !K->IsA<AHUMotorPoolDoor>(); });
	switch (Step)
	{
	case 0:
		Say(FString::Printf(TEXT("briefing on case '%s' (laptop %s, car %s)"), *C->Current.Title, Laptop ? TEXT("found") : TEXT("MISSING"), Car ? TEXT("found") : TEXT("MISSING")));
		Next(2.0);
		return;
	case 1:
		if (!bHost)
		{
			if (!Laptop) { After(1.0); return; }
			if (!C->bBriefed) { I->SendUseOn(Laptop, nullptr, false); After(0.5); return; }
			Check(true, FString::Printf(TEXT("the partner clicked through the briefing (%d slides)"), Laptop->Slides.Num()));
			Next(1.0);
			return;
		}
		if (!C->bBriefed) { After(0.5); return; }
		Check(Laptop && Laptop->IsLastSlide(), FString::Printf(TEXT("the host's projector followed the partner's clicks to the last slide (%d of %d)"), Laptop ? Laptop->Index + 1 : 0, Laptop ? Laptop->Slides.Num() : 0));
		Check(C->bBriefed, TEXT("the host's game counts the squad as briefed"));
		if (APawn* P = Partner()) Mark = P->GetActorLocation();
		Next(0.5);
		return;
	case 2:
		if (!bHost)
		{
			// stand up the way a player does (a move key, on this PC only) and walk forward a moment
			AHUCharacter* Ch = Cast<AHUCharacter>(Me());
			if (Tries == 0) { if (Ch && Ch->bSeated) Ch->StandUp(); Mark = Me()->GetActorLocation(); Tries = 1; StepStart = NowS(); }
			if (NowS() - StepStart < 1.5) { Me()->AddMovementInput(Me()->GetActorForwardVector(), 1.f); return; }
			Say(FString::Printf(TEXT("stood up and walked %.0f cm (on this PC)"), FVector::Dist2D(Mark, Me()->GetActorLocation())));
			Flag(TEXT("walked"));
			Tries = 0;
			Next(0.5);
			return;
		}
		if (!HasFlag(TEXT("walked"))) { After(0.5); return; }
		if (Tries == 0) { Tries = 1; After(2.0); return; }
		Check(Partner() && FVector::Dist2D(Mark, Partner()->GetActorLocation()) > 40.f,
			FString::Printf(TEXT("the host sees the partner stand up from the briefing seat and walk (%.0f cm)"), Partner() ? FVector::Dist2D(Mark, Partner()->GetActorLocation()) : 0.f));
		Tries = 0;
		if (!Car) { Check(false, TEXT("no player car in the briefing level")); Done(TEXT("no car")); return; }
		// the partner alone at the car, the host still in the briefing room
		Teleport(Partner(), Car->GetActorLocation() + Car->GetActorRightVector() * 200.f, Car->GetActorRotation().Yaw);
		Flag(TEXT("partner-at-car"));
		Next(1.0);
		return;
	case 3:
		if (!bHost)
		{
			if (!Car || !HasFlag(TEXT("partner-at-car")) || FVector::Dist2D(Me()->GetActorLocation(), Car->GetActorLocation()) > Car->GatherRadius) { After(0.5); return; }
			if (Tries == 0) { Tries = 1; After(1.0); return; }
			Say(TEXT("pressing the car before the host is there"));
			I->SendUseOn(Car, nullptr, false);
			Tries = 0;
			Next(3.0);
			return;
		}
		if (!HasFlag(TEXT("pressed-early"))) { After(0.5); return; }
		Check(true, TEXT("the host's car didn't leave without the host"));
		Teleport(Me(), Car->GetActorLocation() - Car->GetActorRightVector() * 200.f, Car->GetActorRotation().Yaw);
		Next(1.5);
		return;
	case 4:
		if (!bHost)
		{
			Check(!PC()->IsMoveInputIgnored(), TEXT("pressing the car with the host elsewhere leaves the partner free to move (no black screen)"));
			Flag(TEXT("pressed-early"));
			Next(1.0);
			return;
		}
		Say(TEXT("the host drives: everyone to the scene"));
		I->SendUseOn(Car, nullptr, false);
		Next(1.0);
		return;
	default:
		After(1.0); // driving
		return;
	}
}

// the scene: the partner bags everything, the host interviews the culprit, the partner cuffs them and walks them to
// the van (loading them while still walking: the worst case for two PCs that see each other a moment late)
void UHUSquadRun::Scene()
{
	UHUCaseSubsystem* C = Case();
	UHUInteractComponent* I = Hands();
	if (!I || Squad().Num() < 2) { After(0.5); return; }
	const FString KillerId = C->Current.Solution.Killer;
	AHUSuspectActor* Killer = First<AHUSuspectActor>(World(), [&](AHUSuspectActor* S) { return S->SuspectId == KillerId; });
	AHUPoliceVan* Van = First<AHUPoliceVan>(World(), [](AHUPoliceVan*) { return true; });
	switch (Step)
	{
	case 0:
	{
		Todo.Reset();
		TArray<AHUEvidence*> All;
		for (TActorIterator<AHUEvidence> It(World()); It; ++It) All.Add(*It);
		All.Sort([](const AHUEvidence& A, const AHUEvidence& B) { return A.GetName() < B.GetName(); });
		for (AHUEvidence* E : All) Todo.Add(E);
		int32 Sus = 0;
		for (TActorIterator<AHUSuspectActor> It(World()); It; ++It) Sus++;
		Say(FString::Printf(TEXT("at the scene of '%s': %d pieces of evidence, %d people, culprit %s, van %s"), *C->Current.Title, Todo.Num(), Sus, Killer ? *Killer->GetName() : TEXT("MISSING"), Van ? TEXT("found") : TEXT("MISSING")));
		Digest(TEXT("scene-start"));
		Cursor = 0; Tries = 0;
		Next(1.0);
		return;
	}
	case 1:
		if (!bHost)
		{
			while (Todo.IsValidIndex(Cursor) && !Todo[Cursor].IsValid()) { Cursor++; Tries = 0; }
			if (!Todo.IsValidIndex(Cursor))
			{
				I->Held = 0;
				Check(true, FString::Printf(TEXT("the partner bagged every piece (%d in the bag)"), C->Found.Num()));
				Flag(TEXT("bagged"));
				Next(1.0);
				return;
			}
			AHUEvidence* E = Cast<AHUEvidence>(Todo[Cursor].Get());
			if (Tries >= 4) { Check(false, TEXT("couldn't bag ") + E->ClueId); Cursor++; Tries = 0; return; }
			// the right tool in hand: the crowbar for a strongbox (open, then bag), the print kit for prints
			const int32 Tool = E->Conceal == EHUConceal::Locked ? (int32)EHUTool::Crowbar : E->Conceal == EHUConceal::Prints ? (int32)EHUTool::PrintKit : -1;
			I->Held = Tool + 1;
			I->SendUseOn(E, nullptr, false);
			Tries++;
			After(0.7);
			return;
		}
		if (!HasFlag(TEXT("bagged"))) { After(0.5); return; }
		if (Tries == 0) { Tries = 1; After(2.0); return; }
		{
			int32 Left = 0;
			for (const TWeakObjectPtr<AActor>& E : Todo) if (E.IsValid()) Left++;
			Check(Left == 0, FString::Printf(TEXT("every piece the partner bagged is gone from the host's scene too (%d left, %d bagged here)"), Left, C->Found.Num()));
		}
		Tries = 0;
		Next(0.5);
		return;
	case 2:
		if (!bHost)
		{
			if (!HasFlag(TEXT("interviewed"))) { After(0.5); return; }
			if (!Killer) { Check(false, TEXT("no culprit at the scene")); Done(TEXT("no culprit")); return; }
			Say(TEXT("cuffing ") + Killer->DisplayName());
			I->Held = (int32)EHUTool::Handcuffs + 1;
			I->SendUseOn(Killer, nullptr, false);
			Next(2.0);
			return;
		}
		if (Tries == 0)
		{
			// the interview: every lie of the culprit's the bag can prove, their record, and a push on someone else
			MoveWho.Reset(); MoveClue.Reset(); MoveLine.Reset(); MoveKind.Reset();
			FHUSuspect K;
			if (Killer && C->GetSuspect(KillerId, K))
			{
				for (int32 s = 0; s < K.Statements.Num(); s++)
				{
					if (K.Statements[s].Truth) continue;
					for (const FString& Clue : K.Statements[s].Evidence)
						if (C->Found.Contains(Clue)) { MoveWho.Add(Killer->GetPathName()); MoveClue.Add(Clue); MoveLine.Add(s); MoveKind.Add(0); break; }
				}
				MoveWho.Add(Killer->GetPathName()); MoveClue.Add(FString()); MoveLine.Add(0); MoveKind.Add(1);
			}
			if (AHUSuspectActor* Other = First<AHUSuspectActor>(World(), [&](AHUSuspectActor* S) { return S->SuspectId != KillerId; }))
			{
				MoveWho.Add(Other->GetPathName()); MoveClue.Add(FString()); MoveLine.Add(0); MoveKind.Add(0);
			}
			Say(FString::Printf(TEXT("interviewing: %d moves (the case needs %d lies)"), MoveWho.Num(), C->Current.Solution.RequiredBreaks));
			Tries = 1; Cursor = 0;
		}
		if (MoveWho.IsValidIndex(Cursor))
		{
			I->SendInterrogation(MoveWho[Cursor], MoveKind[Cursor], MoveLine[Cursor], MoveClue[Cursor]);
			Cursor++;
			After(1.2);
			return;
		}
		Say(FString::Printf(TEXT("interview done: %d of %d lies on the record%s"), C->Breaks, C->Current.Solution.RequiredBreaks, C->Confessed.IsEmpty() ? TEXT("") : *(TEXT(", confessed: ") + C->Confessed)));
		Flag(TEXT("interviewed"));
		Tries = 0; Cursor = 0;
		Next(1.0);
		return;
	case 3:
		if (!bHost)
		{
			I->Held = 0;
			Check(Killer && Killer->IsEscortedBy(Me()), TEXT("the partner has the culprit in cuffs, by the arm"));
			Flag(TEXT("arrested"));
			Next(0.5);
			return;
		}
		if (!HasFlag(TEXT("arrested"))) { After(0.5); return; }
		if (Tries == 0) { Tries = 1; After(1.0); return; }
		Check(Killer && Killer->bCuffed && Killer->IsEscortedBy(Partner()), TEXT("the host sees the culprit cuffed and held by the partner"));
		Check(C->Breaks > 0 || C->Current.Solution.RequiredBreaks == 0, FString::Printf(TEXT("the interview put lies on the record (%d)"), C->Breaks));
		if (!Van) { Check(false, TEXT("no van at the scene")); Done(TEXT("no van")); return; }
		// the partner (and the culprit on their arm) well outside loading range, on an open side of the van, facing it
		{
			const float Away = Van->LoadRange + 350.f;
			FVector Side = -Van->GetActorForwardVector();
			FCollisionQueryParams Q(TEXT("HUSquadRunVan"), false, Van);
			for (APawn* S : Squad()) Q.AddIgnoredActor(S);
			if (Killer) Q.AddIgnoredActor(Killer);
			for (const FVector& D : { -Van->GetActorForwardVector(), Van->GetActorRightVector(), -Van->GetActorRightVector(), Van->GetActorForwardVector() })
			{
				FHitResult Hit;
				const FVector From = Van->GetActorLocation() + FVector(0, 0, 90) + D * 300.f;
				if (!World()->SweepSingleByChannel(Hit, From, From + D * (Away - 200.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(60.f), Q)) { Side = D; break; }
			}
			Teleport(Partner(), Van->GetActorLocation() + Side * Away, (-Side).Rotation().Yaw);
		}
		Flag(TEXT("at-van"));
		Tries = 0;
		Next(0.5);
		return;
	case 4:
		if (!bHost)
		{
			if (!Van || !HasFlag(TEXT("at-van"))) { After(0.5); return; }
			if (Tries == 0)
			{
				if (FVector::Dist2D(Me()->GetActorLocation(), Van->GetActorLocation()) > Van->LoadRange + 600.f) { After(0.3); return; }
				Tries = 1; After(1.0); StepStart = NowS() + 1.0; return;
			}
			// walk to the van; once the culprit is inside loading range (on this PC), load them without stopping
			const FVector To = (Van->GetActorLocation() - Me()->GetActorLocation()) * FVector(1, 1, 0);
			PC()->SetControlRotation(To.Rotation());
			Me()->AddMovementInput(To.GetSafeNormal(), 1.f);
			const float D = Killer ? FVector::Dist2D(Killer->GetActorLocation(), Van->GetActorLocation()) : 0.f;
			if (Tries == 1 && (D < Van->LoadRange - 100.f || NowS() - StepStart > 10.0))
			{
				Say(FString::Printf(TEXT("loading the culprit while walking: %.0f cm from the van on this PC"), D));
				if (D >= Van->LoadRange - 100.f) Check(false, TEXT("couldn't walk the culprit up to the van in 10 s"));
				I->SendUseOn(Van, nullptr, false);
				Tries = 2; Mark = FVector(NowS(), 0, 0);
			}
			if (Tries == 2 && NowS() - Mark.X > 0.4) { Tries = 0; Next(3.0); }
			return;
		}
		if (!HasFlag(TEXT("vanned"))) { After(0.5); return; }
		if (Tries == 0) { Tries = 1; After(2.0); return; }
		Check(C->Arrested.Contains(KillerId), TEXT("the host's game has the culprit in the van too"));
		{
			AHUCruiser* Car = First<AHUCruiser>(World(), [](AHUCruiser* K) { return K->bPlayerCar && !K->IsA<AHUMotorPoolDoor>(); });
			if (!Car) { Check(false, TEXT("no car at the scene")); Done(TEXT("no car")); return; }
			Teleport(Partner(), Car->GetActorLocation() + Car->GetActorRightVector() * 200.f, Car->GetActorRotation().Yaw);
			Teleport(Me(), Car->GetActorLocation() - Car->GetActorRightVector() * 200.f, Car->GetActorRotation().Yaw);
		}
		Tries = 0;
		Next(2.0);
		return;
	case 5:
		if (!bHost)
		{
			Check(C->Arrested.Contains(KillerId), TEXT("the partner's game has the culprit in the van"));
			Flag(TEXT("vanned"));
			Next(1.0);
			return;
		}
		if (AHUCruiser* Car = First<AHUCruiser>(World(), [](AHUCruiser* K) { return K->bPlayerCar && !K->IsA<AHUMotorPoolDoor>(); }))
		{
			Say(TEXT("the host drives everyone back to the precinct"));
			I->SendUseOn(Car, nullptr, false);
		}
		Next(1.0);
		return;
	default:
		After(1.0);
		return;
	}
}

// the precinct: the partner books the prisoners at the counter, the host hands the case to the Captain
void UHUSquadRun::Precinct()
{
	UHUCaseSubsystem* C = Case();
	UHUInteractComponent* I = Hands();
	if (!I || (Step < 2 && Squad().Num() < 2)) { After(0.5); return; } // (once submitted, the partner may have gone)
	AHUSquadRoom* Room = First<AHUSquadRoom>(World(), [](AHUSquadRoom* R) { return R->bPrecinct; });
	const TArray<UPrimitiveComponent*> Counter = PartsOf(Room, TEXT("CounterParts")), Tray = PartsOf(Room, TEXT("TrayParts"));
	switch (Step)
	{
	case 0:
		Say(FString::Printf(TEXT("at the precinct: %d in holding, awaiting the Captain %d (counter %d parts, tray %d)"), C->Arrested.Num(), C->bAwaitingCaptain, Counter.Num(), Tray.Num()));
		if (!Room || Counter.Num() == 0 || Tray.Num() == 0) { Check(false, TEXT("no booking counter or in-tray in the precinct")); Done(TEXT("no precinct")); return; }
		Cursor = 0;
		Next(1.0);
		return;
	case 1:
		if (!bHost)
		{
			if (Cursor < C->Arrested.Num() * 3) { I->SendUseOn(Room, Counter[0], false); Cursor++; After(0.8); return; }
			After(1.5);
			Next(1.5);
			return;
		}
		if (!HasFlag(TEXT("booked"))) { After(0.5); return; }
		if (Tries == 0) { Tries = 1; After(1.5); return; }
		Check(C->UnbookedCount() == 0, FString::Printf(TEXT("the host's game has every prisoner booked (%d left)"), C->UnbookedCount()));
		Say(TEXT("the host submits the case to the Captain"));
		I->SendUseOn(Room, Tray[0], false);
		Tries = 0;
		Next(4.0);
		return;
	case 2:
		if (!bHost)
		{
			Check(C->UnbookedCount() == 0, FString::Printf(TEXT("the partner booked every prisoner (%d of %d)"), C->Arrested.Num() - C->UnbookedCount(), C->Arrested.Num()));
			Flag(TEXT("booked"));
			Next(1.0);
			return;
		}
		Check(!C->bCaseActive && !C->LastVerdict.IsEmpty(), TEXT("the host's game closed the case: ") + C->LastVerdict);
		Digest(TEXT("final"));
		Next(1.0);
		return;
	case 3:
		if (!bHost)
		{
			if (C->bCaseActive || C->LastVerdict.IsEmpty()) { if (NowS() - StepStart > 30.0) { Check(false, TEXT("the partner's game never got the Captain's verdict")); Digest(TEXT("final")); Done(TEXT("no verdict")); } else After(0.5); return; }
			Check(true, TEXT("the partner's game closed the case: ") + C->LastVerdict);
			Digest(TEXT("final"));
			Flag(TEXT("partner-final"));
			Done(TEXT("shift over"));
			return;
		}
		if (!HasFlag(TEXT("partner-final")) && NowS() - StepStart < 40.0) { After(0.5); return; }
		Done(TEXT("shift over"));
		return;
	default:
		return;
	}
}

#else
void UHUSquadRun::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UHUSquadRun::Deinitialize() { Super::Deinitialize(); }
bool UHUSquadRun::Tick(float) { return false; }
void UHUSquadRun::OnLeaveMap(const FString&) {}
void UHUSquadRun::Digest(const FString&) const {}
void UHUSquadRun::Check(bool, const FString&) const {}
void UHUSquadRun::Say(const FString&) const {}
void UHUSquadRun::Flag(const FString&) const {}
bool UHUSquadRun::HasFlag(const FString&) const { return false; }
void UHUSquadRun::After(double) {}
void UHUSquadRun::Done(const FString&) {}
void UHUSquadRun::Menu() {}
void UHUSquadRun::Lobby() {}
void UHUSquadRun::Briefing() {}
void UHUSquadRun::Scene() {}
void UHUSquadRun::Precinct() {}
UWorld* UHUSquadRun::World() const { return nullptr; }
APlayerController* UHUSquadRun::PC() const { return nullptr; }
APawn* UHUSquadRun::Me() const { return nullptr; }
UHUInteractComponent* UHUSquadRun::Hands() const { return nullptr; }
UHUCaseSubsystem* UHUSquadRun::Case() const { return nullptr; }
TArray<APawn*> UHUSquadRun::Squad() const { return {}; }
APawn* UHUSquadRun::Partner() const { return nullptr; }
void UHUSquadRun::Teleport(APawn*, FVector, float) const {}
#endif
