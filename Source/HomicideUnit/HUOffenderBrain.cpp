#include "HUOffenderBrain.h"
#include "HUCaseTypes.h"
#include "HUProfiles.h"

namespace
{
	int32 Minutes(const FString& Clock)
	{
		FString H, Rest; if (!Clock.Split(TEXT(":"), &H, &Rest)) return 22 * 60 + 30;
		int32 Hr = FCString::Atoi(*H) % 12; const int32 Mi = FCString::Atoi(*Rest.Left(2));
		if (Rest.Contains(TEXT("PM"))) Hr += 12;
		return Hr * 60 + Mi;
	}
	FString Clock(int32 M)
	{
		M = ((M % 1440) + 1440) % 1440;
		const int32 H = M / 60, Mi = M % 60;
		return FString::Printf(TEXT("%d:%02d %s"), H % 12 == 0 ? 12 : H % 12, Mi, H < 12 ? TEXT("AM") : TEXT("PM"));
	}
	template<typename T> const T& Pick(const TArray<T>& A, FRandomStream& R) { return A[R.RandRange(0, A.Num() - 1)]; }
	FString F(const TCHAR* Fmt, const TMap<FString, FString>& V)
	{
		FString S = Fmt;
		for (const auto& KV : V) S.ReplaceInline(*KV.Key, *KV.Value);
		return S;
	}
	struct FSlip { const TCHAR* What; const TCHAR* Trace; int32 At; }; // At: minutes from the killing
}

const TCHAR* HUOffender::TypeName(EType T)
{
	switch (T) { case EType::Domestic: return TEXT("DOMESTIC"); case EType::Revenge: return TEXT("REVENGE"); default: return TEXT("?"); }
}

HUOffender::FSpots HUOffender::SpotsFor(const FString& Place)
{
	FSpots S;
	if (Place.ToUpper().Contains(TEXT("HOTEL")))
	{
		// the Grand Meridian: lobby and bar, two guest floors, a basement, the fire escape and the roof
		S.Entries = { TEXT("the front doors through the lobby"), TEXT("the basement service door off the alley"), TEXT("the fire escape to the second floor"), TEXT("the roof door from the building next door") };
		S.KillSpots = { TEXT("the victim's room on the second floor"), TEXT("a guest room on the third floor"), TEXT("the bar after last call"), TEXT("the basement laundry"), TEXT("the roof") };
		S.Exits = { TEXT("the front doors through the lobby"), TEXT("the basement service door"), TEXT("down the fire escape into the alley"), TEXT("the stairs to the bar, blending in") };
		S.Dumps = { TEXT("the basement furnace"), TEXT("the laundry chute"), TEXT("the alley dumpster under the fire escape"), TEXT("the roof water tank"), TEXT("their own coat, carried out") };
		S.Cameras = { TEXT("the lobby"), TEXT("the bar"), TEXT("the second-floor hallway"), TEXT("the basement service door") };
		return S;
	}
	S.Entries = { TEXT("the front door"), TEXT("the back door"), TEXT("a side window") };
	S.KillSpots = { TEXT("the back office"), TEXT("the storeroom"), TEXT("the main room after closing") };
	S.Exits = { TEXT("the front door"), TEXT("the back door into the alley") };
	S.Dumps = { TEXT("a dumpster out back"), TEXT("the river"), TEXT("their own car") };
	S.Cameras = { TEXT("the front door"), TEXT("the main room") };
	return S;
}

