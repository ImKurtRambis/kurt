#include "HUCaseSubsystem.h"
#include "HUBrainSubsystem.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HUSave.h"
#include "HUCaseGen.h"
#include "HUProfiles.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"

void UHUCaseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadCases();
	if (UHUSave* S = Cast<UHUSave>(UGameplayStatics::LoadGameFromSlot(TEXT("HomicideUnit"), 0)))
	{
		Cash = S->Cash; OwnedTools = S->OwnedTools; CasesSolved = S->CasesSolved;
		DetectiveName = S->DetectiveName; JoinAddress = S->JoinAddress; MouseSensitivity = S->MouseSensitivity; FieldOfView = S->FieldOfView; VoiceProvider = S->VoiceProvider; VoiceKey = S->VoiceKey; bVoiceQuestions = S->bVoiceQuestions; Look = S->Look;
	}
}

void UHUCaseSubsystem::SaveProgress()
{
	UHUSave* S = Cast<UHUSave>(UGameplayStatics::CreateSaveGameObject(UHUSave::StaticClass()));
	if (!S) return;
	S->Cash = Cash; S->OwnedTools = OwnedTools; S->CasesSolved = CasesSolved;
	S->DetectiveName = DetectiveName; S->JoinAddress = JoinAddress; S->MouseSensitivity = MouseSensitivity; S->FieldOfView = FieldOfView; S->VoiceProvider = VoiceProvider; S->VoiceKey = VoiceKey; S->bVoiceQuestions = bVoiceQuestions; S->Look = Look;
	UGameplayStatics::SaveGameToSlot(S, TEXT("HomicideUnit"), 0);
}

bool UHUCaseSubsystem::BuyTool(EHUTool Tool)
{
	const int32 Price = HUTools::Price(Tool);
	if (HasTool(Tool) || Cash < Price) return false;
	Cash -= Price;
	OwnedTools |= (1 << (int32)Tool);
	SaveProgress();
	return true;
}

void UHUCaseSubsystem::AddCash(int32 Amount)
{
	Cash = FMath::Max(0, Cash + Amount);
	SaveProgress();
}

bool UHUCaseSubsystem::LoadCases()
{
	// every case is generated: incident x hidden offender profile x place, with its own map seed.
	// (Content/Data/Cases.json, the browser game's export, is no longer used.)
	Cases = HUCaseGen::MakeCases(100 + FMath::RandRange(0, 800), 24, FMath::Rand());
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit: %d cases generated"), Cases.Num());
	return Cases.Num() > 0;
}

void UHUCaseSubsystem::UseSquadSeed(int32 Seed)
{
	SquadSeed = Seed;
	bCaseActive = false;
	Cases = HUCaseGen::MakeCases(100 + (Seed != 0 ? Seed % 800 : FMath::RandRange(0, 800)), 24, Seed != 0 ? Seed : FMath::Rand());
}

