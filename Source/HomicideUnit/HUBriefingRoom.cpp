#include "HUBriefingRoom.h"
#include "HUCaseSubsystem.h"
#include "HUBriefingLaptop.h"
#include "HUInteractComponent.h"
#include "HUCharacter.h"
#include "TextToSpeechEngineSubsystem.h"
#include "HUVoiceSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "Engine/Engine.h"
#include "HUMotorPool.h"
#include "HUBuild.h"
#include "HUProps.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Misc/CommandLine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

// room layout, in cm, relative to the actor: +X toward the screen wall, +Y to the right
static const float RoomHalfX = 800.f, RoomHalfY = 1000.f, RoomH = 400.f;
static const FVector ScreenPos(RoomHalfX - 12.f, 0.f, 230.f);

AHUBriefingRoom::AHUBriefingRoom()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatF(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Cube = CubeF.Object; Cylinder = CylF.Object; Sphere = SphF.Object; BaseMat = MatF.Object;

	ScreenHeader = Text(TEXT("ScreenHeader"), ScreenPos + FVector(-4, 0, 115), 22.f, FColor(14, 26, 42));
	ScreenTitle = Text(TEXT("ScreenTitle"), ScreenPos + FVector(-4, 0, 60), 34.f, FColor(238, 244, 250));
	ScreenRows = Text(TEXT("ScreenRows"), ScreenPos + FVector(-4, 0, -10), 20.f, FColor(200, 216, 232));
	ScreenCounter = Text(TEXT("ScreenCounter"), ScreenPos + FVector(-4, 0, -125), 14.f, FColor(106, 122, 138));
	ScreenTitle->SetText(FText::FromString(TEXT("METRO PD · HOMICIDE")));

	// a sign over the back door, facing into the room
	ExitSign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ExitSign"));
	ExitSign->SetupAttachment(RootComponent);
	ExitSign->SetRelativeLocation(FVector(-RoomHalfX + 2, 0, 275));
	ExitSign->SetHorizontalAlignment(EHTA_Center);
	ExitSign->SetVerticalAlignment(EVRTA_TextCenter);
	ExitSign->SetWorldSize(22.f);
	ExitSign->SetTextRenderColor(FColor(255, 181, 71));
	ExitSign->SetText(FText::FromString(TEXT("MOTOR POOL  >")));

	MotorPool = CreateDefaultSubobject<UChildActorComponent>(TEXT("MotorPool"));
	MotorPool->SetupAttachment(RootComponent);
	MotorPool->SetChildActorClass(AHUMotorPool::StaticClass());
	MotorPool->SetRelativeLocation(MotorPoolPos);
	ScreenHeader->SetText(FText::FromString(TEXT("BRIEFING")));
}

UTextRenderComponent* AHUBriefingRoom::Text(FName Name, FVector Pos, float Size, FColor Color)
{
	UTextRenderComponent* T = CreateDefaultSubobject<UTextRenderComponent>(Name);
	T->SetupAttachment(RootComponent);
	T->SetRelativeLocation(Pos);
	T->SetRelativeRotation(FRotator(0.f, 180.f, 0.f)); // face back into the room
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Color);
	return T;
}

UStaticMeshComponent* AHUBriefingRoom::Block(UStaticMesh* Mesh, FVector Pos, FVector SizeCm, FLinearColor Color, FRotator Rot)
{
	UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
	M->SetStaticMesh(Mesh);
	M->SetupAttachment(RootComponent);
	M->SetRelativeLocation(Pos);
	M->SetRelativeRotation(Rot);
	M->SetRelativeScale3D(SizeCm / 100.f); // the basic shapes are 100 cm
	if (BaseMat)
	{
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(BaseMat, M);
		D->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetMaterial(0, D);
	}
	M->RegisterComponent();
	Blocks.Add(M);
	return M;
}

