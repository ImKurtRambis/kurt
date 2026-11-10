// Offender profiles: what kind of person did this. Every case has one true profile. The evidence that
// proves lies also points at that profile (each piece is a "sign" of it); dead ends point at a decoy.
// Suspects have records, and the killer's record fits the true profile - but so does an innocent or two,
// so the profile narrows the field and the lies close it.
#pragma once

#include "CoreMinimal.h"
#include "HUProfiles.generated.h"

UENUM(BlueprintType)
enum class EHUProfile : uint8
{
	None,
	SerialKiller,
	Arsonist,
	Poisoner,
	ChildPredator,
	ContractKiller,
	Passion,
	Robber,
	ConArtist,
	Enforcer,
	Stalker,
	Count UMETA(Hidden)
};

namespace HUProfiles
{
	struct FInfo { const TCHAR* Name; const TCHAR* Blurb; const TCHAR* Signs[5]; const TCHAR* Records[3]; };

	inline const FInfo& Get(EHUProfile P)
	{
		static const FInfo Table[] = {
			{ TEXT("UNKNOWN"), TEXT(""), { TEXT("") , TEXT(""), TEXT(""), TEXT(""), TEXT("") }, { TEXT("no record"), TEXT("no record"), TEXT("no record") } },
			{ TEXT("SERIAL KILLER"), TEXT("Has done this before and will again. Ritual, trophies, a pattern."),
				{ TEXT("a trophy cut from the victim's clothing"), TEXT("a calling-card mark matching two unsolved cases"), TEXT("newspaper clippings about other victims"), TEXT("signs the body was posed on purpose"), TEXT("a city map with three other spots circled") },
				{ TEXT("questioned in two unsolved deaths, never charged"), TEXT("lived near both of the earlier victims"), TEXT("a sealed, court-ordered psych evaluation") } },
			{ TEXT("ARSONIST"), TEXT("Fire is the point, or the cover. Comes back to watch."),
				{ TEXT("accelerant residue"), TEXT("a collection of burnt matchbooks"), TEXT("a scanner tuned to the fire department's channel"), TEXT("singed hairs on a coat cuff"), TEXT("a fire set to cover the scene that never caught") },
				{ TEXT("a juvenile record for setting fires"), TEXT("washed out of the fire academy"), TEXT("questioned about a warehouse fire last year") } },
			{ TEXT("POISONER"), TEXT("Patient, careful, close to the victim. Never has to touch them."),
				{ TEXT("a vial with traces of arsenic"), TEXT("a drink that was mixed separately from the others"), TEXT("a pharmacy receipt for rat poison"), TEXT("gloves with a chemical smell"), TEXT("notes on doses in tiny handwriting") },
				{ TEXT("a former pharmacist whose license was pulled"), TEXT("a late spouse who died of 'stomach trouble'"), TEXT("bought strychnine 'for rats' twice this year") } },
			{ TEXT("CHILD PREDATOR"), TEXT("Preys on children and kills to stay hidden. The victim likely found out."),
				{ TEXT("chat logs with a minor arranging a meeting"), TEXT("photos of a schoolyard taken from a parked car"), TEXT("a false name used to volunteer with a youth group"), TEXT("a second phone with a single contact"), TEXT("the victim's note saying they'd go to the police about someone") },
				{ TEXT("a registered sex offender under another name"), TEXT("banned from coaching a youth league"), TEXT("a restraining order filed by a family") } },
			{ TEXT("CONTRACT KILLER"), TEXT("Paid to do it. No personal link to the victim, and a way out of town."),
				{ TEXT("a cash envelope with a photo of the victim"), TEXT("a weapon with the serial numbers filed off"), TEXT("a one-way train ticket bought this morning"), TEXT("no personal connection to the victim at all"), TEXT("a payphone number written in a matchbook") },
				{ TEXT("an army marksman with a dishonorable discharge"), TEXT("a known associate of the Ferro crew"), TEXT("arrested carrying a silencer, case dismissed") } },
			{ TEXT("CRIME OF PASSION"), TEXT("Love, jealousy, rage. Unplanned - whatever was at hand."),
				{ TEXT("love letters torn in half"), TEXT("a wedding ring thrown in the trash"), TEXT("a jealous diary entry from last night"), TEXT("a weapon grabbed on the spot, no planning"), TEXT("lipstick on a collar that isn't the spouse's") },
				{ TEXT("two domestic disturbance calls this year"), TEXT("the victim's ex"), TEXT("slashed a rival's tires in the spring") } },
			{ TEXT("STICKUP ARTIST"), TEXT("In it for the money. Knows the cash routine, has a way out."),
				{ TEXT("a stocking mask"), TEXT("a list of the place's cash-pickup times"), TEXT("marked bills"), TEXT("a getaway route sketched on a napkin"), TEXT("a stolen license plate") },
				{ TEXT("did five years for armed robbery"), TEXT("a pawn-shop regular with no job"), TEXT("owes money to a bookie") } },
			{ TEXT("CON ARTIST"), TEXT("Lives on paper: fake names, fake numbers. Violence only when cornered."),
				{ TEXT("signatures practised over and over on scrap paper"), TEXT("two driver's licenses with two names"), TEXT("a ledger whose numbers don't add up"), TEXT("a business card for a company that doesn't exist"), TEXT("receipts in someone else's name") },
				{ TEXT("convicted of check fraud in another state"), TEXT("three different names on file"), TEXT("sued for running a pyramid scheme") } },
			{ TEXT("MOB ENFORCER"), TEXT("Muscle for somebody else. A debt, a warning, a message."),
				{ TEXT("a debt ledger with the victim's name crossed out"), TEXT("brass knuckles"), TEXT("a warning note: 'pay up'"), TEXT("a matchbook from a mob-owned bar"), TEXT("a glove stained across the knuckles") },
				{ TEXT("collects for a loan shark"), TEXT("assault charges dropped when the witnesses 'forgot'"), TEXT("union muscle on the docks") } },
			{ TEXT("STALKER"), TEXT("Obsessed with the victim. Watched them for a long time first."),
				{ TEXT("dozens of photos of the victim taken from a distance"), TEXT("a copy of the victim's house key"), TEXT("a notebook logging the victim's movements"), TEXT("letters the victim sent back unopened"), TEXT("a pair of binoculars") },
				{ TEXT("a restraining order from a former partner"), TEXT("followed a coworker home, reported once"), TEXT("fired for watching a customer") } },
		};
		const int32 I = FMath::Clamp((int32)P, 0, (int32)EHUProfile::Count - 1);
		return Table[I];
	}
	inline const TCHAR* Name(EHUProfile P) { return Get(P).Name; }

