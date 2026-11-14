// -HUSquadRun=host|client  (dev builds only): two copies of the game play a whole squad shift by themselves, through
// the same calls real players make - the host replays every use, interview move, belt use and travel - so the run
// shows whether both PCs still agree on the case at the end:
//   desk: host a squad / join it by its code  ->  roll call: both punch in  ->  briefing: the partner clicks through
//   the deck, stands up and walks, presses the car early (nothing may happen), the host drives  ->  the scene: the
//   partner bags every piece, the host interviews the culprit, the partner cuffs them, walks them to the van and loads
//   them still walking  ->  the precinct: the partner books them, the host submits to the Captain.
// Each copy writes its case ("HUSquadRun DIGEST <stage> | ...") whenever it leaves a level and at the end, and
// "HUSquadRun CHECK ok|FAIL ..." for each thing it checks. Tools\TestSquadShift.ps1 runs both, with simulated lag and
// packet loss, and compares the digests. -HUSquadRunDir= is where the host leaves the code (and the two leave each
// other notes when one has to wait for the other).
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HUSquadRun.generated.h"

UCLASS()
class HOMICIDEUNIT_API UHUSquadRun : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float Dt);
	void OnLeaveMap(const FString& Next);
	void Digest(const FString& Stage) const;
	void Check(bool bOk, const FString& What) const;
	void Say(const FString& What) const;
	void Flag(const FString& Name) const;
	bool HasFlag(const FString& Name) const;
	void After(double Seconds);
	void Next(double Seconds) { Step++; After(Seconds); }
	void Done(const FString& Why);

	void Menu();
	void Lobby();
	void Briefing();
	void Scene();
	void Precinct();

	class UWorld* World() const;
	class APlayerController* PC() const;
	class APawn* Me() const;
	class UHUInteractComponent* Hands() const;
	class UHUCaseSubsystem* Case() const;
	TArray<class APawn*> Squad() const;
	class APawn* Partner() const;
	void Teleport(class APawn* P, FVector At, float Yaw) const;

	FTSTicker::FDelegateHandle TickH;
	FDelegateHandle LeaveH, SeamlessH;
	bool bHost = false;
	FString Dir;
	FString Map, Digested;
	int32 Step = 0, LastStep = -1;
	int32 Tries = 0;
	int32 Cursor = 0;
	double Start = 0, MapStart = 0, StepStart = 0, NotBefore = 0;
	FVector Mark = FVector::ZeroVector;
	TArray<TWeakObjectPtr<class AActor>> Todo;
	// the host's interview: one move per entry (0 confront / press, 1 raise their record)
	TArray<FString> MoveWho, MoveClue;
	TArray<int32> MoveLine;
	TArray<uint8> MoveKind;
	bool bQuit = false;
};