bool UHUCaseSubsystem::StartCase(int32 Index)
{
	if (Cases.Num() == 0 && !LoadCases()) return false;
	if (!Cases.IsValidIndex(Index)) Index = SquadSeed != 0 ? (SquadSeed + CasesStarted) % Cases.Num() : FMath::RandRange(0, Cases.Num() - 1); // a squad all gets the same case, and a new one each shift
	CasesStarted++;
	Current = Cases[Index];
	// testing a place: -HUPlace=HOTEL on the command line
	{ FString Forced; if (FParse::Value(FCommandLine::Get(), TEXT("HUPlace="), Forced)) for (const FString& P : HUCaseGen::Places()) if (P.Contains(Forced)) ChosenPlace = P; }
	// a hand-built map only gets the offenders that fit it: if the squad picked one, swap in a case with a fitting offender
	// (made the same way on every machine, from the case number)
	if (const TArray<EHUProfile> Fit = HUProfiles::ForPlace(ChosenPlace); Fit.Num() && !Fit.Contains((EHUProfile)Current.Profile))
		for (int32 Try = 1; Try < 200; Try++)
		{
			const TArray<FHUCase> One = HUCaseGen::MakeCases(Current.Number, 1, Current.Number * 7919 + Try);
			if (One.Num() && Fit.Contains((EHUProfile)One[0].Profile)) { Current = One[0]; break; }
		}
	// the squad room's choices
	if (!ChosenPlace.IsEmpty() && ChosenPlace != Current.Place)
	{
		const FString Old = Current.Place;
		Current.Title = Current.Title.Replace(*Old, *ChosenPlace);
		Current.CaptainLine = Current.CaptainLine.Replace(*Old, *ChosenPlace);
		for (FString& Row : Current.VictimRows) Row = Row.Replace(*Old, *ChosenPlace);
		for (FHUClue& K : Current.Clues) K.Detail = K.Detail.Replace(*Old, *ChosenPlace);
		Current.Place = ChosenPlace;
	}
	if (!ChosenDifficulty.IsEmpty()) Current.Difficulty = ChosenDifficulty;
	bCaseActive = true;
	bBriefed = false;
	PlayDifficulty = Current.Difficulty.IsEmpty() ? TEXT("detective") : Current.Difficulty.ToLower();
	{ int32 D, Hd, L; GetRules(D, Hd, L); Current.Solution.RequiredBreaks = L; }
	InScene.Reset();
	Arrested.Reset();
	BookingStep.Reset();
	bAwaitingCaptain = false;
	BoardPins.Reset();
	BoardMarks.Reset();
	BodyLeads.Reset();
	Pressure.Reset(); Misses.Reset(); Lawyered.Reset(); Confessed.Reset(); PressCount.Reset(); RecordRaised.Reset();
	if (UHUBrainSubsystem* Brain = GetGameInstance()->GetSubsystem<UHUBrainSubsystem>()) Brain->ResetConversations();
	Found.Reset();
	BrokenKeys.Reset();
	Breaks = 0;
	WrongChallenges = 0;
	AssignProfiles();
	OnCaseStarted.Broadcast(Current);
	return true;
}

bool UHUCaseSubsystem::FindClue(const FString& ClueId)
{
	FHUClue Clue;
	if (!bCaseActive || Found.Contains(ClueId) || !GetClue(ClueId, Clue)) return false;
	Found.Add(ClueId);
	if (GetKeyEvidence().Contains(ClueId)) AddCash(10); // the department pays for evidence that holds up
	OnClueFound.Broadcast(Clue);
	return true;
}

bool UHUCaseSubsystem::GetClue(const FString& ClueId, FHUClue& Out) const
{
	for (const FHUClue& C : Current.Clues) if (C.Id == ClueId) { Out = C; return true; }
	return false;
}

bool UHUCaseSubsystem::GetSuspect(const FString& SuspectId, FHUSuspect& Out) const
{
	for (const FHUSuspect& S : Current.Suspects) if (S.Id == SuspectId) { Out = S; return true; }
	return false;
}

EHUChallengeResult UHUCaseSubsystem::Challenge(const FString& SuspectId, int32 StatementIndex, const FString& ClueId)
{
	const FString Key = FString::Printf(TEXT("%s#%d"), *SuspectId, StatementIndex);
	if (BrokenKeys.Contains(Key)) return EHUChallengeResult::AlreadyDone;
	FHUSuspect S;
	if (!GetSuspect(SuspectId, S) || !S.Statements.IsValidIndex(StatementIndex) || !Found.Contains(ClueId))
	{
		WrongChallenges++;
		return EHUChallengeResult::Wrong;
	}
	const FHUStatement& St = S.Statements[StatementIndex];
	if (St.Truth || !St.Evidence.Contains(ClueId))
	{
		WrongChallenges++;
		return EHUChallengeResult::Wrong;
	}
	BrokenKeys.Add(Key);
	Breaks++;
	OnLieBroken.Broadcast(S, StatementIndex, St.OnBreak);
	return EHUChallengeResult::Broken;
}