	// MAPS with their own offenders: a place built by hand (see HUCrimeScene) only gets the kinds of criminal
	// that fit it. Empty = any (the generated places).
	inline TArray<EHUProfile> ForPlace(const FString& Place)
	{
		const FString P = Place.ToUpper();
		if (P.Contains(TEXT("HOTEL"))) return { EHUProfile::Stalker, EHUProfile::Passion, EHUProfile::Poisoner };
		return {};
	}

	// which profiles fit a kind of crime
	inline TArray<EHUProfile> ForCrime(const FString& Crime, const FString& Weapon)
	{
		const FString C = Crime.ToLower(), W = Weapon.ToLower();
		if (C.StartsWith(TEXT("predator-"))) return { EHUProfile::ChildPredator }; // Special Victims cases
		if (C.Contains(TEXT("forg"))) return { EHUProfile::ConArtist, EHUProfile::Robber };
		if (C.Contains(TEXT("heist")) || C.Contains(TEXT("robber")) || C.Contains(TEXT("theft"))) return { EHUProfile::Robber, EHUProfile::ConArtist, EHUProfile::Enforcer };
		if (C.Contains(TEXT("assault"))) return { EHUProfile::Passion, EHUProfile::Enforcer, EHUProfile::Stalker, EHUProfile::ChildPredator };
		TArray<EHUProfile> Out = { EHUProfile::SerialKiller, EHUProfile::ChildPredator, EHUProfile::ContractKiller, EHUProfile::Passion, EHUProfile::Arsonist, EHUProfile::Stalker, EHUProfile::Enforcer };
		if (W.Contains(TEXT("poison")) || W.Contains(TEXT("pill")) || W.Contains(TEXT("drink")) || W.Contains(TEXT("arsenic"))) Out.Add(EHUProfile::Poisoner);
		return Out;
	}
}