void AHUBriefingRoom::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AHUBriefingRoom::Build()
{
	for (UStaticMeshComponent* B : Blocks) if (B) B->DestroyComponent();
	Blocks.Reset();
	Seats.Reset();
	for(UTextRenderComponent* T : CorridorLabels) if(T) T->DestroyComponent();
	CorridorLabels.Reset();
	if (!Cube) return;

	const FLinearColor Carpet(0.05f, 0.07f, 0.10f), Wall(0.35f, 0.38f, 0.42f), Wainscot(0.18f, 0.10f, 0.06f), Ceiling(0.6f, 0.6f, 0.58f);
	const FLinearColor Wood(0.22f, 0.12f, 0.05f), Steel(0.25f, 0.27f, 0.3f), Seat(0.08f, 0.12f, 0.2f);

	// shell: navy commercial carpet, acoustic tiles overhead, plaster over a walnut wainscot (HUSurfaces.h)
	auto Made = [](UStaticMeshComponent* C, EHUSurface S, const FLinearColor& Col) { HUSurfaces::Apply(C, S, Col); return C; };
	Made(Block(Cube, FVector(0, 0, -5), FVector(RoomHalfX * 2, RoomHalfY * 2, 10), Carpet), EHUSurface::Fabric, Carpet);
	Made(Block(Cube, FVector(0, 0, RoomH + 5), FVector(RoomHalfX * 2, RoomHalfY * 2, 10), Ceiling), EHUSurface::CeilingTile, Ceiling);
	Made(Block(Cube, FVector(RoomHalfX + 5, 0, RoomH / 2), FVector(10, RoomHalfY * 2, RoomH), Wall), EHUSurface::Plaster, Wall);
	// the back wall has the doorway out to the corridor and the motor pool
	HUBuild::WallWithDoor(this, RootComponent, Blocks, true, FVector(-RoomHalfX - 5, 0, RoomH / 2), RoomHalfY * 2, RoomH, 10, 0, 180, 250, Wall, EHUSurface::Plaster);
	BuildCorridor();
	Made(Block(Cube, FVector(0, RoomHalfY + 5, RoomH / 2), FVector(RoomHalfX * 2, 10, RoomH), Wall), EHUSurface::Plaster, Wall);
	Made(Block(Cube, FVector(0, -RoomHalfY - 5, RoomH / 2), FVector(RoomHalfX * 2, 10, RoomH), Wall), EHUSurface::Plaster, Wall);
	// dark wood wainscot round the bottom of the walls
	for (int32 S = -1; S <= 1; S += 2) Made(Block(Cube, FVector(-RoomHalfX + 2, S * (RoomHalfY + 90) / 2, 50), FVector(4, RoomHalfY - 90, 100), Wainscot), EHUSurface::Walnut, Wainscot);
	Made(Block(Cube, FVector(0, RoomHalfY - 2, 50), FVector(RoomHalfX * 2, 4, 100), Wainscot), EHUSurface::Walnut, Wainscot);
	Made(Block(Cube, FVector(0, -RoomHalfY + 2, 50), FVector(RoomHalfX * 2, 4, 100), Wainscot), EHUSurface::Walnut, Wainscot);
	// ceiling lights
	for (float X : { -400.f, 0.f, 400.f }) for (float Y : { -500.f, 0.f, 500.f })
		Block(Cube, FVector(X, Y, RoomH - 4), FVector(120, 40, 4), FLinearColor(3.f, 3.f, 2.8f));

	// the projector screen: a black frame, the blue slide, the header bar
	Block(Cube, ScreenPos + FVector(4, 0, 0), FVector(4, 520, 300), FLinearColor(0.01f, 0.01f, 0.01f));
	Block(Cube, ScreenPos, FVector(4, 500, 280), FLinearColor(0.02f, 0.06f, 0.12f));
	Block(Cube, ScreenPos + FVector(-1, 0, 115), FVector(4, 500, 40), FLinearColor(0.15f, 0.55f, 1.f));
	// the projector, hanging from the ceiling
	Block(Cube, FVector(200, 0, RoomH - 50), FVector(40, 30, 14), Steel);
	Block(Cylinder, FVector(200, 0, RoomH - 20), FVector(3, 3, 40), Steel);

	// A hollow walnut lectern, sloped reading surface, microphone and paper notes.
	PodiumParts.Reset();
	if (UStaticMeshComponent* Real = HUBuild::Prop(this, Root, Blocks, TEXT("BriefingPodium"), FVector(600,-380,62), FVector(68,82,124), FRotator(0,180,0))) PodiumParts.Add(Real);
	else
	{
		PodiumParts.Add(Made(Block(Cube,FVector(600,-380,5),FVector(64,82,10),Wood),EHUSurface::Walnut,Wood));
		PodiumParts.Add(Made(Block(Cube,FVector(575,-380,58),FVector(6,76,106),Wood),EHUSurface::Walnut,Wood));
		for(float Y : {-415.f,-345.f}) PodiumParts.Add(Made(Block(Cube,FVector(600,Y,58),FVector(50,6,106),Wood),EHUSurface::Walnut,Wood));
		PodiumParts.Add(Made(Block(Cube,FVector(600,-380,114),FVector(68,82,6),Wood,FRotator(-10,0,0)),EHUSurface::Walnut,Wood));
	}
	Block(Cube,FVector(602,-382,120),FVector(27,38,1),FLinearColor(.65f,.61f,.48f),FRotator(-10,0,0));
	Block(Cylinder,FVector(584,-354,130),FVector(1.3f,1.3f,26),Steel,FRotator(-20,0,0));
	Block(Sphere,FVector(588,-354,143),FVector(4,4,7),FLinearColor(.012f,.012f,.012f));

	CaptainParts.Reset();
	CaptainParts.Add(Block(Cylinder, FVector(670, -380, 80), FVector(50, 50, 160), FLinearColor(0.03f, 0.05f, 0.1f)));
	CaptainParts.Add(Block(Sphere, FVector(670, -380, 180), FVector(34, 30, 38), FLinearColor(0.6f, 0.42f, 0.32f)));
	CaptainParts.Add(Block(Cylinder, FVector(670, -380, 200), FVector(40, 40, 8), FLinearColor(0.02f, 0.03f, 0.07f)));

	// the laptop table (the laptop itself is its own actor, spawned at play): a real desk when the art is in (HUProps)
	if (!HUBuild::Prop(this, RootComponent, Blocks, TEXT("Desk"), FVector(560, 420, 38.5f), FVector(90, 140, 77), FRotator(0, 180, 0)))
	{
		Made(Block(Cube, FVector(560, 420, 75), FVector(90, 140, 4), Wood), EHUSurface::Walnut, Wood);
		for (float DX : { -40.f, 40.f }) for (float DY : { -60.f, 60.f })
			Block(Cube, FVector(560 + DX, 420 + DY, 37), FVector(4, 4, 74), Steel);
	}

	// four rows of chairs either side of the aisle, facing the screen
	for (float X : { 250.f, 80.f, -90.f, -260.f })
		for (int32 Side = -1; Side <= 1; Side += 2)
			for (int32 i = 0; i < 5; i++)
			{
				const float Y = Side * (180.f + i * 130.f);
				// a real stacking chair when the art is in (HUProps): the whole chair is the seat
				if (UStaticMeshComponent* Real = HUBuild::Prop(this, RootComponent, Blocks, TEXT("StackChair"), FVector(X, Y, 50), FVector(0, 0, 100)))
				{
					Seats.Add(Real, FVector(X, Y, 48));
					continue;
				}
				// remember both the cushion and the backrest as "this chair"
				Seats.Add(Made(Block(Cube, FVector(X, Y, 45), FVector(50, 50, 6), Seat), EHUSurface::Fabric, Seat), FVector(X, Y, 48));
				Seats.Add(Made(Block(Cube, FVector(X - 24, Y, 75), FVector(4, 50, 50), Seat), EHUSurface::Fabric, Seat), FVector(X, Y, 48));
				for (float DX : { -20.f, 20.f }) for (float DY : { -20.f, 20.f })
					Block(Cube, FVector(X + DX, Y + DY, 21), FVector(3, 3, 42), Steel);
			}
}