bool UHUCaseSubsystem::IsBroken(const FString& SuspectId, int32 StatementIndex) const
{
	return BrokenKeys.Contains(FString::Printf(TEXT("%s#%d"), *SuspectId, StatementIndex));
}

bool UHUCaseSubsystem::Accuse(const FString& SuspectId)
{
	const bool bRight = bCaseActive && SuspectId == Current.Solution.Killer;
	bCaseActive = false;
	if (bRight) { CasesSolved++; AddCash(200); }
	else AddCash(25); // overtime, anyway
	return bRight;
}

TArray<FHUSlide> UHUCaseSubsystem::BuildBriefingSlides(const FString& Who) const
{
	TArray<FHUSlide> Out;
	const FHUCase& C = Current;
	auto Add = [&Out](const FString& H, const FString& T, TArray<FString> Rows, const FString& Say, const FString& Sid = FString())
	{
		FHUSlide S; S.Header = H; S.Title = T.ToUpper(); S.Rows = MoveTemp(Rows); S.CaptainLine = Say; S.SuspectId = Sid; Out.Add(S);
	};
	Add(TEXT("HOMICIDE BRIEFING"), C.Title, { FString::Printf(TEXT("CASE NO. %d"), C.Number), TEXT("DIFFICULTY: ") + C.Difficulty.ToUpper() },
		FString::Printf(TEXT("\"Sit down, %s. We've got a fresh one. Click through the deck.\""), *Who));
	if (!C.VictimHeader.IsEmpty()) // generated cases describe their own incident (and never the offender)
		Add(C.VictimHeader, C.Victim, C.VictimRows, C.CaptainLine);
	else if (C.Crime == TEXT("predator-abduction"))
		Add(TEXT("THE MISSING CHILD"), C.Victim, { TEXT("LAST SEEN AT: ") + C.Place, TEXT("MISSING SINCE: ") + C.Time, TEXT("SPECIAL VICTIMS — EVERY MINUTE COUNTS") },
			FString::Printf(TEXT("\"%s. Last seen at %s at %s. The one who took them is still in that building. Find out where the kid is.\""), *C.Victim, *C.Place, *C.Time));
	else if (C.Crime == TEXT("predator-sting"))
		Add(TEXT("THE STING"), TEXT("A SCREEN NAME"), { TEXT("UNDERCOVER: ") + C.Victim, TEXT("MEETING SET FOR ") + C.Time + TEXT(" AT ") + C.Place, TEXT("THEY THINK THEY'RE MEETING A CHILD") },
			FString::Printf(TEXT("\"Our undercover's been chatting with a predator for a month. Tonight they set a meeting at %s. Everyone in that room is held. Find the screen name.\""), *C.Place));
	else if (C.Crime == TEXT("predator-assault"))
		Add(TEXT("THE VICTIM"), C.Victim, { TEXT("ASSAULTED AT: ") + C.Place, TEXT("ALIVE AT COUNTY GENERAL"), TEXT("SPECIAL VICTIMS") },
			FString::Printf(TEXT("\"%s, a parent. Recognised someone at %s and got a coat rack for it. Find out who they recognised.\""), *C.Victim, *C.Place));
	else if (C.Crime == TEXT("predator-homicide"))
		Add(TEXT("THE VICTIM"), C.Victim, { TEXT("FOUND AT: ") + C.Place, TEXT("TIME: ") + C.Time, TEXT("SPECIAL VICTIMS") },
			FString::Printf(TEXT("\"%s ran the youth program. Found dead at %s. Word is they were about to go to the police about somebody.\""), *C.Victim, *C.Place));
	else
		Add(TEXT("THE VICTIM"), C.Victim, { TEXT("FOUND AT: ") + C.Place, TEXT("TIME: ") + C.Time },
			FString::Printf(TEXT("\"%s. Found dead at %s.\""), *C.Victim, *C.Place));
	Add(TEXT("THE SCENE"), C.Place, { TEXT("SECURED BY PATROL"), TEXT("BACK AREA NOT YET SEARCHED"), TEXT("BAG EVERYTHING") },
		TEXT("\"Patrol has the place locked down. Check the back rooms too.\""));
	{
		// everyone held at the scene, on one slide
		TArray<FString> People;
		for (const FHUSuspect& S : C.Suspects) People.Add(FString::Printf(TEXT("%s  —  %s"), *S.Name.ToUpper(), S.Role.IsEmpty() ? TEXT("AT THE SCENE") : *S.Role.ToUpper()));
		Add(FString::Printf(TEXT("PERSONS OF INTEREST — %d HELD"), C.Suspects.Num()), TEXT("Who was there"), People,
			FString::Printf(TEXT("\"Patrol's holding %d people. Every name on that list was in the building tonight. One of them is why we're here.\""), C.Suspects.Num()));
	}
	Add(TEXT("THE JOB"), FString::Printf(TEXT("%d LIES ON THE RECORD"), C.Solution.RequiredBreaks), { TEXT("CATCH THEM LYING WITH EVIDENCE"), TEXT("PROFILE: UNKNOWN — LET THE EVIDENCE TELL YOU"), TEXT("RUN EVERY RECORD THAT FITS") },
		FString::Printf(TEXT("\"No hunches. %d lies on the record before you name anybody.\""), C.Solution.RequiredBreaks));
	// the last slide: the whole case at a glance, before you roll
	{
		const FHUCase& K = Current;
		FString Caller = TEXT("someone at the scene");
		if (K.Suspects.Num() > 0) Caller = K.Suspects[FMath::Abs(K.Number) % K.Suspects.Num()].Name;
		const FString What = K.Crime == TEXT("homicide") ? TEXT("HOMICIDE") : K.Crime == TEXT("assault") ? TEXT("ASSAULT") : K.Crime == TEXT("robbery") ? TEXT("ROBBERY")
			: K.Crime == TEXT("missing") ? TEXT("MISSING PERSON") : K.Crime == TEXT("arson") ? TEXT("ARSON") : K.Crime.ToUpper();
		Add(TEXT("SUMMARY — THE CASE AT A GLANCE"), What + TEXT(": ") + K.Victim, {
			FString::Printf(TEXT("WHERE / WHEN:  %s, around %s"), *K.Place, *K.Time),
			K.Crime == TEXT("homicide") || K.Crime == TEXT("assault") ? FString::Printf(TEXT("CAUSE:  %s"), *K.Weapon.ToUpper()) : FString(TEXT("CAUSE:  SEE THE SCENE")),
			FString::Printf(TEXT("HELD AT THE SCENE:  %d  ·  CALLED IN BY:  %s"), K.Suspects.Num(), *Caller.ToUpper()),
			TEXT("WHO DID IT:  UNKNOWN — WATCH WHAT THE EVIDENCE SAYS ABOUT THEM"),
			FString::Printf(TEXT("TO CLOSE IT:  %d LIES ON THE RECORD, ARREST, BOOK, SUBMIT"), K.Solution.RequiredBreaks),
		}, FString::Printf(TEXT("\"So. %s at %s, around %s. %d people held, and %s made the call. Bag everything, catch the lies, bring the right one back in the van and book them. Go.\""),
			*K.Victim, *K.Place, *K.Time, K.Suspects.Num(), *Caller));
	}
	return Out;
}

