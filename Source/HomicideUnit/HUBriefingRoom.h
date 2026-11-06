// Drop one of these in an empty level and press Play: it builds the whole briefing room (walls, carpet,
// rows of chairs, podium, the Captain, the projector screen and the laptop table), seats you in the aisle,
// and gives you E / Q. The slides show on the wall screen; the Captain's lines show as subtitles.
// Everything is placeholder blocks for now — swap in real meshes later without touching the logic.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUCaseTypes.h"
#include "HUInteractable.h"
#include "HUBriefingRoom.generated.h"

class UTextRenderComponent;
class AHUBriefingLaptop;

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUBriefingRoom : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUBriefingRoom();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing") FName SceneLevel = TEXT("L_Scene");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing") FString DetectiveName = TEXT("Detective");
	// put the player in the aisle seat when play starts
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing") bool bSeatPlayer = true;

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) UTextRenderComponent* ScreenHeader;
	UPROPERTY(VisibleAnywhere) UTextRenderComponent* ScreenTitle;
	UPROPERTY(VisibleAnywhere) UTextRenderComponent* ScreenRows;
	UPROPERTY(VisibleAnywhere) UTextRenderComponent* ScreenCounter;

	UPROPERTY(VisibleAnywhere) UTextRenderComponent* ExitSign;
	// the motor pool, built out the back door at the end of a corridor
	UPROPERTY(VisibleAnywhere) class UChildActorComponent* MotorPool;
	UPROPERTY(EditAnywhere, Category = "Briefing") FVector MotorPoolPos = FVector(-2300.f, 0.f, 0.f);

	UPROPERTY(BlueprintReadOnly) AHUBriefingLaptop* Laptop = nullptr;

	// called by the laptop whenever the slide changes
	UFUNCTION(BlueprintCallable, Category = "Briefing") void ShowSlide(const FHUSlide& Slide, int32 SlideIndex, int32 SlideCount);

	// the chairs: E sits you down
	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	// every part of a chair -> the chair's cushion (local)
	UPROPERTY() TMap<UPrimitiveComponent*, FVector> Seats;
	UPROPERTY() TArray<UStaticMeshComponent*> CaptainParts;
	UPROPERTY() TArray<UStaticMeshComponent*> PodiumParts;
	UPROPERTY() UStaticMesh* Cube = nullptr;
	UPROPERTY() UStaticMesh* Cylinder = nullptr;
	UPROPERTY() UStaticMesh* Sphere = nullptr;
	UPROPERTY() UMaterialInterface* BaseMat = nullptr;
	bool bPlayerReady = false;
	// the 911 call
	TArray<FString> CallSpeakers, CallLines;
	int32 CallIdx = -1;
	float CallWait = 3.f, LineTime = 0.f, CallEndTimer = 0.f, LineLength = 0.f, LineWait = 0.f;
	UPROPERTY() TArray<class USoundWave*> CallSounds;
	TArray<float> CallSeconds;
	TArray<uint8> CallState;
	bool bCallDone = false;
	void Play911(float DeltaSeconds);
	void SeatSquad();
	TArray<TWeakObjectPtr<APawn>> Seated;
	FString PendingLine;

	void Build();
	void BuildCorridor();
	UStaticMeshComponent* Block(UStaticMesh* Mesh, FVector Pos, FVector SizeCm, FLinearColor Color, FRotator Rot = FRotator::ZeroRotator);
	UTextRenderComponent* Text(FName Name, FVector Pos, float Size, FColor Color);
};
