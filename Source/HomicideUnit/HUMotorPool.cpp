#include "HUMotorPool.h"
#include "HUCruiser.h"
#include "HUSupplyLocker.h"
#include "HUInteractComponent.h"
#include "HUProps.h"
#include "HUBuild.h"
#include "HUSound.h"
#include "Components/ChildActorComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"

static const float HalfX = 900.f, HalfY = 1000.f, PoolH = 600.f;
static const float BayY[3] = { -600.f, 0.f, 600.f };

AHUMotorPool::AHUMotorPool()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); RootComponent = Root;
	for (int32 i = 0; i < 3; i++)
	{
		UChildActorComponent* C = CreateDefaultSubobject<UChildActorComponent>(*FString::Printf(TEXT("Cruiser%d"), i));
		C->SetupAttachment(Root); C->SetChildActorClass(AHUCruiser::StaticClass());
		C->SetRelativeLocation(FVector(-150, BayY[i], 0)); C->SetRelativeRotation(FRotator(0, 180, 0));
		Cars.Add(C);
	}
	Locker = CreateDefaultSubobject<UChildActorComponent>(TEXT("SupplyLocker"));
	Locker->SetupAttachment(Root); Locker->SetChildActorClass(AHUSupplyLocker::StaticClass());
	Locker->SetRelativeLocation(FVector(HalfX - 35, -420, 0)); Locker->SetRelativeRotation(FRotator(0, 180, 0));
	BaySign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BaySign"));
	BaySign->SetupAttachment(Root); BaySign->SetRelativeLocation(FVector(880, 0, 278));
	BaySign->SetRelativeRotation(FRotator(0, 180, 0)); BaySign->SetHorizontalAlignment(EHTA_Center);
	BaySign->SetWorldSize(24); BaySign->SetTextRenderColor(FColor(221, 205, 160));
	BaySign->SetText(FText::FromString(TEXT("HOMICIDE  /  MOTOR POOL")));
}

