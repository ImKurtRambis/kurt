// The case in play: loads cases, tracks evidence found, lies broken and the accusation.
// One per game instance, so it survives level changes (precinct -> scene -> precinct).
// In Blueprints: Get Game Instance Subsystem (HUCaseSubsystem).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HUCaseTypes.h"
#include "HUTools.h"
#include "HUProfiles.h"
#include "HUInterrogation.h"
#include "HULook.h"
#include "HUCaseSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHUClueFound, const FHUClue&, Clue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHULieBroken, const FHUSuspect&, Suspect, int32, StatementIndex, const FString&, OnBreak);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHUCaseStarted, const FHUCase&, Case);

UCLASS()
class HOMICIDEUNIT_API UHUCaseSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// every case in Content/Data/Cases.json
	UPROPERTY(BlueprintReadOnly, Category = "Case") TArray<FHUCase> Cases;
	UPROPERTY(BlueprintReadOnly, Category = "Case") FHUCase Current;
	UPROPERTY(BlueprintReadOnly, Category = "Case") bool bCaseActive = false;
	// the Captain's briefing has been seen through to the last slide: the cruisers are released
	UPROPERTY(BlueprintReadWrite, Category = "Case") bool bBriefed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Case") TArray<FString> Found;
	UPROPERTY(BlueprintReadOnly, Category = "Case") int32 Breaks = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Case") int32 WrongChallenges = 0;

	UPROPERTY(BlueprintAssignable, Category = "Case") FHUCaseStarted OnCaseStarted;
	UPROPERTY(BlueprintAssignable, Category = "Case") FHUClueFound OnClueFound;
	UPROPERTY(BlueprintAssignable, Category = "Case") FHULieBroken OnLieBroken;

	UFUNCTION(BlueprintCallable, Category = "Case") bool LoadCases();
	// in a squad, everyone generates the same cases from the squad code (0 = playing alone: random)
	UPROPERTY(BlueprintReadOnly, Category = "Case") int32 SquadSeed = 0;
	// how many cases this squad has started (so each shift deals the next one)
	int32 CasesStarted = 0;
	UFUNCTION(BlueprintCallable, Category = "Case") void UseSquadSeed(int32 Seed);
	// picked in the squad room before roll call (empty = random): the next case happens here, at this difficulty
	UPROPERTY(BlueprintReadWrite, Category = "Case") FString ChosenPlace;
	UPROPERTY(BlueprintReadWrite, Category = "Case") FString ChosenDifficulty;
	// -1 picks one at random
	UFUNCTION(BlueprintCallable, Category = "Case") bool StartCase(int32 Index = -1);

	UFUNCTION(BlueprintCallable, Category = "Case") bool FindClue(const FString& ClueId);
	UFUNCTION(BlueprintPure, Category = "Case") bool HasFound(const FString& ClueId) const { return Found.Contains(ClueId); }
	UFUNCTION(BlueprintPure, Category = "Case") bool GetClue(const FString& ClueId, FHUClue& Out) const;
	UFUNCTION(BlueprintPure, Category = "Case") bool GetSuspect(const FString& SuspectId, FHUSuspect& Out) const;

	// confront a suspect's statement with a piece of evidence you've found
	UFUNCTION(BlueprintCallable, Category = "Case") EHUChallengeResult Challenge(const FString& SuspectId, int32 StatementIndex, const FString& ClueId);
	UFUNCTION(BlueprintPure, Category = "Case") bool IsBroken(const FString& SuspectId, int32 StatementIndex) const;
	UFUNCTION(BlueprintPure, Category = "Case") bool CanAccuse() const { return bCaseActive && Breaks >= Current.Solution.RequiredBreaks; }
	// true when you named the killer; ends the case either way
	UFUNCTION(BlueprintCallable, Category = "Case") bool Accuse(const FString& SuspectId);

	// the 911 call that came in, line by line (who's speaking, what they say), played during the briefing
	UFUNCTION(BlueprintCallable, Category = "Case") void Build911Call(TArray<FString>& Speakers, TArray<FString>& Lines) const;

	// the briefing deck for the projector
	UFUNCTION(BlueprintCallable, Category = "Case") TArray<FHUSlide> BuildBriefingSlides(const FString& DetectiveName) const;

	// ---- interrogation (see HUInterrogation.h): pressure per suspect, and what each move does ----
	UPROPERTY(BlueprintReadOnly, Category = "Interrogation") TMap<FString, float> Pressure;
	UPROPERTY(BlueprintReadOnly, Category = "Interrogation") TMap<FString, int32> Misses;
	UPROPERTY(BlueprintReadOnly, Category = "Interrogation") TSet<FString> Lawyered;
	UPROPERTY(BlueprintReadOnly, Category = "Interrogation") FString Confessed;   // the suspect who confessed
	TMap<FString, int32> PressCount;
	TSet<FString> RecordRaised;
	UFUNCTION(BlueprintPure, Category = "Interrogation") float GetPressure(const FString& Id) const { return Pressure.FindRef(Id); }
	UFUNCTION(BlueprintPure, Category = "Interrogation") EHUSuspectMood MoodOf(const FString& Id) const { return HUInterrogation::MoodFor(GetPressure(Id), Lawyered.Contains(Id)); }
	// each returns what the suspect says back
	UFUNCTION(BlueprintCallable, Category = "Interrogation") FString Confront(const FString& Id, int32 Statement, const FString& ClueId);
	UFUNCTION(BlueprintCallable, Category = "Interrogation") FString Press(const FString& Id, int32 Statement);
	UFUNCTION(BlueprintCallable, Category = "Interrogation") FString RaiseRecord(const FString& Id);

	// ---- the offender profile ----
	UPROPERTY(BlueprintReadOnly, Category = "Profile") EHUProfile TrueProfile = EHUProfile::None;
	// what the dead ends point at, to mislead you
	UPROPERTY(BlueprintReadOnly, Category = "Profile") EHUProfile DecoyProfile = EHUProfile::None;
	// clue id -> which profile it points at, and the sign it shows
	UPROPERTY(BlueprintReadOnly, Category = "Profile") TMap<FString, EHUProfile> ClueProfile;
	UPROPERTY(BlueprintReadOnly, Category = "Profile") TMap<FString, FString> ClueSign;
	// suspect id -> the profile their record fits, and the record itself
	UPROPERTY(BlueprintReadOnly, Category = "Profile") TMap<FString, EHUProfile> SuspectProfile;
	UPROPERTY(BlueprintReadOnly, Category = "Profile") TMap<FString, FString> SuspectRecord;
	// how many bagged pieces point at each profile, most first
	UFUNCTION(BlueprintPure, Category = "Profile") TArray<EHUProfile> GetProfileLeads(TArray<int32>& Counts) const;
	UFUNCTION(BlueprintPure, Category = "Profile") FString DescribeProfileLeads() const;
	// signs found on the victim's body (they count toward the leads too)
	TMap<EHUProfile, int32> BodyLeads;
	void AddBodyLead(EHUProfile P) { BodyLeads.FindOrAdd(P)++; }

	// ---- money and kit (saved between sessions) ----
	UPROPERTY(BlueprintReadOnly, Category = "Kit") int32 Cash = 250;
	UPROPERTY(BlueprintReadOnly, Category = "Kit") int32 OwnedTools = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Kit") int32 CasesSolved = 0;
	// ---- you, and your preferences (saved) ----
	UPROPERTY(BlueprintReadWrite, Category = "Player") FString DetectiveName;
	UPROPERTY(BlueprintReadWrite, Category = "Player") FString JoinAddress = TEXT("127.0.0.1");
	UPROPERTY(BlueprintReadWrite, Category = "Player") float MouseSensitivity = 1.f;
	UPROPERTY(BlueprintReadWrite, Category = "Player") float FieldOfView = 90.f;
	// voices: 0 Windows, 1 OpenAI, 2 ElevenLabs (see HUVoiceSubsystem), and the key you pasted in
	UPROPERTY(BlueprintReadWrite, Category = "Player") uint8 VoiceProvider = 2; // ElevenLabs
	// Settings: hold V in interviews to SAY a question (local speech-to-text)
	UPROPERTY(BlueprintReadWrite, Category = "Player") bool bVoiceQuestions = false;
	// how your detective looks (picked at the name plate, saved, shown to the squad)
	UPROPERTY(BlueprintReadWrite, Category = "Player") FHULook Look;
	UPROPERTY() FString VoiceKey;
	UFUNCTION(BlueprintPure, Category = "Kit") bool HasTool(EHUTool Tool) const { return Tool == EHUTool::Handcuffs || (OwnedTools & (1 << (int32)Tool)) != 0; }
	// false if you can't afford it or already own it
	UFUNCTION(BlueprintCallable, Category = "Kit") bool BuyTool(EHUTool Tool);
	UFUNCTION(BlueprintCallable, Category = "Kit") void AddCash(int32 Amount);
	UFUNCTION(BlueprintCallable, Category = "Kit") void SaveProgress();

	// ---- how much evidence a scene holds and needs ----
	// rookie / detective / veteran; defaults to the case's own difficulty when a case starts
	UPROPERTY(BlueprintReadWrite, Category = "Evidence") FString PlayDifficulty;
	// the clue ids actually placed in the scene (set by the crime scene)
	UPROPERTY(BlueprintReadWrite, Category = "Evidence") TArray<FString> InScene;
	// suspects in the back of the van (ids)
	UPROPERTY(BlueprintReadWrite, Category = "Case") TArray<FString> Arrested;
	// how the last case ended (shown back at the precinct)
	UPROPERTY(BlueprintReadOnly, Category = "Case") FString LastVerdict;
	// back at the precinct: each prisoner is booked (mugshot, prints, property: 3 steps), then the case goes to the Captain
	UPROPERTY(BlueprintReadOnly, Category = "Case") TMap<FString, int32> BookingStep;
	UPROPERTY(BlueprintReadOnly, Category = "Case") bool bAwaitingCaptain = false;
	UFUNCTION(BlueprintPure, Category = "Case") bool IsBooked(const FString& Id) const { const int32* S = BookingStep.Find(Id); return S && *S >= 3; }
	UFUNCTION(BlueprintPure, Category = "Case") int32 UnbookedCount() const { int32 N = 0; for (const FString& Id : Arrested) if (!IsBooked(Id)) N++; return N; }
	// the Captain takes the case: judge the arrests, pay out, close it
	UFUNCTION(BlueprintCallable, Category = "Case") void ResolveArrests();

	// evidence that proves some statement is a lie (what you build the case from)
	UFUNCTION(BlueprintPure, Category = "Evidence") TArray<FString> GetKeyEvidence() const;
	// evidence that breaks one of the killer's own lies
	UFUNCTION(BlueprintPure, Category = "Evidence") TArray<FString> GetKillerEvidence() const;
	// dead ends: evidence that proves nothing
	UFUNCTION(BlueprintPure, Category = "Evidence") TArray<FString> GetDeadEnds() const;
	// per difficulty: how many dead ends go in the scene, how many pieces hide under furniture, lies needed to accuse
	UFUNCTION(BlueprintPure, Category = "Evidence") void GetRules(int32& DeadEnds, int32& HiddenUnder, int32& LiesNeeded) const;
	// the counter: bagged of placed, key bagged of key placed
	UFUNCTION(BlueprintPure, Category = "Evidence") void GetEvidenceCount(int32& Bagged, int32& Placed, int32& KeyBagged, int32& KeyPlaced) const;

	// the browser game's 1280x720 floor plan onto a level: Origin is the plan's top-left corner, Scale cm per plan pixel
	UFUNCTION(BlueprintPure, Category = "Case") static FVector MapToWorld(int32 X, int32 Y, FVector Origin, float Scale = 1.5f);

private:
	TSet<FString> BrokenKeys;
	void AddPressure(const FString& Id, float Amount) { Pressure.FindOrAdd(Id) = FMath::Clamp(Pressure.FindRef(Id) + Amount, 0.f, 100.f); }
	FString CheckBreaking(const FString& Id, const FString& Line);
	void AssignProfiles();
};
