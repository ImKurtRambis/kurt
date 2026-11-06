// Case data, matching Content/Data/Cases.json (exported from the browser game's case generator).
#pragma once

#include "CoreMinimal.h"
#include "HUCaseTypes.generated.h"

USTRUCT(BlueprintType)
struct FHUStatement
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Text;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool Truth = true;
	// clue ids that prove this statement is a lie
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FString> Evidence;
	// what they say when caught
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString OnBreak;
};

USTRUCT(BlueprintType)
struct FHUSuspect
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Role;
	// position on the browser game's 1280x720 floor plan; HUCaseSubsystem::MapToWorld converts it
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 X = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Y = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Intro;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FHUStatement> Statements;
};

USTRUCT(BlueprintType)
struct FHUClue
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Detail;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 X = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Y = 0;
	// "main" or "back"
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Area;
	// a dead end
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool Flavor = false;
	// for a dead end written to look like a kind of offender: which (an EHUProfile value; 0 = it points nowhere)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) uint8 PointsTo = 0;
};

USTRUCT(BlueprintType)
struct FHUSolution
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Killer;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Motive;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Means;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Alibi;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 RequiredBreaks = 3;
};

USTRUCT(BlueprintType)
struct FHUCase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Number = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Crime;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Place;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Victim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Time;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Weapon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Brief;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Difficulty;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FHUSolution Solution;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FHUSuspect> Suspects;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FHUClue> Clues;

	// ---- generated cases ----
	// the hidden offender profile (an EHUProfile value; 0 = work it out from the crime)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) uint8 Profile = 0;
	// what the dead ends were written to look like
	UPROPERTY(EditAnywhere, BlueprintReadOnly) uint8 DecoyProfile = 0;
	// the briefing's victim slide: header, rows, and what the Captain says over it
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString VictimHeader;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FString> VictimRows;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString CaptainLine;
	// seeds the map layout, so every case gets its own floor plan
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MapSeed = 0;
};

// one slide of the briefing deck on the projector
USTRUCT(BlueprintType)
struct FHUSlide
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Header;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FString> Rows;
	// what the Captain says over this slide
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString CaptainLine;
	// set on person-of-interest slides
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString SuspectId;
};

UENUM(BlueprintType)
enum class EHUChallengeResult : uint8
{
	Broken,      // caught them in a lie
	Wrong,       // that evidence doesn't contradict it
	AlreadyDone, // already on the record
};
