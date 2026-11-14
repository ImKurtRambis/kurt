#include "HUSound.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"

static USoundBase* HULoadSound(FName Name)
{
	if (Name.IsNone()) return nullptr;
	const FString N = Name.ToString();
	return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

// one falloff for every placed sound: full within a room, gone a corridor or two away
static USoundAttenuation* HUFalloff()
{
	static TStrongObjectPtr<USoundAttenuation> A;
	if (!A.IsValid())
	{
		A.Reset(NewObject<USoundAttenuation>());
		A->Attenuation.bAttenuate = true; A->Attenuation.bSpatialize = true;
		A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		A->Attenuation.AttenuationShapeExtents = FVector(250.f);
		A->Attenuation.FalloffDistance = 2200.f;
	}
	return A.Get();
}

static bool HUHasAudio(const UWorld* W) { return W && W->GetNetMode() != NM_DedicatedServer; }

void HUSound::Play(const UObject* WorldContext, FName Name, const FVector& At, float Volume)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return;
	if (W->GetNetMode() == NM_Standalone || W->GetNetMode() == NM_Client)
	{
		// standalone: just play it. A client calling this is predicting its own action: play it here only.
		if (USoundBase* S = HULoadSound(Name)) UGameplayStatics::PlaySoundAtLocation(W, S, At, Volume, 1.f, 0.f, HUFalloff());
		return;
	}
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AHUSoundCue* C = W->SpawnActorDeferred<AHUSoundCue>(AHUSoundCue::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		C->Sound = Name; C->Volume = Volume;
		C->FinishSpawning(FTransform(At));
	}
}

void HUSound::PlayLocal2D(const UObject* WorldContext, FName Name, float Volume)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (HUHasAudio(W)) if (USoundBase* S = HULoadSound(Name)) UGameplayStatics::PlaySound2D(W, S, Volume);
}

AHUSoundCue::AHUSoundCue()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	InitialLifeSpan = 3.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AHUSoundCue::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHUSoundCue, Sound, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AHUSoundCue, Volume, COND_InitialOnly);
}

void AHUSoundCue::BeginPlay()
{
	Super::BeginPlay();
	if (HUHasAudio(GetWorld())) if (USoundBase* S = HULoadSound(Sound)) UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), Volume, 1.f, 0.f, HUFalloff());
}

// a click for every mouse press while a menu or screen has the cursor up
class FHUClickInput : public IInputProcessor
{
public:
	TWeakObjectPtr<UWorld> World;
	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& App, const FPointerEvent& E) override
	{
		UWorld* W = World.Get();
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (PC && PC->bShowMouseCursor) HUSound::PlayLocal2D(W, TEXT("SFX_Click"), 0.6f);
		return false;
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("HUClickInput"); }
};

void UHUSoundWorld::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if (!HUHasAudio(&World)) return;
	// each map's bed: the street in the rain at a scene, the hum of the station at the precinct, music over the menus
	const FString Map = World.GetMapName();
	struct FBed { const TCHAR* Sound; float Volume; };
	TArray<FBed> Beds;
	if (Map.Contains(TEXT("Scene"))) Beds = { { TEXT("AMB_Rain"), 0.45f }, { TEXT("AMB_Street"), 0.5f }, { TEXT("MUS_Bed"), 0.12f } };
	else if (Map.Contains(TEXT("Precinct")) || Map.Contains(TEXT("Briefing"))) Beds = { { TEXT("AMB_Precinct"), 0.3f }, { TEXT("AMB_Rain"), 0.12f }, { TEXT("MUS_Bed"), 0.15f } };
	else Beds = { { TEXT("AMB_Rain"), 0.3f }, { TEXT("MUS_Bed"), 0.35f } }; // menu and lobby
	for (const FBed& B : Beds)
		if (USoundBase* S = HULoadSound(B.Sound))
			if (UAudioComponent* A = UGameplayStatics::CreateSound2D(&World, S, B.Volume, 1.f, 0.f, nullptr, true))
			{
				A->FadeIn(2.f, B.Volume);
				Loops.Add(A);
			}
	// arriving anywhere but the menus: the door shuts behind you
	if (Map.Contains(TEXT("Scene")) || Map.Contains(TEXT("Precinct")) || Map.Contains(TEXT("Briefing")))
	{
		FTimerHandle H;
		World.GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]() { HUSound::PlayLocal2D(this, TEXT("SFX_Door"), 0.5f); }), 0.4f, false);
	}
	UE_LOG(LogTemp, Log, TEXT("HUSound: %s plays %d of %d beds"), *Map, Loops.Num(), Beds.Num());
	if (FSlateApplication::IsInitialized() && !Clicks)
	{
		TSharedPtr<FHUClickInput> C = MakeShared<FHUClickInput>();
		C->World = &World;
		Clicks = C;
		FSlateApplication::Get().RegisterInputPreProcessor(Clicks);
	}
}

void UHUSoundWorld::Deinitialize()
{
	if (Clicks && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Clicks);
	Clicks.Reset();
	for (UAudioComponent* A : Loops) if (A) A->Stop();
	Loops.Empty();
	Super::Deinitialize();
}

void UHUSoundWorld::Tick(float DeltaTime)
{
	UWorld* W = GetWorld();
	if (!HUHasAudio(W) || !W->HasBegunPlay()) return;
	// a step every so far walked on the ground, by every character (detectives, suspects being walked out)
	for (TActorIterator<ACharacter> It(W); It; ++It)
	{
		ACharacter* C = *It;
		const UCharacterMovementComponent* M = C->GetCharacterMovement();
		if (!M || !M->IsMovingOnGround()) continue;
		const float Speed = C->GetVelocity().Size2D();
		if (Speed < 40.f) { StepDist.Remove(C); continue; }
		float& D = StepDist.FindOrAdd(C);
		D += Speed * DeltaTime;
		const float Stride = Speed > 450.f ? 170.f : 120.f;
		if (D < Stride) continue;
		D = 0.f;
		LastStep = (LastStep + 1 + FMath::RandRange(0, 2)) % 4;
		if (USoundBase* S = HULoadSound(*FString::Printf(TEXT("SFX_Step%d"), LastStep + 1)))
		{
			const bool bMine = C->IsLocallyControlled();
			UGameplayStatics::PlaySoundAtLocation(W, S, C->GetActorLocation() - FVector(0, 0, C->GetSimpleCollisionHalfHeight()),
				(bMine ? 0.35f : 0.5f) * FMath::FRandRange(0.85f, 1.f), FMath::FRandRange(0.94f, 1.06f), 0.f, HUFalloff());
		}
	}
	for (auto It = StepDist.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
}