// out the back door: a corridor to the motor pool, with signs
void AHUBriefingRoom::BuildCorridor()
{
	const float X0=-RoomHalfX-10, X1=MotorPoolPos.X+900, Len=X0-X1, Mid=(X0+X1)/2;
	const FLinearColor Cream=FLinearColor::FromSRGBColor(FColor(201,193,175));
	const FLinearColor Sage=FLinearColor::FromSRGBColor(FColor(89,102,81));
	const FLinearColor Wood=FLinearColor::FromSRGBColor(FColor(73,55,44));
	const FLinearColor Steel=FLinearColor::FromSRGBColor(FColor(77,87,89));
	auto B=[&](FVector P,FVector S,FLinearColor C,EHUSurface Mat=EHUSurface::Plaster)
	{ return HUBuild::Block(this,Root,Blocks,HUBuild::EShape::Cube,P,S,C,FRotator::ZeroRotator,true,Mat); };
	auto Sign=[&](FVector P,float Yaw,const FString& Words,float Size)
	{
		UTextRenderComponent* T=NewObject<UTextRenderComponent>(this); T->SetupAttachment(Root);
		T->SetRelativeLocation(P); T->SetRelativeRotation(FRotator(0,Yaw,0)); T->SetWorldSize(Size);
		T->SetHorizontalAlignment(EHTA_Center); T->SetTextRenderColor(FColor(224,216,191)); T->SetText(FText::FromString(Words));
		T->RegisterComponent(); CorridorLabels.Add(T);
	};
	B(FVector(Mid,0,-5),FVector(Len,320,10),Sage,EHUSurface::Linoleum);
	B(FVector(Mid,0,305),FVector(Len,320,10),Cream,EHUSurface::CeilingTile);
	const TCHAR* Departments[]={TEXT("RECORDS  /  104"),TEXT("EVIDENCE  /  105"),TEXT("TRAFFIC  /  106"),TEXT("DETECTIVE BUREAU  /  107")};
	for(int32 Side : {-1,1})
	{
		B(FVector(Mid,Side*165,150),FVector(Len,10,300),Cream);
		B(FVector(Mid,Side*158,43),FVector(Len,3,86),Sage);
		B(FVector(Mid,Side*155,88),FVector(Len,4,4),Steel,EHUSurface::PaintedMetal);
		B(FVector(Mid,Side*154,7),FVector(Len,4,14),Steel,EHUSurface::PaintedMetal);
		for(int32 i=0;i<2;i++)
		{
			const float X=X0-135-i*285;
			B(FVector(X,Side*149,114),FVector(118,12,228),Steel,EHUSurface::PaintedMetal);
			B(FVector(X,Side*141,111),FVector(104,6,216),Wood,EHUSurface::Walnut);
			// Wired-glass vision panel, kick plate, institutional lever and room plaque.
			B(FVector(X,Side*136,164),FVector(62,3,58),FLinearColor(.06f,.085f,.078f),EHUSurface::Glass);
			for(int32 k=-2;k<=2;k++) B(FVector(X+k*11,Side*133,164),FVector(.5f,1,55),Steel,EHUSurface::PaintedMetal);
			B(FVector(X,Side*135,24),FVector(98,2,32),Steel,EHUSurface::PaintedMetal);
			B(FVector(X+37,Side*131,103),FVector(19,7,3),Steel,EHUSurface::PaintedMetal);
			B(FVector(X,Side*132,220),FVector(108,3,18),Steel,EHUSurface::PaintedMetal);
			Sign(FVector(X,Side*129,216),Side>0?-90:90,Departments[(Side>0?2:0)+i],8);
		}
	}
	for(float X=X0-100;X>X1;X-=240)
	{
		B(FVector(X,0,297),FVector(115,36,6),Steel,EHUSurface::PaintedMetal);
		B(FVector(X,0,293),FVector(104,26,2),FLinearColor(2.7f,2.9f,2.7f),EHUSurface::None);
	}
	B(FVector(X1+35,0,267),FVector(5,250,30),Steel,EHUSurface::PaintedMetal);
	Sign(FVector(X1+39,0,260),0,TEXT("MOTOR POOL  /  FLEET SERVICES"),13);
	if(MotorPool && MotorPool->GetChildActorClass()!=AHUMotorPool::StaticClass()) MotorPool->SetChildActorClass(AHUMotorPool::StaticClass());
	if(MotorPool) MotorPool->SetRelativeLocation(MotorPoolPos);
}

