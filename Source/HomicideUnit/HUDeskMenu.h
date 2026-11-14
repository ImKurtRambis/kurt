// THE DESK MENU: the main menu is your desk at the precinct, late at night (same as the browser game).
// You sit at your desk; everything on it does something:
//   CASE FILES (start / continue your career)  ·  DESK PHONE (host a squad)  ·  YOUR CELL PHONE (join a squad)
//   THE COMPUTER (settings)  ·  POLICE MANUAL (how to play)  ·  THE PAPER (what's new)
//   YOUR NAME PLATE (your personnel file: your name, and how your detective looks - build, height, skin, hair, coat, hat)  ·  FRAMED PHOTO (credits)  ·  CAR KEYS (go home: quit)
// Hover an object to light it up and read what it does; click it - or ← → and ENTER. ESC closes a panel.
// Your name, the join address and your settings are saved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUDeskMenu.generated.h"

UENUM(BlueprintType)
enum class EHUDeskItem : uint8 { Manual, DeskPhone, CellPhone, Photo, CaseFiles, NamePlate, Paper, Computer, CarKeys, Count UMETA(Hidden) };

UENUM(BlueprintType)
enum class EHUMenuPanel : uint8 { None, Manual, Paper, Credits, Personnel, Settings, Join, Quit, Missing, Squad };

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUDeskMenu : public AActor
{
	GENERATED_BODY()
public:
	AHUDeskMenu();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UCameraComponent* Seat;
	UPROPERTY(VisibleAnywhere) class USpotLightComponent* Highlight;
	UPROPERTY(VisibleAnywhere) class UPointLightComponent* Lamp;      // a soft fill around the lamp
	UPROPERTY(VisibleAnywhere) class USpotLightComponent* LampSpot;   // under the shade: the pool of light on the desk
	UPROPERTY(VisibleAnywhere) class USpotLightComponent* Street;     // a streetlight outside, striped by the blinds
	UPROPERTY(VisibleAnywhere) class UPointLightComponent* Neon;      // the sign across the street, flickering
	UPROPERTY(VisibleAnywhere) class UPointLightComponent* ScreenGlow; // the computer's screen
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* PlateText;

	// the level the case files and the desk phone take you to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu") FName BriefingLevel = TEXT("L_Briefing");

	// read by the menu widget
	UPROPERTY(BlueprintReadOnly) int32 Pick = (int32)EHUDeskItem::CaseFiles;
	UPROPERTY(BlueprintReadOnly) int32 Hover = -1;
	UPROPERTY(BlueprintReadOnly) EHUMenuPanel Panel = EHUMenuPanel::None;
	UPROPERTY(BlueprintReadOnly) int32 Row = 0;
	UPROPERTY(BlueprintReadOnly) bool bTyping = false;

	static const TCHAR* ItemName(EHUDeskItem I);
	FString ItemDoes(EHUDeskItem I) const;
	// the panel's title and lines (for the widget)
	void PanelText(FString& Title, TArray<FString>& Lines) const;

	UFUNCTION(BlueprintCallable, Category = "Menu") void Use(EHUDeskItem Item);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() TMap<UPrimitiveComponent*, int32> ItemOf;

	// ---- the night outside: rain falling past the window, drops running down the glass, the neon, lightning ----
	UPROPERTY() class UInstancedStaticMeshComponent* RainFall = nullptr;
	UPROPERTY() class UInstancedStaticMeshComponent* RainDrops = nullptr;
	struct FDrop { FVector P; float Size = 0.4f; float Len = 0.6f; float Speed = 0.f; float Hold = 0.f; };
	TArray<FVector> Falling;
	TArray<FDrop> Drops;
	FRandomStream Weather;
	UPROPERTY() TArray<class UMaterialInstanceDynamic*> NeonMats; // the sign's letters
	UPROPERTY() TArray<class UMaterialInstanceDynamic*> SkyMats;  // the sky bands (lightning lights them)
	TArray<FLinearColor> SkyBase;
	UPROPERTY() TArray<class UTextRenderComponent*> Labels;       // print on things (rebuilt with the blocks)
	UPROPERTY() class UTextRenderComponent* NeonText = nullptr;
	FLinearColor NeonColor;
	float NeonOff = 0.f, NextFlicker = 2.5f, Flash = -1.f, NextFlash = 14.f, StreetBase = 0.f;
	void TickWeather(float Dt);
	void ResetDrop(FDrop& D, bool bAnywhere);
	FVector ItemPos[(int32)EHUDeskItem::Count];
	UPROPERTY() class UHUMenuWidget* Widget = nullptr;
	FString Typed;          // what's being typed
	int32 TypingField = 0;  // 0 name, 1 join address
	bool bPlayAfterName = false;
	FString MissingLevel;

	void Prev(); void Next(); void Activate(); void Back(); void Click(); void Up(); void Down();
	void OnAnyKey(FKey Key);
	void StartTyping(int32 Field);
	void FinishTyping(bool bKeep);
	void ChangeSetting(int32 Dir);
	void ChangeLook(int32 Dir);
	void BuildFigurine();
	UPROPERTY() USceneComponent* Figurine = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> FigureParts;
	int32 RowsIn(EHUMenuPanel P) const;
	bool LevelExists(FName Level) const;
	void GoTo(FName Level, const FString& Options);
	FString Name() const;
	class UHUCaseSubsystem* Cases() const;
	class APlayerController* PC() const;
};
