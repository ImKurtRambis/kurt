// THE SECURITY TAPES.
// At the crime scene this is the building's recorder: you can't watch it there - hold E to PULL THE TAPES.
// Back at the precinct, any squad desk computer plays them (this same class, bAtPrecinct, no desk of its own):
//   the clips play on their own (grainy, black and white, timestamped)
//   A / D camera     W / S step (pauses)     SPACE play / pause     E flag this moment     Q step away
// The night is scripted with the case (see AHUCrimeScene): who was with whom in the half hour before the crime,
// strangers passing through, and the moments around it. Flagging a moment writes down what you make of it
// (on your pager, and the squad's); flag the one clip that shows the culprit where they swore they weren't and
// their lie goes on the record. Sometimes the recorder was switched off right around the crime - a clue too.
// How it works: each camera is a scene capture; people in the recording are "ghost" figures only the capture can
// see, moved to their positions for the moment you're viewing; the real people in the room are hidden from it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUSecurityDesk.generated.h"

// one moment on the tape
USTRUCT(BlueprintType)
struct FHUFootageFrame
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Clock;          // "10:34 PM"
	UPROPERTY(BlueprintReadOnly) TArray<FVector> People; // world positions: one per suspect, then the victim (ZeroVector = not there)
	UPROPERTY(BlueprintReadOnly) bool bLost = false;     // the recorder was off
	UPROPERTY(BlueprintReadOnly) bool bAfter = false;    // after the crime (the victim is down)
};

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUSecurityDesk : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUSecurityDesk();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class USceneCaptureComponent2D* Capture;

	// set by the crime scene before it finishes spawning
	UPROPERTY() TArray<FTransform> Cameras;        // world transforms
	UPROPERTY() TArray<FString> CameraNames;
	UPROPERTY() TArray<FHUFootageFrame> Frames;
	UPROPERTY() TArray<FLinearColor> Coats;        // one per suspect
	UPROPERTY() AActor* VictimActor = nullptr;
	UPROPERTY() TArray<FString> Notes;             // per frame (camera * moments + moment)
	int32 CatchFrame = -1;
	FString CatchSuspect;
	// the precinct copy: plays the pulled tapes, no desk, no cameras of its own
	UPROPERTY() bool bAtPrecinct = false;
	void OpenFor(APawn* User) { Open(User); }
	bool bPaused = false;
	virtual float UseTime(APawn* User, bool bSecondary) const override;

	// what the viewer shows
	UPROPERTY(BlueprintReadOnly) class UTextureRenderTarget2D* Feed = nullptr;
	UPROPERTY(BlueprintReadOnly) int32 Cam = 0;
	UPROPERTY(BlueprintReadOnly) int32 Frame = 0;
	UPROPERTY(BlueprintReadOnly) bool bViewing = false;
	FString CamLabel() const;
	bool FrameLost() const { return Frames.IsValidIndex(Frame) && Frames[Frame].bLost; }

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() TArray<AActor*> Ghosts; // the same cast, visible only to scene captures
	TWeakObjectPtr<APawn> Viewer;
	float Flicker = 0.f;
	void Open(APawn* User);
	void Close();
	void Show();          // move the camera and the ghosts, capture
	void NextCam(); void PrevCam(); void NextFrame(); void PrevFrame(); void CloseKey();
	void PlayKey(); void FlagKey();
	void PullTapes(APawn* User);  // at the scene: record every camera, every moment, into the case
	float PlayT = 0.f;
	class UHUCaseSubsystem* Cases() const;
};
