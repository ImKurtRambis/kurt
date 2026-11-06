// THE SQUAD ROOM: the pre-lobby. Everyone waits here before the shift starts.
// A chalkboard shows the squad's 6-digit code (friends type it into their cell phone at the desk menu) and
// who's here. E on the board copies the code. The host walks to the ROLL CALL door and presses E to start:
// the whole squad goes to the Captain's briefing together. ESC leaves the squad and goes back to the desk.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUSquadRoom.generated.h"

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUSquadRoom : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUSquadRoom();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* BoardTitle;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* BoardCode;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* BoardRoster;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* DoorSign;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* LaptopScreen;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* FolderLabel;

	// the host's picks for the shift (-1 = random). Replicated, so the whole squad gets the same case.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Squad") int32 PlaceChoice = -1;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Squad") int32 DifficultyChoice = -1;
	// seconds left on the 'everyone's ready' countdown (-1 = not counting)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Squad") int32 Countdown = -1;
	// punch in / out: ready or not (the time clock by the doors, or Y anywhere in the room)
	UFUNCTION(BlueprintCallable, Category = "Squad") void ToggleReady();
	static const TCHAR* DifficultyName(int32 I);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() TArray<UPrimitiveComponent*> BoardParts;
	UPROPERTY() TArray<UPrimitiveComponent*> DoorParts;
	UPROPERTY() TArray<UPrimitiveComponent*> LaptopParts;
	UPROPERTY() TArray<UPrimitiveComponent*> FolderParts;
	UPROPERTY() TArray<UPrimitiveComponent*> ComputerParts;
	UPROPERTY() TArray<UPrimitiveComponent*> CoffeeParts;
	UPROPERTY() TArray<UPrimitiveComponent*> ClockParts;
	float Tock = 0.f;
	bool bStarting = false;
	bool bKeysBound = false;
	class AHUPlayerState* LocalState() const;
	FString PlaceLabel() const;
	bool bWelcomed = false;
	class UHUSquadSubsystem* Squad() const;
	bool IsHost() const;
};
