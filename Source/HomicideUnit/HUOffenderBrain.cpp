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
	// Breaks: 0 none, 1 the means lie, 2 the alibi lie.  At: minutes from the killing.  Clue: what's bagged at the scene.
	struct FSlip { const TCHAR* Tag; HUOffender::ESlipKind Kind; int32 Breaks; const TCHAR* What; const TCHAR* Trace; int32 At; const TCHAR* ClueName; const TCHAR* ClueDetail; };
}

bool HUOffender::FPlan::Has(const TCHAR* Tag) const { return SlipInfo.ContainsByPredicate([&](const FSlipInfo& S) { return S.Tag == Tag; }); }

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
		S.Office = TEXT("the manager's office behind the front desk");
		S.Public = { TEXT("the bar"), TEXT("the lobby") };
		S.Stashes = { TEXT("behind a ceiling tile in a third-floor room"), TEXT("in the bar's ice chest"), TEXT("in the roof water tank"), TEXT("in a laundry cart in the basement") };
		return S;
	}
	S.Entries = { TEXT("the front door"), TEXT("the back door"), TEXT("a side window") };
	S.KillSpots = { TEXT("the back office"), TEXT("the storeroom"), TEXT("the main room after closing") };
	S.Exits = { TEXT("the front door"), TEXT("the back door into the alley") };
	S.Dumps = { TEXT("a dumpster out back"), TEXT("the trunk of their own car, parked outside"), TEXT("a storm drain in the alley") };
	S.Cameras = { TEXT("the front door"), TEXT("the main room") };
	S.Office = TEXT("the back office");
	S.Public = { TEXT("the main room") };
	S.Stashes = { TEXT("behind a loose ceiling tile"), TEXT("under the floorboards in the storeroom"), TEXT("inside a coat in the cloakroom") };
	return S;
}

