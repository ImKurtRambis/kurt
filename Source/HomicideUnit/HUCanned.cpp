#include "HUCanned.h"
#include "HUCaseSubsystem.h"
#include "HUBrainSubsystem.h"
#include "HUInterrogation.h"

namespace
{
	using ETopic = UHUBrainSubsystem::ETopic;

	// what was said, as lower-case words with single spaces between and around: " where were you last night "
	FString Words(const FString& Said)
	{
		FString Out = TEXT(" ");
		for (const TCHAR C : Said)
		{
			if (FChar::IsAlpha(C) || C == '\'') Out.AppendChar(FChar::ToLower(C));
			else if (!Out.EndsWith(TEXT(" "))) Out.AppendChar(' ');
		}
		if (!Out.EndsWith(TEXT(" "))) Out.AppendChar(' ');
		return Out;
	}

	// What one of their statements has to contain to be the answer to a question, likeliest first.
	// (How they knew the victim is found by the victim's name instead; the two "who did it" questions have no
	// statement that answers them.)
	const TCHAR* const Tells[UHUBrainSubsystem::TopicCount][8] = {
		/* DidYouDoIt   */ { nullptr },
		/* WhoDidIt     */ { nullptr },
		/* Vouch        */ { TEXT("'ll tell you"), TEXT("can tell you"), TEXT("i called"), TEXT("i was with"), TEXT("ask "), nullptr },
		/* SawAnything  */ { TEXT("i saw"), TEXT("i heard"), TEXT("i noticed"), TEXT("flickered"), TEXT("slam"), nullptr },
		/* KnewVictim   */ { nullptr },
		/* WhereWereYou */ { TEXT("most of the night"), TEXT("all night"), TEXT("all evening"), TEXT("i was "), TEXT("i got here"), TEXT("i came in"), TEXT("i left"), nullptr },
	};

	// which of their statements answers this question (INDEX_NONE: none does). bOutBeside: it only half answers it -
	// "I came in through the front door" isn't where they were.
	int32 StatementFor(const FHUSuspect& S, int32 Topic, const FString& Victim, bool& bOutBeside)
	{
		bOutBeside = false;
		if (Topic == ETopic::KnewVictim)
		{
			// "Tommy Hale, age 9" -> tommy: whichever statement speaks of them
			FString First = Victim.ToLower();
			int32 Cut;
			if (First.FindChar(' ', Cut)) First.LeftInline(Cut);
			if (First.Len() < 2) return INDEX_NONE;
			for (int32 i = 0; i < S.Statements.Num(); i++) if (S.Statements[i].Text.ToLower().Contains(First)) return i;
			return INDEX_NONE;
		}
		for (int32 k = 0; k < UE_ARRAY_COUNT(Tells[Topic]) && Tells[Topic][k]; k++)
			for (int32 i = 0; i < S.Statements.Num(); i++)
				if (S.Statements[i].Text.ToLower().Contains(Tells[Topic][k]))
				{
					bOutBeside = Topic == ETopic::WhereWereYou && k >= 4; // when they got here, how they came in: beside the point
					return i;
				}
		return INDEX_NONE;
	}

	// The lines for when no statement answers the question: one per mood (calm, defensive, rattled, cracking, broken).
	// They say nothing that could be checked, so they are true of anyone - and safe in the mouth of the culprit.
	const TCHAR* const Plain[UHUBrainSubsystem::TopicCount][5] = {
		/* DidYouDoIt   */ { TEXT("No. I had nothing to do with it."), TEXT("No. And I don't like being asked."), TEXT("No! No, I - of course not."), TEXT("I didn't. I swear I didn't. Please."), TEXT("No. I didn't do it.") },
		/* WhoDidIt     */ { TEXT("I don't know. I wish I did."), TEXT("How would I know? That's your job."), TEXT("I - I don't know. I've been trying not to think about it."), TEXT("I don't know! Why does everyone keep asking me?"), TEXT("I don't know. I really don't.") },
		/* Vouch        */ { TEXT("Ask anyone who was here. I wasn't hiding."), TEXT("I don't need anyone to vouch for me."), TEXT("I - somebody must have seen me. Ask around."), TEXT("I don't know who saw me! I wasn't keeping track."), TEXT("I don't know. Maybe nobody.") },
		/* SawAnything  */ { TEXT("Nothing out of the ordinary. Not until all this."), TEXT("I wasn't watching the room, detective."), TEXT("No. I don't think so. It's all a blur."), TEXT("I didn't see anything. I didn't."), TEXT("No. Nothing.") },
		/* KnewVictim   */ { TEXT("From here, that's all."), TEXT("From here, same as everybody."), TEXT("From here. That's all, I swear."), TEXT("From here! Just from here."), TEXT("From here.") },
		/* WhereWereYou */ { TEXT("Here, like everyone else. I didn't see it happen."), TEXT("I was here. That's not a crime."), TEXT("Here. Around. I wasn't watching the clock."), TEXT("I was here! I don't know exactly where - here."), TEXT("Here. Somewhere in the building.") },
	};

