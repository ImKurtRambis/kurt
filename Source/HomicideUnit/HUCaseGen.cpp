#include "HUCaseGen.h"
#include "HUProfiles.h"

namespace
{
	// placeholders: {K} offender  {V} victim  {R} victim's role  {P} place  {T} time  {W} weapon  {S} speaker  {A} alias
	struct FLie { const TCHAR* Says; const TCHAR* Clue; const TCHAR* Detail; };

	// ---------- the offender's lies, by profile (3 of 5 are used per case) ----------
	const FLie ProfileLies[(int32)EHUProfile::Count][5] = {
		{}, // None
		{ // Serial killer
			{ TEXT("I'd never met {V} before tonight."), TEXT("Polaroids of the Victim"), TEXT("Polaroids of {V} taken over weeks, in {K}'s bag — the same kind found with two unsolved cases.") },
			{ TEXT("I don't collect anything."), TEXT("Trophy Box"), TEXT("A cigar box in {K}'s locker: a watch, a ring, a hair ribbon. None of them {K}'s. The newest is {V}'s.") },
			{ TEXT("I've lived here my whole life."), TEXT("Bus Tickets"), TEXT("Bus tickets in {K}'s wallet to the two towns with unsolved killings — on the dates they happened.") },
			{ TEXT("I don't read the crime pages."), TEXT("Clippings Scrapbook"), TEXT("A scrapbook of clippings about unsolved killings, annotated in {K}'s hand.") },
			{ TEXT("I've never seen that symbol before."), TEXT("Calling-Card Sketches"), TEXT("The mark left at tonight's scene, drawn over and over in {K}'s notebook.") },
		},
		{ // Arsonist
			{ TEXT("I don't smoke. Never carry matches."), TEXT("Matchbook Collection"), TEXT("Forty matchbooks in {K}'s coat, all from places that later burned.") },
			{ TEXT("I was nowhere near the back office."), TEXT("Accelerant-Soaked Rag"), TEXT("A rag reeking of lighter fluid, stuffed in {K}'s bag.") },
			{ TEXT("I've never had trouble with the fire department."), TEXT("Academy Dismissal Letter"), TEXT("{K}'s dismissal from the fire academy: 'unfit — an unhealthy fascination with ignition.'") },
			{ TEXT("I don't own a scanner."), TEXT("Police Scanner"), TEXT("A scanner in {K}'s car, tuned to the fire department's channel.") },
			{ TEXT("My coat's always smelled like that."), TEXT("Singed Coat Cuff"), TEXT("{K}'s coat cuff, freshly singed, and a new burn on the wrist under it.") },
		},
		{ // Poisoner
			{ TEXT("I never touched {V}'s drink."), TEXT("The Second Glass"), TEXT("A glass mixed apart from the others, {K}'s prints on the stem. There's a residue in the bottom.") },
			{ TEXT("Rat poison? We don't have rats."), TEXT("Pharmacy Receipt"), TEXT("A receipt in {K}'s name: rat poison, bought on Monday.") },
			{ TEXT("I don't know a thing about medicine."), TEXT("Revoked License"), TEXT("{K}'s pharmacist license — revoked three years ago.") },
			{ TEXT("They're just gloves."), TEXT("Chemical Gloves"), TEXT("Rubber gloves in {K}'s bag with a bitter-almond smell.") },
			{ TEXT("I don't keep notes about anybody."), TEXT("Dosage Notebook"), TEXT("A notebook in {K}'s tiny handwriting: dates, amounts, and {V}'s initials.") },
		},
		{ // Child predator (written as the investigation's paper trail - nothing depicted)
			{ TEXT("I've only got the one phone."), TEXT("Second Phone"), TEXT("A prepaid phone sewn into {K}'s coat lining. One contact saved. The messages are to a child.") },
			{ TEXT("Computers? I've never used one."), TEXT("Chat Logs"), TEXT("Printed chat logs between a screen name on {K}'s home line and a 12-year-old, arranging a meeting.") },
			{ TEXT("Check the volunteer list. My name's right there."), TEXT("Volunteer Application"), TEXT("{K}'s application to a youth program, signed {A}. The real name is on the state registry.") },
			{ TEXT("I've never been arrested in my life."), TEXT("Registry Printout"), TEXT("{K}'s face on the state offender registry — under the name {A}.") },
			{ TEXT("{V} and I got along fine."), TEXT("Victim's Note"), TEXT("In {V}'s handwriting: 'Going to the police tomorrow about {K}. The kids can't be left alone with them.'") },
		},
		{ // Contract killer
			{ TEXT("I'm just passing through. Never met {V}."), TEXT("Cash Envelope"), TEXT("$2,000 in an envelope in {K}'s jacket, a photo of {V} paper-clipped to it.") },
			{ TEXT("I don't carry anything."), TEXT("Filed-Down Pistol"), TEXT("A pistol with the serial numbers filed off, taped under {K}'s table.") },
			{ TEXT("I'm in town all week."), TEXT("One-Way Ticket"), TEXT("A one-way train ticket in {K}'s name, leaving at midnight.") },
			{ TEXT("Nobody sent me anywhere."), TEXT("Payphone Number"), TEXT("A payphone number in a matchbook. It rings at the Ferro crew's social club.") },
			{ TEXT("I've never been in the army."), TEXT("Discharge Papers"), TEXT("{K}'s dishonorable discharge. Rated marksman.") },
		},
		{ // Crime of passion
			{ TEXT("{V} and I were just friends."), TEXT("Torn Love Letters"), TEXT("Love letters from {K} to {V}, torn in half.") },
			{ TEXT("I'm happily married."), TEXT("Ring in the Trash"), TEXT("{K}'s wedding ring, thrown in the trash by the back door.") },
			{ TEXT("I wasn't upset tonight."), TEXT("Diary Entry"), TEXT("{K}'s diary, last night: 'If I can't have {V}, nobody will.'") },
			{ TEXT("I didn't go near {V} all night."), TEXT("Lipstick on a Collar"), TEXT("{V}'s shade of lipstick on {K}'s collar.") },
			{ TEXT("I've never laid a hand on anybody."), TEXT("Domestic Call Log"), TEXT("Two domestic disturbance calls to {K}'s address this year.") },
		},
		{ // Stickup artist
			{ TEXT("I'm doing fine for money."), TEXT("Pawn Tickets"), TEXT("Six pawn tickets in {K}'s name this month — a watch, a radio, a wedding band.") },
			{ TEXT("I don't know when the cash gets picked up."), TEXT("Pickup Schedule"), TEXT("The armored car's pickup times, copied out in {K}'s handwriting.") },
			{ TEXT("I've never worn a mask in my life."), TEXT("Stocking Mask"), TEXT("A stocking mask balled up in {K}'s coat pocket.") },
			{ TEXT("I came on the bus."), TEXT("Stolen Plate"), TEXT("A stolen license plate in the trunk of {K}'s car out back.") },
			{ TEXT("I've never been inside."), TEXT("Prison ID"), TEXT("{K}'s prison ID from five years for armed robbery.") },
		},
		{ // Con artist
			{ TEXT("My name's {K}. Always has been."), TEXT("Two Driver's Licenses"), TEXT("Two licenses in {K}'s wallet. Same face, two names: {K} and {A}.") },
			{ TEXT("I don't sign anything for anybody."), TEXT("Practice Signatures"), TEXT("{V}'s signature, practised forty times on scrap paper in {K}'s desk.") },
			{ TEXT("The books balance."), TEXT("Doctored Ledger"), TEXT("A ledger 'corrected' in {K}'s pen. $9,000 doesn't add up.") },
			{ TEXT("I run a real business."), TEXT("Fake Business Card"), TEXT("{K}'s card for 'Sterling Holdings'. The address is a vacant lot.") },
			{ TEXT("I've never been to court."), TEXT("Fraud Conviction"), TEXT("A check-fraud conviction in Ohio, under the name {A}. The mugshot is {K}.") },
		},
		{ // Mob enforcer
			{ TEXT("{V} didn't owe anybody."), TEXT("Debt Ledger"), TEXT("A loan shark's ledger in {K}'s pocket. {V}'s name is crossed out — tonight's date beside it.") },
			{ TEXT("I don't carry anything."), TEXT("Brass Knuckles"), TEXT("Brass knuckles in {K}'s coat.") },
			{ TEXT("I never threatened anyone."), TEXT("'Pay Up' Note"), TEXT("A note in {K}'s handwriting: 'Pay up by Friday, {V}.'") },
			{ TEXT("I don't drink at the Shamrock."), TEXT("Shamrock Matchbook"), TEXT("A matchbook from the Shamrock — the mob's bar — in {K}'s pocket.") },
			{ TEXT("My knuckles? I fell."), TEXT("Stained Gloves"), TEXT("{K}'s gloves, dark stains across the knuckles.") },
		},
		{ // Stalker
			{ TEXT("I barely knew {V}."), TEXT("Photos from a Distance"), TEXT("Dozens of photos of {V}, taken from across the street, in {K}'s car.") },
			{ TEXT("I've never been to {V}'s place."), TEXT("Copied House Key"), TEXT("A copy of {V}'s house key on {K}'s keyring.") },
			{ TEXT("I don't keep track of people."), TEXT("Movements Notebook"), TEXT("Three months of {V}'s comings and goings, logged in {K}'s hand.") },
			{ TEXT("{V} and I write to each other sometimes."), TEXT("Returned Letters"), TEXT("Letters from {K} to {V}. Every one returned unopened.") },
			{ TEXT("I'm into bird-watching."), TEXT("Binoculars"), TEXT("Binoculars in {K}'s bag. From where {K} parks, the only window you can see is {V}'s.") },
		},
	};
	// a missing child with a predator behind it: the paper trail ends at where the child is
	const FLie FoundChild = { TEXT("The old pump house? I don't know where that is."), TEXT("Hand-Drawn Map"), TEXT("A map in {K}'s handwriting to the old pump house on Mill Road. Patrol went straight there: {V} was inside — scared, cold, unhurt.") };

