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
	Handcuffs,  // standard issue, on the belt (not the hotbar): look down, pick the cuffs, click with a suspect in front of you
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

// The utility belt: always on you, never on the hotbar. Look down at it (with nothing to pick up there),
// pick a pouch with the mouse or the wheel, LEFT CLICK to use it. Whatever's in your hand stays there.
namespace HUBelt
{
	enum : uint8 { Cuffs, Pepper, Taser, Radio, Count };
	inline const TCHAR* Name(int32 i)
	{
		static const TCHAR* N[] = { TEXT("Cuffs"), TEXT("Pepper Spray"), TEXT("Taser"), TEXT("Radio") };
		return i >= 0 && i < Count ? N[i] : TEXT("?");
	}
	// seconds before it's ready again
	inline float Cooldown(int32 i)
	{
		static const float C[] = { 1.f, 5.f, 12.f, 6.f };
		return i >= 0 && i < Count ? C[i] : 1.f;
	}
	inline const TCHAR* Hint(int32 i)
	{
		static const TCHAR* H[] = { TEXT("arrest who's in front of you"), TEXT("a burst at whoever's close"), TEXT("drops one person, at range"), TEXT("call it in") };
		return i >= 0 && i < Count ? H[i] : TEXT("");
	}
}

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
		case EHUTool::Handcuffs:  return TEXT("Standard issue, on your belt. Look down, pick the cuffs, click.");
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
