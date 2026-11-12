// The offender's brain: before a case starts, the culprit PLANS the crime, carries it out, makes mistakes and
// covers up. Everything is rules plus the case's seed - no language model - so it takes well under a millisecond,
// and every machine in the squad gets the same plan from the same case (nothing to send over the network).
//
//   1. WHO   - the offender type (domestic, revenge...) decides how they think: heat of the moment or weeks of prep.
//   2. PLAN  - motive, weapon and where it came from, the way in, the spot, the way out, the alibi they'll tell.
//              Only spots the MAP actually has (see Spots) - a back door plan needs a back door.
//   3. NIGHT - a timeline of what they really did, minute by minute around the time of death.
//   4. SLIPS - what went wrong. Careless types slip more; every type slips at least three times, so every case
//              stays solvable. Each slip leaves a TRACE (evidence, a camera, a phone record, a witness).
//   5. COVER - the alibi story, what they deleted, who they leaned on to lie, where the weapon went.
//
// Step 1 of the case rework: the plan exists and can be read on the desk computer's CRIME PLAN (debug) app.
// Next: statements, phones, footage and scene evidence get written FROM the plan instead of on their own.
#pragma once

#include "CoreMinimal.h"

struct FHUCase;

namespace HUOffender
{
	enum class EType : uint8 { Domestic, Revenge, Count };

	// the places a map offers the offender (hand-built maps list their own; generated ones get generic rooms)
	struct FSpots
	{
		TArray<FString> Entries, KillSpots, Exits, Dumps, Cameras;
	};
	HOMICIDEUNIT_API FSpots SpotsFor(const FString& Place);

	struct FStep
	{
		int32 Minute = 0;   // minutes from midnight on the night (days before: see Day)
		int32 Day = 0;      // 0 = the night of; -14 = two weeks before
		FString Where, What;
		FString Trace;      // what this step leaves behind for the squad ("" = nothing)
	};

	enum class ESlipKind : uint8 { Scene, Tape, Phone, Witness };
	struct FSlipInfo
	{
		FString Tag;            // "blood", "camera", "cell"...: what other systems ask about (FPlan::Has)
		ESlipKind Kind = ESlipKind::Scene;
		int32 Breaks = 0;       // 1 = its evidence breaks the means lie, 2 = the alibi
		FString ClueName, ClueDetail;
	};

	struct FPlan
	{
		bool Has(const TCHAR* Tag) const;
		TArray<FSlipInfo> SlipInfo;
		FString MeansLie, AlibiLie;           // the two lies the plan makes them tell
		bool bValid = false;
		EType Type = EType::Domestic;
		int32 Care = 50;                      // 0..100: how careful they are
		FString KillerId, KillerName, VictimName;
		FString Relationship, Motive;
		FString Weapon, WeaponSource;
		FString Entry, KillSpot, Exit, WeaponDump;
		FString AlibiStory, AlibiTruth;
		FString LeanedOnId, LeanedOnName;     // a suspect they talked into backing the alibi ("" = nobody)
		TArray<FStep> Timeline;               // in order
		TArray<FString> Slips;                // what went wrong (each one also has a step with its trace)
		TArray<FString> CoverUp;
	};

	HOMICIDEUNIT_API const TCHAR* TypeName(EType T);
	// the plan for a case (same case = same plan, on every machine)
	HOMICIDEUNIT_API FPlan Make(const FHUCase& Case);
	// write the plan into the case: the culprit's means and alibi lies and the evidence that breaks them, and the
	// friend's cover story. Call once the place is final (StartCase), on every machine.
	HOMICIDEUNIT_API void ApplyToCase(FHUCase& Case);
	// the whole plan as readable lines (debug screen now, the "how it happened" reveal later)
	HOMICIDEUNIT_API TArray<FString> Describe(const FPlan& P);
}
