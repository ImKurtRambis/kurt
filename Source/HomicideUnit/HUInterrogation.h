// Interrogation rules: the game decides what's true; how a suspect behaves follows their PRESSURE.
//
//   Confront a statement with the evidence that breaks it  -> the lie goes on the record, pressure +35
//   Confront with evidence that doesn't break it           -> they relax, pressure -15 (three misses: they lawyer up)
//   Press them on a statement, no evidence                 -> pressure +6, less each time on the same statement
//   Their record fits the offender profile                 -> pressure +12 (once)
//
// Pressure 0-100 sets their state: calm, defensive, rattled, cracking, broken. At 100 the one who did it
// CONFESSES (only with at least two of their lies on the record); an innocent breaks down, owns up to their
// little secret, and swears they didn't do it.
// Later the AI brain and the voice read the state to decide how they say things; the facts never come from the AI.
#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EHUSuspectMood : uint8 { Calm, Defensive, Rattled, Cracking, Broken, Lawyered };

namespace HUInterrogation
{
	inline EHUSuspectMood MoodFor(float Pressure, bool bLawyered)
	{
		if (bLawyered) return EHUSuspectMood::Lawyered;
		return Pressure >= 100.f ? EHUSuspectMood::Broken : Pressure >= 75.f ? EHUSuspectMood::Cracking : Pressure >= 50.f ? EHUSuspectMood::Rattled
			: Pressure >= 25.f ? EHUSuspectMood::Defensive : EHUSuspectMood::Calm;
	}
	inline const TCHAR* MoodName(EHUSuspectMood M)
	{
		switch (M) { case EHUSuspectMood::Calm: return TEXT("CALM"); case EHUSuspectMood::Defensive: return TEXT("DEFENSIVE"); case EHUSuspectMood::Rattled: return TEXT("RATTLED");
		case EHUSuspectMood::Cracking: return TEXT("CRACKING"); case EHUSuspectMood::Broken: return TEXT("BROKEN"); default: return TEXT("LAWYERED UP"); }
	}
	// how they react when pressed, by mood (written lines - the AI replaces these later)
	inline const TCHAR* PressLine(EHUSuspectMood M, int32 Seed)
	{
		static const TCHAR* Calm[] = { TEXT("I already told you. That's what happened."), TEXT("Ask me again, you'll get the same answer."), TEXT("I'm not sure what you want me to say, detective.") };
		static const TCHAR* Def[] = { TEXT("Why do you keep asking me that?"), TEXT("I don't like what you're implying."), TEXT("Look - I said what I said.") };
		static const TCHAR* Rat[] = { TEXT("I... I don't remember exactly, okay? It was a long night."), TEXT("Can I get some water? Please?"), TEXT("Why are you looking at me like that?") };
		static const TCHAR* Crack[] = { TEXT("Stop - just stop. I need a minute."), TEXT("You don't understand, it's not - it isn't what it looks like."), TEXT("I want - I don't know what I want. Just stop.") };
		const TCHAR** Pool = M == EHUSuspectMood::Calm ? Calm : M == EHUSuspectMood::Defensive ? Def : M == EHUSuspectMood::Rattled ? Rat : Crack;
		return Pool[FMath::Abs(Seed) % 3];
	}
	inline const TCHAR* WrongLine(int32 Seed)
	{
		static const TCHAR* L[] = { TEXT("That? That's got nothing to do with me."), TEXT("You're reaching, detective. And we both know it."), TEXT("Is that all you've got?") };
		return L[FMath::Abs(Seed) % 3];
	}
}
