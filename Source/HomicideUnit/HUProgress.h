// Squad progression: what makes closing cases pay off, together.
//   RANK      - squad XP, earned by every detective in the room when a case is closed (more with more squadmates).
//               Higher ranks open the Veteran difficulty and harder places.
//   UPGRADES  - bought for the precinct with your own cash at the upgrades board. While you're in the session the
//               whole squad gets it, so detectives with different upgrades make a stronger squad.
//   SPECIALTY - picked at the name plate. Each levels up with the cases you close and makes one job faster or harder
//               hitting. A mixed squad also gets a tag-team bonus in interviews.
// Everything here is read the same way on every machine (from the replicated AHUPlayerState), so the shared scene
// stays identical for the whole squad.
#pragma once

#include "CoreMinimal.h"

class APawn;
class UObject;
class UWorld;
class AActor;
class AHUPlayerState;

namespace HUProgress
{
	// ---- specialties ----
	enum ESpec : uint8 { None = 0, Forensics, Interrogator, Tech, Field, SpecCount };
	const TCHAR* SpecName(uint8 Spec);
	const TCHAR* SpecPerk(uint8 Spec);
	// 1..5 from that specialty's XP
	int32 SpecLevel(int32 SpecXP);
	int32 SpecLevelXP(int32 Level); // XP needed to reach a level

	// ---- rank ----
	constexpr int32 RankCount = 6;
	int32 RankOf(int32 XP);
	const TCHAR* RankName(int32 Rank);
	int32 RankXP(int32 Rank); // XP needed to reach a rank
	// what each rank opens (the Veteran difficulty at 2, the hotel and other big places at 1)
	constexpr int32 VeteranRank = 2, BigPlaceRank = 1;

	// ---- precinct upgrades (bits) ----
	enum EUpgrade : int32 { Lab = 1, Suite = 2, TapeDeck = 4, Overtime = 8 };
	constexpr int32 UpgradeCount = 4;
	const TCHAR* UpgradeName(int32 Index);
	const TCHAR* UpgradePitch(int32 Index);
	int32 UpgradePrice(int32 Index);

	// ---- the squad in this session ----
	// upgrades anyone here owns, the best rank here, how many detectives are here
	int32 SquadUpgrades(const UWorld* World);
	int32 BestRank(const UWorld* World);
	int32 SquadSize(const UWorld* World);
	const AHUPlayerState* StateOf(const APawn* Pawn);

	// how much faster User does a held job on Target (1 = normal, 2 = twice as fast)
	float TaskMult(const APawn* User, const UObject* Target);
	// how much harder User's interview moves land on a suspect (tag-team: a partner with another specialty nearby)
	float PressureMult(const APawn* User, const AActor* Suspect, bool* bOutTagTeam = nullptr);
	// run speed (Field specialty)
	float RunMult(const APawn* User);
	// XP for closing a case: 2 solid close, 1 thin close, 0 nobody, -1 wrong arrest
	int32 CaseXP(int32 Outcome, int32 Wrong, int32 SquadSize);
}