void AHUBriefingRoom::BuildProjection()
{
	// A soft additive frustum is bounded by the actual lens and screen, not room-wide haze.
	const FVector Lens(227,0,350), Target(778,0,230);
	UStaticMesh* BeamMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_ProjectorFrustum.SM_ProjectorFrustum"));
	if(BeamMesh)
	{
		UStaticMeshComponent* Beam=NewObject<UStaticMeshComponent>(this); Beam->SetupAttachment(Root);
		Beam->SetStaticMesh(BeamMesh); Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision); Beam->SetCastShadow(false);
		Beam->RegisterComponent(); Blocks.Add(Beam);
	}
	USpotLightComponent* Light=NewObject<USpotLightComponent>(this); Light->SetupAttachment(Root);
	Light->SetRelativeLocation(Lens); Light->SetRelativeRotation((Target-Lens).Rotation());
	Light->SetIntensity(1700); Light->SetAttenuationRadius(700); Light->SetInnerConeAngle(10); Light->SetOuterConeAngle(25);
	Light->SetLightColor(FLinearColor(.72f,.83f,1)); Light->SetCastShadows(false); Light->RegisterComponent();
	UMaterialInterface* DustMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_HU_ProjectorDust.M_HU_ProjectorDust"));
	FRandomStream R(2005);
	for(int32 i=0;i<72;i++)
	{
		const float A=R.FRand(), Radius=R.FRandRange(.05f,.88f), Angle=R.FRandRange(0,2*PI);
		const FVector Pos=FMath::Lerp(Lens,Target,A)+FVector(0,FMath::Cos(Angle)*220*A*Radius,FMath::Sin(Angle)*110*A*Radius);
		UStaticMeshComponent* D=Block(Sphere,Pos,FVector(.65f),FLinearColor(2.5f,2.9f,3.4f));
		D->SetCollisionEnabled(ECollisionEnabled::NoCollision); D->SetCastShadow(false); if(DustMat) D->SetMaterial(0,DustMat); Dust.Add(D);
		DustOrigins.Add(Pos);
	}
}