// Pick the case's offender profile (fixed per case), a decoy, and hand out signs and records.
void UHUCaseSubsystem::AssignProfiles()
{
	FRandomStream Rng(Current.Number * 131 + GetTypeHash(Current.Crime));
	if (Current.Profile != 0) TrueProfile = (EHUProfile)Current.Profile; // generated cases carry their own
	else { TArray<EHUProfile> Fits = HUProfiles::ForCrime(Current.Crime, Current.Weapon); TrueProfile = Fits[Rng.RandRange(0, Fits.Num() - 1)]; }
	TArray<EHUProfile> Others;
	for (int32 i = 1; i < (int32)EHUProfile::Count; i++) if ((EHUProfile)i != TrueProfile) Others.Add((EHUProfile)i);
	DecoyProfile = Current.DecoyProfile != 0 ? (EHUProfile)Current.DecoyProfile : Others[Rng.RandRange(0, Others.Num() - 1)];

	// evidence: the pieces that prove lies are signs of the true profile; dead ends point at the decoy
	ClueProfile.Reset(); ClueSign.Reset();
	const TArray<FString> Key = GetKeyEvidence();
	int32 TrueSign = Rng.RandRange(0, 4), DecoySign = Rng.RandRange(0, 4);
	for (const FHUClue& C : Current.Clues)
	{
		const bool bKey = Key.Contains(C.Id);
		const EHUProfile P = bKey ? TrueProfile : DecoyProfile;
		int32& S = bKey ? TrueSign : DecoySign;
		ClueProfile.Add(C.Id, P);
		// generated cases: the evidence itself is the sign (its lies were written for the profile)
		if (Current.Profile != 0)
		{
			// a dead end points at a profile only if it was written to (decoys); herrings and junk point nowhere
			if (C.Flavor && C.PointsTo == 0) ClueProfile.Remove(C.Id);
			else { if (C.Flavor) ClueProfile.Add(C.Id, (EHUProfile)C.PointsTo); ClueSign.Add(C.Id, C.Name.ToLower()); }
		}
		else ClueSign.Add(C.Id, HUProfiles::Get(P).Signs[S++ % 5]);
	}

	// records: the killer fits; so do one or two innocents; the rest fit something else or nothing
	SuspectProfile.Reset(); SuspectRecord.Reset();
	TArray<int32> Innocent;
	for (int32 i = 0; i < Current.Suspects.Num(); i++) if (Current.Suspects[i].Id != Current.Solution.Killer) Innocent.Add(i);
	for (int32 i = Innocent.Num() - 1; i > 0; i--) Innocent.Swap(i, Rng.RandRange(0, i));
	const int32 LookAlikes = PlayDifficulty == TEXT("rookie") ? 1 : 2;
	int32 TrueRecord = Rng.RandRange(0, 2);
	auto Give = [&](const FHUSuspect& S, EHUProfile P, int32 Rec)
	{
		SuspectProfile.Add(S.Id, P);
		SuspectRecord.Add(S.Id, P == EHUProfile::None ? FString(TEXT("no record")) : FString(HUProfiles::Get(P).Records[Rec % 3]));
	};
	for (const FHUSuspect& S : Current.Suspects) if (S.Id == Current.Solution.Killer) Give(S, TrueProfile, TrueRecord++);
	for (int32 n = 0; n < Innocent.Num(); n++)
	{
		const FHUSuspect& S = Current.Suspects[Innocent[n]];
		if (n < LookAlikes) Give(S, TrueProfile, TrueRecord++);
		else if (Rng.FRand() < 0.55f) Give(S, Others[Rng.RandRange(0, Others.Num() - 1)], Rng.RandRange(0, 2));
		else Give(S, EHUProfile::None, 0);
	}
}