HUOffender::FPlan HUOffender::Make(const FHUCase& Case)
{
	FPlan P;
	const FHUSuspect* K = Case.Suspects.FindByPredicate([&](const FHUSuspect& S) { return S.Id == Case.Solution.Killer; });
	if (!K) return P;
	FRandomStream R(Case.MapSeed * 31 + Case.Number * 7919 + 17);
	const FSpots Sp = SpotsFor(Case.Place);

	// 1. WHO: the case's offender profile decides the type where it can; the seed decides the rest
	const EHUProfile Prof = (EHUProfile)Case.Profile;
	if (Prof == EHUProfile::Passion) P.Type = EType::Domestic;
	else if (Prof == EHUProfile::Stalker || Prof == EHUProfile::Poisoner || Prof == EHUProfile::SerialKiller) P.Type = EType::Revenge;
	else P.Type = (EType)R.RandRange(0, (int32)EType::Count - 1);
	const bool bDom = P.Type == EType::Domestic;
	P.Care = bDom ? R.RandRange(15, 45) : R.RandRange(60, 90);
	P.bValid = true;
	P.KillerId = K->Id; P.KillerName = K->Name; P.VictimName = Case.Victim;
	P.Weapon = Case.Weapon;

	// 2. PLAN
	if (bDom)
	{
		P.Relationship = Pick<FString>({ TEXT("married to the victim"), TEXT("the victim's partner of six years"), TEXT("the victim's brother-in-law"), TEXT("the victim's business partner and old friend") }, R);
		P.Motive = Pick<FString>({ TEXT("found out about the affair tonight"), TEXT("the victim was leaving and taking the money"), TEXT("an old fight about the will boiled over"), TEXT("the victim laughed at them in front of the whole bar") }, R);
		P.WeaponSource = P.Weapon.Contains(TEXT("revolver")) ? TEXT("the victim's own, from the nightstand drawer")
			: P.Weapon.Contains(TEXT("drink")) ? TEXT("sleeping pills from the victim's toiletry bag, crushed into a drink on impulse")
			: TEXT("grabbed on the spot - it was just there");
	}
	else
	{
		P.Relationship = Pick<FString>({ TEXT("barely knew the victim - or so it looks"), TEXT("an old employee the victim fired"), TEXT("a former friend nobody's seen with the victim in years") }, R);
		P.Motive = Pick<FString>({ TEXT("the victim's testimony put them away for four years"), TEXT("the victim ruined their business and walked away clean"), TEXT("the victim was driving the night their brother died, and was never charged"), TEXT("the victim took everything in a crooked deal") }, R);
		P.WeaponSource = P.Weapon.Contains(TEXT("drink")) ? TEXT("rat poison, bought two weeks ago and measured out at home")
			: P.Weapon.Contains(TEXT("revolver")) ? TEXT("bought off the books from a pawnbroker three weeks ago")
			: TEXT("bought at a hardware store across town and brought in a bag");
	}
	P.Entry = bDom ? Sp.Entries[0] : Pick(Sp.Entries, R);           // domestic: already inside, came in the normal way
	P.KillSpot = Pick(Sp.KillSpots, R);
	P.Exit = Pick(Sp.Exits, R);
	P.WeaponDump = Pick(Sp.Dumps, R);

	// the alibi: domestic make one up afterwards; revenge built it in advance and leaned on someone
	TArray<const FHUSuspect*> Others; for (const FHUSuspect& S : Case.Suspects) if (S.Id != K->Id) Others.Add(&S);
	if (!bDom && Others.Num() && R.FRand() < 0.7f) { const FHUSuspect* L = Others[R.RandRange(0, Others.Num() - 1)]; P.LeanedOnId = L->Id; P.LeanedOnName = L->Name; }
	P.AlibiStory = bDom ? Pick<FString>({ TEXT("\"I was at the bar the whole night.\""), TEXT("\"I was asleep - I didn't hear a thing.\""), TEXT("\"I went out for cigarettes and came back to find them.\"") }, R)
		: P.LeanedOnName.IsEmpty() ? TEXT("\"I was across town at the pictures. I've got the ticket stub.\"")
		: FString::Printf(TEXT("\"I was with %s all evening. Ask them.\""), *P.LeanedOnName);

	// 3. THE NIGHT
	const int32 T = Minutes(Case.Time);
	const TMap<FString, FString> V = { { TEXT("{V}"), P.VictimName }, { TEXT("{K}"), P.KillerName }, { TEXT("{W}"), P.Weapon }, { TEXT("{E}"), P.Entry }, { TEXT("{S}"), P.KillSpot }, { TEXT("{X}"), P.Exit }, { TEXT("{D}"), P.WeaponDump } };
	auto Step = [&](int32 Day, int32 Min, const FString& Where, const TCHAR* What, const FString& Trace = FString())
	{ FStep S; S.Day = Day; S.Minute = Min; S.Where = Where; S.What = F(What, V); S.Trace = Trace; P.Timeline.Add(S); };
	if (bDom)
	{
		Step(0, T - 120, TEXT("the bar"), TEXT("{K} and {V} drink together. It's fine, at first."));
		Step(0, T - 45, TEXT("the bar"), TEXT("The fight starts. Voices raised; {V} storms off."), TEXT("the bartender remembers the argument"));
		Step(0, T - 10, P.KillSpot, TEXT("{K} follows {V} to finish it - {S}."));
		Step(0, T, P.KillSpot, TEXT("{K} kills {V} with {W}."));
		Step(0, T + 4, P.KillSpot, TEXT("Panic. {K} wipes what they touched - not everything."));
		Step(0, T + 15, P.WeaponDump, TEXT("{K} gets rid of {W}: {D}."));
		Step(0, T + 30, P.Exit, TEXT("{K} leaves by {X} and starts telling everyone their story."));
	}
	else
	{
		Step(-21, 14 * 60, TEXT("home"), TEXT("{K} decides. Starts reading up on {V}'s routine."), TEXT("web searches about the victim, weeks old"));
		Step(-14, 11 * 60, TEXT("town"), *FString::Printf(TEXT("{K} gets the weapon - %s."), *P.WeaponSource));
		Step(-3, 21 * 60, Case.Place.ToLower(), TEXT("{K} scouts the place: the way in ({E}) and out ({X})."));
		if (!P.LeanedOnName.IsEmpty()) Step(-1, 19 * 60, TEXT("a diner"), *FString::Printf(TEXT("{K} asks %s to say they spent tonight together. Calls it a favour."), *P.LeanedOnName));
		Step(0, T - 40, P.Entry, TEXT("{K} comes in by {E}."));
		Step(0, T - 25, P.KillSpot, TEXT("{K} lies in wait for {V} - {S}."));
		Step(0, T, P.KillSpot, TEXT("{K} kills {V} with {W}."));
		Step(0, T + 6, P.Exit, TEXT("{K} leaves by {X}."));
		Step(0, T + 20, P.WeaponDump, TEXT("{K} disposes of {W}: {D}."));
		Step(0, T + 45, TEXT("across town"), TEXT("{K} turns up where the alibi says they were, and makes sure they're seen."));
	}

	// 4. SLIPS: careless types slip more, everyone slips at least three times (every case stays solvable)
	TArray<FSlip> Pool = bDom
		? TArray<FSlip>{
			{ TEXT("Cut their hand in the struggle"), TEXT("a blood drop that isn't the victim's, near the body"), 1 },
			{ TEXT("Walked past a camera on the way out"), TEXT("tape: the killer leaving minutes after the time of death"), 28 },
			{ TEXT("Called the victim's phone afterwards, to look worried"), TEXT("call log: a call to a phone they knew was dead"), 50 },
			{ TEXT("Kept the victim's key without thinking"), TEXT("the victim's room key in the killer's pocket"), 5 },
			{ TEXT("A guest heard them through the wall"), TEXT("a witness: raised voices, then a thud, at the time of death"), 0 },
			{ TEXT("Changed shirts and stuffed the old one in a bag"), TEXT("a stained shirt in the killer's bag"), 12 },
			{ TEXT("Their story doesn't match the bar tab"), TEXT("the bar tab closed an hour before they say they left"), 35 } }
		: TArray<FSlip>{
			{ TEXT("Searched the weapon and the victim from their own phone"), TEXT("web history: weeks of searches about the victim and the weapon"), -30000 },
			{ TEXT("Paid for something with their own card"), TEXT("a receipt in their name for the weapon or what went with it"), -20000 },
			{ TEXT("Their phone pinged the tower by the scene"), TEXT("cell pings: in the area at the time of death, not across town"), 0 },
			{ TEXT("A camera caught them coming in"), TEXT("tape: someone matching the killer at the way in"), -40 },
			{ TEXT("Left a fibre from their gloves on the way out"), TEXT("a glove fibre on the exit route that matches their gloves"), 6 },
			{ TEXT("The friend they leaned on is a bad liar"), TEXT("the alibi witness's story falls apart under pressure"), 0 },
			{ TEXT("Kept a souvenir"), TEXT("something of the victim's, hidden in the killer's things"), 8 } };
	if (P.LeanedOnName.IsEmpty() && !bDom) Pool.RemoveAt(5);
	for (int32 i = Pool.Num() - 1; i > 0; i--) Pool.Swap(i, R.RandRange(0, i));
	const int32 Extra = FMath::RoundToInt((100 - P.Care) / 30.f);   // careful = 3, careless = up to 6
	const int32 NSlips = FMath::Min(3 + Extra, Pool.Num());
	for (int32 i = 0; i < NSlips; i++)
	{
		const FSlip& S = Pool[i];
		P.Slips.Add(S.What);
		FStep St; St.Trace = S.Trace; St.What = FString::Printf(TEXT("SLIP: %s."), S.What);
		if (S.At <= -1440) { St.Day = S.At / 1440; St.Minute = 15 * 60; St.Where = TEXT("in the weeks before"); }
		else { St.Minute = T + S.At; St.Where = S.At < 0 ? P.Entry : S.At <= 1 ? P.KillSpot : S.At <= 6 ? P.Exit : TEXT("after"); }
		P.Timeline.Add(St);
	}
	P.Timeline.StableSort([](const FStep& A, const FStep& B) { return A.Day != B.Day ? A.Day < B.Day : A.Minute < B.Minute; });

	// 5. COVER-UP
	P.CoverUp.Add(FString::Printf(TEXT("Tells the squad: %s"), *P.AlibiStory));
	P.AlibiTruth = FString::Printf(TEXT("Was at %s at %s."), *P.KillSpot, *Clock(T));
	P.CoverUp.Add(FString::Printf(TEXT("The weapon went into %s."), *P.WeaponDump));
	if (!bDom) P.CoverUp.Add(TEXT("Deleted the texts and calls from the last month - the phone company still has the call records."));
	else P.CoverUp.Add(TEXT("Wiped the handle and the door; forgot the glass."));
	if (!P.LeanedOnName.IsEmpty()) P.CoverUp.Add(FString::Printf(TEXT("%s will back the alibi - until they're pushed."), *P.LeanedOnName));
	return P;
}