void AHUBriefingRoom::BeginPlay()
{
	Super::BeginPlay();
	Build(); // refresh construction geometry serialized in the level before this rework
	BuildProjection();
#if !UE_BUILD_SHIPPING
	if(FParse::Param(FCommandLine::Get(),TEXT("HUGarageTest")))
	{
		FHitResult Hit;
		const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,GetActorTransform().TransformPosition(FVector(-750,0,88)),
			GetActorTransform().TransformPosition(FVector(MotorPoolPos.X+750,0,88)),ECC_Visibility);
		UE_LOG(LogTemp,Display,TEXT("HUGarageTest: %s corridor clear at capsule height"),Blocked?TEXT("FAILED"):TEXT("PASSED"));
	}
#endif
	// Dim the briefing while the projector is on; the corridor stays readable.
	for (float X : { -400.f, 0.f, 400.f })
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetRelativeLocation(FVector(X, 0, RoomH - 40));
		L->SetIntensity(2400.f);
		L->SetAttenuationRadius(1400.f);
		L->SetLightColor(FLinearColor(1.f, 0.96f, 0.88f));
		L->RegisterComponent();
	}
	for(float X : {-960.f,-1230.f})
	{
		UPointLightComponent* L=NewObject<UPointLightComponent>(this); L->SetupAttachment(Root);
		L->SetRelativeLocation(FVector(X,0,260)); L->SetIntensity(2400); L->SetAttenuationRadius(340);
		L->SetLightColor(FLinearColor(.89f,.94f,.87f)); L->RegisterComponent();
	}
	// the laptop, wired to this room's screen
	const FTransform T = FTransform(FRotator::ZeroRotator, FVector(560, 420, 78)) * GetActorTransform();
	FActorSpawnParameters LP; LP.Name = TEXT("HU_BriefingLaptop"); LP.bDeferConstruction = true; LP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Laptop = GetWorld()->SpawnActor<AHUBriefingLaptop>(AHUBriefingLaptop::StaticClass(), T, LP); // same name on every machine
	if (Laptop)
	{
		Laptop->Room = this;
		Laptop->SceneLevel = SceneLevel;
		Laptop->DetectiveName = DetectiveName;
		Laptop->FinishSpawning(T);
	}
}