TArray<EHUProfile> UHUCaseSubsystem::GetProfileLeads(TArray<int32>& Counts) const
{
	TMap<EHUProfile, int32> Tally;
	for (const FString& Id : Found) if (const EHUProfile* P = ClueProfile.Find(Id)) Tally.FindOrAdd(*P)++;
	for (const TPair<EHUProfile, int32>& B : BodyLeads) Tally.FindOrAdd(B.Key) += B.Value;
	Tally.ValueSort([](int32 A, int32 B) { return A > B; });
	TArray<EHUProfile> Out; Counts.Reset();
	for (const TPair<EHUProfile, int32>& T : Tally) { Out.Add(T.Key); Counts.Add(T.Value); }
	return Out;
}

FString UHUCaseSubsystem::DescribeProfileLeads() const
{
	TArray<int32> Counts;
	const TArray<EHUProfile> Leads = GetProfileLeads(Counts);
	if (Leads.Num() == 0) return TEXT("PROFILE: unknown — bag evidence to find out who you're dealing with");
	FString S = TEXT("PROFILE LEADS:  ");
	for (int32 i = 0; i < Leads.Num(); i++) S += FString::Printf(TEXT("%s%s ×%d"), i ? TEXT("   ·   ") : TEXT(""), HUProfiles::Name(Leads[i]), Counts[i]);
	return S;
}