HUOffender::FPlan HUOffender::Make(const FHUCase& Case)
{
	FPlan P;
	const FHUSuspect* K = Case.Suspects.FindByPredicate([&](const FHUSuspect& S) { return S.Id == Case.Solution.Killer; });
	if (!K) return P;
	if (Case.Victim.Contains(TEXT(", age "))) return P; // a missing child: not planned yet (the profiles behind those need their own types)
	const FString Crime = Case.Crime.IsEmpty() ? FString(TEXT("homicide")) : Case.Crime;
	const bool bRob = Crime == TEXT("robbery"), bFire = Crime == TEXT("arson"), bGone = Crime == TEXT("missing"), bHurt = Crime == TEXT("assault");
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
		P.Motive = bRob ? Pick<FString>({ TEXT("owes a bookie, and the victim wouldn't lend a cent"), TEXT("the victim cut them out of the business money"), TEXT("knew the safe combination, and a debt was due Friday") }, R)
			: bFire ? Pick<FString>({ TEXT("wanted the insurance on a business that was going under"), TEXT("the victim was about to find the books they'd cooked - the fire was for the books") }, R)
			: bGone ? Pick<FString>({ TEXT("the victim was leaving them, tonight"), TEXT("the victim found out about the money and was going to the police") }, R)
			: Pick<FString>({ TEXT("found out about the affair tonight"), TEXT("the victim was leaving and taking the money"), TEXT("an old fight about the will boiled over"), TEXT("the victim laughed at them in front of the whole bar") }, R);
		P.WeaponSource = P.Weapon.Contains(TEXT("revolver")) ? TEXT("the victim's own, from the nightstand drawer")
			: P.Weapon.Contains(TEXT("drink")) ? TEXT("sleeping pills from the victim's toiletry bag, crushed into a drink on impulse")
			: TEXT("grabbed on the spot - it was just there");
	}
	else
	{
		P.Relationship = Pick<FString>({ TEXT("barely knew the victim - or so it looks"), TEXT("an old employee the victim fired"), TEXT("a former friend nobody's seen with the victim in years") }, R);
		P.Motive = bRob ? Pick<FString>({ TEXT("the victim's company stiffed them for a year's wages - this was collecting"), TEXT("the victim cheated them out of their half of the business") }, R)
			: bFire ? Pick<FString>({ TEXT("the victim's place put their family business under; now it burns"), TEXT("the victim got them fired and blacklisted") }, R)
			: Pick<FString>({ TEXT("the victim's testimony put them away for four years"), TEXT("the victim ruined their business and walked away clean"), TEXT("the victim was driving the night their brother died, and was never charged"), TEXT("the victim took everything in a crooked deal") }, R);
		P.WeaponSource = P.Weapon.Contains(TEXT("drink")) ? TEXT("rat poison, bought two weeks ago and measured out at home")
			: P.Weapon.Contains(TEXT("revolver")) ? TEXT("bought off the books from a pawnbroker three weeks ago")
			: TEXT("bought at a hardware store across town and brought in a bag");
	}
	P.Entry = bDom ? Sp.Entries[0] : Pick(Sp.Entries, R);           // domestic: already inside, came in the normal way
	P.KillSpot = (bRob || bFire) ? Sp.Office : Pick(Sp.KillSpots, R);
	P.Exit = Pick(Sp.Exits, R);
	P.WeaponDump = Pick(Sp.Dumps, R);
	if (bRob) P.Stash = Pick(Sp.Stashes, R);
	P.Public = Pick(Sp.Public, R);

	// the alibi: domestic make one up afterwards; revenge built it in advance and leaned on someone
	TArray<const FHUSuspect*> Others; for (const FHUSuspect& S : Case.Suspects) if (S.Id != K->Id) Others.Add(&S);
	if (!bDom && Others.Num() && R.FRand() < 0.7f) { const FHUSuspect* L = Others[R.RandRange(0, Others.Num() - 1)]; P.LeanedOnId = L->Id; P.LeanedOnName = L->Name; }
	P.AlibiStory = bDom ? Pick<FString>({ FString::Printf(TEXT("\"I was in %s the whole night.\""), *P.Public), FString(TEXT("\"I was asleep upstairs - I didn't hear a thing.\"")), FString(TEXT("\"I stepped out for cigarettes and came back to all this.\"")) }, R)
		: P.LeanedOnName.IsEmpty() ? FString::Printf(TEXT("\"I was in %s all evening. I checked my coat at eight and never left.\""), *P.Public)
		: FString::Printf(TEXT("\"I was with %s all evening. Ask them.\""), *P.LeanedOnName);

	// 3. THE NIGHT
	const int32 T = Minutes(Case.Time);
	const TCHAR* Act = bRob ? TEXT("{K} empties the safe and knocks {V} cold with {W}.")
		: bFire ? TEXT("{K} hits {V} with {W} and sets the fire, with {V} still inside.")
		: bGone ? TEXT("{K} overpowers {V} with {W}.")
		: bHurt ? TEXT("{K} attacks {V} with {W} and leaves them for dead.")
		: TEXT("{K} kills {V} with {W}.");
	const TMap<FString, FString> V = { { TEXT("{V}"), P.VictimName }, { TEXT("{M}"), P.Stash }, { TEXT("{U}"), P.Public }, { TEXT("{K}"), P.KillerName }, { TEXT("{W}"), P.Weapon }, { TEXT("{E}"), P.Entry }, { TEXT("{S}"), P.KillSpot }, { TEXT("{X}"), P.Exit }, { TEXT("{D}"), P.WeaponDump } };
	auto Step = [&](int32 Day, int32 Min, const FString& Where, const TCHAR* What, const FString& Trace = FString())
	{ FStep S; S.Day = Day; S.Minute = Min; S.Where = Where; S.What = F(What, V); S.Trace = Trace; P.Timeline.Add(S); };
	if (bDom)
	{
		Step(0, T - 120, TEXT("the bar"), TEXT("{K} and {V} drink together. It's fine, at first."));
		Step(0, T - 45, TEXT("the bar"), TEXT("The fight starts. Voices raised; {V} storms off."), TEXT("the bartender remembers the argument"));
		Step(0, T - 10, P.KillSpot, TEXT("{K} follows {V} to finish it - {S}."));
		Step(0, T, P.KillSpot, Act);
		if (bGone) Step(0, T + 2, P.Exit, TEXT("{K} gets {V} out by {X}, to their car."));
		if (bRob) Step(0, T + 8, P.Stash, TEXT("Too risky to walk out with it: {K} stashes the money {M}."));
		Step(0, T + 4, P.KillSpot, TEXT("Panic. {K} wipes what they touched - not everything."));
		Step(0, T + 15, P.WeaponDump, TEXT("{K} gets rid of {W}: {D}."));
		Step(0, T + 30, P.Public, TEXT("{K} goes back to {U} and starts telling everyone their story."));
	}
	else
	{
		Step(-21, 14 * 60, TEXT("home"), TEXT("{K} decides. Starts reading up on {V}'s routine."), TEXT("web searches about the victim, weeks old"));
		Step(-14, 11 * 60, TEXT("town"), *FString::Printf(TEXT("{K} gets the weapon - %s."), *P.WeaponSource));
		Step(-3, 21 * 60, Case.Place.ToLower(), TEXT("{K} scouts the place: the way in ({E}) and out ({X})."));
		if (!P.LeanedOnName.IsEmpty()) Step(-1, 19 * 60, TEXT("a diner"), *FString::Printf(TEXT("{K} asks %s to say they spent tonight together. Calls it a favour."), *P.LeanedOnName));
		Step(0, T - 40, P.Entry, TEXT("{K} comes in by {E}."));
		Step(0, T - 25, P.KillSpot, bRob || bFire ? TEXT("{K} waits for {V} to open up {S}.") : TEXT("{K} lies in wait for {V} - {S}."));
		Step(0, T, P.KillSpot, Act);
		if (bRob) Step(0, T + 3, P.Stash, TEXT("{K} stashes the money {M}, to collect when it's quiet."));
		Step(0, T + 6, P.Exit, bGone ? TEXT("{K} takes {V} out by {X}, to their car.") : TEXT("{K} gets away from {S} by {X}."));
		Step(0, T + 20, P.WeaponDump, TEXT("{K} disposes of {W}: {D}."));
		Step(0, T + 25, P.Public, TEXT("{K} slips back into {U}, where the alibi says they were all along, and makes sure they're seen."));
	}

	// 4. SLIPS: careless types slip more, everyone slips at least three times (every case stays solvable).
	// Scene slips leave a piece of evidence that breaks one of their two plan lies (the means, the alibi);
	// tape, phone and witness slips show up on the tapes, the phone apps and in the interview room.
	using EK = ESlipKind;
	TArray<FSlip> Pool = bDom
		? TArray<FSlip>{
			{ TEXT("blood"), EK::Scene, 1, TEXT("Cut their hand in the struggle"), TEXT("a blood drop that isn't the victim's, at the scene"), 1,
				TEXT("Blood Drop"), TEXT("A drop of blood by the door that isn't {V}'s. {K} has a fresh cut across the palm.") },
			{ TEXT("camera"), EK::Tape, 0, TEXT("Walked past a camera on the way out"), TEXT("tape: the killer leaving minutes after the time of death"), 28, nullptr, nullptr },
			{ TEXT("calldead"), EK::Phone, 0, TEXT("Called the victim's phone afterwards, to look worried"), TEXT("call log: a call to a phone they knew was dead"), 50, nullptr, nullptr },
			{ TEXT("key"), EK::Scene, 2, TEXT("Kept the victim's key without thinking"), TEXT("the victim's room key in the killer's pocket"), 5,
				TEXT("Victim's Key"), TEXT("{V}'s key, found in {K}'s coat. {V} still had it when the two of them split up - {K} saw them after that.") },
			{ TEXT("witness"), EK::Scene, 2, TEXT("A guest heard them through the wall"), TEXT("a witness: raised voices, then a thud, at the time of death"), 0,
				TEXT("Noise Complaint"), TEXT("A complaint slip from the front desk, logged at {T}: raised voices, then {NOISE}. The caller describes {K}'s voice.") },
			{ TEXT("shirt"), EK::Scene, 1, TEXT("Changed shirts and stuffed the old one in a bag"), TEXT("a stained shirt in the killer's bag"), 12,
				TEXT("Stained Shirt"), TEXT("A shirt balled up in {K}'s bag, {STAIN}.") },
			{ TEXT("tab"), EK::Scene, 2, TEXT("Their story doesn't match the bar tab"), TEXT("the bar tab closed an hour before they say they left"), 35,
				TEXT("Bar Tab"), TEXT("{K}'s tab, closed out and paid well before {T}. Their story doesn't fit the time on it.") } }
		: TArray<FSlip>{
			{ TEXT("web"), EK::Phone, 0, TEXT("Searched the weapon and the victim from their own phone"), TEXT("web history: weeks of searches about the victim and the weapon"), -30000, nullptr, nullptr },
			{ TEXT("receipt"), EK::Scene, 1, TEXT("Paid for something with their own card"), TEXT("a receipt in their name for the weapon or what went with it"), -20000,
				TEXT("Card Receipt"), TEXT("A receipt in {K}'s name, paid by card three weeks ago: {W}, or what goes with it.") },
			{ TEXT("cell"), EK::Phone, 0, TEXT("Kept their phone on - it pinged the tower by the scene"), TEXT("cell pings: in the area at the time of death, not across town"), 0, nullptr, nullptr },
			{ TEXT("camin"), EK::Tape, 0, TEXT("A camera caught them coming in"), TEXT("tape: someone matching the killer at the way in"), -40, nullptr, nullptr },
			{ TEXT("fibre"), EK::Scene, 1, TEXT("Left a fibre from their gloves on the way out"), TEXT("a glove fibre on the exit route that matches their gloves"), 6,
				TEXT("Glove Fibre"), TEXT("A dark wool fibre snagged on {X}. It matches the gloves in {K}'s coat pocket.") },
			{ TEXT("friend"), EK::Witness, 0, TEXT("The friend they leaned on is a bad liar"), TEXT("the alibi witness's story falls apart under pressure"), 0, nullptr, nullptr },
			{ TEXT("souvenir"), EK::Scene, 1, TEXT("Kept a souvenir"), TEXT("something of the victim's, hidden in the killer's things"), 8,
				TEXT("Keepsake"), TEXT("{V}'s wristwatch, wrapped in a handkerchief in {K}'s things.") },
			{ TEXT("stub"), EK::Scene, 2, TEXT("Took their coat back out of the coat check"), TEXT("a coat-check ticket stamped out before the crime"), -45,
				TEXT("Coat-Check Ticket"), TEXT("{K}'s coat-check ticket: the coat went out before {T} and came back after. Somebody needed their coat - and their gloves.") },
			{ TEXT("parking"), EK::Scene, 2, TEXT("Parked close and got a ticket"), TEXT("a parking ticket two streets from the scene"), -10,
				TEXT("Parking Ticket"), TEXT("A parking ticket on {K}'s car, written two streets from {P} ten minutes before {T}.") } };
	Pool.RemoveAll([&](const FSlip& S) {
		const FString Tag = S.Tag;
		return (Tag == TEXT("friend") && P.LeanedOnName.IsEmpty()) || (Tag == TEXT("stub") && !P.LeanedOnName.IsEmpty()); });
	for (int32 i = Pool.Num() - 1; i > 0; i--) Pool.Swap(i, R.RandRange(0, i));
	// solvable: one slip that breaks the means lie and one that breaks the alibi go first
	for (int32 Need = 1; Need <= 2; Need++)
		if (const int32 At = Pool.IndexOfByPredicate([&](const FSlip& S) { return S.Breaks == Need; }); At != INDEX_NONE) Pool.Swap(Need - 1, At);
	const int32 Extra = FMath::RoundToInt((100 - P.Care) / 30.f);   // careful = 3, careless = up to 6
	const int32 NSlips = FMath::Min(3 + Extra, Pool.Num());
	P.MeansLie = bRob ? FString(TEXT("I never went near the office. I don't even know where the safe is."))
		: bFire ? FString(TEXT("I never set foot in that office tonight."))
		: bDom ? FString::Printf(TEXT("I never went near %s after the bar. Never laid a hand on them."), *P.VictimName)
		: FString::Printf(TEXT("I had nothing to do with %s. Haven't laid eyes on them in years."), *P.VictimName);
	P.AlibiLie = P.AlibiStory.TrimChar(TEXT('"'));
	for (int32 i = 0; i < NSlips; i++)
	{
		const FSlip& S = Pool[i];
		FSlipInfo Info; Info.Tag = S.Tag; Info.Kind = S.Kind; Info.Breaks = S.Breaks;
		if (S.ClueName)
		{
			const FString Stain = bFire ? FString(TEXT("reeking of lighter fluid")) : bGone ? FString::Printf(TEXT("torn at the collar, %s's hair on it"), *P.VictimName) : FString::Printf(TEXT("spotted with %s's blood"), *P.VictimName);
			const FString Noise = bFire ? TEXT("the smell of smoke") : bGone ? TEXT("a door slamming and footsteps on the stairs") : TEXT("a thud");
			Info.ClueName = S.ClueName;
			Info.ClueDetail = F(S.ClueDetail, V).Replace(TEXT("{T}"), *Case.Time).Replace(TEXT("{P}"), *Case.Place).Replace(TEXT("{STAIN}"), *Stain).Replace(TEXT("{NOISE}"), *Noise);
		}
		P.SlipInfo.Add(Info);
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
	if (bRob) P.CoverUp.Add(FString::Printf(TEXT("The money is still in the building, %s."), *P.Stash));
	if (bGone) P.CoverUp.Add(FString::Printf(TEXT("%s is alive, in the trunk of %s's car - for now."), *P.VictimName, *P.KillerName));
	P.Crime = Crime;
	if (!bDom) P.CoverUp.Add(TEXT("Deleted the texts and calls from the last month - the phone company still has the call records."));
	else P.CoverUp.Add(TEXT("Wiped the handle and the door; forgot the glass."));
	if (!P.LeanedOnName.IsEmpty()) P.CoverUp.Add(FString::Printf(TEXT("%s will back the alibi - until they're pushed."), *P.LeanedOnName));
	return P;
}