void AHUBriefingRoom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	DustClock+=DeltaSeconds;
	for(int32 i=0;i<Dust.Num();i++) if(Dust[i])
		Dust[i]->SetRelativeLocation(DustOrigins[i]+FVector(0,FMath::Sin(DustClock*.23f+i)*2.2f,FMath::Sin(DustClock*.31f+i*1.7f)*3.f));
	Play911(DeltaSeconds);
	SeatSquad();
	if (bPlayerReady) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) return;
	bPlayerReady = true;
	UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>();
	if (!I)
	{
		I = NewObject<UHUInteractComponent>(P, TEXT("HUInteract"));
		I->RegisterComponent();
	}
	I->Subtitle = FText::FromString(PendingLine);
	if (bSeatPlayer && GetNetMode() == NM_Standalone)
	{
		// you start in the front row, aisle seat, facing the screen
		P->SetActorLocation(GetActorTransform().TransformPosition(FVector(150, -100, 100)), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(GetActorRotation());
		if (AHUCharacter* Ch = Cast<AHUCharacter>(P)) Ch->SitDown(GetActorTransform().TransformPosition(FVector(250, -180, 48)), GetActorRotation().Yaw);
	}
}

// in a squad, the host seats everyone along the front row as they arrive
void AHUBriefingRoom::SeatSquad()
{
	if (!bSeatPlayer || GetNetMode() == NM_Standalone || GetNetMode() == NM_Client) return;
	static const FVector2D FrontRow[] = { { 250, -180 }, { 250, 180 }, { 250, -310 }, { 250, 310 }, { 80, -180 }, { 80, 180 } };
	int32 i = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++i)
	{
		APlayerController* PC = It->Get();
		AHUCharacter* Ch = PC ? Cast<AHUCharacter>(PC->GetPawn()) : nullptr;
		if (!Ch || Seated.Contains(Ch)) continue;
		Seated.Add(Ch);
		const FVector2D S = FrontRow[i % UE_ARRAY_COUNT(FrontRow)];
		const FVector Seat = GetActorTransform().TransformPosition(FVector(S.X, S.Y, 48));
		Ch->SetActorLocation(GetActorTransform().TransformPosition(FVector(S.X - 100, S.Y > 0 ? 100 : -100, 100)), false, nullptr, ETeleportType::TeleportPhysics);
		Ch->SitDown(Seat, GetActorRotation().Yaw);
		Ch->ClientSitDown(Seat, GetActorRotation().Yaw);
	}
}