TArray<FString> UHUCaseSubsystem::GetKeyEvidence() const
{
	TArray<FString> Out;
	for (const FHUSuspect& S : Current.Suspects)
		for (const FHUStatement& St : S.Statements)
			if (!St.Truth) for (const FString& E : St.Evidence) { FHUClue C; if (GetClue(E, C)) Out.AddUnique(E); }
	return Out;
}

TArray<FString> UHUCaseSubsystem::GetKillerEvidence() const
{
	TArray<FString> Out;
	FHUSuspect K;
	if (!GetSuspect(Current.Solution.Killer, K)) return Out;
	for (const FHUStatement& St : K.Statements)
		if (!St.Truth) for (const FString& E : St.Evidence) { FHUClue C; if (GetClue(E, C)) Out.AddUnique(E); }
	return Out;
}

TArray<FString> UHUCaseSubsystem::GetDeadEnds() const
{
	const TArray<FString> Key = GetKeyEvidence();
	TArray<FString> Out;
	for (const FHUClue& C : Current.Clues) if (!Key.Contains(C.Id)) Out.Add(C.Id);
	return Out;
}

void UHUCaseSubsystem::GetRules(int32& DeadEnds, int32& HiddenUnder, int32& LiesNeeded) const
{
	// rookie: fewer red herrings, one thing under furniture, 2 lies to accuse
	// detective: every dead end in the case, two under furniture, 3 lies
	// veteran: every dead end, three under furniture, 4 lies
	if (PlayDifficulty == TEXT("rookie")) { DeadEnds = 3; HiddenUnder = 1; LiesNeeded = 2; }
	else if (PlayDifficulty == TEXT("veteran")) { DeadEnds = 8; HiddenUnder = 3; LiesNeeded = 4; }
	else { DeadEnds = 5; HiddenUnder = 2; LiesNeeded = 3; }
}

void UHUCaseSubsystem::GetEvidenceCount(int32& Bagged, int32& Placed, int32& KeyBagged, int32& KeyPlaced) const
{
	const TArray<FString> Key = GetKeyEvidence();
	Bagged = Placed = KeyBagged = KeyPlaced = 0;
	for (const FString& Id : InScene)
	{
		const bool bKey = Key.Contains(Id), bGot = Found.Contains(Id);
		Placed++; if (bGot) Bagged++;
		if (bKey) { KeyPlaced++; if (bGot) KeyBagged++; }
	}
}

FVector UHUCaseSubsystem::MapToWorld(int32 X, int32 Y, FVector Origin, float Scale)
{
	return Origin + FVector(Y * Scale, X * Scale, 0.f);
}

