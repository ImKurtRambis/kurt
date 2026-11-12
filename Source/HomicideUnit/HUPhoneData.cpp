#include "HUPhoneData.h"
#include "HUOffenderBrain.h"
#include "HUCaseTypes.h"
#include "HUCaseSubsystem.h"

namespace HUPhone
{
// ---- small helpers ----
static FString Title(const FString& In)
{
	FString O = In.ToLower(); bool bUp = true;
	for (TCHAR& Ch : O) { if (bUp && FChar::IsAlpha(Ch)) { Ch = FChar::ToUpper(Ch); bUp = false; } else if (Ch == ' ' || Ch == '-' || Ch == '.') bUp = true; }
	return O;
}
static FString VictimName(const FHUCase& Case) { FString N = Case.Victim; int32 i; if (N.FindChar(',', i)) N = N.Left(i); return N; }
static FString FirstName(const FString& Full) { FString L, R; return Full.Split(TEXT(" "), &L, &R) ? L : Full; }
static bool IsChildCase(const FHUCase& Case) { return Case.Victim.Contains(TEXT("age ")); }

// minutes after midnight-ish for "10:38 PM" (22:38 -> 1358)
static int32 Minutes(const FString& Clock)
{
	FString H, Rest; if (!Clock.Split(TEXT(":"), &H, &Rest)) return 22 * 60 + 30;
	int32 Hr = FCString::Atoi(*H) % 12, Mn = FCString::Atoi(*Rest.Left(2));
	if (Rest.Contains(TEXT("PM"))) Hr += 12;
	return Hr * 60 + Mn;
}
static FString Clock(int32 M)
{
	M = (M % 1440 + 1440) % 1440;
	const int32 H = M / 60, Mn = M % 60;
	return FString::Printf(TEXT("%d:%02d %s"), H % 12 == 0 ? 12 : H % 12, Mn, H < 12 ? TEXT("AM") : TEXT("PM"));
}
static FString PhoneNo(FRandomStream& R) { return FString::Printf(TEXT("(555) %03d-%04d"), R.RandRange(201, 989), R.RandRange(1000, 9999)); }

enum class EKind { Clean, Secret, Culprit };
static EKind KindOf(const FHUCase& Case, const FHUSuspect& S)
{
	if (S.Id == Case.Solution.Killer) return EKind::Culprit;
	for (const FHUStatement& St : S.Statements) if (!St.Truth) return EKind::Secret;
	return EKind::Clean;
}
static int32 IndexOf(const FHUCase& Case, const FHUSuspect& S) { return Case.Suspects.IndexOfByPredicate([&](const FHUSuspect& O) { return O.Id == S.Id; }); }
static FRandomStream Seed(const FHUCase& Case, const FHUSuspect& S, int32 Salt) { return FRandomStream(Case.Number * 7919 + IndexOf(Case, S) * 131 + Salt); }
// one of the other suspects (never this one)
static FString OtherName(const FHUCase& Case, const FHUSuspect& S, FRandomStream& R)
{
	if (Case.Suspects.Num() < 2) return TEXT("Ma");
	int32 i; do { i = R.RandRange(0, Case.Suspects.Num() - 1); } while (Case.Suspects[i].Id == S.Id);
	return Case.Suspects[i].Name;
}
template <typename T, int32 N> static const T& Pick(const T (&A)[N], FRandomStream& R) { return A[R.RandRange(0, N - 1)]; }

static const TCHAR* Contacts[] = { TEXT("Ma"), TEXT("Work"), TEXT("Dentist"), TEXT("Sis"), TEXT("Landlord"), TEXT("Tony (garage)"), TEXT("Pizza Roma"), TEXT("Bank"), TEXT("Dad"), TEXT("Coach"), TEXT("Marie"), TEXT("Unknown") };
static const TCHAR* BoringTexts[] = {
	TEXT("can u grab milk on the way"), TEXT("running late, start without me"), TEXT("did you see the game??"), TEXT("call me when you're up"),
	TEXT("rent's due friday"), TEXT("love you, night"), TEXT("ok"), TEXT("lol no"), TEXT("where'd you park"), TEXT("happy birthday!!"),
	TEXT("shift swap still on for tuesday?"), TEXT("tell your mother I said thanks") };
static const TCHAR* BoringSearches[] = {
	TEXT("weather tomorrow"), TEXT("late night pharmacy near me"), TEXT("how long to boil an egg"), TEXT("bus 14 schedule"),
	TEXT("cheap flights to miami"), TEXT("score last night"), TEXT("how to unclog a drain"), TEXT("used cars under 3000"),
	TEXT("is it a cold or the flu"), TEXT("movie times tonight"), TEXT("lyrics to that song from the radio"), TEXT("best pizza downtown") };
static const TCHAR* BoringPhotos[] = {
	TEXT("A plate of spaghetti, from above."), TEXT("A dog asleep on a couch."), TEXT("A blurry photo of a ceiling. Pocket shot."),
	TEXT("A receipt, photographed flat. Groceries, $23.40."), TEXT("Two people grinning at a bar, red-eyed from the flash."),
	TEXT("A parking sign, close up - the hours circled in pen."), TEXT("A sunset over rooftops."), TEXT("A screenshot of a bus timetable."),
	TEXT("A cat on a windowsill."), TEXT("A selfie in a car, sunglasses on.") };
static const TCHAR* Towers[] = { TEXT("Northside / Elm St"), TEXT("Eastgate"), TEXT("Harbor Rd"), TEXT("Midtown / 5th Ave"), TEXT("Southfield"), TEXT("Westbrook"), TEXT("Old Mill"), TEXT("Union Station") };

// what the weapon makes someone look up afterwards / beforehand
static FString MeansSearch(const FString& Weapon, int32 Which)
{
	const FString W = Weapon.ToLower();
	if (W.Contains(TEXT("knife")) || W.Contains(TEXT("ice pick"))) return Which ? TEXT("does bleach get blood out of fabric") : TEXT("how deep is a fatal stab wound");
	if (W.Contains(TEXT("revolver"))) return Which ? TEXT("gunshot residue how long on hands") : TEXT("buy revolver no paperwork");
	if (W.Contains(TEXT("rope"))) return Which ? TEXT("how long do ligature marks last") : TEXT("strongest knot that won't slip");
	if (W.Contains(TEXT("poison"))) return Which ? TEXT("does a tox screen find everything") : TEXT("tasteless poison in a drink");
	return Which ? TEXT("can police get prints off metal") : TEXT("how hard to hit someone to knock them out");
}

bool HasWarrant(const UHUCaseSubsystem* C, const FString& SuspectId)
{
	if (!C) return false;
	if (C->Arrested.Contains(SuspectId)) return true;
	FHUSuspect S;
	if (!C->GetSuspect(SuspectId, S)) return false;
	for (int32 i = 0; i < S.Statements.Num(); i++) if (C->IsBroken(SuspectId, i)) return true;
	return false;
}

TArray<FString> Lines(const UHUCaseSubsystem* C, const FHUSuspect& S, EApp App)
{
	TArray<FString> L;
	if (!C) return L;
	const FHUCase& Case = C->Current;
	const EKind K = KindOf(Case, S);
	const FString Vic = VictimName(Case), VicFirst = FirstName(Vic);
	const bool bChild = IsChildCase(Case);
	const int32 T = Minutes(Case.Time);
	const FString Place = Title(Case.Place);
	FRandomStream R = Seed(Case, S, (int32)App);
	// the culprit's phone shows what their plan and their slips left behind (a careful one leaves little)
	HUOffender::FPlan Plan; if (K == EKind::Culprit) Plan = HUOffender::Make(Case);
	const bool bPlan = Plan.bValid, bDom = bPlan && Plan.Type == HUOffender::EType::Domestic;

	if (App == Database)
	{
		FRandomStream D = Seed(Case, S, 99);
		const TCHAR* Streets[] = { TEXT("Elm St"), TEXT("Harbor Rd"), TEXT("Kessler Ave"), TEXT("Pine Ct"), TEXT("Mercer St"), TEXT("Delancey Blvd"), TEXT("Cobb Ln"), TEXT("River Dr") };
		const TCHAR* Cars[] = { TEXT("1988 Buick LeSabre, brown"), TEXT("1994 Ford Taurus, white"), TEXT("1991 Chevy Caprice, black"), TEXT("No vehicle registered"), TEXT("1997 Honda Civic, green"), TEXT("1985 Cadillac DeVille, maroon"), TEXT("1999 Dodge Ram, grey") };
		L.Add(FString::Printf(TEXT("NAME:     %s"), *S.Name.ToUpper()));
		const int32 Mo = D.RandRange(1, 12), Dy = D.RandRange(1, 28), Age = D.RandRange(24, 61);
		L.Add(FString::Printf(TEXT("DOB:      %02d/%02d/%d   (age %d)"), Mo, Dy, 1999 - Age, Age));
		L.Add(FString::Printf(TEXT("ADDRESS:  %d %s, Apt %d"), D.RandRange(12, 2400), Pick(Streets, D), D.RandRange(1, 30)));
		L.Add(FString::Printf(TEXT("PHONE:    %s"), *PhoneNo(D)));
		L.Add(FString::Printf(TEXT("WORK:     %s"), *S.Role));
		L.Add(FString::Printf(TEXT("VEHICLE:  %s"), Pick(Cars, D)));
		const FString Rec = C->SuspectRecord.FindRef(S.Id);
		L.Add(TEXT("PRIORS:   ") + (Rec.IsEmpty() ? FString(TEXT("none on file")) : Rec));
		L.Add(FString::Printf(TEXT("KNOWN ASSOCIATES: %s, %s"), *OtherName(Case, S, D), *OtherName(Case, S, D)));
		L.Add(FString::Printf(TEXT("PHONE WARRANT: %s"), HasWarrant(C, S.Id) ? TEXT("GRANTED - open their phone in Calls, Texts, Web, Photos, Cell Pings") : TEXT("not yet (break one of their stories, or bring them in)")));
		return L;
	}

	if (!HasWarrant(C, S.Id))
	{
		L.Add(TEXT("[ LOCKED ]  No warrant for this phone yet."));
		L.Add(TEXT("  The judge wants probable cause: break one of their stories on the record, or have them in custody."));
		return L;
	}

	switch (App)
	{
	case Calls:
	{
		// a day of calls, oldest first; the interesting ones slotted in
		struct FCall { int32 At; FString Line; };
		TArray<FCall> Log;
		for (int32 i = 0; i < 6; i++)
		{
			const int32 At = T - R.RandRange(60, 600);
			const bool bOut = R.FRand() < 0.5f;
			Log.Add({ At, FString::Printf(TEXT("%s  %s %-16s %d:%02d"), *Clock(At), bOut ? TEXT("OUT ->") : TEXT("IN  <-"), Pick(Contacts, R), R.RandRange(0, 9), R.RandRange(5, 59)) });
		}
		if (bPlan)
		{
			if (!Plan.LeanedOnName.IsEmpty()) Log.Add({ T - 1440 + R.RandRange(-60, 60), FString::Printf(TEXT("YESTERDAY  OUT -> %-16s 4:12"), *Plan.LeanedOnName) });
			if (bDom) Log.Add({ T - 50, FString::Printf(TEXT("%s  OUT -> %-16s 6:12"), *Clock(T - 50), *Vic) });
			if (Plan.Has(TEXT("calldead"))) Log.Add({ T + 50, FString::Printf(TEXT("%s  OUT -> %-16s 0:00   NO ANSWER"), *Clock(T + 50), *VicFirst) });
			if (!bDom && !Plan.Has(TEXT("cell"))) Log.Add({ T - 90, TEXT("------   NO ACTIVITY ALL EVENING   (the phone never left home?)") });
		}
		else if (K == EKind::Culprit)
		{
			if (!bChild) Log.Add({ T - R.RandRange(35, 80), FString::Printf(TEXT("%s  OUT -> %-16s 6:12"), *Clock(T - 50), *Vic) });
			Log.Add({ T - R.RandRange(8, 20), FString::Printf(TEXT("%s  IN  <- %-16s MISSED"), *Clock(T - 14), bChild ? TEXT("Unknown") : *VicFirst) });
			Log.Add({ T + 1, TEXT("------   NO ACTIVITY FOR 74 MIN   (phone switched off?)") });
			Log.Add({ T + 75, FString::Printf(TEXT("%s  OUT -> %-16s 0:41"), *Clock(T + 75), TEXT("Unknown")) });
		}
		else if (K == EKind::Secret)
		{
			Log.Add({ T - R.RandRange(15, 40), FString::Printf(TEXT("%s  OUT -> %-16s 11:03   (number blocked on their bill)"), *Clock(T - 25), TEXT("Unknown")) });
			Log.Add({ T + R.RandRange(5, 30), FString::Printf(TEXT("%s  IN  <- %-16s 2:20"), *Clock(T + 15), *OtherName(Case, S, R)) });
		}
		else Log.Add({ T + R.RandRange(-10, 10), FString::Printf(TEXT("%s  IN  <- %-16s 18:44"), *Clock(T), Pick(Contacts, R)) });
		Log.Sort([](const FCall& A, const FCall& B) { return A.At < B.At; });
		L.Add(TEXT("TIME      DIR    CONTACT          LENGTH"));
		for (const FCall& Cl : Log) L.Add(Cl.Line);
		break;
	}
	case Texts:
	{
		auto Msg = [&](int32 At, const FString& Who, const FString& What) { L.Add(FString::Printf(TEXT("[%s] %s: %s"), *Clock(At), *Who, *What)); };
		Msg(T - R.RandRange(200, 400), Pick(Contacts, R), Pick(BoringTexts, R));
		Msg(T - R.RandRange(120, 199), TEXT("Me"), Pick(BoringTexts, R));
		if (bPlan)
		{
			if (bDom)
			{
				Msg(T - 90, VicFirst, TEXT("don't make a scene tonight. please"));
				Msg(T - 88, TEXT("Me"), TEXT("then don't give me a reason"));
				Msg(T + 95, TEXT("Me"), FString::Printf(TEXT("if anyone asks: %s"), *Plan.AlibiLie.ToLower()));
			}
			else
			{
				L.Add(FString::Printf(TEXT("[ 41 MESSAGES DELETED over the last month - recovered: headers only ]")));
				if (!Plan.LeanedOnName.IsEmpty()) { Msg(T - 1400, TEXT("Me"), TEXT("still ok for tomorrow? like we said")); Msg(T - 1395, Plan.LeanedOnName, TEXT("yeah. you owe me")); }
			}
		}
		else if (K == EKind::Culprit)
		{
			if (bChild)
			{
				Msg(T - 70, TEXT("Me"), TEXT("is the kid still at the park"));
				Msg(T - 66, TEXT("Unknown"), TEXT("why"));
				Msg(T - 65, TEXT("Me"), TEXT("no reason"));
			}
			else
			{
				Msg(T - 90, VicFirst, TEXT("we need to talk. tonight. I'm not doing this anymore"));
				Msg(T - 88, TEXT("Me"), TEXT("fine. I'll come to you"));
			}
			L.Add(FString::Printf(TEXT("[%s - %s]  9 MESSAGES DELETED   (recovered: headers only)"), *Clock(T - 30), *Clock(T + 40)));
			Msg(T + 95, TEXT("Me"), TEXT("if anyone asks I was home all night ok"));
		}
		else if (K == EKind::Secret)
		{
			const FString Who = OtherName(Case, S, R);
			Msg(T - 45, TEXT("Unknown"), TEXT("same place. don't be late. don't tell anyone"));
			Msg(T - 44, TEXT("Me"), TEXT("delete this"));
			Msg(T + 60, Who, TEXT("where were you tonight? you said you'd be home"));
			Msg(T + 62, TEXT("Me"), TEXT("long story. later"));
		}
		else
		{
			Msg(T - 10, Pick(Contacts, R), Pick(BoringTexts, R));
			Msg(T + 20, TEXT("Me"), Pick(BoringTexts, R));
		}
		break;
	}
	case Web:
	{
		auto Q = [&](int32 At, const FString& What) { L.Add(FString::Printf(TEXT("%s   search: \"%s\""), *Clock(At), *What)); };
		Q(T - R.RandRange(500, 900), Pick(BoringSearches, R));
		Q(T - R.RandRange(300, 499), Pick(BoringSearches, R));
		if (bPlan)
		{
			if (Plan.Has(TEXT("web")))
			{
				Q(T - 21 * 1440, FString::Printf(TEXT("%s address"), *Vic.ToLower()));
				Q(T - 15 * 1440, MeansSearch(Case.Weapon, 0));
				Q(T - 3 * 1440, FString::Printf(TEXT("%s floor plan"), *Place.ToLower()));
			}
			else if (!bDom) Q(T - 1440, TEXT("[ private browsing - nothing kept for a month ]"));
			if (bDom) { Q(T + 110, TEXT("how long do cell phone records last")); Q(T + 115, TEXT("does bleach get rid of blood")); }
		}
		else if (K == EKind::Culprit)
		{
			Q(T - 1440 - R.RandRange(60, 300), MeansSearch(Case.Weapon, 0));
			Q(T - 200, FString::Printf(TEXT("%s hours"), *Place.ToLower()));
			Q(T + 110, MeansSearch(Case.Weapon, 1));
			Q(T + 140, TEXT("how long do cell phone records last"));
			Q(T + 150, TEXT("[ private browsing session - history cleared ]"));
		}
		else if (K == EKind::Secret)
		{
			Q(T - 120, TEXT("how to tell if your phone is being tracked"));
			Q(T + 80, TEXT("can police read deleted texts"));
			Q(T + 90, Pick(BoringSearches, R));
		}
		else
		{
			Q(T - 30, Pick(BoringSearches, R));
			Q(T + 140, FString::Printf(TEXT("%s news"), *Place.ToLower()));
		}
		break;
	}
	case Photos:
	{
		auto P = [&](int32 At, const FString& What) { L.Add(FString::Printf(TEXT("IMG_%04d  %s   %s"), R.RandRange(1000, 9999), *Clock(At), *What)); };
		P(T - R.RandRange(1440, 4000), Pick(BoringPhotos, R));
		P(T - R.RandRange(300, 1400), Pick(BoringPhotos, R));
		if (bPlan)
		{
			if (bDom) P(T - 110, FString::Printf(TEXT("A selfie with %s at the bar, both smiling. The last photo of them."), *VicFirst));
			else if (Plan.Care < 80)
			{
				P(T - 3 * 1440, FString::Printf(TEXT("%s from across the street, at night. Nobody in shot."), *Place));
				P(T - 3 * 1440 + 2, FString::Printf(TEXT("The same - zoomed in on %s."), *Plan.Entry));
			}
		}
		else if (K == EKind::Culprit)
		{
			P(T - 160, FString::Printf(TEXT("The front of %s from across the street, at night. Nobody in shot."), *Place));
			P(T - 158, TEXT("The same - zoomed in on the side entrance."));
			L.Add(FString::Printf(TEXT("IMG_----  %s   [ deleted from the roll - thumbnail only: dark, blurred, a hand ]"), *Clock(T + 3)));
		}
		else if (K == EKind::Secret)
		{
			P(T - 40, TEXT("A hotel-room mirror selfie. Someone else's coat on the chair behind."));
			P(T - 20, TEXT("A bar tab, photographed - two drinks, two names scribbled off."));
		}
		else P(T + R.RandRange(-30, 30), Pick(BoringPhotos, R));
		break;
	}
	case Cell:
	{
		// which towers the phone pinged around the time
		const FString Near = FString::Printf(TEXT("%s (covers %s)"), Towers[Case.Number % UE_ARRAY_COUNT(Towers)], *Place);
		FString Home = Pick(Towers, R); if (Near.StartsWith(Home)) Home = Towers[(Case.Number + 3) % UE_ARRAY_COUNT(Towers)];
		auto Ping = [&](int32 At, const FString& Tower) { L.Add(FString::Printf(TEXT("%s   %s"), *Clock(At), *Tower)); };
		Ping(T - 180, Home);
		if (bPlan)
		{
			if (bDom || Plan.Has(TEXT("cell"))) { Ping(T - 60, Near); Ping(T - 5, Near); Ping(T + 40, Near); Ping(T + 90, Home); }
			else { Ping(T - 60, Home); Ping(T, Home); Ping(T + 60, Home); L.Add(TEXT("")); L.Add(TEXT("Note: the phone sat at home all night. Phones don't kill people - but they make good alibis.")); }
		}
		else if (K == EKind::Culprit)
		{
			Ping(T - 60, Towers[(Case.Number + 5) % UE_ARRAY_COUNT(Towers)]);
			Ping(T - 22, Near);
			Ping(T + 1, TEXT("-- no signal: phone off or out of service --"));
			Ping(T + 75, Home);
			L.Add(TEXT(""));
			L.Add(TEXT("Note: one tower covers several blocks. It puts the phone in the area, not in the room."));
		}
		else if (K == EKind::Secret)
		{
			// near too - for their own reasons (a red herring until their secret comes out)
			Ping(T - 30, Near);
			Ping(T + 20, Near);
			Ping(T + 90, Home);
		}
		else
		{
			Ping(T - 30, Home);
			Ping(T + 10, Home);
			Ping(T + 60, Home);
		}
		break;
	}
	default: break;
	}
	return L;
}
}
