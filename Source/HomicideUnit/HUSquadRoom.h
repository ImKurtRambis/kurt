// Two places, one class:
// ROLL CALL (L_Lobby, AHUSquadRoom): just where the squad clocks in. A chalkboard shows the squad's 6-digit code
// (friends type it into their cell phone at the desk menu) and who's punched in; E on the board copies the code.
// The host picks the case at the table; once everyone's punched in at the time clock (or pressed Y) the squad goes
// to the Captain's briefing together.
// THE PRECINCT (L_Precinct, AHUPrecinct): the squad's base, the same for every case - where it gets solved. Back
// from a scene you land here: the desk computers (the case archive, the security tapes), the case board, holding
// and the booking counter, the Captain's in-tray, the motor pool (back out to the scene), coffee - and the door to
// roll call for the next shift once the case is in.
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
	UPROPERTY(VisibleAnywhere, Category = "Visuals") class UPostProcessComponent* VisualGrade;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals") class UMaterialInterface* FloorSurface = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals") class UMaterialInterface* WallSurface = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals") class UMaterialInterface* WoodSurface = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals") class UMaterialInterface* PropSurfaceParent = nullptr;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* BoardTitle;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* BoardCode;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* BoardRoster;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* DoorSign;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* LaptopScreen;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* FolderLabel;
	// true: this is the precinct (see AHUPrecinct), not roll call
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad") bool bPrecinct = false;

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
	virtual float UseTime(APawn* User, bool bSecondary) const override;
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
	UPROPERTY() TArray<UPrimitiveComponent*> CounterParts;
	UPROPERTY() TArray<UPrimitiveComponent*> TrayParts;
	UPROPERTY() TArray<UPrimitiveComponent*> ShiftParts; // the precinct's door to roll call
	// the prisoners in holding (figures, by suspect id)
	UPROPERTY() TMap<FString, USceneComponent*> Prisoners;
	void UpdateHolding();
	float Tock = 0.f;
	bool bStarting = false;
	bool bKeysBound = false;
	class AHUPlayerState* LocalState() const;
	FString PlaceLabel() const;
	bool bWelcomed = false;
	class UHUSquadSubsystem* Squad() const;
	bool IsHost() const;
};

// THE PRECINCT: the squad's base (its own level, L_Precinct). Same rooms for every case.
UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUPrecinct : public AHUSquadRoom
{
	GENERATED_BODY()
public:
	AHUPrecinct() { bPrecinct = true; }
};
