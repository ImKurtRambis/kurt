// LAMPS YOU CAN SEE: every room is lit by fittings that are really there, not by lights hung in the air.
//   Tube       a ceiling strip: a steel channel with two bare fluorescent tubes, giving a soft rect light down. They
//              hum (heard close up), and now and then a tired one stutters and flickers
//   Pendant    a warm bulb under an enamel shade on a drop (diners, back rooms, lofts) - or flush to the ceiling as a
//              dome (hotel rooms and corridors)
//   DeskLamp   a lamp on a desk with its bulb's pool of light; E switches it (HUUsables)
//   ExitSign   EXIT over a door, glowing red, tinting the wall
//   Window     a window with venetian blinds and a streetlight outside: the slats stripe the floor and desks
// Units: candela, in the game's scale (a ceiling fixture ~9, a desk lamp ~2) - the film camera's exposure is set for it.
// The light from a fixture is its own: a room's light switch (HUUsables) turns off tubes and pendants, never desk
// lamps, exit signs or the street (those are tagged HUOwnLight).
// Brightness 0 / bDead: the fitting is there with its tubes or bulb out (a dark room at a scene).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HULamps.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
class URectLightComponent;
class UPointLightComponent;
class USpotLightComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;

namespace HULamps
{
	// a self-lit look for C (ChatGPT's M_HU_Glow, tinted Col; without it, a bright plain colour)
	HOMICIDEUNIT_API UMaterialInstanceDynamic* Glow(UPrimitiveComponent* C, FLinearColor Col);

	// At: the middle of the ceiling's underside where it hangs (Parent's frame); Yaw: which way its length runs;
	// Drop: hung that far down on two chains (a high ceiling)
	HOMICIDEUNIT_API URectLightComponent* Tube(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out,
		FVector At, float Yaw = 0.f, float Candela = 9.f, bool bDead = false, float Drop = 0.f);
	// Drop: how far below the ceiling the bulb hangs (0 = a dome flush to the ceiling)
	HOMICIDEUNIT_API UPointLightComponent* Pendant(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out,
		FVector At, float Drop = 60.f, float Candela = 8.f, FLinearColor Col = FLinearColor(1.f, 0.78f, 0.5f), bool bDead = false);
	// Base: the desk top under the lamp's foot; Yaw: the way it faces (its light falls a little that way)
	HOMICIDEUNIT_API USpotLightComponent* DeskLamp(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out,
		FVector Base, float Yaw, bool bOn = true);
	// light a lamp that's already standing (a marked HUUsables lamp - an office's banker's or floor lamp): its bulb
	// lights the desk (a spot down) or the room (a floor lamp), and E switches it
	HOMICIDEUNIT_API void LightLamp(UPrimitiveComponent* Shade, bool bOn = true);
	// At: on the wall's face over the door; Yaw: the way it faces (into the room)
	HOMICIDEUNIT_API void ExitSign(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, FVector At, float Yaw);
	// At: the middle of the window on the wall's room-side face; Yaw: into the room
	HOMICIDEUNIT_API void Window(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, FVector At, float Yaw,
		float Width = 120.f, float Height = 140.f, FLinearColor Street = FLinearColor(1.f, 0.62f, 0.3f));
}

// the tubes' flicker and hum
UCLASS()
class HOMICIDEUNIT_API UHULampWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	struct FTube
	{
		TWeakObjectPtr<URectLightComponent> Light;
		TWeakObjectPtr<UPointLightComponent> Up; // its faint glow onto the ceiling
		TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> Glows;
		FLinearColor GlowColor;
		float Candela = 9.f;
		bool bTired = false;       // this one stutters now and then
		float NextGlitch = 0.f, GlitchEnd = -1.f, NextStep = 0.f, Level = 1.f;
		uint32 Seed = 0;
	};
	TArray<FTube> Tubes;
	void AddTube(URectLightComponent* L, UPointLightComponent* Up, const TArray<UMaterialInstanceDynamic*>& Glows, FLinearColor GlowColor, float Candela);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHULampWorld, STATGROUP_Tickables); }
	virtual void Deinitialize() override;

private:
	float Time = 0.f, Reassign = 0.f;
	// the hum: a few voices, moved to the tubes nearest the listener (a precinct has dozens of tubes)
	UPROPERTY() TObjectPtr<AActor> HumHolder;
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Hums;
	TArray<int32> HumTube;
	void StartHums();
};
