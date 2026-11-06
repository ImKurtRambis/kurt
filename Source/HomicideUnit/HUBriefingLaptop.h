// The laptop in the briefing room that drives the projector. E = next slide, Q = previous;
// on the last slide E rolls out to the scene. AHUBriefingRoom spawns one and wires it to its wall screen.
// For a custom look, make a Blueprint child, give it a real laptop mesh, and override OnSlideChanged.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUCaseTypes.h"
#include "HUBriefingLaptop.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUBriefingLaptop : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUBriefingLaptop();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* Laptop;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* Lid = nullptr;

	// the level to open when the briefing's over
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing") FName SceneLevel = TEXT("L_Scene");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing") FString DetectiveName = TEXT("Detective");
	// the room whose wall screen shows the slides (set by AHUBriefingRoom; optional)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing") class AHUBriefingRoom* Room = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Briefing") TArray<FHUSlide> Slides;
	UPROPERTY(BlueprintReadOnly, Category = "Briefing") int32 Index = 0;

	UFUNCTION(BlueprintCallable, Category = "Briefing") void Go(int32 Delta);
	UFUNCTION(BlueprintPure, Category = "Briefing") bool IsLastSlide() const { return Index >= Slides.Num() - 1; }

	// default: shows the slide on Room's screen. Override in a Blueprint to draw it your own way.
	UFUNCTION(BlueprintNativeEvent, Category = "Briefing") void OnSlideChanged(const FHUSlide& Slide, int32 SlideIndex, int32 SlideCount);
	// default: opens SceneLevel. Override to play a drive cutscene first.
	UFUNCTION(BlueprintNativeEvent, Category = "Briefing") void RollOut();

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;

protected:
	virtual void BeginPlay() override;
};
