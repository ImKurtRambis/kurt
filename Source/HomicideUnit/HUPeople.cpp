#include "HUPeople.h"
#include "HUBuild.h"
#include "HULook.h"
#include "HUCaseTypes.h"
#include "HUPersonAnimation.h"
#include "Misc/PackageName.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UnrealClient.h"

using namespace HUBuild;

namespace
{
	FString FacePath(const FString& Who) { return FString::Printf(TEXT("/Game/MetaHumans/MH_%s/BP_MH_%s.BP_MH_%s_C"), *Who, *Who, *Who); }
	FString AvailableFace(const FString& Preferred, const FString& Existing)
	{
		return FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/MetaHumans/MH_%s/BP_MH_%s"), *Preferred, *Preferred)) ? Preferred : Existing;
	}
}

FString HUPeople::CastFace(int32 Index)
{
	static const TCHAR* Faces[] = { TEXT("Walter"), TEXT("Bruce"), TEXT("Victor"), TEXT("Lorenzo"), TEXT("Vivian"), TEXT("Grace"), TEXT("Celeste"), TEXT("Etta") };
	return Faces[(Index % CastCount + CastCount) % CastCount];
}

FString HUPeople::SuspectFace(const FHUCase& Case, const FString& Id)
{
	const FHUSuspect* Person = Case.Suspects.FindByPredicate([&](const FHUSuspect& S) { return S.Id == Id; });
	if (!Person) return CastFace(0);
	int32 Slot = 0;
	for (const FHUSuspect& S : Case.Suspects)
	{
		if (S.Id == Id) break;
		if (IsWoman(S.Name) == IsWoman(Person->Name)) ++Slot;
	}
	return CastFace((IsWoman(Person->Name) ? 4 : 0) + ((Slot + Case.Number) % 4 + 4) % 4);
}

FString HUPeople::OfficerFace(int32 Variant)
{
	static const TCHAR* Preferred[] = { TEXT("Mateo"), TEXT("Zuri"), TEXT("Kelvin") };
	static const int32 Existing[] = { 1, 5, 2 };
	const int32 Index = (Variant % 3 + 3) % 3;
	return AvailableFace(Preferred[Index], CastFace(Existing[Index]));
}
FString HUPeople::CaptainFace() { return AvailableFace(TEXT("Orlando"), CastFace(0)); }
FString HUPeople::VictimFace(const FString& Name, int32 Number)
{
	static const TCHAR* Men[] = { TEXT("Dominic"), TEXT("Jorge") };
	static const TCHAR* Women[] = { TEXT("Jelena"), TEXT("Sunita") };
	const int32 Index = (Number % 2 + 2) % 2;
	return AvailableFace(IsWoman(Name) ? Women[Index] : Men[Index], CastFace((IsWoman(Name) ? 4 : 2) + Index));
}

bool HUPeople::ValidateCast()
{
	bool bOK = true;
	for (int32 Index = 0; Index < CastCount; ++Index)
		if (!LoadClass<AActor>(nullptr, *FacePath(CastFace(Index)))) { UE_LOG(LogTemp, Error, TEXT("HomicideUnit: required cast model missing: %s"), *CastFace(Index)); bOK = false; }
	UE_LOG(LogTemp, Display, TEXT("HomicideUnit: cast validation %s (%d models)"), bOK ? TEXT("PASSED") : TEXT("FAILED"), CastCount);
	return bOK;
}

void HUPeople::DestroyOwned(AActor* Owner)
{
	TArray<AActor*> Attached; Owner->GetAttachedActors(Attached);
	for (AActor* Person : Attached) if (Person && Person->GetOwner() == Owner && Person->ActorHasTag(TEXT("HUPerson"))) Person->Destroy();
}

bool HUPeople::IsWoman(const FString& FullName)
{
	static const TCHAR* Women[] = { TEXT("Doris"), TEXT("Helen"), TEXT("June"), TEXT("Irene"), TEXT("Marge"), TEXT("Vera"), TEXT("Nora"), TEXT("Pearl"), TEXT("Edie"), TEXT("Lorraine"), TEXT("Lucy"), TEXT("Rosie"), TEXT("Maggie") };
	FString First; if (!FullName.Split(TEXT(" "), &First, nullptr)) First = FullName;
	for (const TCHAR* W : Women) if (First == W) return true;
	return false;
}

USkeletalMeshComponent* HUPeople::BodyOf(AActor* Look)
{
	if (!Look) return nullptr;
	TArray<USkeletalMeshComponent*> Meshes; Look->GetComponents(Meshes);
	for (USkeletalMeshComponent* M : Meshes) if (M && M->GetFName() == TEXT("Body")) return M;
	return nullptr;
}



