#include "HUPrecinctLife.h"
#include "HUSquadRoom.h"
#include "HUBuild.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"

// EA-07 supplies the only character factory and its sitting/walking animation.
// A checkout awaiting EA-07 still builds; it never substitutes primitive people.
#if __has_include("HUPeople.h") && __has_include("HUPersonAnimation.h")
#include "HUPeople.h"
#include "HUPersonAnimation.h"
#define HU_STAFF_CAST 1
#else
#define HU_STAFF_CAST 0
#endif

AHUPrecinctLifeLayout::AHUPrecinctLifeLayout()
{
	RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("LayoutRoot"));
	SetActorEnableCollision(false);
	auto Add=[&](int32 Face,FVector At,float Yaw,bool Seat,FVector To=FVector::ZeroVector)
	{
		FHUBackgroundWorker W; W.Face=Face; W.Feet=At; W.Yaw=Yaw; W.bSeated=Seat; W.WalkTo=To; Staff.Add(W);
	};
	Add(0,FVector(-2050,580,0),-90,true);
	Add(1,FVector(-1350,-620,0),-90,true);
	Add(2,FVector(-750,-1130,0),90,true);
	Add(-1,FVector(-2010,275,0),90,false);
	Add(2,FVector(-1320,-65,0),0,false,FVector(780,-65,0));
	Add(0,FVector(60,380,0),90,false,FVector(60,1370,0));
	PhonePositions={FVector(-2090,520,86),FVector(-1298,-738,86)};
}

void UHUPrecinctLife::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if(World.GetNetMode()==NM_DedicatedServer || FParse::Param(FCommandLine::Get(),TEXT("HUNoStaff"))) return;
	for(TActorIterator<AHUSquadRoom> It(&World);It;++It) if(It->bPrecinct) { Station=*It; bActive=true; break; }
	for(TActorIterator<AHUPrecinctLifeLayout> It(&World);It;++It) { Layout=*It; bActive &= It->bEnabled; break; }
}

UAudioComponent* UHUPrecinctLife::Audio(const TCHAR* Name, FVector At, float Volume, float Radius)
{
	const FString Path=FString::Printf(TEXT("/Game/Art/Audio/PrecinctLife/%s.%s"),Name,Name);
	USoundBase* Sound=LoadObject<USoundBase>(nullptr,*Path);
	if(!Sound) return nullptr;
	UAudioComponent* A=NewObject<UAudioComponent>(Decor);
	A->SetupAttachment(Decor->GetRootComponent()); A->SetRelativeLocation(At); A->SetSound(Sound);
	A->bAutoActivate=false; A->bAutoDestroy=false; A->bStopWhenOwnerDestroyed=true;
	A->bOverrideAttenuation=true;
	A->AttenuationOverrides.bAttenuate=true; A->AttenuationOverrides.bSpatialize=true;
	A->AttenuationOverrides.AttenuationShape=EAttenuationShape::Sphere;
	A->AttenuationOverrides.AttenuationShapeExtents=FVector(100);
	A->AttenuationOverrides.FalloffDistance=Radius;
	A->AttenuationOverrides.bEnableOcclusion=true;
	A->AttenuationOverrides.OcclusionTraceChannel=ECC_Visibility;
	A->AttenuationOverrides.OcclusionLowPassFilterFrequency=1200;
	A->AttenuationOverrides.OcclusionVolumeAttenuation=.25f;
	A->AttenuationOverrides.OcclusionInterpolationTime=.3f;
	A->SetVolumeMultiplier(Volume); A->RegisterComponent();
	return A;
}

