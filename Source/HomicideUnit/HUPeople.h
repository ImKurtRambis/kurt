// The only full-body character factory. All roles use built MetaHumans; collision belongs to gameplay actors.
// Models are local to each machine: deterministic casting gives every squad member the same people.
//
// -HUPeopleShots: once the level is up, photograph every one of them (and the suspects) to Saved/Screenshots, then quit.
#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UHUPersonAnimation;
struct FHULook;
struct FHUCase;

namespace HUPeople
{
	constexpr int32 CastCount = 8;
	HOMICIDEUNIT_API FString CastFace(int32 Index);
	HOMICIDEUNIT_API FString SuspectFace(const FHUCase& Case, const FString& Id);
	// Prefer dedicated faces when installed; otherwise reuse the built cast for every role.
	HOMICIDEUNIT_API FString OfficerFace(int32 Variant);
	HOMICIDEUNIT_API FString CaptainFace();
	HOMICIDEUNIT_API FString VictimFace(const FString& VictimName, int32 CaseNumber);

	// MH_<Who> standing in its idle loop, attached to Parent at RelLoc, turned so it faces Parent's RelYaw direction
	// (0 = Parent's +X). nullptr if that face isn't built.
	HOMICIDEUNIT_API AActor* Stand(AActor* Owner, USceneComponent* Parent, const FString& Who, FVector RelLoc, float RelYaw);
	// MH_<Who> lying face up along Parent's +X (head that way), feet at RelFeet: it falls limp, settles, and stays put.
	HOMICIDEUNIT_API AActor* Lie(AActor* Owner, USceneComponent* Parent, const FString& Who, FVector RelFeet, float Scale = 1.f);
	// Reuse the existing model when only proportions change. The desk preview uses this same path.
	HOMICIDEUNIT_API AActor* Detective(AActor* Owner, USceneComponent* Parent, AActor* Previous, const FHULook& Look, bool bOwnerNoSee, float Scale = 1.f);
	// the clip for how fast they're moving (cm/s): standing idle, walking, running - played at the pace of their feet
	HOMICIDEUNIT_API void Motion(AActor* Look, float Speed);
	// their body's animation (sitting, cuffs, talking, flinching... HUPersonAnimation.h), or nullptr
	HOMICIDEUNIT_API UHUPersonAnimation* AnimOf(AActor* Look);
	// talk with the hands and head for this long; flinch, caught out (1 = a lie proven)
	HOMICIDEUNIT_API void Talk(AActor* Look, float Seconds);
	HOMICIDEUNIT_API void Flinch(AActor* Look, float Strength = 1.f);
	// about how long saying this takes, when there's no recorded voice to time it by
	HOMICIDEUNIT_API float SpeakingTime(const FString& Line);
	HOMICIDEUNIT_API void DestroyOwned(AActor* Owner);
	HOMICIDEUNIT_API bool ValidateCast();
	// a patrol cap and a badge, on a standing MetaHuman's head and chest
	HOMICIDEUNIT_API void Uniform(AActor* Look);
	// HUCaseGen's first names: "Nora Whitfield" -> true
	HOMICIDEUNIT_API bool IsWoman(const FString& FullName);
	HOMICIDEUNIT_API USkeletalMeshComponent* BodyOf(AActor* Look);
}