	// ---------- what happened ----------
	struct FIncident
	{
		const TCHAR* Crime;
		const TCHAR* Header;
		const TCHAR* Rows[3];
		const TCHAR* Captain;
		float ProfileBoost[(int32)EHUProfile::Count]; // likelier profiles; everything else stays possible
	};
	const FIncident Incidents[] = {
		{ TEXT("homicide"), TEXT("THE VICTIM"), { TEXT("FOUND AT: {P}"), TEXT("TIME: {T}"), TEXT("CAUSE: {W}") },
			TEXT("\"{V}, {R}. Found dead at {P} at {T}. Nobody's left the building since.\""),
			{ 0, 3, 1, 2, 1, 3, 3, 1, 1, 2, 2 } },
		{ TEXT("assault"), TEXT("THE VICTIM"), { TEXT("ATTACKED AT: {P}"), TEXT("TIME: {T}"), TEXT("ALIVE AT COUNTY GENERAL") },
			TEXT("\"{V}, {R}. Beaten at {P} around {T} — alive at County General, can't talk yet. Whoever did it is still in there.\""),
			{ 0, 1, 1, 1, 2, 1, 3, 2, 1, 3, 3 } },
		{ TEXT("robbery"), TEXT("THE ROBBERY"), { TEXT("THE OFFICE SAFE AT {P}"), TEXT("CLEANED OUT AT {T}"), TEXT("{V} KNOCKED COLD") },
			TEXT("\"The office safe at {P}, cleaned out at {T}. {V}, {R}, was knocked cold. Nobody's left — the money's still in there somewhere.\""),
			{ 0, 1, 1, 1, 1, 2, 1, 4, 3, 3, 1 } },
		{ TEXT("missing"), TEXT("THE MISSING PERSON"), { TEXT("LAST SEEN AT: {P}"), TEXT("MISSING SINCE: {T}"), TEXT("EVERY MINUTE COUNTS") },
			TEXT("\"{V} hasn't been seen since {T}. Last seen at {P}. Somebody in that building knows where.\""),
			{ 0, 2, 1, 1, 3, 2, 2, 1, 1, 2, 3 } },
		{ TEXT("arson"), TEXT("THE FIRE"), { TEXT("SET AT: {P}"), TEXT("TIME: {T}"), TEXT("{V} PULLED OUT ALIVE") },
			TEXT("\"A fire in the back office at {P}, {T}. {V}, {R}, was pulled out alive. Fire marshal says it was set.\""),
			{ 0, 2, 4, 1, 1, 2, 2, 2, 2, 2, 1 } },
	};