static void StartShots(UWorld* World);

static AActor* SpawnFace(AActor* Owner, USceneComponent* Parent, const FString& Who)
{
	UWorld* W = Owner ? Owner->GetWorld() : nullptr;
	if (!W || !Parent || Who.IsEmpty()) return nullptr;
	UClass* Cls = LoadClass<AActor>(nullptr, *FacePath(Who));
	if (!Cls) { UE_LOG(LogTemp, Error, TEXT("HomicideUnit: required MetaHuman %s is missing for %s"), *Who, *Owner->GetName()); return nullptr; }
	FActorSpawnParameters P; P.Owner = Owner; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Look = W->SpawnActor<AActor>(Cls, Parent->GetComponentTransform(), P);
	if (!Look) return nullptr;
	Look->SetActorEnableCollision(false);
	Look->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Look->Tags.Add(TEXT("HUPerson"));
	Look->Tags.Add(FName(*(TEXT("HUFace:") + Who)));
	StartShots(W);
	return Look;
}

AActor* HUPeople::Stand(AActor* Owner, USceneComponent* Parent, const FString& Who, FVector RelLoc, float RelYaw)
{
	AActor* Look = SpawnFace(Owner, Parent, Who);
	if (!Look) return nullptr;
	Look->SetActorRelativeLocation(RelLoc);
	Look->SetActorRelativeRotation(FRotator(0, RelYaw - 90.f, 0)); // a MetaHuman faces its own +Y
	if (USkeletalMeshComponent* Body = BodyOf(Look))
	{
		Body->SetAnimInstanceClass(UHUPersonAnimation::StaticClass());
		Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Motion(Look, 0.f);
	}

	UE_LOG(LogTemp, Log, TEXT("HomicideUnit: %s is played by MetaHuman %s"), *Owner->GetName(), *Who);
	return Look;
}

AActor* HUPeople::Lie(AActor* Owner, USceneComponent* Parent, const FString& Who, FVector RelFeet, float Scale)
{
	AActor* Look = SpawnFace(Owner, Parent, Who);
	USkeletalMeshComponent* B = BodyOf(Look);
	if (!Look || !B) return Look;
	// head along +X, face up: its +Z (feet to head) along Parent's +X, its +Y (facing) up
	Look->SetActorRelativeLocation(RelFeet);
	Look->SetActorRelativeScale3D(FVector(Scale));
	Look->SetActorRelativeRotation(FRotationMatrix::MakeFromXY(FVector(0, 1, 0), FVector(0, 0, 1)).Rotator());
	Look->Tags.Add(TEXT("HULying"));
	// it falls limp onto the floor: only the body collides, and only with the world - not with the player, the
	// interaction traces or the owning actor's examination zones.
	TInlineComponentArray<UPrimitiveComponent*> Zones(Owner); for (UPrimitiveComponent* Zone : Zones) Zone->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	Look->SetActorEnableCollision(true);
	TArray<UPrimitiveComponent*> Prims; Look->GetComponents(Prims);
	for (UPrimitiveComponent* C : Prims) if (C != B) C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	B->SetCollisionObjectType(ECC_PhysicsBody);
	B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	B->SetCollisionResponseToAllChannels(ECR_Ignore);
	B->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	B->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	B->SetAllBodiesSimulatePhysics(true);
	B->SetSimulatePhysics(true);
	B->WakeAllRigidBodies();
	// once it's settled it stays where it fell
	TWeakObjectPtr<USkeletalMeshComponent> WB(B);
	FTimerHandle T;
	Owner->GetWorldTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([WB]() { if (WB.IsValid()) WB->PutAllRigidBodiesToSleep(); }), 4.f, false);
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit: the body at %s is MetaHuman %s"), *Owner->GetName(), *Who);
	return Look;
}

AActor* HUPeople::Detective(AActor* Owner, USceneComponent* Parent, AActor* Previous, const FHULook& Appearance, bool bOwnerNoSee, float Scale)
{
	const FString Face = CastFace(Appearance.Model);
	if (Previous && !Previous->ActorHasTag(FName(*(TEXT("HUFace:") + Face)))) { Previous->Destroy(); Previous = nullptr; }
	AActor* Person = Previous ? Previous : Stand(Owner, Parent, Face, FVector::ZeroVector, 0.f);
	if (!Person) return nullptr;
	static const float Widths[] = { 0.85f, 1.f, 1.15f, 1.3f };
	const float Height = HULook::HeightCm(Appearance) / 180.f * Scale;
	const float Width = Widths[FMath::Min<int32>(Appearance.Build, 3)];
	Person->SetActorRelativeScale3D(FVector(Height * Width, Height * Width, Height));
	TInlineComponentArray<UPrimitiveComponent*> Parts(Person);
	for (UPrimitiveComponent* Part : Parts) Part->SetOwnerNoSee(bOwnerNoSee);
	return Person;
}

