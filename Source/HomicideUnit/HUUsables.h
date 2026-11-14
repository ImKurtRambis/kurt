// USABLE PROPS (checklist D-44): everything that looks usable can be used - drawers, lockers, phones, files, chairs,
// light switches, a TV, a lamp, the water cooler, a clock, notices. The walk-through of every room is in
// Docs/UsableProps.md.
//
// A prop is MARKED with what it is (Mark / MarkProp). Real furniture (HUProps) is marked where it's placed, so a desk
// anywhere has drawers and a phone anywhere has a dial tone; a room can mark its own blocks too. The interact
// component then treats a marked part as usable - unless its actor already does something with it (its own prompt
// wins), so nothing that worked before changes.
//
// Squad play: marking renames the part from its kind and position, so every machine's copy has the same name and the
// use reaches the same drawer everywhere (UHUInteractComponent replays it as usual). Use() runs on every machine; a
// line of text only shows for the player who did it, a sound only plays once (from the host).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HUUsables.generated.h"

class UPrimitiveComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class APawn;

UENUM()
enum class EHUUse : uint8
{
	None,
	Drawer,   // slides open / shut; Words: what's in it
	Files,    // a filing cabinet: the top drawer slides; Q pulls an old file to read
	Locker,   // opens / shuts (the open locker mesh when the art is in)
	Phone,    // pick up: the switchboard
	Seat,     // sit / stand (Extra: the seat's height as a share of the prop's height)
	Bench,    // a seat you sit on where you stand along it
	Switch,   // the lights in its room off / on
	Screen,   // a TV on / off
	Lamp,     // a lamp on / off
	Drink,    // the water cooler
	Clock,    // the time
	Read,     // read it: Words
};

namespace HUUsables
{
	// make C usable. Words: what it says / holds (empty = something fitting the kind). FrontYaw: for drawers, the way
	// the drawers face in the part's own frame (0 = its +X). Extra: per kind (Seat: seat height share).
	HOMICIDEUNIT_API void Mark(UPrimitiveComponent* C, EHUUse Kind, const FString& Words = FString(), float FrontYaw = 0.f, float Extra = 0.f);
	// what a piece of real furniture is for (called by HUBuild::Prop): a Desk has drawers, a DeskPhone is a phone...
	HOMICIDEUNIT_API void MarkProp(UPrimitiveComponent* C, const TCHAR* PropName);
	HOMICIDEUNIT_API bool IsMarked(const UPrimitiveComponent* C);
	// should this use go to the usable (C marked, and its actor has nothing of its own to say about C)?
	// The user's interact component must already be focused on Target / Part.
	HOMICIDEUNIT_API bool Handles(AActor* Target, UPrimitiveComponent* Part, APawn* User);
	HOMICIDEUNIT_API FText Prompt(UPrimitiveComponent* C, APawn* User);
	HOMICIDEUNIT_API void Use(UPrimitiveComponent* C, APawn* User, bool bSecondary);
	// a light switch on a wall at Pos (Owner's frame), facing Yaw: it switches Owner's lights over Room (Owner's XY)
	HOMICIDEUNIT_API UStaticMeshComponent* AddSwitch(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, FVector Pos, float Yaw, FBox2D Room);
}

// what every usable part is and where it's at (open, on...), and the drawers sliding
UCLASS()
class HOMICIDEUNIT_API UHUUsableWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	struct FUse
	{
		EHUUse Kind = EHUUse::None;
		FString Words;
		float FrontYaw = 0.f, Extra = 0.f;
		FBox2D Room = FBox2D(ForceInit);
		bool bOn = false;            // open / off the hook / lights out / screen on...
		int32 Uses = 0;              // how many times it's been used (the same on every machine)
		float Slide = 0.f;           // a drawer: 0 shut .. 1 open
		TWeakObjectPtr<UStaticMeshComponent> Tray, Front, Fill, Glow; // the drawer that slides out (box, front, what's in it); a lit screen
		TWeakObjectPtr<UPointLightComponent> Light;        // a lamp's bulb
		TMap<TWeakObjectPtr<UPrimitiveComponent>, FLinearColor> Dimmed; // light panels a switch turned down
	};
	TMap<TObjectKey<UPrimitiveComponent>, FUse> Parts;
	FUse* Find(const UPrimitiveComponent* C) { return C ? Parts.Find(TObjectKey<UPrimitiveComponent>(C)) : nullptr; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHUUsableWorld, STATGROUP_Tickables); }
	void PlaceTray(UPrimitiveComponent* C, FUse& U);
	float TestClock = 0.f;
	void TestStep(float DeltaTime); // -HUUseTest
};