FText AHUBriefingRoom::GetPrompt_Implementation(APawn* User) const
{
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (I && CaptainParts.Contains(I->FocusPart)) return FText::FromString(TEXT("[E] Talk to the Captain"));
	if (I && PodiumParts.Contains(I->FocusPart)) return FText::FromString(TEXT("[E] Read the Captain's notes"));
	if (!I || !Seats.Contains(I->FocusPart)) return FText::GetEmpty();
	const AHUCharacter* Ch = Cast<AHUCharacter>(User);
	return FText::FromString(Ch && Ch->bSeated ? TEXT("[E] Stand up") : TEXT("[E] Sit down"));
}

void AHUBriefingRoom::Interact_Implementation(APawn* User, bool bSecondary)
{
	AHUCharacter* Ch = Cast<AHUCharacter>(User);
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I || bSecondary) return;
	if (CaptainParts.Contains(I->FocusPart))
	{
		const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		const bool bDone = C && C->bBriefed;
		const TCHAR* Lines[] = { TEXT("\"Eyes on the screen, detective. The laptop's on the side table.\""), TEXT("\"Don't look at me, look at the evidence.\""), TEXT("\"Every one of them is lying about something. Find out which lie matters.\"") };
		I->Say(TEXT("THE CAPTAIN:  ") + FString(bDone ? TEXT("\"What are you still doing here? Your car's in the motor pool.\"") : Lines[FMath::RandRange(0, 2)]));
		return;
	}
	if (PodiumParts.Contains(I->FocusPart))
	{
		const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		if (C && C->bCaseActive) I->Say(FString::Printf(TEXT("NOTES: %s — %s. %d people held. Difficulty: %s."), *C->Current.Title, *C->Current.Victim, C->Current.Suspects.Num(), *C->PlayDifficulty));
		return;
	}
	if (!Ch) return;
	const FVector* Seat = Seats.Find(I->FocusPart);
	if (!Seat) return;
	if (Ch->bSeated) { Ch->StandUp(); return; }
	Ch->SitDown(GetActorTransform().TransformPosition(*Seat), GetActorRotation().Yaw); // every chair faces the screen
}

void AHUBriefingRoom::ShowSlide(const FHUSlide& Slide, int32 SlideIndex, int32 SlideCount)
{
	ScreenHeader->SetText(FText::FromString(TEXT("METRO PD · ") + Slide.Header));
	ScreenTitle->SetText(FText::FromString(Slide.Title));
	ScreenRows->SetText(FText::FromString(FString::Join(Slide.Rows, TEXT("<br>"))));
	// a long list (everyone held at the scene) gets smaller lines, lower down
	const bool bList = Slide.Rows.Num() > 4;
	ScreenRows->SetWorldSize(bList ? 12.f : 20.f);
	ScreenRows->SetRelativeLocation(ScreenPos + FVector(-4, 0, bList ? -30.f : -10.f));
	ScreenCounter->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), SlideIndex + 1, SlideCount)));
	PendingLine = TEXT("THE CAPTAIN:  ") + Slide.CaptainLine;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (APawn* P = PC->GetPawn())
			if (UHUInteractComponent* I = P->FindComponentByClass<UHUInteractComponent>())
				I->Subtitle = FText::FromString(PendingLine);
}

void AHUBriefingRoom::HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const
{
	if (CaptainParts.Contains(Part)) { for (UStaticMeshComponent* P : CaptainParts) Out.Add(P); return; }
	if (PodiumParts.Contains(Part)) { for (UStaticMeshComponent* P : PodiumParts) Out.Add(P); return; }
	// a chair: its cushion and its backrest
	if (const FVector* Seat = Seats.Find(Part)) for (const TPair<UPrimitiveComponent*, FVector>& S : Seats) if (S.Value.Equals(*Seat)) Out.Add(S.Key);
}
// ---------- the 911 call, played over the room while the briefing runs ----------