void HUPeople::Motion(AActor* Person, float Speed)
{
	USkeletalMeshComponent* Body = BodyOf(Person);
	UHUPersonAnimation* Animation = Body ? Cast<UHUPersonAnimation>(Body->GetAnimInstance()) : nullptr;
	if (!Animation) return;
	const bool bRun = Speed > 420.f, bWalk = !bRun && Speed > 5.f;
	const TCHAR* Clip = bRun ? TEXT("AS_MH_Neutral_Run_Loop_F") : bWalk ? TEXT("AS_MH_Neutral_Walk_Loop_F") : TEXT("AS_MH_Neutral_Stand_Idle_Loop");
	// the clip at the pace of their feet (the walk loop covers ~150 cm/s, the run ~400)
	Animation->SetPlayRate(bRun ? FMath::Clamp(Speed / 400.f, 0.8f, 1.4f) : bWalk ? FMath::Clamp(Speed / 150.f, 0.55f, 1.5f) : 1.f);
	if (Animation->GetCurrentAsset() && Animation->GetCurrentAsset()->GetFName() == FName(Clip)) return;
	UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/MetaHumanCharacter/Optional/Animation/UEFNAnimPreset/Locomotion/%s.%s"), Clip, Clip));
	if (Sequence && Animation->GetCurrentAsset() != Sequence) Animation->SetAnimationAsset(Sequence, true);
}

UHUPersonAnimation* HUPeople::AnimOf(AActor* Person)
{
	USkeletalMeshComponent* Body = BodyOf(Person);
	return Body ? Cast<UHUPersonAnimation>(Body->GetAnimInstance()) : nullptr;
}
void HUPeople::Talk(AActor* Person, float Seconds) { if (UHUPersonAnimation* A = AnimOf(Person)) A->TalkFor(Seconds); }
void HUPeople::Flinch(AActor* Person, float Strength) { if (UHUPersonAnimation* A = AnimOf(Person)) A->Flinch(Strength); }
float HUPeople::SpeakingTime(const FString& Line)
{
	TArray<FString> Words;
	Line.ParseIntoArrayWS(Words);
	return FMath::Clamp(0.6f + Words.Num() * 0.36f, 0.8f, 14.f); // about 165 words a minute
}
void HUPeople::Uniform(AActor* Look)
{
	USkeletalMeshComponent* B = BodyOf(Look);
	if (!B) return;
	// placed once the idle has posed the head (a frame or two in), then carried by the head and chest bones
	TWeakObjectPtr<AActor> WL(Look);
	TWeakObjectPtr<USkeletalMeshComponent> WB(B);
	FTimerHandle T;
	Look->GetWorldTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([WL, WB]()
	{
		AActor* L = WL.Get(); USkeletalMeshComponent* Body = WB.Get();
		if (!L || !Body || !Body->DoesSocketExist(TEXT("head"))) return;
		TArray<UStaticMeshComponent*> Parts;
		const FVector Fwd = L->GetActorRightVector(); // the MetaHuman's facing
		const FRotator Flat(0, Fwd.Rotation().Yaw, 0);
		const FLinearColor Navy(0.02f, 0.03f, 0.07f), Black(0.01f, 0.01f, 0.01f), Brass(0.85f, 0.68f, 0.25f);
		const FVector Head = Body->GetSocketLocation(TEXT("head"));
		auto Put = [&](EShape S, FVector At, FVector Size, FLinearColor Col, FName Bone)
		{
			UStaticMeshComponent* C = Block(L, Body, Parts, S, FVector::ZeroVector, Size, Col, FRotator::ZeroRotator, false);
			if (!C) return;
			C->SetWorldLocationAndRotation(At, Flat);
			C->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, Bone);
			C->SetCastShadow(true);
		};
		// the cap: crown, band and visor, sitting on the top of the head
		Put(EShape::Cylinder, Head + FVector(0, 0, 17) - Fwd * 1.f, FVector(24, 24, 7), Navy, TEXT("head"));
		Put(EShape::Cylinder, Head + FVector(0, 0, 13) - Fwd * 1.f, FVector(22, 22, 4), Black, TEXT("head"));
		Put(EShape::Cube, Head + FVector(0, 0, 11.5f) + Fwd * 11.f, FVector(9, 18, 1.2f), Black, TEXT("head"));
		Put(EShape::Cube, Head + FVector(0, 0, 15) + Fwd * 11.4f, FVector(0.6f, 3, 3.5f), Brass, TEXT("head")); // cap badge
		// the shield on the left breast
		if (Body->DoesSocketExist(TEXT("spine_05")))
		{
			const FVector Chest = Body->GetSocketLocation(TEXT("spine_05"));
			const FVector Left = -L->GetActorForwardVector(); // its left, seen from the front
			Put(EShape::Cube, Chest + Fwd * 13.f + Left * 9.f + FVector(0, 0, 4), FVector(0.6f, 4.5f, 5.5f), Brass, TEXT("spine_05"));
		}
	}), 0.25f, false);
}

