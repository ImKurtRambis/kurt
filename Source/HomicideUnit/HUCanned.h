// HAND-WRITTEN ANSWERS for what every detective says to a suspect, built from the case's own facts - no model.
//
// Most of what gets said to a suspect is the same few things. Those should cost nothing to wait for and need nothing
// installed, so they are written by hand here; the thinking brain (UHUBrainSubsystem) is for what's off-script.
//   "Hello" / "Who are you?"   -> their introduction. Always: nothing to think about.
//   the six questions the brain writes ahead (UHUBrainSubsystem::ETopic: where were you, did you see anything, who
//   can vouch for you, how did you know the victim, who did it, did you do it)
//                              -> used whenever the brain has nothing ready: not installed, still loading, failed.
//                                 If one of their statements answers the question they say that (a lie that has
//                                 been proven gives way to what they said when caught); else a line written for
//                                 that question and their mood.
// Nothing here invents a fact: a line either repeats what they've already told the detective or says nothing that
// could be checked. A lie is kept until it's broken, and nobody confesses unless the game says they have.
#pragma once

#include "CoreMinimal.h"

class UHUCaseSubsystem;

namespace HUCanned
{
	// just a hello, or asking who they are? (Name: theirs, so "Hello, Walter" still counts)
	HOMICIDEUNIT_API bool Greeting(const FString& Said, const FString& Name);
	// their introduction, as they'd say it
	HOMICIDEUNIT_API FString Introduction(const UHUCaseSubsystem& Cases, const FString& SuspectId);
	// an answer to Said with no model; empty if Said isn't one of the common questions.
	// bAgain: they were asked the same thing last time - they say so instead of repeating themselves.
	HOMICIDEUNIT_API FString Answer(const UHUCaseSubsystem& Cases, const FString& SuspectId, const FString& Said, bool bAgain = false);
	// the same, by topic (a UHUBrainSubsystem::ETopic)
	HOMICIDEUNIT_API FString AnswerTopic(const UHUCaseSubsystem& Cases, const FString& SuspectId, int32 Topic, bool bAgain = false);
}
