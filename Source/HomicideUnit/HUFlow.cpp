#include "HUFlow.h"
#include "HUPeople.h"
#include "HUCaseSubsystem.h"
#include "HUCruiser.h"
#include "HUMotorPoolDoor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"
#if !UE_BUILD_SHIPPING
#include "HUSquadRoom.h"
#include "HUInteractComponent.h"
#include "HUInteractable.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"
#endif

namespace
{
	const float MaxCover = 5.f;             // the longest a place stays behind black, however it's running
	const float SmoothFrame = 1.f / 15.f;   // a frame this quick counts as running smoothly
	const float FadeIn = 0.5f;
	FString FaceClass(const FString& Who) { return FString::Printf(TEXT("/Game/MetaHumans/MH_%s/BP_MH_%s.BP_MH_%s_C"), *Who, *Who, *Who); }
	FString SeenFile() { return FPaths::ProjectSavedDir() / TEXT("HUFlowFaces.txt"); }
	bool IsPlace(const FString& Map) { return Map.StartsWith(TEXT("L_")) && Map != TEXT("L_Menu"); }
}

// where the game goes next from here. From the precinct, going back out to the scene is a choice: it's warmed once
// you head for the motor pool (the scene is the biggest cast there is); a submitted case goes on to roll call.
TArray<FString> UHUFlow::NextStops(UWorld* World) const
{
	TArray<FString> Out;
	const UHUCaseSubsystem* C = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	if (HereMap == TEXT("L_Scene")) Out.Add(TEXT("L_Precinct"));
	else if (HereMap == TEXT("L_Briefing")) Out.Add(TEXT("L_Scene"));
	else if (HereMap == TEXT("L_Lobby")) Out.Add(TEXT("L_Briefing"));
	else if (HereMap == TEXT("L_Precinct"))
	{
		if (!C || !C->bAwaitingCaptain) Out.Add(TEXT("L_Lobby"));
		else if (const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
			if (const APawn* P = PC->GetPawn())
				for (TActorIterator<AHUMotorPoolDoor> It(World); It; ++It)
					if (FVector::Dist(P->GetActorLocation(), It->GetActorLocation()) < 2000.f) { Out.Add(TEXT("L_Scene")); break; }
	}
	return Out;
}

void UHUFlow::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// no screen (headless tests): nothing to cover, and nobody to warm up for
	bWarming = FApp::CanEverRender() && !FParse::Param(FCommandLine::Get(), TEXT("HUFlowNoWarm"));
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToStringArray(Lines, *SeenFile()))
		for (const FString& L : Lines)
		{
			FString Map, Faces;
			if (L.Split(TEXT("="), &Map, &Faces)) Faces.ParseIntoArray(Seen.FindOrAdd(Map.TrimStartAndEnd()), TEXT(","), true);
		}
	PreH = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UHUFlow::OnPreLoad);
	SeamH = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &UHUFlow::OnSeamlessStart);
	PostH = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UHUFlow::OnPostLoad);
	TickH = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UHUFlow::Tick));
#if !UE_BUILD_SHIPPING
	bTest = FParse::Param(FCommandLine::Get(), TEXT("HUFlowTest"));
#endif
}

void UHUFlow::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreH);
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamH);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostH);
	FTSTicker::GetCoreTicker().RemoveTicker(TickH);
	for (TPair<FString, TSharedPtr<FStreamableHandle>>& W : Warm) if (W.Value.IsValid()) W.Value->ReleaseHandle();
	Warm.Empty();
	Super::Deinitialize();
}

FString UHUFlow::MapOf(const UWorld* World) const
{
	return World ? UWorld::RemovePIEPrefix(World->GetMapName()) : FString();
}

int32 UHUFlow::WarmCount(int32* Ready) const
{
	if (Ready)
	{
		*Ready = 0;
		for (const TPair<FString, TSharedPtr<FStreamableHandle>>& W : Warm) if (W.Value.IsValid() && W.Value->HasLoadCompleted()) ++*Ready;
	}
	return Warm.Num();
}