void AHUMotorPool::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Retire names before destroying: editor reconstruction and BeginPlay may rebuild twice before GC.
	for(UStaticMeshComponent* C : Blocks) if(C) C->Rename(*MakeUniqueObjectName(this,C->GetClass(),TEXT("RetiredBayPart")).ToString(),this,REN_DontCreateRedirectors|REN_NonTransactional);
	HUBuild::Clear(Blocks); Slats.Reset(); Controls.Reset();
	for (UTextRenderComponent* T : Labels) if (T) T->DestroyComponent(); Labels.Reset();
	const FLinearColor Concrete = FLinearColor::FromSRGBColor(FColor(89, 96, 93));
	const FLinearColor Wall = FLinearColor::FromSRGBColor(FColor(155, 158, 148));
	const FLinearColor Steel = FLinearColor::FromSRGBColor(FColor(69, 83, 91));
	const FLinearColor Yellow = FLinearColor::FromSRGBColor(FColor(180, 152, 89));
	const FLinearColor Dark(.022f, .025f, .024f), Red(.24f, .035f, .025f);
	auto B = [&](FVector P, FVector S, FLinearColor C, EHUSurface Mat = EHUSurface::PaintedMetal, bool Collide = true, FRotator R = FRotator::ZeroRotator)
	{ return HUBuild::Block(this, Root, Blocks, HUBuild::EShape::Cube, P, S, C, R, Collide, Mat); };
	auto Label = [&](FVector P, float Yaw, const FString& Words, float Size = 18.f)
	{
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this); T->SetupAttachment(Root);
		T->SetRelativeLocation(P); T->SetRelativeRotation(FRotator(0, Yaw, 0));
		T->SetHorizontalAlignment(EHTA_Center); T->SetWorldSize(Size); T->SetTextRenderColor(FColor(225, 216, 187));
		T->SetText(FText::FromString(Words)); T->RegisterComponent(); Labels.Add(T);
	};
	B(FVector(0,0,-6), FVector(1800,2000,12), Concrete, EHUSurface::Concrete);
	B(FVector(0,0,605), FVector(1800,2000,10), Concrete, EHUSurface::Concrete);
	HUBuild::WallWithDoor(this, Root, Blocks, true, FVector(905,0,300), 2000,600,10,0,180,250,Wall,EHUSurface::Concrete);
	for (int32 S : {-1,1})
	{
		B(FVector(0,S*1005,300),FVector(1800,10,600),Wall,EHUSurface::Concrete);
		B(FVector(0,S*998,90),FVector(1800,3,180),Steel,EHUSurface::Concrete);
		B(FVector(0,S*995,185),FVector(1800,4,8),Yellow,EHUSurface::None,false);
	}
	// Actual openings: piers between bays and a lintel, never a solid wall behind a shutter.
	for (float Y : {-950.f,-300.f,300.f,950.f})
		B(FVector(-905,Y,300),FVector(20,FMath::Abs(Y)>900?100:180,600),Wall,EHUSurface::Concrete);
	B(FVector(-905,0,485),FVector(20,2000,230),Wall,EHUSurface::Concrete);
	for (int32 i=0; i<3; i++)
	{
		const float Y = BayY[i];
		for (int32 S : {-1,1})
		{
			B(FVector(-875,Y+S*216,185),FVector(14,12,370),Steel);
			B(FVector(-650,Y+S*218,395),FVector(460,8,8),Steel);
			B(FVector(-844,Y+S*235,48),FVector(16,16,96),Yellow);
			for (int32 k=0;k<4;k++) B(FVector(-833,Y+S*235,14+k*23),FVector(2,17,9),Dark,EHUSurface::None,false);
		}
		for (int32 k=0;k<12;k++)
		{
			UStaticMeshComponent* C = B(FVector(-898,Y,15+k*30),FVector(7,420,29),Steel);
			C->Rename(*FString::Printf(TEXT("BaySection_%d_%d"),i,k));
			C->SetMobility(EComponentMobility::Movable); Slats.Add(C); Controls.Add(C,i);
		}
		UStaticMeshComponent* Control = B(FVector(-870,Y+255,122),FVector(13,28,46),Dark);
		Control->Rename(*FString::Printf(TEXT("BayControl_%d"),i)); Controls.Add(Control,i);
		for (int32 k=0;k<2;k++)
		{
			UStaticMeshComponent* Button = B(FVector(-861,Y+255,132-k*18),FVector(5,12,10),k?Red:FLinearColor(.04f,.3f,.09f));
			Button->Rename(*FString::Printf(TEXT("BayButton_%d_%d"),i,k)); Controls.Add(Button,i);
		}
		Label(FVector(-860,Y,416),0,FString::Printf(TEXT("0%d  /  %s"),i+1,i==1?TEXT("HOMICIDE"):TEXT("PATROL")),25);
		Label(FVector(-859,Y+255,154),0,TEXT("BAY"),12);
		// Faded parking paint, tire tracks and irregular oil darkening under engine locations.
		for (int32 S : {-1,1})
		{
			B(FVector(-160,Y+S*225,0.6f),FVector(1000,7,1),Yellow,EHUSurface::None,false);
			for (int32 k=0;k<8;k++) B(FVector(-790+k*63,Y+S*76,0.8f),FVector(44,15,0.6f),Concrete*.65f,EHUSurface::Concrete,false);
		}
		FRandomStream Wear(480+i);
		for (int32 k=0;k<14;k++)
		{
			HUBuild::Block(this,Root,Blocks,HUBuild::EShape::Cylinder,FVector(-325+Wear.FRandRange(-60,60),Y+Wear.FRandRange(-55,55),1.1f),
				FVector(Wear.FRandRange(12,54),Wear.FRandRange(8,32),.25f),Concrete*Wear.FRandRange(.23f,.45f),FRotator(0,Wear.FRandRange(0,180),0),false);
		}
	}
	// The exterior apron is walkable: amber street light beyond the open garage.
	B(FVector(-1230,0,-8),FVector(650,2000,16),Concrete*.48f,EHUSurface::Asphalt);
	for (int32 S : {-1,1}) B(FVector(-1230,S*1010,20),FVector(650,20,40),Concrete,EHUSurface::Concrete);
	B(FVector(-1565,0,80),FVector(20,2000,160),Steel,EHUSurface::Concrete);
	for (float X : {-600.f,0.f,600.f})
	{
		B(FVector(X,0,555),FVector(18,2000,25),Steel);
		for (float Y : {-600.f,0.f,600.f})
		{
			B(FVector(X,Y,535),FVector(145,28,9),Dark);
			B(FVector(X,Y,529),FVector(130,18,2),FLinearColor(2.6f,2.8f,2.7f),EHUSurface::None,false);
		}
	}
	// A two-place maintenance bench: legs, drawers, vise, pegboard and hanging tools.
	for (float X : {180.f,470.f})
	{
		B(FVector(X,981,238),FVector(210,24,8),Steel);
		B(FVector(X,975,232),FVector(196,18,3),FLinearColor(2.2f,2.5f,2.6f),EHUSurface::None,false);
		B(FVector(X,928,91),FVector(260,110,8),Concrete*.6f,EHUSurface::Walnut);
		for (float DX : {-110.f,110.f}) for (float Y : {886.f,970.f}) B(FVector(X+DX,Y,44),FVector(8,8,88),Steel);
		B(FVector(X,950,40),FVector(230,65,5),Steel);
		B(FVector(X,994,170),FVector(250,5,110),Concrete*.5f,EHUSurface::Walnut);
		for (int32 k=0;k<7;k++)
		{
			B(FVector(X-90+k*29,984,169+(k%3)*12),FVector(4,7,32),Steel);
			B(FVector(X-90+k*29,982,186+(k%3)*12),FVector(13,8,6),Steel);
		}
		B(FVector(X-60,878,103),FVector(38,26,18),Steel);
		B(FVector(X-60,859,106),FVector(40,5,9),Concrete*.8f);
	}
	B(FVector(720,922,51),FVector(100,75,94),Red);
	for (int32 k=0;k<5;k++)
	{
		B(FVector(720,882,18+k*16),FVector(90,3,13),Red*.8f);
		B(FVector(720,879,22+k*16),FVector(47,4,2),Steel);
	}
	for (int32 k=0;k<4;k++) HUBuild::Block(this,Root,Blocks,HUBuild::EShape::Cylinder,FVector(720,-900,12+k*23),FVector(67,67,22),Dark);
	HUBuild::Prop(this,Root,Blocks,TEXT("WallClock"),FVector(70,993,268),FVector(8,38,38),FRotator(0,-90,0));
	Label(FVector(360,982,260),-90,TEXT("FLEET SERVICES  /  RETURN KEYS TO DISPATCH"),19);
	for (float Y : {-103.f,103.f}) B(FVector(555,Y,1),FVector(680,7,1),Yellow,EHUSurface::None,false);
	PlaceSlats();
}

