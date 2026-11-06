// The case system. Every case is three independent picks:
//
//   WHAT HAPPENED  - an incident: homicide, assault, robbery, a missing person, a fire
//   WHO DID IT     - a hidden offender profile (serial killer, arsonist, poisoner, child predator, contract killer,
//                    crime of passion, stickup artist, con artist, mob enforcer, stalker). Any profile can be
//                    behind any incident - some are likelier for some incidents, none is ruled out.
//   WHERE          - any location, and the map is generated per case from MapSeed.
//
// The title and the briefing only describe the incident. The offender's lies - and the evidence that breaks
// them - are written for their profile, so the evidence is how you find out what you're dealing with.
// Dead ends point at a decoy profile. Innocent people lie too, about their own small secrets.
#pragma once

#include "CoreMinimal.h"
#include "HUCaseTypes.h"

namespace HUCaseGen
{
	HOMICIDEUNIT_API TArray<FHUCase> MakeCases(int32 FirstNumber, int32 Count, int32 Seed);
	// every location name a case can happen at (each has its own look; see HUCrimeScene)
	HOMICIDEUNIT_API const TArray<FString>& Places();
}