void UHUPrecinctLife::AddPerson(int32 Face, FVector At, float Yaw, bool bSeated, FVector WalkTo)
{
#if HU_STAFF_CAST
	const bool bVisitor=Face<0;
	AActor* Model=HUPeople::Stand(Decor,Decor->GetRootComponent(),bVisitor?HUPeople::CastFace(6):HUPeople::OfficerFace(Face),At,Yaw);
	if(!Model) return;
	Model->Tags.Add(TEXT("HUBackgroundStaff")); Model->SetActorEnableCollision(false);
	if(!bVisitor) HUPeople::Uniform(Model);
	if(UHUPersonAnimation* Pose=HUPeople::AnimOf(Model))
	{
		Pose->bSeated=bSeated; Pose->SeatHeight=46; Pose->bTable=bSeated; Pose->TableHeight=76;
		Pose->HeadPitch=bSeated?9.f:0.f;
		if(bVisitor) Pose->Arms=1;
	}
	People.Add(Model);
	FWorker W; W.Model=Model; W.From=At; W.To=WalkTo; W.Yaw=Yaw;
	W.bSeated=bSeated; W.bWalk=!WalkTo.IsNearlyZero(); W.Phase=Workers.Num()*1.37f;
	Workers.Add(W);
#endif
}

void UHUPrecinctLife::Build()
{
	if(!Station.IsValid()) return;
	FActorSpawnParameters SP; SP.Name=TEXT("HU_PrecinctLife"); SP.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Frame=Layout.IsValid()?Layout->GetActorTransform():Station->GetActorTransform();
	Decor=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),Frame,SP);
	if(!Decor) return;
	USceneComponent* Root=NewObject<USceneComponent>(Decor,TEXT("AmbientRoot")); Decor->SetRootComponent(Root); Root->RegisterComponent();
	Decor->SetActorTransform(Frame); Decor->SetActorEnableCollision(false);
	// Staff occupy the existing chairs, not the squad's personal offices or booking/case-board stations.
	const AHUPrecinctLifeLayout* Plan=Layout.IsValid()?Layout.Get():GetDefault<AHUPrecinctLifeLayout>();
	for(const FHUBackgroundWorker& W : Plan->Staff) AddPerson(W.Face,W.Feet,W.Yaw,W.bSeated,W.WalkTo);
	// Ceiling fan over the bullpen: stem and motor stay still, three blades rotate together.
	using namespace HUBuild;
	const FVector Fan=Plan->FanPosition;
	const FLinearColor Metal(.12f,.14f,.13f);
	Block(Decor,Root,FanParts,EShape::Cylinder,Fan+FVector(0,0,41),FVector(22,22,7),Metal,FRotator::ZeroRotator,false);
	Block(Decor,Root,FanParts,EShape::Cylinder,Fan+FVector(0,0,24),FVector(3,3,31),Metal,FRotator::ZeroRotator,false);
	Block(Decor,Root,FanParts,EShape::Cylinder,Fan+FVector(0,0,7),FVector(19,19,13),Metal,FRotator::ZeroRotator,false);
	Rotor=NewObject<USceneComponent>(Decor,TEXT("FanRotor")); Rotor->SetupAttachment(Root); Rotor->SetRelativeLocation(Fan); Rotor->RegisterComponent();
	for(int32 i=0;i<3;i++)
	{
		const FRotator R(0,i*120.f,0);
		Block(Decor,Rotor,FanParts,EShape::Cube,R.RotateVector(FVector(43,0,0)),FVector(80,16,2),Metal,R,false,EHUSurface::PaintedMetal);
	}
	Block(Decor,Rotor,FanParts,EShape::Sphere,FVector(0,0,-2),FVector(16,16,8),Metal,FRotator::ZeroRotator,false);
	FanAudio=Audio(TEXT("AMB_OfficeFan"),Fan,.14f,650);
	if(FanAudio) FanAudio->FadeIn(2,.14f);
	for(int32 i=0;i<Plan->PhonePositions.Num();i++) Phones.Add(Audio(TEXT("SFX_OfficePhone"),Plan->PhonePositions[i],i==0?.24f:.19f,1000));
	UE_LOG(LogTemp,Display,TEXT("HUPrecinctLife: %d EA-07 people, 2 phones and a ceiling fan (background only)"),People.Num());
}