	// what innocent people lie about: small secrets, not this crime
	const FLie Secrets[] = {
		{ TEXT("I was right here all night."), TEXT("Bar Tab"), TEXT("A tab at the bar across the street, signed by {S} at 9:40. Not a crime — but they weren't here.") },
		{ TEXT("I don't gamble."), TEXT("Betting Slips"), TEXT("Betting slips in {S}'s jacket. Three races, three losers.") },
		{ TEXT("Every dollar went in the register."), TEXT("Cash in an Apron"), TEXT("$60 folded into {S}'s apron pocket. The register came up exactly $60 short.") },
		{ TEXT("I don't drink on the job."), TEXT("Hip Flask"), TEXT("A flask engraved with {S}'s initials, half empty.") },
		{ TEXT("I'm happily married."), TEXT("Motel Receipt"), TEXT("A motel receipt from Tuesday, two guests, paid by {S}. Their spouse was out of town.") },
		{ TEXT("I clocked in at six, like always."), TEXT("Time Card"), TEXT("{S}'s time card, punched at 7:15 — in someone else's handwriting.") },
		{ TEXT("I've never been in any trouble."), TEXT("Parking Tickets"), TEXT("Eleven unpaid parking tickets in {S}'s glovebox.") },
		{ TEXT("I don't really know anyone here."), TEXT("Ballgame Photo"), TEXT("A photo of {S} and the manager at a ballgame last month, arm in arm.") },
		{ TEXT("I've never set foot in the office."), TEXT("Office Key"), TEXT("A key to the office on {S}'s ring. They 'borrow' the phone for long-distance calls.") },
		{ TEXT("I'm not looking for another job."), TEXT("Job Application"), TEXT("A filled-in application for the place across town, in {S}'s bag.") },
	};
	const TCHAR* Truths[] = {
		TEXT("I've been coming here for three years."), TEXT("I came in through the front door."), TEXT("I saw the manager by the office around nine."),
		TEXT("I heard a door slam around ten."), TEXT("My car's the green one out front."), TEXT("I called my sister at eight. She'll tell you."),
		TEXT("I was by the coat check most of the night."), TEXT("The back door sticks — everyone knows that."), TEXT("I got here at seven, same as every week."),
		TEXT("The lights flickered right before it happened."), TEXT("I had the fish. It was terrible."),
	};
	// dead ends that are just things: they look like something until you look closer
	const FLie Neutral[] = {
		{ TEXT(""), TEXT("Cold Coffee"), TEXT("A full cup of coffee, stone cold. Somebody forgot it hours ago.") },
		{ TEXT(""), TEXT("Crumpled Flyer"), TEXT("A flyer for Saturday's bake sale. Nothing written on the back.") },
		{ TEXT(""), TEXT("Burnt-Out Bulb"), TEXT("A dead light bulb on the floor. Someone changed it and didn't bother with the old one.") },
		{ TEXT(""), TEXT("Lost Umbrella"), TEXT("A black umbrella with no name on it. It hasn't rained in a week.") },
		{ TEXT(""), TEXT("Torn Ticket Stub"), TEXT("Half a ticket from the picture house on Fifth. Dated last March.") },
		{ TEXT(""), TEXT("Lipstick-Smudged Glass"), TEXT("A glass with a smudge of lipstick. The shade matches half the women in town.") },
		{ TEXT(""), TEXT("Cigarette Butts"), TEXT("A small heap of cigarette butts by the back wall. Three brands. The staff's smoking spot.") },
		{ TEXT(""), TEXT("Scuffed Floor"), TEXT("Long black scuff marks across the floor. Rubber heels — and a mop bucket that was dragged.") },
		{ TEXT(""), TEXT("Dropped Earring"), TEXT("A single fake-pearl earring. It's been in the lost-and-found box since summer.") },
		{ TEXT(""), TEXT("Phone Number on a Napkin"), TEXT("A phone number on a napkin. It rings at a pizza place.") },
		{ TEXT(""), TEXT("Broken Watch"), TEXT("A cheap watch, stopped at 3:15. The battery died weeks ago.") },
		{ TEXT(""), TEXT("Blood-Spotted Rag"), TEXT("A rag with a few spots of blood. The cook cut a thumb on Tuesday — there's a bandage in the trash.") },
		{ TEXT(""), TEXT("Muddy Footprint"), TEXT("A muddy boot print, size 11. The delivery man's — he came through the back at six.") },
		{ TEXT(""), TEXT("Locked Drawer Key"), TEXT("A small brass key. It opens the petty-cash drawer, which has exactly what the book says.") },
	};
	// dead ends that point at someone innocent: they're theirs, but they prove nothing about tonight
	const FLie Herrings[] = {
		{ TEXT(""), TEXT("Monogrammed Lighter"), TEXT("A silver lighter engraved with {S}'s initials. They dropped it on their smoke break.") },
		{ TEXT(""), TEXT("Angry Letter"), TEXT("A furious letter from {S} to the management about unpaid overtime. Dated last month — and it was settled.") },
		{ TEXT(""), TEXT("Dropped Glove"), TEXT("A leather glove, {S}'s. The other one is still in their coat. Nothing on it.") },
		{ TEXT(""), TEXT("Spare Key"), TEXT("A spare back-door key on {S}'s ring. Half the staff have one.") },
		{ TEXT(""), TEXT("Scribbled Note"), TEXT("A note in {S}'s hand: 'Tonight — don't forget.' Their sister's birthday; the card's in their bag.") },
		{ TEXT(""), TEXT("Overdue Bill"), TEXT("A final notice for {S}'s electric bill. Money trouble — but nothing tonight touches money.") },
		{ TEXT(""), TEXT("Bloody Handkerchief"), TEXT("{S}'s handkerchief, a little blood on it. A nosebleed at eight — two people saw it.") },
		{ TEXT(""), TEXT("Pocket Knife"), TEXT("A pocket knife belonging to {S}. Clean, dull, used for opening boxes.") },
	};
	const TCHAR* Intros[] = {
		TEXT("I just want to go home."), TEXT("Ask me anything. I've got nothing to hide."), TEXT("This is a nightmare."),
		TEXT("I've already told the officer everything."), TEXT("Is everyone all right? Nobody tells us anything."), TEXT("I don't see why I have to be here."),
		TEXT("Make it quick, detective."), TEXT("I knew something like this would happen in this place."),
	};
	const TCHAR* Roles[] = {
		TEXT("night manager"), TEXT("bartender"), TEXT("cook"), TEXT("janitor"), TEXT("regular"), TEXT("bookkeeper"), TEXT("bouncer"),
		TEXT("youth coach"), TEXT("volunteer"), TEXT("delivery driver"), TEXT("photographer"), TEXT("owner's nephew"), TEXT("cashier"), TEXT("musician"),
	};
	const TCHAR* VictimRoles[] = { TEXT("the owner"), TEXT("the night manager"), TEXT("a bartender"), TEXT("the bookkeeper"), TEXT("a regular"), TEXT("the youth program director") };
	const TCHAR* Weapons[] = { TEXT("a tire iron"), TEXT("a kitchen knife"), TEXT("a brass lamp"), TEXT("a length of rope"), TEXT("an ice pick"), TEXT("a revolver"), TEXT("a poisoned drink") };
	const TCHAR* Times[] = { TEXT("9:12 PM"), TEXT("9:47 PM"), TEXT("10:05 PM"), TEXT("10:38 PM"), TEXT("11:10 PM"), TEXT("11:52 PM") };
	const TCHAR* First[] = { TEXT("Walter"), TEXT("Doris"), TEXT("Ray"), TEXT("Helen"), TEXT("Carl"), TEXT("June"), TEXT("Frank"), TEXT("Irene"), TEXT("Leon"), TEXT("Marge"), TEXT("Gus"), TEXT("Vera"), TEXT("Stan"), TEXT("Nora"), TEXT("Earl"), TEXT("Pearl"), TEXT("Sal"), TEXT("Edie"), TEXT("Hank"), TEXT("Lorraine") };
	const TCHAR* Last[] = { TEXT("Kowalski"), TEXT("Hale"), TEXT("Brennan"), TEXT("Ortiz"), TEXT("Lindqvist"), TEXT("Pruitt"), TEXT("Mancuso"), TEXT("Whitfield"), TEXT("Dunne"), TEXT("Abernathy"), TEXT("Szabo"), TEXT("Caldwell"), TEXT("Russo"), TEXT("Ferro"), TEXT("Marsh"), TEXT("Pierce") };
	const TCHAR* Kids[] = { TEXT("Tommy"), TEXT("Lucy"), TEXT("Danny"), TEXT("Rosie"), TEXT("Billy"), TEXT("Maggie") };
	const TCHAR* Difficulties[] = { TEXT("rookie"), TEXT("detective"), TEXT("detective"), TEXT("veteran") };