void UHUCaseSubsystem::Build911Call(TArray<FString>& Speakers, TArray<FString>& Lines) const
{
	Speakers.Reset(); Lines.Reset();
	const FHUCase& C = Current;
	if (C.Suspects.Num() == 0) return;
	// whoever called it in - one of the people held at the scene (it could be anyone... even them)
	const FHUSuspect& Caller = C.Suspects[FMath::Abs(C.Number) % C.Suspects.Num()];
	FString First = Caller.Name; int32 Space; if (First.FindChar(' ', Space)) First = First.Left(Space);
	auto D = [&](const FString& L) { Speakers.Add(TEXT("DISPATCH")); Lines.Add(L); };
	auto K = [&](const FString& L) { Speakers.Add(Caller.Name.ToUpper()); Lines.Add(L); };
	const FString Place = C.Place.Replace(TEXT("THE "), TEXT("the ")).ToLower();
	D(TEXT("Nine one one, what is your emergency?"));
	if (C.Crime == TEXT("assault"))
	{
		K(FString::Printf(TEXT("Somebody's been beaten. It's %s. There's blood everywhere. They're breathing, but they won't wake up."), *C.Victim));
		D(TEXT("Okay. Stay with me. Where are you?"));
	}
	else if (C.Crime == TEXT("robbery"))
	{
		K(FString::Printf(TEXT("We've been robbed. The office safe is wide open, and %s is on the floor. They hit their head, I think."), *C.Victim));
		D(TEXT("Is the person who did this still there?"));
		K(TEXT("I don't know. Nobody's left. Nobody's said anything."));
		D(TEXT("What's the address?"));
	}
	else if (C.Crime == TEXT("missing"))
	{
		K(FString::Printf(TEXT("%s is gone. They were right here, and now they're just gone. Their coat's still here. Their bag."), *C.Victim));
		D(TEXT("When did you last see them?"));
		K(FString::Printf(TEXT("Around %s. We've looked everywhere."), *C.Time));
		D(TEXT("Where are you calling from?"));
	}
	else if (C.Crime == TEXT("arson"))
	{
		K(FString::Printf(TEXT("Fire. There was a fire in the back office. We pulled %s out, they're coughing, they can't stand up."), *C.Victim));
		D(TEXT("Is the fire out?"));
		K(TEXT("Yes. Somebody got it with the extinguisher. It smells like gasoline in here."));
		D(TEXT("Where are you?"));
	}
	else
	{
		K(FString::Printf(TEXT("There's... oh God. There's somebody dead. It's %s. I think they're dead. There's so much blood."), *C.Victim));
		D(TEXT("Okay. Are you safe right now? Where are you?"));
	}
	K(FString::Printf(TEXT("%s. Please, hurry."), *Place));
	D(TEXT("What's your name?"));
	K(FString::Printf(TEXT("%s. It's %s. Everybody's still here. Nobody's left."), *First, *Caller.Name));
	D(TEXT("Officers are on their way. Don't touch anything, and don't let anyone leave."));
	K(TEXT("Okay. Okay. Please hurry."));
}
void UHUCaseSubsystem::ResolveArrests()
{
	if (!bCaseActive) return;
	const FHUCase& C = Current;
	FHUSuspect K; GetSuspect(C.Solution.Killer, K);
	const bool bGotThem = Arrested.Contains(C.Solution.Killer);
	int32 Wrong = 0;
	for (const FString& Id : Arrested) if (Id != C.Solution.Killer) Wrong++;
	int32 Pay = 0;
	FString V;
	if (Arrested.Num() == 0) V = FString::Printf(TEXT("CASE LEFT OPEN — nobody came back in the van. %s is still out there."), K.Name.IsEmpty() ? TEXT("Whoever did it") : *K.Name);
	else if (bGotThem && (Breaks >= C.Solution.RequiredBreaks || Confessed == C.Solution.Killer)) { Pay = 250; V = FString::Printf(TEXT("CASE CLOSED — %s is booked, and the lies on the record make it stick."), *K.Name); }
	else if (bGotThem) { Pay = 150; V = FString::Printf(TEXT("CASE CLOSED (THIN) — you brought in %s, the right one. With only %d of %d lies on the record, the DA's nervous."), *K.Name, Breaks, C.Solution.RequiredBreaks); }
	else V = FString::Printf(TEXT("WRONG ARREST — the one who did it walked. It was %s."), *K.Name);
	if (Wrong > 0) { Pay -= 75 * Wrong; V += FString::Printf(TEXT("  %d innocent %s booked: −$%d."), Wrong, Wrong == 1 ? TEXT("person") : TEXT("people"), 75 * Wrong); }
	if (bGotThem) CasesSolved++;
	AddCash(Pay); // never goes below zero
	if (Pay > 0) V += FString::Printf(TEXT("  Pay: +$%d."), Pay);
	LastVerdict = V;
	Arrested.Reset();
	BookingStep.Reset();
	bAwaitingCaptain = false;
	bCaseActive = false;
}
// ---------- interrogation ----------
FString UHUCaseSubsystem::Confront(const FString& Id, int32 Statement, const FString& ClueId)
{
	FHUSuspect S;
	if (!GetSuspect(Id, S) || Lawyered.Contains(Id)) return TEXT("\"I'm not saying another word without my lawyer.\"");
	const EHUChallengeResult R = Challenge(Id, Statement, ClueId);
	if (R == EHUChallengeResult::AlreadyDone) return TEXT("\"We've been over that. You already know.\"");
	if (R == EHUChallengeResult::Broken)
	{
		AddPressure(Id, 35.f);
		const FString Line = S.Statements.IsValidIndex(Statement) && !S.Statements[Statement].OnBreak.IsEmpty() ? S.Statements[Statement].OnBreak : FString(TEXT("...Okay. Okay, that's not exactly how it went."));
		return CheckBreaking(Id, TEXT("\"") + Line + TEXT("\""));
	}
	// the wrong evidence: they get their confidence back, and three misses ends the interview
	AddPressure(Id, -15.f);
	const int32 M = ++Misses.FindOrAdd(Id);
	if (M >= 3) { Lawyered.Add(Id); return TEXT("\"We're done here. I want a lawyer.\"  (They won't talk any more this case.)"); }
	return TEXT("\"") + FString(HUInterrogation::WrongLine(M + Statement)) + TEXT("\"");
}