TArray<FString> HUOffender::Describe(const FPlan& P)
{
	TArray<FString> L;
	if (!P.bValid) { L.Add(TEXT("No plan: the case has no culprit on file.")); return L; }
	L.Add(FString::Printf(TEXT("TYPE: %s    CARE: %d/100"), TypeName(P.Type), P.Care));
	L.Add(FString::Printf(TEXT("OFFENDER: %s, %s"), *P.KillerName, *P.Relationship));
	L.Add(FString::Printf(TEXT("VICTIM: %s"), *P.VictimName));
	L.Add(FString::Printf(TEXT("MOTIVE: %s"), *P.Motive));
	L.Add(FString::Printf(TEXT("WEAPON: %s - %s"), *P.Weapon, *P.WeaponSource));
	L.Add(FString::Printf(TEXT("IN: %s    SPOT: %s"), *P.Entry, *P.KillSpot));
	L.Add(FString::Printf(TEXT("OUT: %s    WEAPON TO: %s"), *P.Exit, *P.WeaponDump));
	L.Add(TEXT(""));
	L.Add(TEXT("---- WHAT REALLY HAPPENED ----"));
	for (const FStep& S : P.Timeline)
	{
		const FString When = S.Day < 0 ? FString::Printf(TEXT("%d days before"), -S.Day) : Clock(S.Minute);
		L.Add(FString::Printf(TEXT("%-14s  %s  (%s)"), *When, *S.What, *S.Where));
		if (!S.Trace.IsEmpty()) L.Add(TEXT("                  leaves: ") + S.Trace);
	}
	L.Add(TEXT(""));
	L.Add(TEXT("---- COVER-UP ----"));
	for (const FString& C : P.CoverUp) L.Add(TEXT("- ") + C);
	L.Add(TEXT("- The truth: ") + P.AlibiTruth);
	return L;
}

