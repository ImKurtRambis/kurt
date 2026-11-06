// The detective's kit, bought at the supply locker in the motor pool.
// Each tool opens up a kind of evidence you otherwise can't find or can't collect.
#pragma once

#include "CoreMinimal.h"
#include "HUTools.generated.h"

UENUM(BlueprintType)
enum class EHUTool : uint8
{
	Flashlight, // F: see evidence lying in the dark (the back room, the crawlspace)
	UVLight,    // V: shows trace evidence - blood, fluids, fibres - that's invisible otherwise
	PrintKit,   // lifts fingerprints off glass and metal (without it you can see them but not bag them)
	Crowbar,    // pries open locked strongboxes and drawers
	Scanner,    // R: pings the nearest evidence you haven't bagged: how far, which way
	Handcuffs,  // standard issue (everyone has a pair): E on a suspect to arrest them
	Count UMETA(Hidden)
};

// how a piece of evidence is concealed
UENUM(BlueprintType)
enum class EHUConceal : uint8
{
	Plain,  // just lying there - small, easy to walk past
	Dark,   // only seen in a flashlight beam
	Trace,  // only seen under UV light
	Prints, // seen, but needs the print kit to lift
	Locked, // in a strongbox: needs the crowbar
};

namespace HUTools
{
	inline const TCHAR* Name(EHUTool T)
	{
		switch (T)
		{
		case EHUTool::Flashlight: return TEXT("Flashlight");
		case EHUTool::UVLight:    return TEXT("UV Light");
		case EHUTool::PrintKit:   return TEXT("Print Kit");
		case EHUTool::Crowbar:    return TEXT("Crowbar");
		case EHUTool::Scanner:    return TEXT("Evidence Scanner");
		case EHUTool::Handcuffs:  return TEXT("Handcuffs");
		default:                  return TEXT("?");
		}
	}
	inline int32 Price(EHUTool T)
	{
		switch (T)
		{
		case EHUTool::Flashlight: return 50;
		case EHUTool::UVLight:    return 160;
		case EHUTool::PrintKit:   return 120;
		case EHUTool::Crowbar:    return 90;
		case EHUTool::Scanner:    return 250;
		default:                  return 0;
		}
	}
	inline const TCHAR* Pitch(EHUTool T)
	{
		switch (T)
		{
		case EHUTool::Flashlight: return TEXT("Back rooms and crawlspaces are pitch black. [F] to switch on.");
		case EHUTool::UVLight:    return TEXT("Blood and fibres glow under it. Invisible otherwise. [V] to switch on.");
		case EHUTool::PrintKit:   return TEXT("Powder, brush and tape. Lifts prints off glass and metal.");
		case EHUTool::Crowbar:    return TEXT("Strongboxes and drawers don't stay locked.");
		case EHUTool::Scanner:    return TEXT("[R] pings the nearest evidence you haven't bagged.");
		case EHUTool::Handcuffs:  return TEXT("Standard issue. E on a suspect to arrest them.");
		default:                  return TEXT("");
		}
	}
	inline const TCHAR* Key(EHUTool T)
	{
		switch (T)
		{
		case EHUTool::Flashlight: return TEXT("F");
		case EHUTool::UVLight:    return TEXT("V");
		case EHUTool::Scanner:    return TEXT("R");
		default:                  return TEXT("");
		}
	}
}