FString UHUCaseSubsystem::Press(const FString& Id, int32 Statement)
{
	if (Lawyered.Contains(Id)) return TEXT("\"Lawyer.\"");
	const int32 N = PressCount.FindOrAdd(Id + FString::Printf(TEXT("#%d"), Statement))++;
	AddPressure(Id, 6.f / (1 + N)); // pushing the same point gets less and less
	return CheckBreaking(Id, TEXT("\"") + FString(HUInterrogation::PressLine(MoodOf(Id), N + Statement)) + TEXT("\""));
}

FString UHUCaseSubsystem::RaiseRecord(const FString& Id)
{
	if (Lawyered.Contains(Id)) return TEXT("\"Lawyer.\"");
	const FString* Rec = SuspectRecord.Find(Id);
	const EHUProfile* P = SuspectProfile.Find(Id);
	if (RecordRaised.Contains(Id)) return TEXT("\"You already threw that in my face.\"");
	RecordRaised.Add(Id);
	if (P && *P == TrueProfile && *P != EHUProfile::None)
	{
		AddPressure(Id, 12.f);
		return CheckBreaking(Id, FString::Printf(TEXT("\"That - %s - that was a long time ago. It's got nothing to do with tonight.\""), Rec ? **Rec : TEXT("that")));
	}
	return TEXT("\"My record? Go ahead and read it. It won't tell you anything about tonight.\"");
}

// at full pressure: the one who did it confesses (if enough of their lies are on the record); anyone else breaks down
FString UHUCaseSubsystem::CheckBreaking(const FString& Id, const FString& Line)
{
	if (GetPressure(Id) < 100.f || !Confessed.IsEmpty()) return Line;
	FHUSuspect S; GetSuspect(Id, S);
	if (Id == Current.Solution.Killer)
	{
		int32 Theirs = 0;
		for (int32 i = 0; i < S.Statements.Num(); i++) if (!S.Statements[i].Truth && IsBroken(Id, i)) Theirs++;
		if (Theirs >= 2)
		{
			Confessed = Id;
			return Line + FString::Printf(TEXT("  ...  \"Fine. FINE. It was me. I did it - %s. I didn't... it wasn't supposed to go like that.\"  (CONFESSION ON THE RECORD)"), *Current.Victim);
		}
		return Line + TEXT("  (They're close to breaking - but you need more of their lies on the record.)");
	}
	return Line + TEXT("  ...  \"All right! Yes, I lied about that. But I didn't do this. I swear to God I didn't do this.\"");
}