void HUOffender::ApplyToCase(FHUCase& Case)
{
	const FPlan P = Make(Case);
	if (!P.bValid) return;
	FHUSuspect* K = Case.Suspects.FindByPredicate([&](const FHUSuspect& S) { return S.Id == Case.Solution.Killer; });
	if (!K) return;
	// the means and alibi lies, and the evidence that breaks them, come from the plan; every extra scene slip of
	// the same kind is one more piece of evidence that breaks the same lie
	auto Rewrite = [&](const FString& ClueId, int32 Breaks, const FString& Lie)
	{
		FHUStatement* St = K->Statements.FindByPredicate([&](const FHUStatement& X) { return X.Evidence.Contains(ClueId); });
		FHUClue* Cl = Case.Clues.FindByPredicate([&](const FHUClue& X) { return X.Id == ClueId; });
		if (!St || !Cl) return;
		int32 n = 0;
		for (const FSlipInfo& S : P.SlipInfo)
		{
			if (S.Breaks != Breaks || S.ClueName.IsEmpty()) continue;
			if (n == 0) { Cl->Name = S.ClueName; Cl->Detail = S.ClueDetail; St->Text = Lie; }
			else
			{
				FHUClue Extra = *Cl; Extra.Id = FString::Printf(TEXT("%s_%d"), *ClueId, n); Extra.Name = S.ClueName; Extra.Detail = S.ClueDetail;
				St->Evidence.Add(Extra.Id);
				Case.Clues.Add(Extra);
				St = K->Statements.FindByPredicate([&](const FHUStatement& X) { return X.Evidence.Contains(ClueId); }); // (the add may move the array)
				Cl = Case.Clues.FindByPredicate([&](const FHUClue& X) { return X.Id == ClueId; });
			}
			n++;
		}
	};
	Rewrite(Case.Solution.Means, 1, P.MeansLie);
	Rewrite(Case.Solution.Alibi, 2, P.AlibiLie);
	// the friend they leaned on backs the alibi - and the same evidence breaks their story too
	if (FHUSuspect* L = Case.Suspects.FindByPredicate([&](const FHUSuspect& S) { return S.Id == P.LeanedOnId; }))
	{
		const FHUStatement* KA = Case.Suspects.FindByPredicate([&](const FHUSuspect& S) { return S.Id == Case.Solution.Killer; })->Statements.FindByPredicate([&](const FHUStatement& X) { return X.Evidence.Contains(Case.Solution.Alibi); });
		FHUStatement St;
		St.Text = FString::Printf(TEXT("I was with %s all evening. We never left."), *P.KillerName);
		St.Truth = false;
		St.Evidence = KA ? KA->Evidence : TArray<FString>{ Case.Solution.Alibi };
		St.OnBreak = FString::Printf(TEXT("...Alright. %s asked me to say it. Called it a favour. I didn't ask why."), *P.KillerName);
		L->Statements.Add(St);
	}
}