// ---- leaving: note who was here, and hold on to whoever the next place needs, through the trip ----
void UHUFlow::OnPreLoad(const FString& Map)
{
	UWorld* W = Here.Get();
	if (!W || W->bIsTearingDown) return; // (a squad's trip tells us twice: the second time this place is already going)
	NoteFaces(W);
	// where we're actually going (the precinct after a submitted case goes to roll call, not the scene): its people too
	const FString To = FPackageName::GetShortName(Map);
	if (IsPlace(To)) { PendingTo = FacesFor(To); WarmFor(nullptr); }
#if !UE_BUILD_SHIPPING
	if (bTest && PressedAt > 0.0 && LeftAt == 0.0) LeftAt = FPlatformTime::Seconds();
#endif
}

void UHUFlow::OnSeamlessStart(UWorld* World, const FString& Map)
{
	if (World && World == Here.Get()) OnPreLoad(Map);
}

void UHUFlow::NoteFaces(UWorld* World)
{
	const FString Map = MapOf(World);
	if (!IsPlace(Map)) return;
	TArray<FString> Faces;
	for (TActorIterator<AActor> It(World); It; ++It)
		for (const FName& Tag : It->Tags)
		{
			const FString T = Tag.ToString();
			if (T.StartsWith(TEXT("HUFace:"))) Faces.AddUnique(T.Mid(7));
		}
	Faces.Sort();
	if (Faces.Num() == 0 || Seen.FindRef(Map) == Faces) return;
	Seen.Add(Map, Faces);
	SaveSeen();
}

void UHUFlow::SaveSeen() const
{
	TArray<FString> Lines;
	for (const TPair<FString, TArray<FString>>& S : Seen) Lines.Add(S.Key + TEXT("=") + FString::Join(S.Value, TEXT(",")));
	Lines.Sort();
	FFileHelper::SaveStringArrayToFile(Lines, *SeenFile());
}

// ---- arriving: behind black until it runs smoothly ----
void UHUFlow::OnPostLoad(UWorld* World)
{
	if (!World || !World->IsGameWorld()) return;
	const FString Map = MapOf(World);
	const bool bTravelled = Here.IsValid() || !HereMap.IsEmpty(); // not the first place the game opened on
	Here = World;
	HereMap = Map;
	const double Now = FPlatformTime::Seconds();
#if !UE_BUILD_SHIPPING
	if (bTest && LeftAt > 0.0 && ArrivedAt == 0.0) ArrivedAt = Now;
#endif
	if (bTravelled && IsPlace(Map) && FApp::CanEverRender())
	{
		bCovering = true; CoverAt = Now; Smooth = 0; CoverLongest = 0.f;
		Dark(World);
	}
	else { bCovering = false; UpAt = Now; NextWarm = Now + 2.0; }
}

void UHUFlow::Dark(UWorld* World) const
{
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		if (APlayerController* PC = It->Get())
			if (PC->IsLocalController() && PC->PlayerCameraManager) PC->PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, false);
}

void UHUFlow::Lift(UWorld* World)
{
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		if (APlayerController* PC = It->Get())
			if (PC->IsLocalController() && PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeIn, FLinearColor::Black, false, false);
	const double Now = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("HUFlow: %s on screen %.2f s after it loaded (its longest frame %.0f ms)%s"), *HereMap, Now - CoverAt, CoverLongest * 1000.f,
		Now - CoverAt > MaxCover ? TEXT(" - still not smooth, shown anyway") : TEXT(""));
	bCovering = false; UpAt = Now;
	// the people who came along are standing here now (or aren't needed): let them go, and warm the next stop
	for (TPair<FString, TSharedPtr<FStreamableHandle>>& W : Warm) if (W.Value.IsValid()) W.Value->ReleaseHandle();
	Warm.Empty();
	NextWarm = Now + 2.0;
#if !UE_BUILD_SHIPPING
	if (bTest && ArrivedAt > 0.0 && LiftedAt == 0.0) LiftedAt = Now;
#endif
}

