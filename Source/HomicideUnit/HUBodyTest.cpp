// -HUBodyTest on L_Precinct: stands two of the cast up and photographs every way a body moves (HUPersonAnimation):
// idle, walking, talking, arms crossed, hands up, flinching, cuffed (from behind, and walking), crouching, a partner's
// aimed arm, and - in Interview 1 - sitting at the table, talking and flinching there, and sitting with hands in the
// lap. Screenshots land in Saved/Screenshots/<platform>/Body_*.png; then the game quits.
#include "CoreMinimal.h"
#include "HUPeople.h"
#include "HUPersonAnimation.h"
#include "HUSquadRoom.h"
#include "HUCharacter.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "UnrealClient.h"

namespace
{
	struct FBodyTest
	{
		TWeakObjectPtr<AHUSquadRoom> Room;
		TWeakObjectPtr<AActor> Walker, Sitter;
		float Clock = -1.f;
		int32 Done = 0;
	};
	FBodyTest GTest;

	UWorld* GameWorld()
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& C : GEngine->GetWorldContexts()) if (C.World() && C.World()->IsGameWorld()) return C.World();
		return nullptr;
	}

	bool Step(float Dt)
	{
		UWorld* W = GameWorld();
		if (!W) return true;
		APlayerController* PC = W->GetFirstPlayerController();
		APawn* P = PC ? PC->GetPawn() : nullptr;
		if (!GTest.Room.IsValid()) for (TActorIterator<AHUSquadRoom> It(W); It; ++It) if (It->bPrecinct) GTest.Room = *It;
		AHUSquadRoom* Room = GTest.Room.Get();
		if (!Room || !P) return true;
		const FTransform RT = Room->GetActorTransform();
		if (GTest.Clock < 0.f)
		{
			// one standing in the main corridor facing the street door (-X), one in Interview 1's suspect chair
			GTest.Walker = HUPeople::Stand(Room, Room->GetRootComponent(), HUPeople::CastFace(1), FVector(1100, 0, 0), 180.f);
			GTest.Sitter = HUPeople::Stand(Room, Room->GetRootComponent(), HUPeople::CastFace(5), FVector(1725, 451, 0), -90.f);
			if (UHUPersonAnimation* A = HUPeople::AnimOf(GTest.Sitter.Get())) { A->bSeated = true; A->bTable = true; A->SeatHeight = 47.f; }
			GTest.Clock = 0.f;
			UE_LOG(LogTemp, Display, TEXT("HUBodyTest: walker %s, sitter %s"), GTest.Walker.IsValid() ? TEXT("up") : TEXT("MISSING"), GTest.Sitter.IsValid() ? TEXT("up") : TEXT("MISSING"));
			return true;
		}
		const float Was = GTest.Clock;
		GTest.Clock += Dt;
		auto At = [&](float T) { return Was < T && GTest.Clock >= T; };
		AActor* Wk = GTest.Walker.Get();
		AActor* St = GTest.Sitter.Get();
		UHUPersonAnimation* WA = HUPeople::AnimOf(Wk);
		UHUPersonAnimation* SA = HUPeople::AnimOf(St);
		// stand the camera at From (precinct cm, eye height) looking at To
		auto Camera = [&](FVector From, FVector To)
		{
			const FVector F = RT.TransformPosition(From), T = RT.TransformPosition(To);
			P->SetActorLocation(F - FVector(0, 0, 64.f), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation((T - F).Rotation());
		};
		auto Shot = [&](const TCHAR* Name)
		{
			FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Body_%s"), Name), false, false);
			const AActor* VT = PC->GetViewTarget();
			UE_LOG(LogTemp, Display, TEXT("HUBodyTest: %s  (camera at %s looking %s, view target %s, seated %d)"), Name, *RT.InverseTransformPosition(P->GetActorLocation()).ToCompactString(),
				*PC->GetControlRotation().ToCompactString(), VT ? *VT->GetName() : TEXT("none"), Cast<AHUCharacter>(P) ? (int32)Cast<AHUCharacter>(P)->bSeated : -1);
			GTest.Done++;
		};
		const FVector Front(870, 30, 160), Face(1100, 0, 120), Side(1100, 120, 160), Behind(1330, 30, 160);
		const FVector Across(1725, 235, 150), Seated(1725, 451, 95), SeatSide(1880, 400, 140);

		// ---- standing, in the corridor ----
		if (At(8.f)) Camera(Front, Face);
		if (At(9.f)) Shot(TEXT("Idle"));
		if (At(9.5f)) { HUPeople::Motion(Wk, 150.f); Camera(Side, Face); }
		if (At(10.7f)) Shot(TEXT("WalkSide"));
		if (At(11.2f)) Camera(Front, Face);
		if (At(12.f)) Shot(TEXT("WalkFront"));
		if (At(12.5f)) { HUPeople::Motion(Wk, 0.f); if (WA) WA->TalkFor(8.f); }
		if (At(13.6f)) Shot(TEXT("Talk1"));
		if (At(14.3f)) Shot(TEXT("Talk2"));
		if (At(14.9f)) Shot(TEXT("Talk3"));
		if (At(15.5f) && WA) { WA->StopTalking(); WA->Arms = 1; }
		if (At(16.6f)) Shot(TEXT("ArmsCrossed"));
		if (At(17.f) && WA) WA->Arms = 2;
		if (At(18.f)) Shot(TEXT("HandsUp"));
		if (At(18.4f) && WA) { WA->Arms = 0; }
		if (At(19.4f) && WA) WA->Flinch(1.f);
		if (At(19.62f)) Shot(TEXT("Flinch"));
		if (At(21.f) && WA) WA->bCuffed = true;
		if (At(22.f)) Camera(Behind, Face);
		if (At(22.6f)) Shot(TEXT("CuffedBack"));
		if (At(23.f)) { HUPeople::Motion(Wk, 150.f); Camera(Side, Face); }
		if (At(24.f)) Shot(TEXT("CuffedWalk"));
		if (At(24.4f) && WA) { HUPeople::Motion(Wk, 0.f); WA->bCuffed = false; WA->bCrouched = true; }
		if (At(25.4f)) Shot(TEXT("Crouch"));
		if (At(25.8f) && WA) { WA->bCrouched = false; WA->RightArm = 95.f; Camera(Front, Face); }
		if (At(26.8f)) Shot(TEXT("PartnerAim"));
		// ---- seated, in Interview 1 ----
		if (At(27.5f)) { if (WA) WA->RightArm = 0.f; Camera(Across, Seated); }
		if (At(28.3f)) Shot(TEXT("SeatTable"));
		if (At(28.6f) && SA) SA->TalkFor(6.f);
		if (At(29.6f)) Shot(TEXT("SeatTalk1"));
		if (At(30.3f)) Shot(TEXT("SeatTalk2"));
		if (At(31.f) && SA) { SA->StopTalking(); }
		if (At(31.8f) && SA) SA->Flinch(1.f);
		if (At(32.02f)) Shot(TEXT("SeatFlinch"));
		if (At(33.5f)) Camera(SeatSide, Seated - FVector(0, 0, 20));
		if (At(34.2f)) Shot(TEXT("SeatSide"));
		if (At(34.5f) && SA) SA->bTable = false;
		if (At(35.5f)) Shot(TEXT("SeatLapSide"));
		if (At(36.5f))
		{
			UE_LOG(LogTemp, Display, TEXT("HUBodyTest: done, %d shots"), GTest.Done);
			FPlatformMisc::RequestExit(false);
			return false;
		}
		return true;
	}

	FDelayedAutoRegisterHelper GRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []()
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("HUBodyTest"))) FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Step), 0.f);
	});
}
