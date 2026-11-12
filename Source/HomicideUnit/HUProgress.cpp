#include "HUProgress.h"
#include "HUPlayerState.h"
#include "HUCaseSubsystem.h"
#include "HUEvidence.h"
#include "HUSecurityDesk.h"
#include "HUVictim.h"
#include "HUSuspectActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

namespace HUProgress
{
const TCHAR* SpecName(uint8 Spec)
{
	switch (Spec) { case Forensics: return TEXT("Forensics"); case Interrogator: return TEXT("Interrogator"); case Tech: return TEXT("Tech"); case Field: return TEXT("Field"); default: return TEXT("No specialty"); }
}
const TCHAR* SpecPerk(uint8 Spec)
{
	switch (Spec)
	{
	case Forensics:    return TEXT("Bags evidence and works the body faster; finds trace and dark evidence without a partner's light.");
	case Interrogator: return TEXT("Every interview move lands harder - suspects crack sooner.");
	case Tech:         return TEXT("Pulls security tapes in a fraction of the time.");
	case Field:        return TEXT("Runs faster and cuffs and searches suspects quicker.");
	default:           return TEXT("Pick one at the name plate. It levels up with every case you close.");
	}
}
int32 SpecLevelXP(int32 Level) { static const int32 T[] = { 0, 0, 200, 500, 1000, 1800 }; return T[FMath::Clamp(Level, 1, 5)]; }
int32 SpecLevel(int32 SpecXP) { int32 L = 1; while (L < 5 && SpecXP >= SpecLevelXP(L + 1)) L++; return L; }

int32 RankXP(int32 Rank) { static const int32 T[RankCount] = { 0, 300, 800, 1600, 3000, 5000 }; return T[FMath::Clamp(Rank, 0, RankCount - 1)]; }
int32 RankOf(int32 XP) { int32 R = 0; while (R < RankCount - 1 && XP >= RankXP(R + 1)) R++; return R; }
const TCHAR* RankName(int32 Rank)
{
	static const TCHAR* N[RankCount] = { TEXT("Probationary Detective"), TEXT("Detective"), TEXT("Detective II"), TEXT("Detective Sergeant"), TEXT("Lieutenant"), TEXT("Major Crimes") };
	return N[FMath::Clamp(Rank, 0, RankCount - 1)];
}

const TCHAR* UpgradeName(int32 i)
{
	static const TCHAR* N[UpgradeCount] = { TEXT("Forensics Lab"), TEXT("Interview Suite"), TEXT("Video Deck"), TEXT("Overtime Budget") };
	return N[FMath::Clamp(i, 0, UpgradeCount - 1)];
}
const TCHAR* UpgradePitch(int32 i)
{
	static const TCHAR* N[UpgradeCount] = {
		TEXT("Everyone bags evidence 25% faster."),
		TEXT("A one-way mirror and a hard chair: every interview move lands 20% harder."),
		TEXT("Pulling the security tapes takes half the time."),
		TEXT("Closed cases pay the whole squad 20% more.") };
	return N[FMath::Clamp(i, 0, UpgradeCount - 1)];
}
int32 UpgradePrice(int32 i) { static const int32 P[UpgradeCount] = { 400, 500, 300, 600 }; return P[FMath::Clamp(i, 0, UpgradeCount - 1)]; }

// standalone (or before the player states arrive): this machine's own save
static const UHUCaseSubsystem* LocalCases(const UWorld* W) { return W && W->GetGameInstance() ? W->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr; }

int32 SquadUpgrades(const UWorld* World)
{
	int32 U = 0;
	if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
		for (const APlayerState* P : GS->PlayerArray) if (const AHUPlayerState* HP = Cast<AHUPlayerState>(P)) U |= HP->Upgrades;
	if (U == 0) if (const UHUCaseSubsystem* C = LocalCases(World)) if (World->GetNetMode() == NM_Standalone) U = C->Upgrades;
	return U;
}
int32 BestRank(const UWorld* World)
{
	int32 R = 0;
	if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
		for (const APlayerState* P : GS->PlayerArray) if (const AHUPlayerState* HP = Cast<AHUPlayerState>(P)) R = FMath::Max(R, HP->Rank);
	if (const UHUCaseSubsystem* C = LocalCases(World)) R = FMath::Max(R, RankOf(C->SquadXP));
	return R;
}
int32 SquadSize(const UWorld* World)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? FMath::Max(1, GS->PlayerArray.Num()) : 1;
}
const AHUPlayerState* StateOf(const APawn* Pawn) { return Pawn ? Pawn->GetPlayerState<AHUPlayerState>() : nullptr; }

// a specialty's perk at its level: level 1 gives Base, each level adds a quarter more of the bonus
static float Perk(const AHUPlayerState* S, uint8 Spec, float Base)
{
	if (!S || S->Specialty != Spec) return 1.f;
	return 1.f + (Base - 1.f) * (1.f + 0.25f * (FMath::Clamp(S->SpecLevel, 1, 5) - 1));
}

float TaskMult(const APawn* User, const UObject* Target)
{
	const AHUPlayerState* S = StateOf(User);
	const int32 U = SquadUpgrades(User ? User->GetWorld() : nullptr);
	float M = 1.f;
	if (Target && (Target->IsA<AHUEvidence>() || Target->IsA<AHUVictim>()))
	{
		M *= Perk(S, Forensics, 1.4f);
		if (U & Lab) M *= 1.25f;
	}
	if (Target && Target->IsA<AHUSecurityDesk>())
	{
		M *= Perk(S, Tech, 2.f);
		if (U & TapeDeck) M *= 2.f;
	}
	if (Target && Target->IsA<AHUSuspectActor>()) M *= Perk(S, Field, 1.5f);
	return M;
}

float PressureMult(const APawn* User, const AActor* Suspect, bool* bOutTagTeam)
{
	const AHUPlayerState* S = StateOf(User);
	float M = Perk(S, Interrogator, 1.3f);
	if (SquadUpgrades(User ? User->GetWorld() : nullptr) & Suite) M *= 1.2f;
	// TAG-TEAM: a partner with a different specialty in the room with you
	bool bTag = false;
	if (User && Suspect && S && S->Specialty != None)
		if (const AGameStateBase* GS = User->GetWorld()->GetGameState())
			for (const APlayerState* P : GS->PlayerArray)
			{
				const AHUPlayerState* HP = Cast<AHUPlayerState>(P);
				const APawn* Other = P ? P->GetPawn() : nullptr;
				if (HP && Other && Other != User && HP->Specialty != None && HP->Specialty != S->Specialty
					&& FVector::Dist(Other->GetActorLocation(), Suspect->GetActorLocation()) < 600.f) { bTag = true; break; }
			}
	if (bTag) M *= 1.25f;
	if (bOutTagTeam) *bOutTagTeam = bTag;
	return M;
}

float RunMult(const APawn* User) { return Perk(StateOf(User), Field, 1.1f); }

int32 CaseXP(int32 Outcome, int32 Wrong, int32 Squad)
{
	int32 XP = Outcome == 2 ? 120 : Outcome == 1 ? 70 : Outcome == -1 ? 15 : 0;
	XP -= 30 * Wrong;
	XP = FMath::Max(0, XP);
	// working it together pays: +25% per squadmate
	return FMath::RoundToInt(XP * (1.f + 0.25f * (FMath::Clamp(Squad, 1, 4) - 1)));
}
}
