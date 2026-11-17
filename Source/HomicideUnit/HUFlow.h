// NO DEAD TIME BETWEEN STEPS: the scene, the drive, the precinct and the interviews run into each other.
//  - LEAVING: your car (or the precinct's motor pool door) goes the moment the screen is dark - half a second - not a
//    fade and then a wait on black (AHUCruiser::Depart: FadeOut, LeaveAfter).
//  - ARRIVING: wherever you travel to comes up behind black and fades in as soon as it runs smoothly, so you never see
//    a frozen frame or people popping in, and it never holds you longer than a few seconds. It also clears the fade the
//    trip left on your camera (on a squad's trip your camera comes along with you, still black).
//  - WARMING THE NEXT STOP: the people at the next place - MetaHumans, by far the heaviest thing a level loads - load
//    in the background while you work this one and stay loaded through the trip, so arriving doesn't wait on the disk.
//    Who's at each place is noted on the way out (Saved/HUFlowFaces.txt, so the first trip of a session is warm too),
//    plus whoever the case puts there (the suspects and victim at the scene, the prisoners at the precinct).
// Every machine does this for its own screen. Nothing here changes the game: no case state, nothing replicated.
// -HUFlowTest on L_Scene (dev builds): drives to the precinct, books a prisoner, drives back out through the motor
// pool and logs each trip ("HUFlowTest: ..."), screenshots each arrival (Flow_*.png), the last line "all checks passed".
// -HUFlowNoWarm turns the warming off, to compare.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HUFlow.generated.h"

struct FStreamableHandle;

UCLASS()
class HOMICIDEUNIT_API UHUFlow : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	// a car's fade to black, and when it leaves: just after, on a dark screen
	static constexpr float FadeOut = 0.5f;
	static constexpr float LeaveAfter = 0.6f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// the place you're in is on screen (its arrival fade has started, or there wasn't one)
	bool IsUp() const { return !bCovering; }
	// people of the next stop loading or loaded in the background, and how many of them are in
	int32 WarmCount(int32* Ready = nullptr) const;

private:
	bool Tick(float Dt);
	void OnPreLoad(const FString& Map);
	void OnSeamlessStart(UWorld* World, const FString& Map);
	void OnPostLoad(UWorld* World);
	void NoteFaces(UWorld* World);   // who's here, for the next time we head this way
	void Dark(UWorld* World) const;  // every local camera to black, held
	void Lift(UWorld* World);
	void WarmFor(UWorld* World);
	TSet<FString> FacesFor(const FString& Map) const;
	TArray<FString> NextStops(UWorld* World) const;
	FString MapOf(const UWorld* World) const;
	void SaveSeen() const;

	FTSTicker::FDelegateHandle TickH;
	FDelegateHandle PreH, SeamH, PostH;
	TMap<FString, TArray<FString>> Seen;                      // a place -> the faces there last time
	TMap<FString, TSharedPtr<FStreamableHandle>> Warm;        // a face -> its class loading or loaded
	TMap<FString, bool> Exists;                               // a face -> built on this PC
	TSet<FString> PendingTo;                                  // on the way out: the destination's people
	TWeakObjectPtr<UWorld> Here;
	FString HereMap;
	bool bCovering = false, bWarming = true;
	double CoverAt = 0.0, UpAt = 0.0, NextWarm = 0.0;
	int32 Smooth = 0;
	float CoverLongest = 0.f;

#if !UE_BUILD_SHIPPING
	// -HUFlowTest
	void TestTick(float Dt);
	void TestLog(const FString& Line, bool bOk = true);
	int32 TestStage = 0, TestFails = 0, TestTrip = 0;
	double TestAt = 0.0, PressedAt = 0.0, DarkAt = 0.0, LeftAt = 0.0, ArrivedAt = 0.0, LiftedAt = 0.0;
	float TestLongest = 0.f; int32 TestHitches = 0;
	FString TestFrom;
	bool bTest = false, bBookChecked = false;
#endif
};
