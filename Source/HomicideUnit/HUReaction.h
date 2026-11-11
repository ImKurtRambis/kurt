// What a suspect does when you talk to them: the THINKING brain (the local language model) picks the words,
// the emotion and a body action; the VOICE (Chatterbox / Kokoro) speaks the words with that emotion, and the
// figure acts it out. Shared by HUBrainSubsystem (which asks for it) and HUSuspectActor (which performs it).
#pragma once

#include "CoreMinimal.h"

namespace HUReact
{
	enum EEmotion : uint8 { Calm, Nervous, Angry, Scared, Defiant, Sad, Broken, EmotionCount };
	enum EAction : uint8 { None, LookAway, FaceYou, CrossArms, LeanIn, StepBack, HeadDown, ShakeHead, Nod, Sit, Stand, HandsUp, Fidget, ActionCount };

	inline const TCHAR* EmotionName(uint8 E)
	{
		static const TCHAR* N[] = { TEXT("calm"), TEXT("nervous"), TEXT("angry"), TEXT("scared"), TEXT("defiant"), TEXT("sad"), TEXT("broken") };
		return E < EmotionCount ? N[E] : TEXT("calm");
	}
	inline const TCHAR* ActionName(uint8 A)
	{
		static const TCHAR* N[] = { TEXT("none"), TEXT("look_away"), TEXT("face_you"), TEXT("cross_arms"), TEXT("lean_in"), TEXT("step_back"), TEXT("head_down"),
			TEXT("shake_head"), TEXT("nod"), TEXT("sit"), TEXT("stand"), TEXT("hands_up"), TEXT("fidget") };
		return A < ActionCount ? N[A] : TEXT("none");
	}
	// what the HUD says they're doing
	inline const TCHAR* ActionLabel(uint8 A)
	{
		static const TCHAR* N[] = { TEXT(""), TEXT("looks away"), TEXT("looks right at you"), TEXT("crosses their arms"), TEXT("leans in"), TEXT("steps back"), TEXT("drops their head"),
			TEXT("shakes their head"), TEXT("nods"), TEXT("sits down"), TEXT("stands up"), TEXT("shows their hands"), TEXT("fidgets") };
		return A < ActionCount ? N[A] : TEXT("");
	}
	inline uint8 EmotionFrom(const FString& S) { for (uint8 i = 0; i < EmotionCount; i++) if (S.TrimStartAndEnd().Equals(EmotionName(i), ESearchCase::IgnoreCase)) return i; return Calm; }
	inline uint8 ActionFrom(const FString& S) { const FString T = S.TrimStartAndEnd().Replace(TEXT(" "), TEXT("_")); for (uint8 i = 0; i < ActionCount; i++) if (T.Equals(ActionName(i), ESearchCase::IgnoreCase)) return i; return None; }
	// how the voice delivers it: Chatterbox's exaggeration (0.25..1) and the speaking speed
	inline void Delivery(uint8 E, float& Exaggeration, float& Speed)
	{
		static const float X[] = { 0.35f, 0.65f, 0.95f, 0.8f, 0.6f, 0.55f, 0.5f };
		static const float S[] = { 1.f, 1.12f, 1.08f, 1.15f, 1.f, 0.9f, 0.85f };
		Exaggeration = E < EmotionCount ? X[E] : 0.5f; Speed = E < EmotionCount ? S[E] : 1.f;
	}
}

// PREMADE LINES: said straight away, in the suspect's own voice, while the thinking brain works out the real
// answer - so the conversation never stalls. Recorded once when the interview starts (see AHUSuspectActor).
namespace HUFiller
{
	enum EKind : uint8 { Question, Order, Evidence, Record, Accused, KindCount };
	inline const TArray<FString>& Lines(uint8 K)
	{
		static const TArray<FString> L[KindCount] = {
			{ TEXT("Let me get this straight..."), TEXT("Hm. Okay..."), TEXT("Well..."), TEXT("Let me think.") },
			{ TEXT("What, now?"), TEXT("Excuse me?"), TEXT("Seriously?") },
			{ TEXT("What's that supposed to be?"), TEXT("Where'd you get that?"), TEXT("Hold on..."), TEXT("Let me see that.") },
			{ TEXT("Oh, here we go."), TEXT("That was a long time ago.") },
			{ TEXT("Excuse me?"), TEXT("Whoa, whoa..."), TEXT("You've got some nerve.") },
		};
		return L[K < KindCount ? K : 0];
	}
}

// the thinking brain's answer
struct FHUReaction
{
	FString Say;
	uint8 Emotion = HUReact::Calm;
	uint8 Action = HUReact::None;
};