void UHUPrecinctLife::Tick(float DeltaTime)
{
	if(!GetWorld() || !GetWorld()->HasBegunPlay()) return;
	Clock+=DeltaTime;
	if(!Decor) { if(Clock>1) Build(); return; }
	FanAngle=FMath::Fmod(FanAngle+DeltaTime*110.f,360.f);
	if(Rotor) Rotor->SetRelativeRotation(FRotator(0,FanAngle,0));
#if HU_STAFF_CAST
	for(FWorker& W : Workers)
	{
		AActor* Model=W.Model.Get(); if(!Model) continue;
		if(UHUPersonAnimation* Pose=HUPeople::AnimOf(Model))
		{
			Pose->HeadYaw=FMath::Sin(Clock*.21f+W.Phase)*(W.bSeated?5.f:12.f);
			Pose->HeadPitch=(W.bSeated?9.f:0.f)+FMath::Sin(Clock*.16f+W.Phase)*2.f;
		}
		if(!W.bWalk) continue;
		const float Distance=FVector::Distance(W.From,W.To), Leg=Distance/80.f, Cycle=2*(Leg+5.f);
		const float T=FMath::Fmod(W.Progress+W.Phase,Cycle);
		const bool Back=T>=Leg+5;
		const float Travel=Back?T-Leg-5:T;
		const float A=FMath::Clamp(Travel/Leg,0.f,1.f);
		const FVector Pos=Back?FMath::Lerp(W.To,W.From,A):FMath::Lerp(W.From,W.To,A);
		const FVector Direction=(Back?W.From-W.To:W.To-W.From).GetSafeNormal();
		bool bYield=false;
		// A corridor walker waits for a detective rather than stepping through them. Never blocks the squad.
		const FVector WorldPos=Decor->GetActorTransform().TransformPosition(Pos);
		for(TActorIterator<APawn> It(GetWorld());It;++It)
		{
			const FVector D=It->GetActorLocation()-WorldPos;
			if(D.Size2D()<140 && FVector::DotProduct(D.GetSafeNormal2D(),Decor->GetActorTransform().TransformVectorNoScale(Direction))>-.1f) { bYield=true; break; }
		}
		if(!bYield) W.Progress+=DeltaTime;
		const bool bMoving=Travel<Leg && !bYield;
		Model->SetActorRelativeLocation(Pos);
		const FRotator Want(0,Direction.Rotation().Yaw-90,0);
		Model->SetActorRelativeRotation(FMath::RInterpTo(Model->GetActorRotation()-Decor->GetActorRotation(),Want,DeltaTime,3.f));
		HUPeople::Motion(Model,bMoving?80.f:0.f);
	}
#endif
	if(Clock>=NextRing)
	{
		const int32 Index=Rings%2;
		if(Phones.IsValidIndex(Index) && Phones[Index]) Phones[Index]->Play();
		++Rings;
		static const float Gaps[]={39,53,44,61}; NextRing=Clock+Gaps[(Rings-1)%4];
	}
	if(!bChecked && Clock>23 && FParse::Param(FCommandLine::Get(),TEXT("HUStaffTest"))) { bChecked=true; Check(); }
}

void UHUPrecinctLife::Check()
{
	bool Pass=People.Num()==6 && Rotor && FanAudio && Phones.Num()==2 && Phones[0] && Phones[1] && Rings>0;
	int32 Moving=0;
	for(const FWorker& W : Workers)
	{
		AActor* Model=W.Model.Get();
		Pass &= Model && !Model->GetActorEnableCollision() && !Model->GetClass()->ImplementsInterface(UHUInteractable::StaticClass());
		if(W.bWalk && W.Progress>3) ++Moving;
#if HU_STAFF_CAST
		Pass &= Model && HUPeople::BodyOf(Model) && HUPeople::AnimOf(Model);
#endif
	}
	Pass &= Moving==2 && FanAngle>0;
	UE_LOG(LogTemp,Display,TEXT("HUStaffTest: %s - %d people, %d walkers, fan angle %.1f, %d automatic ring event(s); no collision or interactions"),Pass?TEXT("PASSED"):TEXT("FAILED"),People.Num(),Moving,FanAngle,Rings);
}

void UHUPrecinctLife::Deinitialize()
{
	if(FanAudio) FanAudio->Stop();
	for(UAudioComponent* A : Phones) if(A) A->Stop();
	for(AActor* A : People) if(IsValid(A)) A->Destroy();
	People.Reset(); Workers.Reset(); Phones.Reset(); FanParts.Reset();
	if(IsValid(Decor)) Decor->Destroy();
	Decor=nullptr; Rotor=nullptr; FanAudio=nullptr; bActive=false;
	Super::Deinitialize();
}
