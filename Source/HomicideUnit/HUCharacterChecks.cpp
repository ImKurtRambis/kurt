#include "HUCharacter.h"

#if !UE_BUILD_SHIPPING
#include "HUPeople.h"
#include "HUPersonAnimation.h"
#include "HUCaseSubsystem.h"
#include "HUCaseGen.h"
#include "HUInteractComponent.h"
#include "HUSuspectActor.h"
#include "HUOfficer.h"
#include "HUVictim.h"
#include "HUSquadRoom.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"

namespace
{
	bool CheckCharacters(UWorld* World)
	{
		bool bOK = HUPeople::ValidateCast();
		auto Check = [&](bool bPass, const FString& What)
		{
			if (!bPass) { UE_LOG(LogTemp, Error, TEXT("HUCharacterTest: %s"), *What); bOK = false; }
		};
		int32 Models = 0, Suspects = 0, Officers = 0, Victims = 0, Holding = 0;
		AHUCharacter* Player = nullptr;
		for (TActorIterator<AHUCharacter> It(World); It; ++It)
		{
			if (It->IsLocallyControlled()) Player = *It;
			Check(It->VisualBody() != nullptr, TEXT("Detective has no real model"));
		}
		for (TActorIterator<AActor> It(World); It; ++It)
			if (It->ActorHasTag(TEXT("HUPerson")))
			{
				++Models;
				USkeletalMeshComponent* Body = HUPeople::BodyOf(*It);
				Check(Body && Body->GetSkeletalMeshAsset(), It->GetName() + TEXT(" has no body mesh"));
				Check(Body && Body->DoesSocketExist(TEXT("head")) && Body->DoesSocketExist(TEXT("hand_r")), It->GetName() + TEXT(" lacks interaction/equipment bones"));
				if (!It->ActorHasTag(TEXT("HULying"))) Check(Body && Cast<UHUPersonAnimation>(Body->GetAnimInstance()), It->GetName() + TEXT(" is not using the shared character animation"));
				if (Cast<AHUSquadRoom>(It->GetOwner())) ++Holding;
			}
		auto CheckCollision = [&](AActor* Actor)
		{
			TInlineComponentArray<UStaticMeshComponent*> OldParts(Actor);
			Check(OldParts.Num() == 0, Actor->GetName() + TEXT(" still owns a primitive figure"));
			UCapsuleComponent* Capsule = Actor->FindComponentByClass<UCapsuleComponent>();
			Check(Capsule && Capsule->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block, Actor->GetName() + TEXT(" has no conversation target"));
			if (!Capsule) return;
			const FTransform Original = Actor->GetActorTransform();
			Actor->SetActorLocation(FVector(50000, 0, 1000), false, nullptr, ETeleportType::TeleportPhysics);
			const FVector Center = Capsule->GetComponentLocation();
			FHitResult Hit;
			World->LineTraceSingleByChannel(Hit, Center + FVector(150, 0, 0), Center - FVector(150, 0, 0), ECC_Visibility);
			Check(Hit.GetActor() == Actor, Actor->GetName() + TEXT(" cannot be targeted after removing its blocks"));
			Actor->SetActorTransform(Original, false, nullptr, ETeleportType::TeleportPhysics);
		};
		for (TActorIterator<AHUOfficer> It(World); It; ++It) { ++Officers; CheckCollision(*It); }
		for (TActorIterator<AHUSuspectActor> It(World); It; ++It)
		{
			if (It->IsHidden()) continue;
			++Suspects; CheckCollision(*It);
			Check(It->HasLook(), TEXT("Suspect has no real model"));
		}
		UHUInteractComponent* Interaction = Player ? Player->FindComponentByClass<UHUInteractComponent>() : nullptr;
		UHUCaseSubsystem* Case = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
		if (Player && Interaction && Case)
		{
			const uint8 HeldBefore = Interaction->RepHeld, PoseBefore = Interaction->RepPose;
			const float PitchBefore = Interaction->WorldPitch;
			Interaction->RepHeld = 1; Interaction->RepPose = UHUInteractComponent::PoseIdle; Interaction->WorldPitch = 0.f;
			Interaction->TickWorldGear(Player, 0.5f);
			USkeletalMeshComponent* PlayerBody = Player->VisualBody();
			Check(Interaction->WorldArm && Interaction->WorldArm->GetAttachParent() == PlayerBody && Interaction->WorldArm->GetAttachSocketName() == TEXT("hand_r") && Interaction->WorldArmParts.Num() > 0, TEXT("Partner equipment is not attached to the real hand"));
			UHUPersonAnimation* PlayerAnimation = PlayerBody ? Cast<UHUPersonAnimation>(PlayerBody->GetAnimInstance()) : nullptr;
			Check(PlayerAnimation && PlayerAnimation->RightArm > 20.f, TEXT("Partner equipment did not pose the real arm"));
			Interaction->RepHeld = HeldBefore; Interaction->RepPose = PoseBefore; Interaction->WorldPitch = PitchBefore;
			Interaction->TickWorldGear(Player, 0.f);
			for (TActorIterator<AHUSuspectActor> It(World); It; ++It)
			{
				if (It->IsHidden() || It->bCuffed) continue;
				const int32 Held = Interaction->Held;
				const TArray<FString> Phones = Case->SeizedPhones;
				Interaction->Held = (int32)EHUTool::Handcuffs + 1;
				It->Interact_Implementation(Player, false);
				UCapsuleComponent* Capsule = It->FindComponentByClass<UCapsuleComponent>();
				Check(It->bCuffed && It->IsEscortedBy(Player), TEXT("Cuff/escort stopped working"));
				Check(Capsule && Capsule->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore, TEXT("Cuffed suspect blocks the escort"));
				It->Interact_Implementation(Player, true);
				Check(!It->bCuffed && Capsule && Capsule->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block, TEXT("Uncuff did not restore collision"));
				Interaction->Held = Held;
				Case->SeizedPhones = Phones;
				break;
			}
			for (TActorIterator<AHUVictim> It(World); It; ++It)
			{
				if (It->Kind != EHUVictimKind::Body) continue;
				++Victims;
				UPrimitiveComponent* OriginalFocus = Interaction->FocusPart;
				TInlineComponentArray<USphereComponent*> Zones(*It);
				TSet<FString> Parts;
				for (USphereComponent* Zone : Zones)
				{
					Check(Cast<USkeletalMeshComponent>(Zone->GetAttachParent()) != nullptr && Zone->GetAttachSocketName() != NAME_None, TEXT("Examination target does not follow the real body"));
					Interaction->FocusPart = Zone;
					const FString Prompt = It->GetPrompt_Implementation(Player).ToString();
					for (const TCHAR* Part : { TEXT("head"), TEXT("neck"), TEXT("hands and arms"), TEXT("torso"), TEXT("legs and shoes") }) if (Prompt.Contains(Part)) Parts.Add(Part);
				}
				Check(Parts.Num() == 5, TEXT("Victim no longer exposes all five examination areas"));
				Interaction->FocusPart = OriginalFocus;
			}
		}
		if (World->GetMapName().Contains(TEXT("L_Precinct")) && FParse::Param(FCommandLine::Get(), TEXT("HUCharacterTest"))) Check(Holding > 0, TEXT("Holding did not create a real prisoner"));
		Check(Models > 0, TEXT("Map contains no character models"));
		UE_LOG(LogTemp, Display, TEXT("HUCharacterTest: %s map=%s models=%d suspects=%d officers=%d bodies=%d holding=%d"), bOK ? TEXT("PASSED") : TEXT("FAILED"), *World->GetMapName(), Models, Suspects, Officers, Victims, Holding);
		return bOK;
	}
}

static FAutoConsoleCommandWithWorld GHUCharacterChecks(TEXT("HU.TestCharacters"), TEXT("Check real models, interaction traces, cuffs and body examination"), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
	if (!World) return;
	if (World->GetMapName().Contains(TEXT("L_Precinct")) && FParse::Param(FCommandLine::Get(), TEXT("HUCharacterTest")))
		if (UHUCaseSubsystem* Case = World->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>())
		{
			if (Case->Current.Suspects.IsEmpty()) Case->Current = HUCaseGen::MakeCases(1, 1, 274)[0];
			Case->bCaseActive = true; Case->bAwaitingCaptain = true;
			Case->Arrested.AddUnique(Case->Current.Suspects[0].Id);
		}
	TWeakObjectPtr<UWorld> WeakWorld(World);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld](float)
	{
		if (UWorld* W = WeakWorld.Get()) CheckCharacters(W);
		if (FParse::Param(FCommandLine::Get(), TEXT("HUCharacterTest"))) FPlatformMisc::RequestExit(false);
		return false;
	}), 8.f);
}));
#endif
