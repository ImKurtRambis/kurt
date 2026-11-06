// The security desk at a crime scene: a monitor wired to the building's cameras. E sits you down to review
// the night's recording - grainy black and white, timestamped:
//   A / D   switch camera        W / S   step through the recording        Q   step away
// The recording is a timeline generated with the case (see AHUCrimeScene): where everyone was at a handful of
// moments around the crime, shown as dark figures in their coats. Sometimes the recorder was switched off right
// around the crime - and that's a clue too.
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

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() TArray<USceneComponent*> Ghosts;   // one figure per person on the tape
	TWeakObjectPtr<APawn> Viewer;
	float Flicker = 0.f;
	void Open(APawn* User);
	void Close();
	void Show();          // move the camera and the ghosts, capture
	void NextCam(); void PrevCam(); void NextFrame(); void PrevFrame(); void CloseKey();
};