TArray<FString> HUOffender::Reveal(const FPlan& P, TFunctionRef<bool(const FString&)> Found, bool bTapeCaught)
{
	TArray<FString> L;
	if (!P.bValid) return L;
	L.Add(FString::Printf(TEXT("%s did it."), *P.KillerName));
	L.Add(FString::Printf(TEXT("WHO THEY WERE: %s."), *P.Relationship.Left(1).ToUpper().Append(P.Relationship.Mid(1))));
	L.Add(FString::Printf(TEXT("WHY: %s."), *P.Motive.Left(1).ToUpper().Append(P.Motive.Mid(1))));
	L.Add(P.Type == EType::Domestic ? TEXT("It wasn't planned. It happened in the heat of the moment, and they spent the rest of the night covering it up.")
		: TEXT("It was planned for weeks. They chose the night, the way in and the way out - and built their alibi in advance."));
	L.Add(TEXT(""));
	L.Add(TEXT("---- THE NIGHT ----"));
	for (const FStep& S : P.Timeline)
	{
		if (S.What.StartsWith(TEXT("SLIP:"))) continue;
		L.Add(FString::Printf(TEXT("%-14s  %s"), *(S.Day < 0 ? (S.Day == -1 ? FString(TEXT("the day before")) : FString::Printf(TEXT("%d days before"), -S.Day)) : Clock(S.Minute)), *S.What));
	}
	for (const FString& C : P.CoverUp) if (C.Contains(TEXT("money is still")) || C.Contains(TEXT("is alive"))) L.Add(TEXT("               ") + C);
	L.Add(TEXT(""));
	L.Add(TEXT("---- WHAT GAVE THEM AWAY ----"));
	int32 Got = 0, Of = 0;
	for (int32 i = 0; i < P.SlipInfo.Num() && i < P.Slips.Num(); i++)
	{
		const FSlipInfo& S = P.SlipInfo[i];
		FString Mark;
		if (!S.ClueName.IsEmpty()) { const bool b = Found(S.ClueName); Mark = b ? TEXT("[FOUND]  ") : TEXT("[MISSED] "); Of++; Got += b; }
		else if (S.Kind == ESlipKind::Tape) Mark = bTapeCaught ? TEXT("[FOUND]  ") : TEXT("[ON TAPE]");
		else if (S.Kind == ESlipKind::Phone) Mark = TEXT("[PHONE]  ");
		else Mark = TEXT("[PEOPLE] ");
		L.Add(FString::Printf(TEXT("%s %s%s"), *Mark, *P.Slips[i], S.ClueName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" - the %s"), *S.ClueName.ToLower())));
	}
	if (Of > 0) L.Add(FString::Printf(TEXT("You bagged %d of the %d pieces of evidence they left at the scene."), Got, Of));
	if (!P.LeanedOnName.IsEmpty()) L.Add(FString::Printf(TEXT("%s lied for them. They called it a favour."), *P.LeanedOnName));
	return L;
}