void AHUMotorPool::PlaceSlats()
{
	for (int32 i=0;i<Slats.Num();i++)
	{
		const int32 Bay=i/12, K=i%12;
		const float Travel=15+K*30+Lift[Bay]*390;
		// Each section follows its vertical track, a curved elbow, then the ceiling rail.
		const float Arc=FMath::Clamp((Travel-340)/60,0.f,1.f)*HALF_PI;
		const float X=-898+60*(1-FMath::Cos(Arc))+FMath::Max(0.f,Travel-400);
		const float Z=Travel<340?Travel:340+60*FMath::Sin(Arc);
		Slats[i]->SetRelativeLocation(FVector(X,BayY[Bay],Z));
		Slats[i]->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(Arc),0,0));
	}
}

bool AHUMotorPool::IsBayClear(int32 Bay) const
{
	if (Bay<0 || Bay>=3) return false;
	for (TActorIterator<APawn> It(GetWorld());It;++It)
	{
		const FVector P=GetActorTransform().InverseTransformPosition(It->GetActorLocation());
		if (FMath::Abs(P.X+898)<150 && FMath::Abs(P.Y-BayY[Bay])<270 && P.Z<430) return false;
	}
	return true;
}

void AHUMotorPool::SetBayOpen(int32 Bay, bool bOpen)
{
	if (Bay<0 || Bay>=3 || Open[Bay]==bOpen) return;
	Open[Bay]=bOpen;
	if (GetNetMode()!=NM_Client) HUSound::Play(this,TEXT("SFX_RollDoor"),GetActorTransform().TransformPosition(FVector(-898,BayY[Bay],170)));
}

int32 AHUMotorPool::FocusBay(APawn* User) const
{
	const UHUInteractComponent* I=User?User->FindComponentByClass<UHUInteractComponent>():nullptr;
	const int32* Bay=I?Controls.Find(I->FocusPart):nullptr;
	return Bay?*Bay:INDEX_NONE;
}