bool UHUFlow::Tick(float Dt)
{
	const double Now = FPlatformTime::Seconds();
	UWorld* W = Here.Get();
	if (bCovering)
	{
		if (!W) bCovering = false;
		else
		{
			Dark(W); // a squadmate's camera can turn up a frame or two after the place does
			CoverLongest = FMath::Max(CoverLongest, Dt);
			Smooth = Dt < SmoothFrame ? Smooth + 1 : 0;
			const double Age = Now - CoverAt;
			if ((Age > 0.25 && Smooth >= 4) || Age > MaxCover) Lift(W);
		}
	}
	// the next stop's people, once this place is up (and again every few seconds: the case moves on)
	if (W && !bCovering && bWarming && Now >= NextWarm) { NextWarm = Now + 5.0; WarmFor(W); }
#if !UE_BUILD_SHIPPING
	if (bTest) TestTick(Dt);
#endif
	return true;
}

// ---- warming the next stop ----
TSet<FString> UHUFlow::FacesFor(const FString& Map) const
{
	TSet<FString> F;
	if (const TArray<FString>* S = Seen.Find(Map)) F.Append(*S);
	F.Add(HUPeople::DetectiveFace());
	const UHUCaseSubsystem* C = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	if (C && C->bCaseActive)
	{
		if (Map == TEXT("L_Scene"))
		{
			for (const FHUSuspect& S : C->Current.Suspects) F.Add(HUPeople::SuspectFace(C->Current, S.Id));
			F.Add(HUPeople::VictimFace(C->Current.Victim, C->Current.Number));
			for (int32 V = 0; V < 3; V++) F.Add(HUPeople::OfficerFace(V));
		}
		else if (Map == TEXT("L_Precinct"))
			for (const FString& Id : C->Arrested) F.Add(HUPeople::SuspectFace(C->Current, Id));
	}
	if (Map == TEXT("L_Briefing")) F.Add(HUPeople::CaptainFace());
	F.Remove(FString());
	return F;
}