TArray<FString> HUOffender::Describe(const FPlan& P)
{
	TArray<FString> L;
	if (!P.bValid) { L.Add(TEXT("No plan: the case has no culprit on file.")); return L; }
	L.Add(FString::Printf(TEXT("CRIME: %s    TYPE: %s    CARE: %d/100"), *P.Crime.ToUpper(), TypeName(P.Type), P.Care));
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
		const FString When = S.Day < 0 ? (S.Day == -1 ? FString(TEXT("the day before")) : FString::Printf(TEXT("%d days before"), -S.Day)) : Clock(S.Minute);
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
		int32 Bad = 0, Skipped = 0;
		TArray<HUOffender::FPlan> Plans;
		for (const FHUCase& C : Cases) Plans.Add(HUOffender::Make(C));
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		for (int32 i = 0; i < Plans.Num(); i++)
		{
			int32 Traces = 0; for (const HUOffender::FStep& S : Plans[i].Timeline) if (!S.Trace.IsEmpty()) Traces++;
			if (!Plans[i].bValid) { Skipped++; continue; }
			if (Plans[i].Slips.Num() < 3) Bad++;
			UE_LOG(LogTemp, Display, TEXT("==== CRIME PLAN case %d at %s (%d traces) ===="), Cases[i].Number, *Cases[i].Place, Traces);
			for (const FString& L : HUOffender::Describe(Plans[i])) UE_LOG(LogTemp, Display, TEXT("  %s"), *L);
			// what the case looks like once the plan is written into it: every lie needs evidence that exists
			FHUCase W = Cases[i]; HUOffender::ApplyToCase(W);
			for (const FHUSuspect& S : W.Suspects)
				for (const FHUStatement& St : S.Statements)
				{
					if (St.Truth || (S.Id != W.Solution.Killer && S.Id != Plans[i].LeanedOnId)) continue;
					TArray<FString> Ev;
					for (const FString& E : St.Evidence) { const FHUClue* Cl = W.Clues.FindByPredicate([&](const FHUClue& X) { return X.Id == E; }); Ev.Add(Cl ? Cl->Name : TEXT("MISSING!")); if (!Cl) Bad++; }
					UE_LOG(LogTemp, Display, TEXT("  LIE %s: \"%s\"  <- broken by: %s"), *S.Name, *St.Text, *FString::Join(Ev, TEXT(", ")));
				}
		}
		for (const HUOffender::FPlan& Pl : Plans)
			if (Pl.bValid)
			{
				UE_LOG(LogTemp, Display, TEXT("==== HOW IT HAPPENED (sample, evidence named 'Blood Drop' counted as found) ===="));
				for (const FString& L : HUOffender::Reveal(Pl, [](const FString& N) { return N == TEXT("Blood Drop"); }, false)) UE_LOG(LogTemp, Display, TEXT("  %s"), *L);
				break;
			}
		UE_LOG(LogTemp, Display, TEXT("HU.CrimePlans: %d cases generated and planned in %.2f ms, %d not homicides (not planned yet), %d problems"), Plans.Num(), Ms, Skipped, Bad);
	}));
#endif