	// how they come in when repeating something they've already told the detective, by mood
	const TCHAR* const Again[5] = { TEXT(""), TEXT("I already said. "), TEXT("Look - "), TEXT("I told you - "), TEXT("") };
}

bool HUCanned::Greeting(const FString& Said, const FString& Name)
{
	const FString L = Words(Said);
	for (const TCHAR* Asks : { TEXT(" who are you "), TEXT(" your name "), TEXT(" who am i talking to "), TEXT(" who am i speaking to "), TEXT(" introduce yourself "), TEXT(" what do you do "), TEXT(" and you are ") })
		if (L.Contains(Asks)) return true;
	// a bare hello: every word is a greeting, a courtesy or their own name
	static const TSet<FString> Hello = { TEXT("hello"), TEXT("hi"), TEXT("hey"), TEXT("yo"), TEXT("howdy"), TEXT("greetings"), TEXT("evening"), TEXT("morning"), TEXT("afternoon"), TEXT("excuse"), TEXT("pardon") };
	static const TSet<FString> Courtesy = { TEXT("good"), TEXT("there"), TEXT("sir"), TEXT("ma'am"), TEXT("madam"), TEXT("mister"), TEXT("mr"), TEXT("mrs"), TEXT("miss"), TEXT("ms"), TEXT("me") };
	const FString Theirs = Words(Name);
	TArray<FString> Each;
	L.ParseIntoArrayWS(Each);
	bool bHello = false;
	for (const FString& W : Each)
	{
		if (Hello.Contains(W)) bHello = true;
		else if (!Courtesy.Contains(W) && !Theirs.Contains(TEXT(" ") + W + TEXT(" "))) return false;
	}
	return bHello;
}

FString HUCanned::Introduction(const UHUCaseSubsystem& Cases, const FString& SuspectId)
{
	FHUSuspect S;
	if (!Cases.GetSuspect(SuspectId, S)) return FString();
	return Cases.MoodOf(SuspectId) == EHUSuspectMood::Lawyered ? FString(TEXT("I want my lawyer.")) : S.Intro;
}

FString HUCanned::Answer(const UHUCaseSubsystem& Cases, const FString& SuspectId, const FString& Said, bool bAgain)
{
	return AnswerTopic(Cases, SuspectId, UHUBrainSubsystem::TopicOf(Said), bAgain);
}

FString HUCanned::AnswerTopic(const UHUCaseSubsystem& Cases, const FString& SuspectId, int32 Topic, bool bAgain)
{
	FHUSuspect S;
	if (Topic < 0 || Topic >= UHUBrainSubsystem::TopicCount || !Cases.GetSuspect(SuspectId, S)) return FString();
	const EHUSuspectMood Mood = Cases.MoodOf(SuspectId);
	if (Mood == EHUSuspectMood::Lawyered) return TEXT("I want my lawyer.");
	// they've owned up (the game decided that, not these lines)
	if (Cases.Confessed == SuspectId && Topic == ETopic::DidYouDoIt) return TEXT("Yes. I did it. I've told you.");
	if (Cases.Confessed == SuspectId && Topic == ETopic::WhoDidIt) return TEXT("You know who did it. I told you.");
	// asked the same thing twice running: they don't recite it again
	if (bAgain) return HUInterrogation::PressLine(Mood, GetTypeHash(SuspectId) + Topic);
	const int32 M = FMath::Clamp((int32)Mood, 0, 4);
	bool bBeside = false;
	const int32 At = StatementFor(S, Topic, Cases.Current.Victim, bBeside);
	if (At == INDEX_NONE) return Plain[Topic][M];
	const FHUStatement& St = S.Statements[At];
	// a lie the detective has proven: they can't stand on it any more - what they said when caught
	if (!St.Truth && Cases.IsBroken(SuspectId, At)) return St.OnBreak.IsEmpty() ? FString(Plain[Topic][M]) : St.OnBreak;
	// true, or a lie they're still protecting. One that only half answers it follows a plain "Here."
	return (bBeside ? FString(TEXT("Here. ")) : FString(Again[M])) + St.Text;
}