	FString Fill(const FString& Fmt, const TMap<FString, FString>& V)
	{
		FString S = Fmt;
		for (const TPair<FString, FString>& P : V) S = S.Replace(*P.Key, *P.Value);
		return S;
	}
	template <typename T> void Shuffle(TArray<T>& A, FRandomStream& R) { for (int32 i = A.Num() - 1; i > 0; i--) A.Swap(i, R.RandRange(0, i)); }
	template <typename T, int32 N> const T& Pick(const T (&A)[N], FRandomStream& R) { return A[R.RandRange(0, N - 1)]; }
	FString Person(FRandomStream& R) { return FString(Pick(First, R)) + TEXT(" ") + Pick(Last, R); }
}

const TArray<FString>& HUCaseGen::Places()
{
	static const TArray<FString> P = {
		TEXT("THE MIRAMAR DINER"), TEXT("PIERCE & SONS WAREHOUSE"), TEXT("THE RIVERSIDE PIER"), TEXT("THE GRAND MERIDIAN HOTEL"),
		TEXT("THE VELVET LOUNGE"), TEXT("RAINES STABLES"), TEXT("LA BELLE SALON"), TEXT("THE MAJESTIC THEATRE"), TEXT("ST. JUDE'S HOSPITAL"),
		TEXT("THE STARLITE BOWLING LANES"), TEXT("FIRST CITY BANK"), TEXT("THE ARAGON BALLROOM"), TEXT("THE ELKS LODGE HALL"),
	};
	return P;
}