// World: where we are (warm its next stops), or null on the way out (add the destination's people). Only ever adds:
// what's warm stays warm until the next place is on screen (Lift lets it all go).
void UHUFlow::WarmFor(UWorld* World)
{
	if (!bWarming || !UAssetManager::IsInitialized()) return;
	TSet<FString> Want;
	if (World) for (const FString& Next : NextStops(World)) Want.Append(FacesFor(Next));
	else Want = PendingTo;
	bool bChanged = false;
	FStreamableManager& Streams = UAssetManager::GetStreamableManager();
	for (const FString& Who : Want)
	{
		if (Warm.Contains(Who)) continue;
		bool* Built = Exists.Find(Who);
		if (!Built) Built = &Exists.Add(Who, FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/MetaHumans/MH_%s/BP_MH_%s"), *Who, *Who)));
		if (!*Built) continue;
		Warm.Add(Who, Streams.RequestAsyncLoad(FSoftObjectPath(FaceClass(Who))));
		bChanged = true;
	}
	if (bChanged)
	{
		TArray<FString> Names; Warm.GetKeys(Names); Names.Sort();
		UE_LOG(LogTemp, Log, TEXT("HUFlow: %s - keeping %d people warm for the next stop: %s"), *HereMap, Names.Num(), *FString::Join(Names, TEXT(", ")));
	}
}

#if !UE_BUILD_SHIPPING
// ---- -HUFlowTest on L_Scene: scene -> drive -> precinct (book a prisoner) -> motor pool -> scene ----
namespace
{
	TArray<UPrimitiveComponent*> PartsOf(AActor* A, const TCHAR* Prop)
	{
		TArray<UPrimitiveComponent*> Out;
		FArrayProperty* P = A ? FindFProperty<FArrayProperty>(A->GetClass(), Prop) : nullptr;
		FObjectProperty* Inner = P ? CastField<FObjectProperty>(P->Inner) : nullptr;
		if (!Inner) return Out;
		FScriptArrayHelper H(P, P->ContainerPtrToValuePtr<void>(A));
		for (int32 i = 0; i < H.Num(); i++)
			if (UPrimitiveComponent* Part = Cast<UPrimitiveComponent>(Inner->GetObjectPropertyValue(H.GetRawPtr(i)))) Out.Add(Part);
		return Out;
	}
}

void UHUFlow::TestLog(const FString& Line, bool bOk)
{
	if (!bOk) TestFails++;
	UE_LOG(LogTemp, Display, TEXT("HUFlowTest: %s%s"), bOk ? TEXT("") : TEXT("FAIL "), *Line);
}

void UHUFlow::TestTick(float Dt)
{
	const double Now = FPlatformTime::Seconds();
	UWorld* W = Here.Get();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	APawn* P = PC ? PC->GetPawn() : nullptr;
	UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (TestAt == 0.0) TestAt = Now;
	// a trip under way: the screen going dark, then how the new place ran once it was there
	if (PressedAt > 0.0)
	{
		if (DarkAt == 0.0 && LeftAt == 0.0 && PC && PC->PlayerCameraManager && PC->PlayerCameraManager->FadeAmount >= 0.99f) DarkAt = Now;
		if (ArrivedAt > 0.0 && Now - ArrivedAt < 5.0) { TestLongest = FMath::Max(TestLongest, Dt); if (Dt > 0.1f) TestHitches++; }
	}
	auto Finish = [&]()
	{
		TestLog(TestFails == 0 ? FString(TEXT("all checks passed")) : FString::Printf(TEXT("%d checks failed"), TestFails), TestFails == 0);
		TestStage = 99;
		FPlatformMisc::RequestExit(false, TEXT("HUFlowTest"));
	};
	if (TestStage < 99 && Now - TestAt > 150.0) { TestLog(FString::Printf(TEXT("stuck at step %d on %s"), TestStage, *HereMap), false); Finish(); return; }
	auto Press = [&](AActor* Car, const FVector& Stand)
	{
		P->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
		TestLog(FString::Printf(TEXT("pressing E at %s: \"%s\""), *Car->GetName(), *IHUInteractable::Execute_GetPrompt(Car, P).ToString()));
		TestFrom = HereMap; TestTrip++;
		PressedAt = Now; DarkAt = LeftAt = ArrivedAt = LiftedAt = 0.0; TestLongest = 0.f; TestHitches = 0;
		IHUInteractable::Execute_Interact(Car, P, false);
	};
	auto Report = [&]()
	{
		int32 Ready = 0; const int32 Warmed = WarmCount(&Ready);
		TestLog(FString::Printf(TEXT("trip %d, %s -> %s: dark at %.2f s, left at %.2f s, arrived at %.2f s, on screen at %.2f s; in its first 5 s the longest frame was %.0f ms, %d frames over 100 ms (warming %s)"),
			TestTrip, *TestFrom, *HereMap, DarkAt - PressedAt, LeftAt - PressedAt, ArrivedAt - PressedAt, LiftedAt - PressedAt, TestLongest * 1000.f, TestHitches, bWarming ? TEXT("on") : TEXT("off")));
		TestLog(FString::Printf(TEXT("the car left %.2f s after E (it used to be 3.7 s: a 1.2 s fade, then 2.5 s on black)"), LeftAt - PressedAt), LeftAt > 0.0 && LeftAt - PressedAt < 1.0);
		TestLog(TEXT("the screen was dark before the trip started"), DarkAt > 0.0 && DarkAt <= LeftAt + 0.02);
		TestLog(FString::Printf(TEXT("the new place came up %.2f s after it loaded"), LiftedAt - ArrivedAt), LiftedAt > 0.0 && LiftedAt - ArrivedAt <= MaxCover + 0.5);
		const APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager : nullptr;
		TestLog(FString::Printf(TEXT("the picture is clear (fade %.2f, fading %d)"), Cam ? Cam->FadeAmount : -1.f, Cam ? (int32)Cam->bEnableFading : -1), Cam && (!Cam->bEnableFading || Cam->FadeAmount < 0.02f));
	};
	auto Warmed = [&]() { int32 Ready = 0; const int32 N = WarmCount(&Ready); return !bWarming || (N > 0 && Ready == N) || Now - TestAt > 40.0; };

	switch (TestStage)
	{
	case 0: // at the scene: settle, let the precinct's people warm up, then put a suspect in the van and drive
	{
		if (HereMap != TEXT("L_Scene") || !P || !C || !IsUp() || Now - UpAt < 3.0 || !Warmed()) return;
		AHUCruiser* Car = nullptr;
		for (TActorIterator<AHUCruiser> It(W); It; ++It) if (It->bPlayerCar && !It->IsA<AHUMotorPoolDoor>()) { Car = *It; break; }
		if (!Car || C->Current.Suspects.Num() == 0) { TestLog(TEXT("no car of yours at the scene, or nobody in the case (run it on L_Scene)"), false); Finish(); return; }
		C->Arrested.AddUnique(C->Current.Suspects[0].Id);
		Press(Car, Car->GetActorLocation() + Car->GetActorRightVector() * 250.f + FVector(0, 0, 100));
		TestStage = 1; TestAt = Now;
		return;
	}
	case 1: // the precinct, on screen
		if (HereMap != TEXT("L_Precinct") || LiftedAt == 0.0 || Now - LiftedAt < 1.5 || !P) return;
		Report();
		FScreenshotRequest::RequestScreenshot(TEXT("Flow_Precinct"), false, false);
		TestStage = 2; TestAt = Now;
		return;
	case 2: // book the prisoner: one hold does it
	{
		if (Now - TestAt < 1.0) return;
		AHUSquadRoom* Room = nullptr;
		for (TActorIterator<AHUSquadRoom> It(W); It; ++It) if (It->bPrecinct) { Room = *It; break; }
		const TArray<UPrimitiveComponent*> Counter = PartsOf(Room, TEXT("CounterParts"));
		UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>();
		if (!Room || Counter.Num() == 0 || !I || !C) { TestLog(TEXT("no booking counter in the precinct"), false); Finish(); return; }
		TestLog(FString::Printf(TEXT("in holding %d, to book %d"), C->Arrested.Num(), C->UnbookedCount()), C->UnbookedCount() == 1);
		I->SendUseOn(Room, Counter[0], false);
		TestStage = 3; TestAt = Now;
		return;
	}
	case 3: // booked in one go; then back out through the motor pool once the scene's people are warm
	{
		if (Now - TestAt < 1.0) return;
		if (!bBookChecked)
		{
			bBookChecked = true;
			TestLog(FString::Printf(TEXT("one use of the counter books a prisoner start to finish (%d left)"), C->UnbookedCount()), C->UnbookedCount() == 0);
		}
		AHUMotorPoolDoor* Door = nullptr;
		for (TActorIterator<AHUMotorPoolDoor> It(W); It; ++It) { Door = *It; break; }
		if (!Door) { TestLog(TEXT("no motor pool door in the precinct"), false); Finish(); return; }
		// walk up to it (the scene's people warm up as you head that way), then go
		const FVector Stand = Door->GetActorLocation() + Door->GetActorForwardVector() * 320.f + FVector(0, 0, 95);
		if (FVector::Dist2D(P->GetActorLocation(), Stand) > 50.f) { P->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics); NextWarm = 0.0; TestAt = Now; return; }
		if (Now - TestAt < 0.5 || !Warmed()) return;
		Press(Door, Stand);
		TestStage = 4; TestAt = Now;
		return;
	}
	case 4: // back at the scene, on screen
		if (HereMap != TEXT("L_Scene") || LiftedAt == 0.0 || Now - LiftedAt < 1.5 || !P) return;
		Report();
		FScreenshotRequest::RequestScreenshot(TEXT("Flow_Scene"), false, false);
		TestStage = 5; TestAt = Now;
		return;
	case 5:
		if (Now - TestAt > 1.0) Finish();
		return;
	default:
		return;
	}
}
#endif
