// What the desk computer knows about each suspect beyond their statements: the DMV/records profile, and - with a
// warrant - their phone: calls, texts, browser history, camera roll and the cell towers it pinged.
// Everything is written from the case (same case number = same phone on every machine, so the squad sees one story).
//   The culprit's phone backs up the case against them, but never names them outright: contact with the victim,
//   a gap or a deletion around the time, searches that fit the means, a tower near the scene.
//   Suspects hiding something else (their "secret" lie) have phones that look bad too - for the wrong reason.
//   Everyone else's phone is just a life.
// A WARRANT for a suspect's phone: one of their stories broken on the record, or they're in custody.
#pragma once

#include "CoreMinimal.h"

struct FHUCase;
struct FHUSuspect;
class UHUCaseSubsystem;

namespace HUPhone
{
	enum EApp { Database, Calls, Texts, Web, Photos, Cell };
	// can the squad get into this suspect's phone yet?
	bool HasWarrant(const UHUCaseSubsystem* C, const FString& SuspectId);
	// the lines one app shows for one suspect
	TArray<FString> Lines(const UHUCaseSubsystem* C, const FHUSuspect& S, EApp App);
}
