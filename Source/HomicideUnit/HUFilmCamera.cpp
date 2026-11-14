#include "HUFilmCamera.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Texture2D.h"

static TAutoConsoleVariable<float> CVarFilmFstop(TEXT("HU.Film.Fstop"), 0.f, TEXT("Lens aperture: 0 = automatic (f/4 on foot, f/2.8 at the desk and in the car)"));
static TAutoConsoleVariable<float> CVarFilmGrain(TEXT("HU.Film.Grain"), 0.45f, TEXT("Film grain, 0..1"));
static TAutoConsoleVariable<int32> CVarFilmOff(TEXT("HU.Film.Off"), 0, TEXT("1 = no film camera (compare with the plain render)"));

void UHUFilmCamera::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FActorSpawnParameters SP;
	SP.Name = TEXT("HU_FilmCamera");
	SP.ObjectFlags |= RF_Transient;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Volume = InWorld.SpawnActor<APostProcessVolume>(SP);
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit film camera: %s in %s"), Volume ? TEXT("on") : TEXT("couldn't spawn its volume"), *InWorld.GetMapName());
	if (!Volume) return;
	Volume->bUnbound = true;   // the whole level, whatever the camera
	Volume->Priority = -10.f;  // a volume placed in a map (or EA-08's grade) wins where they overlap
	Volume->BlendWeight = 1.f;
	FPostProcessSettings& S = Volume->Settings;

	// exposure: histogram eye adaptation held near the fixed exposure the game's lights were made for
	// (legacy luminance units - r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange is off - where 1 is that fixed point)
	S.bOverride_AutoExposureMethod = true;      S.AutoExposureMethod = AEM_Histogram;
	S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = 0.6f;
	S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = 1.8f;
	S.bOverride_AutoExposureSpeedUp = true;     S.AutoExposureSpeedUp = 2.f;   // into the light: quick, like squinting
	S.bOverride_AutoExposureSpeedDown = true;   S.AutoExposureSpeedDown = 0.8f; // into the dark: slow, eyes take time

	// the lens: a full-frame sensor so a wide view can still throw the background soft; focus is set every frame
	S.bOverride_DepthOfFieldSensorWidth = true; S.DepthOfFieldSensorWidth = 36.f;
	S.bOverride_DepthOfFieldFstop = true;       S.DepthOfFieldFstop = 4.f;
	S.bOverride_DepthOfFieldFocalDistance = true; S.DepthOfFieldFocalDistance = Focus;

	// the film
	S.bOverride_FilmGrainIntensity = true;      S.FilmGrainIntensity = CVarFilmGrain.GetValueOnGameThread();
	// the engine's own grain scan (named as the default in BaseEngine.ini, but not always loaded by then: without it the
	// grain is a flat placeholder). Cooked via DirectoriesToAlwaysCook in DefaultGame.ini
	if (UTexture2D* Grain = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/FilmGrains/Marcie_Grain_v3_128_M2_000.Marcie_Grain_v3_128_M2_000")))
	{
		S.bOverride_FilmGrainTexture = true;    S.FilmGrainTexture = Grain;
	}
	S.bOverride_VignetteIntensity = true;       S.VignetteIntensity = 0.45f;
	S.bOverride_BloomIntensity = true;          S.BloomIntensity = 0.55f;
	S.bOverride_SceneFringeIntensity = true;    S.SceneFringeIntensity = 0.3f;
	S.bOverride_MotionBlurAmount = true;        S.MotionBlurAmount = 0.3f;
	S.bOverride_LensFlareIntensity = true;      S.LensFlareIntensity = 0.f; // no lens flares: it's a film camera, not a game trailer
}

// a focus puller: find what's in the middle of the frame and ease the focus onto it
void UHUFilmCamera::Tick(float DeltaTime)
{
	if (!Volume) return;
	Volume->bEnabled = CVarFilmOff.GetValueOnGameThread() == 0;
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager) return;
	const FVector From = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Dir = PC->PlayerCameraManager->GetCameraRotation().Vector();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(HUFilmFocus), false);
	if (APawn* P = PC->GetPawn()) Q.AddIgnoredActor(P);
	FHitResult Hit;
	OverrideLeft -= DeltaTime;
	const float Target = OverrideLeft > 0.f ? FocusOverride
		: W->LineTraceSingleByChannel(Hit, From, From + Dir * 20000.f, ECC_Visibility, Q) ? FMath::Max(Hit.Distance, 30.f) : 20000.f;
	Focus = FMath::FInterpTo(Focus, Target, DeltaTime, 4.f);

	// on foot, a working aperture on a full-frame sensor (the background goes soft when you lean in to examine
	// something); looking out from a seat (the desk menu, the cruiser), a large-format camera at f/2.8: the whole
	// desk sharp, the window just soft, the city beyond it a blur of lights
	const bool bOnFoot = Cast<APawn>(PC->GetViewTarget()) != nullptr;
	const float Fixed = CVarFilmFstop.GetValueOnGameThread();
	FPostProcessSettings& S = Volume->Settings;
	S.DepthOfFieldFocalDistance = Focus;
	S.DepthOfFieldSensorWidth = bOnFoot ? 36.f : 60.f;
	S.DepthOfFieldFstop = Fixed > 0.f ? Fixed : bOnFoot ? 4.f : 2.8f;
	S.FilmGrainIntensity = CVarFilmGrain.GetValueOnGameThread();
#if !UE_BUILD_SHIPPING
	static double LastLog = 0.0;
	if (W->GetTimeSeconds() - LastLog > 5.0)
	{
		LastLog = W->GetTimeSeconds();
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit film camera: focus %.0f cm, f/%.1f, grain %.2f, %s"), Focus, S.DepthOfFieldFstop, S.FilmGrainIntensity, Volume->bEnabled ? TEXT("on") : TEXT("off"));
	}
#endif
}