TArray<FHUCase> HUCaseGen::MakeCases(int32 FirstNumber, int32 Count, int32 Seed)
{
	TArray<FHUCase> Out;
	FRandomStream R(Seed);
	for (int32 n = 0; n < Count; n++)
	{
		FHUCase C;
		C.Number = FirstNumber + n;
		C.MapSeed = R.RandHelper(1 << 30);
		C.Difficulty = Pick(Difficulties, R);
		C.Place = Places()[R.RandRange(0, Places().Num() - 1)];       // where: any place, any incident
		const FIncident& I = Pick(Incidents, R);                        // what happened
		C.Crime = I.Crime;
		C.Time = Pick(Times, R);
		C.Weapon = Pick(Weapons, R);

		// who did it: weighted by the incident, but every profile stays possible
		float Sum = 0; for (int32 p = 1; p < (int32)EHUProfile::Count; p++) Sum += 1.f + I.ProfileBoost[p];
		float Roll = R.FRand() * Sum; EHUProfile Prof = EHUProfile::Stalker;
		for (int32 p = 1; p < (int32)EHUProfile::Count; p++) { Roll -= 1.f + I.ProfileBoost[p]; if (Roll <= 0) { Prof = (EHUProfile)p; break; } }
		C.Profile = (uint8)Prof;

		// a missing child is possible behind some profiles (a predator, a parent in a custody fight, a ransom, a debt)
		const bool bChild = C.Crime == TEXT("missing") && (Prof == EHUProfile::ChildPredator || Prof == EHUProfile::Passion || Prof == EHUProfile::ContractKiller || Prof == EHUProfile::Enforcer) && R.FRand() < 0.6f;

		TArray<FString> Names;
		while (Names.Num() < 9) { const FString Nm = Person(R); if (!Names.Contains(Nm)) Names.Add(Nm); }
		const FString Victim = bChild ? FString(Pick(Kids, R)) + TEXT(" ") + Pick(Last, R) + TEXT(", age 9") : Names.Pop();
		TMap<FString, FString> Vars = {
			{ TEXT("{V}"), Victim }, { TEXT("{R}"), bChild ? TEXT("a child") : Pick(VictimRoles, R) }, { TEXT("{P}"), C.Place },
			{ TEXT("{T}"), C.Time }, { TEXT("{W}"), C.Weapon }, { TEXT("{A}"), Person(R) },
		};
		C.Victim = Victim;
		C.Title = FString::Printf(TEXT("CASE #%d: %s"), C.Number, *C.Place);
		C.VictimHeader = bChild ? TEXT("THE MISSING CHILD") : I.Header;
		for (const TCHAR* Row : I.Rows) C.VictimRows.Add(Fill(Row, Vars));
		C.CaptainLine = Fill(I.Captain, Vars);

		// eight people held at the scene
		TArray<int32> RoleIdx; for (int32 i = 0; i < UE_ARRAY_COUNT(Roles); i++) RoleIdx.Add(i);
		TArray<int32> SecretIdx; for (int32 i = 0; i < UE_ARRAY_COUNT(Secrets); i++) SecretIdx.Add(i);
		TArray<int32> TruthIdx; for (int32 i = 0; i < UE_ARRAY_COUNT(Truths); i++) TruthIdx.Add(i);
		Shuffle(RoleIdx, R); Shuffle(SecretIdx, R);
		const int32 KillerAt = R.RandRange(0, 7);
		int32 SecretsUsed = 0;
		for (int32 s = 0; s < 8; s++)
		{
			FHUSuspect S;
			S.Id = FString::Printf(TEXT("s%d"), s);
			S.Name = Names[s];
			S.Role = Roles[RoleIdx[s]];
			S.Intro = FString::Printf(TEXT("%s, %s. %s"), *S.Name, *S.Role, Pick(Intros, R));
			Vars.Add(TEXT("{K}"), S.Name);
			Vars.Add(TEXT("{S}"), S.Name);
			Shuffle(TruthIdx, R);
			auto AddTruth = [&](int32 i) { FHUStatement T; T.Text = Truths[TruthIdx[i]]; S.Statements.Add(T); };
			auto AddLie = [&](const FLie& Lie, const FString& Id, const TCHAR* OnBreak)
			{
				FHUClue Clue; Clue.Id = Id; Clue.Name = Lie.Clue; Clue.Detail = Fill(Lie.Detail, Vars); Clue.Area = TEXT("main");
				C.Clues.Add(Clue);
				FHUStatement St; St.Text = Fill(Lie.Says, Vars); St.Truth = false; St.Evidence = { Id }; St.OnBreak = OnBreak;
				S.Statements.Add(St);
			};
			if (s == KillerAt)
			{
				// three of their profile's five lies
				TArray<FLie> Pool; for (const FLie& L : ProfileLies[(int32)Prof]) Pool.Add(L);
				if (bChild && Prof == EHUProfile::ChildPredator) Pool[4] = FoundChild;
				Shuffle(Pool, R);
				if (bChild && Prof == EHUProfile::ChildPredator) { const int32 At = Pool.IndexOfByPredicate([](const FLie& L) { return FCString::Strcmp(L.Clue, FoundChild.Clue) == 0; }); Pool.Swap(0, At); }
				for (int32 i = 0; i < 3; i++) AddLie(Pool[i], FString::Printf(TEXT("c%d_k%d"), C.Number, i), TEXT("...I want a lawyer."));
				AddTruth(0);
				C.Solution.Killer = S.Id;
				C.Solution.Motive = FString::Printf(TEXT("c%d_k0"), C.Number);
				C.Solution.Means = FString::Printf(TEXT("c%d_k1"), C.Number);
				C.Solution.Alibi = FString::Printf(TEXT("c%d_k2"), C.Number);
			}
			else if (SecretsUsed < 4)
			{
				AddTruth(0);
				AddLie(Secrets[SecretIdx[SecretsUsed]], FString::Printf(TEXT("c%d_s%d"), C.Number, SecretsUsed), TEXT("Fine. Yes. But that's got nothing to do with this."));
				AddTruth(1);
				SecretsUsed++;
			}
			else { AddTruth(0); AddTruth(1); AddTruth(2); }
			Shuffle(S.Statements, R);
			C.Suspects.Add(S);
		}
		Vars.Remove(TEXT("{K}")); Vars.Remove(TEXT("{S}"));

		// dead ends, three kinds (a case writes 8; difficulty decides how many go in the scene):
		//   decoys   - look like a different kind of offender (they push the profile the wrong way)
		//   herrings - belong to an innocent person, with an innocent explanation
		//   junk     - things that look like something and aren't
		EHUProfile Decoy = Prof;
		while (Decoy == Prof) Decoy = (EHUProfile)R.RandRange(1, (int32)EHUProfile::Count - 1);
		C.DecoyProfile = (uint8)Decoy;
		int32 DeadN = 0;
		auto AddDead = [&](const FString& Name, const FString& Detail, EHUProfile PointsTo)
		{
			FHUClue Clue; Clue.Id = FString::Printf(TEXT("c%d_d%d"), C.Number, DeadN++); Clue.Name = Name; Clue.Detail = Detail;
			Clue.Area = TEXT("main"); Clue.Flavor = true; Clue.PointsTo = (uint8)PointsTo;
			C.Clues.Add(Clue);
		};
		TArray<int32> D = { 0, 1, 2, 3, 4 }; Shuffle(D, R);
		for (int32 i = 0; i < 2; i++)
		{
			const FLie& L = ProfileLies[(int32)Decoy][D[i]];
			AddDead(L.Clue, FString::Printf(TEXT("%s. Nobody here claims it, and nothing ties it to anyone."), L.Clue), Decoy);
		}
		TArray<int32> Innocents; for (const FHUSuspect& S : C.Suspects) if (S.Id != C.Solution.Killer) Innocents.Add(&S - C.Suspects.GetData());
		Shuffle(Innocents, R);
		TArray<int32> H; for (int32 i = 0; i < UE_ARRAY_COUNT(Herrings); i++) H.Add(i);
		Shuffle(H, R);
		for (int32 i = 0; i < 3 && i < Innocents.Num(); i++)
		{
			TMap<FString, FString> SV = { { TEXT("{S}"), C.Suspects[Innocents[i]].Name } };
			AddDead(Herrings[H[i]].Clue, Fill(Herrings[H[i]].Detail, SV), EHUProfile::None);
		}
		TArray<int32> N; for (int32 i = 0; i < UE_ARRAY_COUNT(Neutral); i++) N.Add(i);
		Shuffle(N, R);
		for (int32 i = 0; i < 3; i++) AddDead(Neutral[N[i]].Clue, Neutral[N[i]].Detail, EHUProfile::None);
		C.Solution.RequiredBreaks = 3;
		Out.Add(C);
	}
	return Out;
}
