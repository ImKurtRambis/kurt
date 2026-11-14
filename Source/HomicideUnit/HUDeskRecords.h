// What the squad has done on the desk computers this case: prints and plates run, evidence sent to the lab,
// backgrounds pulled, warrants filed. One per game instance (it lasts the case across levels); every machine keeps
// its own copy, kept in step by UHUInteractComponent::SendDeskRecord, and works results out the same way from the
// case (which every machine has), so the whole squad reads the same answers.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HUDeskRecords.generated.h"

class UHUCaseSubsystem;

UENUM()
enum class EHUDeskJob : uint8 { Print, Plate, Lab, Background, ArrestWarrant, SearchWarrant };

UCLASS()
class HOMICIDEUNIT_API UHUDeskRecords : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static constexpr double LabSeconds = 30.0; // how long the lab takes

	struct FWarrant { FString Suspect; bool bArrest = false; bool bApproved = false; FString Who, Reason; };

	// a job done on some machine's desk (Arg: the clue for prints, plates and the lab; the suspect otherwise)
	void Apply(const UHUCaseSubsystem* C, EHUDeskJob Job, const FString& Arg, const FString& Who);

	// ---- what's on file (cleared when a new case starts) ----
	TMap<FString, FString> Prints;      // clue -> who ran it
	TMap<FString, FString> Plates;
	TMap<FString, double> LabIn;        // clue -> when it went to the lab (FPlatformTime)
	TMap<FString, FString> LabBy;
	TSet<FString> Backgrounds;          // suspects
	TArray<FWarrant> Warrants;
	const FWarrant* WarrantFor(const FString& Suspect, bool bArrest) const;
	void Sync(const UHUCaseSubsystem* C); // forget the last case's work when the case changes

	// ---- the answers (the same on every machine) ----
	static bool IsPrint(const UHUCaseSubsystem* C, const FString& Clue);
	static bool IsPlate(const UHUCaseSubsystem* C, const FString& Clue);
	static FString Implicates(const UHUCaseSubsystem* C, const FString& Clue); // the suspect whose lie it breaks ("" if none)
	static FString PrintResult(const UHUCaseSubsystem* C, const FString& Clue);
	static FString PlateNumber(const FString& Clue);
	static FString PlateResult(const UHUCaseSubsystem* C, const FString& Clue);
	static FString LabResult(const UHUCaseSubsystem* C, const FString& Clue);
	static TArray<FString> Background(const UHUCaseSubsystem* C, const FString& Suspect);
	// a warrant: would the judge sign it, and why (or why not)
	static bool Judge(const UHUCaseSubsystem* C, const UHUDeskRecords* R, const FString& Suspect, bool bArrest, FString& Reason);
	// one line for the squad's pagers about a job just done
	static FString Summary(const UHUCaseSubsystem* C, EHUDeskJob Job, const FString& Arg);

private:
	FString CaseKey;
};