// ---------- -HUPeopleShots: a photo of everyone, for checking how they look ----------
static void StartShots(UWorld* World)
{
	static bool bStarted = false;
	if (bStarted || !World || !FParse::Param(FCommandLine::Get(), TEXT("HUPeopleShots"))) return;
	bStarted = true;
	TWeakObjectPtr<UWorld> WW(World);
	// (a delayed ticker's DeltaTime is the frame's, not the time since it last fired: time it with the real clock)
	const double Since = FPlatformTime::Seconds();
	TSharedRef<int32> Step = MakeShared<int32>(0);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WW, Since, Step](float)
	{
		UWorld* W = WW.Get();
		if (!W) return false;
		if (FPlatformTime::Seconds() - Since < 10.0) return true; // the level, the MetaHumans' streaming, the body settling
		APlayerController* PC = W->GetFirstPlayerController();
		APawn* P = PC ? PC->GetPawn() : nullptr;
		if (!P) return true;
		TArray<AActor*> All;
		for (TActorIterator<AActor> It(W); It; ++It) if (It->GetClass()->GetName().StartsWith(TEXT("BP_MH_"))) All.Add(*It);
		// the body first, then the officers and the Captain, then everyone else
		auto Rank = [](const AActor& A) { return A.ActorHasTag(TEXT("HULying")) ? 0 : A.GetOwner() && (A.GetOwner()->GetClass()->GetName().Contains(TEXT("Officer")) || A.GetOwner()->GetClass()->GetName().Contains(TEXT("Briefing"))) ? 1 : 2; };
		All.Sort([&Rank](const AActor& A, const AActor& B) { return Rank(A) < Rank(B); });
		const int32 N = (*Step)++;
		if (N / 2 >= FMath::Min(All.Num(), 8)) { FPlatformMisc::RequestExit(false); return false; }
		AActor* A = All[N / 2];
		USkeletalMeshComponent* Body = HUPeople::BodyOf(A);
		const FVector Center = Body && Body->DoesSocketExist(TEXT("spine_03")) ? Body->GetSocketLocation(TEXT("spine_03")) : A->GetActorLocation();
		if (N % 2 == 0)
		{
			FVector At, Target;
			if (A->ActorHasTag(TEXT("HULying")))
			{
				// standing over the body, a little to one side
				const FVector Side = A->GetActorForwardVector().GetSafeNormal2D();
				At = Center + Side * 110.f + FVector(0, 0, 40.f);
				Target = Center;
			}
			else
			{
				// head and shoulders from a metre and a half
				const FVector Head = Body && Body->DoesSocketExist(TEXT("head")) ? Body->GetSocketLocation(TEXT("head")) : A->GetActorLocation() + FVector(0, 0, 160);
				At = Head + A->GetActorRightVector().GetSafeNormal2D() * 140.f - FVector(0, 0, 70.f);
				Target = Head - FVector(0, 0, 15.f);
			}
			P->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation((Target - (At + FVector(0, 0, 64.f))).Rotation());
		}
		else
		{
			const FString Name = FString::Printf(TEXT("People_%d_%s"), N / 2, *A->GetClass()->GetName().Replace(TEXT("BP_MH_"), TEXT("")).Replace(TEXT("_C"), TEXT("")));
			FScreenshotRequest::RequestScreenshot(Name, false, false);
			UE_LOG(LogTemp, Log, TEXT("HUPeopleShots: %s"), *Name);
		}
		return true;
	}), 1.5f);
}

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
static FAutoConsoleCommand GHUValidateCast(TEXT("HU.ValidateCast"), TEXT("Validate the required real character models"), FConsoleCommandDelegate::CreateLambda([]() { HUPeople::ValidateCast(); }));
#endif