#if !UE_BUILD_SHIPPING
#include "HUCaseGen.h"
#include "HAL/IConsoleManager.h"
// HU.CrimePlans [count] [seed]: plan that many random cases and print every plan to the log (a quick sanity check
// that the brain makes sense and that every plan leaves at least three traces)
static FAutoConsoleCommand GHUCrimePlans(TEXT("HU.CrimePlans"), TEXT("Print the offender brain's plans for N random cases"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 5, Seed = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1234;
		const double T0 = FPlatformTime::Seconds();
		const TArray<FHUCase> Cases = HUCaseGen::MakeCases(1, FMath::Max(1, N), Seed);
		int32 Bad = 0;
		TArray<HUOffender::FPlan> Plans;
		for (const FHUCase& C : Cases) Plans.Add(HUOffender::Make(C));
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		for (int32 i = 0; i < Plans.Num(); i++)
		{
			int32 Traces = 0; for (const HUOffender::FStep& S : Plans[i].Timeline) if (!S.Trace.IsEmpty()) Traces++;
			if (!Plans[i].bValid || Plans[i].Slips.Num() < 3) Bad++;
			UE_LOG(LogTemp, Display, TEXT("==== CRIME PLAN case %d at %s (%d traces) ===="), Cases[i].Number, *Cases[i].Place, Traces);
			for (const FString& L : HUOffender::Describe(Plans[i])) UE_LOG(LogTemp, Display, TEXT("  %s"), *L);
		}
		UE_LOG(LogTemp, Display, TEXT("HU.CrimePlans: %d cases generated and planned in %.2f ms, %d unsolvable"), Plans.Num(), Ms, Bad);
	}));
#endif