void AHUBriefingRoom::Play911(float DeltaSeconds)
{
	UHUInteractComponent* I = nullptr;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (APawn* P = PC->GetPawn()) I = P->FindComponentByClass<UHUInteractComponent>();
	if (bCallDone)
	{
		// clear the caption a few seconds after the line goes dead
		if (CallEndTimer > 0.f && (CallEndTimer -= DeltaSeconds) <= 0.f && I) I->CallLine = FText::GetEmpty();
		return;
	}
	UHUVoiceSubsystem* Voice = GetGameInstance()->GetSubsystem<UHUVoiceSubsystem>();
	if (CallIdx < 0)
	{
		// wait a moment after you sit down, then the tape starts
		CallWait -= DeltaSeconds;
		if (CallWait > 0.f || !I) return;
		const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		if (C) C->Build911Call(CallSpeakers, CallLines);
		if (CallLines.Num() == 0) { bCallDone = true; return; }
		// with a voice key, ask for every line up front, acted: a calm dispatcher and a terrified caller
		CallSounds.Init(nullptr, CallLines.Num());
		CallSeconds.Init(0.f, CallLines.Num());
		CallState.Init(Voice && Voice->HasNeuralVoice() ? 0 : 2, CallLines.Num()); // 0 waiting, 1 ready, 2 caption only
		if (Voice && Voice->HasNeuralVoice())
		{
			const int32 CallerVoice = 1 + FMath::Abs(C ? C->Current.Number : 0) % 7;
			for (int32 i = 0; i < CallLines.Num(); i++)
			{
				const bool bDispatch = CallSpeakers[i] == TEXT("DISPATCH");
				const FString Direction = bDispatch
					? TEXT("You are a 911 emergency dispatcher on a recorded call. Calm, steady, professional; warm but firm. Clear, measured pace.")
					: TEXT("You are a terrified bystander on the phone to 911 moments after finding something horrible. Breathless and shaky, voice cracking, rushing the words, close to tears, small gasps between phrases.");
				TWeakObjectPtr<AHUBriefingRoom> Self(this);
				Voice->Synthesize(CallLines[i], bDispatch ? 0 : CallerVoice, Direction, true, FHUVoiceReady::CreateLambda([Self, i](USoundWave* S, float Secs)
				{
					if (!Self.IsValid() || !Self->CallState.IsValidIndex(i)) return;
					Self->CallSounds[i] = S; Self->CallSeconds[i] = Secs; Self->CallState[i] = S ? 1 : 2;
				}));
			}
		}
		CallIdx = 0;
		LineTime = -1.f;
		LineWait = 0.f;
	}
	LineTime += DeltaSeconds;
	// the current line still playing (or still up long enough to read)?
	if (LineTime >= 0.f)
	{
		if (LineTime < LineLength) return;
		CallIdx++;
		LineTime = -1.f;
		LineWait = 0.f;
	}
	if (!CallLines.IsValidIndex(CallIdx))
	{
		bCallDone = true;
		if (I) I->CallLine = FText::FromString(TEXT("911  ▸  [call ended]"));
		CallEndTimer = 4.f;
		return;
	}
	// the next line's voice still on its way: give it a few seconds, then fall back to Windows
	LineWait += DeltaSeconds;
	if (CallState[CallIdx] == 0 && LineWait < 8.f) return;
	const bool bDispatch = CallSpeakers[CallIdx] == TEXT("DISPATCH");
	if (I) I->CallLine = FText::FromString(FString::Printf(TEXT("911 CALL  ▸  %s:  \"%s\""), *CallSpeakers[CallIdx], *CallLines[CallIdx]));
	if (CallState[CallIdx] == 1 && CallSounds[CallIdx])
	{
		UGameplayStatics::PlaySound2D(this, CallSounds[CallIdx], 1.f);
		LineLength = CallSeconds[CallIdx] + 0.35f; // a breath between lines
	}
	else
	{
		// no voice key (or the request failed): caption only, long enough to read - no robotic voice
		LineLength = 1.5f + CallLines[CallIdx].Len() * 0.055f;
	}
	LineTime = 0.f;
}
void AHUBriefingRoom::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
}