FText AHUMotorPool::GetPrompt_Implementation(APawn* User) const
{
	const int32 Bay=FocusBay(User);
	return Bay==INDEX_NONE?FText::GetEmpty():FText::FromString(FString::Printf(TEXT("[E] %s bay %02d"),Open[Bay]?TEXT("Close"):TEXT("Open"),Bay+1));
}

void AHUMotorPool::Interact_Implementation(APawn* User, bool bSecondary)
{
	const int32 Bay=FocusBay(User);
	if (bSecondary || Bay==INDEX_NONE) return;
	SetBayOpen(Bay,!Open[Bay]);
}

void AHUMotorPool::BeginPlay()
{
	Super::BeginPlay();
	// Saved child-actor templates contain the old construction geometry. Rebuild for this code version.
	OnConstruction(GetActorTransform());
	if (FParse::Param(FCommandLine::Get(),TEXT("HUGarageOpen"))) Open[1]=true;
	for (int32 i=0;i<Cars.Num();i++) if (AHUCruiser* C=Cast<AHUCruiser>(Cars[i]->GetChildActor())) C->bPlayerCar=i==1;
	for (float X : {-450.f,400.f,-1250.f})
	{
		UPointLightComponent* L=NewObject<UPointLightComponent>(this); L->SetupAttachment(Root);
		L->SetRelativeLocation(FVector(X,0,X<-900?330:480)); L->SetIntensity(X<-900?16000:12000);
		L->SetAttenuationRadius(1500); L->SetLightColor(X<-900?FLinearColor(1,.52f,.22f):FLinearColor(.85f,.92f,1)); L->RegisterComponent();
	}
	for(const FVector& P : {FVector(180,840,215),FVector(470,840,215),FVector(-500,-600,430),FVector(-500,600,430)})
	{
		UPointLightComponent* L=NewObject<UPointLightComponent>(this); L->SetupAttachment(Root); L->SetRelativeLocation(P);
		L->SetIntensity(P.X>0?3000:4500); L->SetAttenuationRadius(P.X>0?480:850); L->SetLightColor(FLinearColor(.87f,.93f,1));
		L->SetCastShadows(false); L->RegisterComponent();
	}

}

void AHUMotorPool::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 i=0;i<3;i++)
	{
		// Pause a closing door at the safety sensor; keep the shared replay target unchanged.
		if (!Open[i] && Lift[i]>0 && !IsBayClear(i)) continue;
		Lift[i]=FMath::FInterpConstantTo(Lift[i],Open[i]?1.f:0.f,DeltaSeconds,.28f);
	}
	PlaceSlats();
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(),TEXT("HUGarageTest")))
	{
		TestClock+=DeltaSeconds;
		auto OpeningBlocked=[&](int32 Bay)
		{
			FHitResult Hit;
			const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,GetActorTransform().TransformPosition(FVector(-1010,BayY[Bay],140)),
				GetActorTransform().TransformPosition(FVector(-780,BayY[Bay],140)),ECC_Visibility);
			if(Blocked) UE_LOG(LogTemp,Display,TEXT("HUGarageTest: bay %d hit %s at %s"),Bay,*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString());
			return Blocked;
		};
		if (TestStage==0 && TestClock>2) { for(int32 i=0;i<3;i++) SetBayOpen(i,true); TestStage++; TestClock=0; }
		if (TestStage==1 && TestClock>5)
		{
			const bool Pass=Lift[0]>.99f && Lift[1]>.99f && Lift[2]>.99f && !OpeningBlocked(0) && !OpeningBlocked(1) && !OpeningBlocked(2);
			UE_LOG(LogTemp,Display,TEXT("HUGarageTest: %s open (36 sections on ceiling tracks)"),Pass?TEXT("PASSED"):TEXT("FAILED"));
			UE_LOG(LogTemp,Display,TEXT("HUGarageTest: lift %.2f %.2f %.2f"),Lift[0],Lift[1],Lift[2]);
			for(int32 i=0;i<3;i++) SetBayOpen(i,false); TestStage++; TestClock=0;
		}
		if(TestStage==2 && TestClock>5)
		{
			const bool Pass=Lift[0]<.01f && Lift[1]<.01f && Lift[2]<.01f && OpeningBlocked(0) && OpeningBlocked(1) && OpeningBlocked(2);
			UE_LOG(LogTemp,Display,TEXT("HUGarageTest: %s closed"),Pass?TEXT("PASSED"):TEXT("FAILED"));
			SetBayOpen(1,true); TestStage++;
		}
	}
#endif
}
