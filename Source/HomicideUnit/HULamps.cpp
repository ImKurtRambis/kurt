#include "HULamps.h"
#include "HUBuild.h"
#include "HUProps.h"
#include "HUUsables.h"
#include "Components/StaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundWaveProcedural.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HUSquadRoom.h"
#include "HUCrimeScene.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "UnrealClient.h"

using namespace HUBuild;

namespace
{
	const FName OwnLight(TEXT("HUOwnLight")); // not a room's ceiling light: the room's switch leaves it alone
	constexpr float UpShare = 0.25f;           // how much of a strip's light goes up onto the ceiling and the walls' tops

	UStaticMeshComponent* Part(AActor* O, USceneComponent* P, TArray<UStaticMeshComponent*>& Out, EShape S, FVector At, FVector Size, FLinearColor Col, FRotator R, EHUSurface Sf = EHUSurface::None)
	{
		UStaticMeshComponent* C = Block(O, P, Out, S, At, Size, Col, R, false, Sf);
		if (C) C->SetCastShadow(true);
		return C;
	}
	template <class T> T* MakeLight(AActor* Owner, USceneComponent* Parent, FVector At, FRotator Rot, float Candela, FLinearColor Col, float Radius, bool bShadows)
	{
		T* L = NewObject<T>(Owner);
		L->SetupAttachment(Parent);
		L->SetRelativeLocationAndRotation(At, Rot);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(Candela);
		L->SetLightColor(Col);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(bShadows);
		L->RegisterComponent();
		return L;
	}
}

UMaterialInstanceDynamic* HULamps::Glow(UPrimitiveComponent* C, FLinearColor Col)
{
	if (!C) return nullptr;
	static TWeakObjectPtr<UMaterialInterface> GlowMat;
	static bool bLooked = false;
	if (!bLooked) { bLooked = true; GlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_HU_Glow.M_HU_Glow")); }
	UMaterialInstanceDynamic* D = nullptr;
	if (GlowMat.IsValid()) { D = UMaterialInstanceDynamic::Create(GlowMat.Get(), C); C->SetMaterial(0, D); }
	else D = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)); // the block's own: just very bright
	if (D) D->SetVectorParameterValue(TEXT("Color"), Col);
	C->SetCastShadow(false); // a lit tube or bulb doesn't shade its own light
	return D;
}

// ---------- the ceiling strip ----------
URectLightComponent* HULamps::Tube(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, FVector At, float Yaw, float Candela, bool bDead, float Drop)
{
	const FRotator R(0, Yaw, 0);
	const FVector X = R.Vector(), Y = FRotator(0, Yaw + 90.f, 0).Vector(), Z(0, 0, 1);
	const FLinearColor Steel(0.62f, 0.62f, 0.6f);
	if (Drop > 1.f)
	{
		for (float S : { -1.f, 1.f }) Part(Owner, Parent, Out, EShape::Cylinder, At + X * S * 50.f - Z * Drop / 2.f, FVector(0.6f, 0.6f, Drop), FLinearColor(0.2f, 0.2f, 0.2f), FRotator::ZeroRotator);
		At -= Z * Drop;
	}
	Part(Owner, Parent, Out, EShape::Cube, At - Z * 3.f, FVector(126, 18, 6), Steel, R, EHUSurface::PaintedMetal);            // the channel
	for (float S : { -1.f, 1.f }) Part(Owner, Parent, Out, EShape::Cube, At + X * S * 61.f - Z * 8.f, FVector(3, 15, 6), Steel * 0.5f, R); // the end caps
	// two bare tubes under it: cool white, a touch green
	const FLinearColor Lit = bDead ? FLinearColor(0.32f, 0.33f, 0.31f) : FLinearColor(2.3f, 2.6f, 2.25f);
	TArray<UMaterialInstanceDynamic*> Glows;
	for (float S : { -1.f, 1.f })
	{
		UStaticMeshComponent* T = Part(Owner, Parent, Out, EShape::Cylinder, At + Y * S * 4.f - Z * 8.5f, FVector(3.6f, 3.6f, 119), Lit, FRotator(90, Yaw, 0));
		if (UMaterialInstanceDynamic* G = bDead ? nullptr : Glow(T, Lit)) Glows.Add(G);
	}
	if (bDead || Candela <= 0.f) return nullptr;
	// a soft panel of light the length of the tubes, facing the floor (its length along the rect light's height)
	URectLightComponent* L = MakeLight<URectLightComponent>(Owner, Parent, At - Z * 11.f, FRotator(-90, Yaw, 0), Candela, FLinearColor(0.95f, 1.f, 0.93f), 950.f, true);
	L->SetSourceWidth(10.f);
	L->SetSourceHeight(118.f);
	L->SetBarnDoorAngle(88.f);
	// and a little of it up and sideways: the ceiling and the tops of the walls round a strip aren't black
	UPointLightComponent* Up = MakeLight<UPointLightComponent>(Owner, Parent, At - Z * 14.f, FRotator::ZeroRotator, Candela * UpShare, FLinearColor(0.95f, 1.f, 0.93f), 650.f, false);
	if (UWorld* W = Owner ? Owner->GetWorld() : nullptr) if (UHULampWorld* S = W->GetSubsystem<UHULampWorld>()) S->AddTube(L, Up, Glows, Lit, Candela);
	return L;
}

// ---------- a bulb under a shade, or a ceiling dome ----------
UPointLightComponent* HULamps::Pendant(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, FVector At, float Drop, float Candela, FLinearColor Col, bool bDead)
{
	const FVector Z(0, 0, 1);
	const FLinearColor Bulb = bDead ? FLinearColor(0.3f, 0.29f, 0.27f) : Col * 3.f;
	FVector BulbAt;
	if (Drop <= 1.f)
	{
		// flush to the ceiling: a brass rim and a frosted glass dome
		Part(Owner, Parent, Out, EShape::Cylinder, At - Z * 1.5f, FVector(36, 36, 3), FLinearColor(0.55f, 0.42f, 0.18f), FRotator::ZeroRotator);
		UStaticMeshComponent* Dome = Part(Owner, Parent, Out, EShape::Sphere, At - Z * 4.f, FVector(32, 32, 14), Bulb, FRotator::ZeroRotator);
		if (!bDead) Glow(Dome, Col * 1.8f);
		BulbAt = At - Z * 16.f;
	}
	else
	{
		// a cord, an enamel shade (dark green outside), the bulb showing under it
		Part(Owner, Parent, Out, EShape::Cylinder, At - Z * Drop / 2.f, FVector(0.8f, 0.8f, Drop), FLinearColor(0.03f, 0.03f, 0.03f), FRotator::ZeroRotator);
		Part(Owner, Parent, Out, EShape::Cylinder, At - Z * (Drop + 2.f), FVector(10, 10, 6), FLinearColor(0.05f, 0.18f, 0.1f), FRotator::ZeroRotator);
		Part(Owner, Parent, Out, EShape::Cylinder, At - Z * (Drop + 8.f), FVector(38, 38, 7), FLinearColor(0.05f, 0.18f, 0.1f), FRotator::ZeroRotator, EHUSurface::PaintedMetal);
		UStaticMeshComponent* B = Part(Owner, Parent, Out, EShape::Sphere, At - Z * (Drop + 14.f), FVector(9, 9, 11), Bulb, FRotator::ZeroRotator);
		if (!bDead) Glow(B, Col * 4.f);
		BulbAt = At - Z * (Drop + 22.f);
	}
	if (bDead || Candela <= 0.f) return nullptr;
	UPointLightComponent* L = MakeLight<UPointLightComponent>(Owner, Parent, BulbAt, FRotator::ZeroRotator, Candela, Col, 1000.f, true);
	L->SetSourceRadius(4.f);
	return L;
}

// ---------- a desk lamp ----------
USpotLightComponent* HULamps::DeskLamp(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, FVector Base, float Yaw, bool bOn)
{
	const FRotator R(0, Yaw, 0);
	const FVector X = R.Vector(), Y = FRotator(0, Yaw + 90.f, 0).Vector(), Z(0, 0, 1);
	UStaticMeshComponent* Lamp = Prop(Owner, Parent, Out, TEXT("DeskLamp"), Base + Z * 22.f, FVector(0, 0, 44), R); // marks itself a lamp (HUUsables)
	FVector BulbAt;
	if (Lamp)
	{
		// the arm lamp leans forward: its shade is up at the front of its box
		const FTransform PT = Parent->GetComponentTransform();
		const FVector Ext = PT.InverseTransformVector(Lamp->Bounds.BoxExtent).GetAbs();
		const float Reach = FMath::Abs(FVector::DotProduct(X, Ext)) * 0.55f;
		BulbAt = PT.InverseTransformPosition(Lamp->Bounds.Origin) + X * Reach + Z * (Ext.Z * 0.62f);
	}
	else
	{
		// a green banker's lamp: a brass foot and stem, a long green glass shade
		const FLinearColor Brass(0.6f, 0.45f, 0.15f), Green(0.03f, 0.3f, 0.12f);
		Part(Owner, Parent, Out, EShape::Cylinder, Base + Z * 1.f, FVector(16, 16, 2), Brass, R);
		Part(Owner, Parent, Out, EShape::Cylinder, Base + Z * 17.f, FVector(1.6f, 1.6f, 30), Brass, R);
		Lamp = Part(Owner, Parent, Out, EShape::Cylinder, Base + Z * 34.f + X * 4.f, FVector(13, 13, 34), Green, FRotator(0, Yaw, 90));
		if (Lamp) Lamp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // E still finds it
		HUUsables::Mark(Lamp, EHUUse::Lamp);
		BulbAt = Base + Z * 28.f + X * 4.f;
	}
	UStaticMeshComponent* Bulb = Part(Owner, Parent, Out, EShape::Sphere, BulbAt + Z * 1.5f, FVector(5, 5, 5), FLinearColor(3, 2.4f, 1.6f), R);
	Glow(Bulb, FLinearColor(4.f, 3.1f, 2.f));
	// a pool of warm light on the desk, falling a little forward
	USpotLightComponent* L = MakeLight<USpotLightComponent>(Owner, Parent, BulbAt, FRotator(-78, Yaw, 0), 3.5f, FLinearColor(1.f, 0.8f, 0.55f), 320.f, true);
	L->SetInnerConeAngle(28.f);
	L->SetOuterConeAngle(52.f);
	L->SetSourceRadius(2.f);
	L->ComponentTags.Add(OwnLight);
	if (Bulb) Bulb->ComponentTags.Add(OwnLight);
	HUUsables::SetLamp(Lamp, L, Bulb, bOn);
	return L;
}

void HULamps::LightLamp(UPrimitiveComponent* Shade, bool bOn)
{
	AActor* Owner = Shade ? Shade->GetOwner() : nullptr;
	if (!Owner) return;
	// a desk lamp (low) lights the desk under it; a floor lamp (shade up past a standing waist) lights the room
	const FVector C = Shade->Bounds.Origin, E = Shade->Bounds.BoxExtent;
	const bool bFloor = C.Z - Owner->GetActorLocation().Z > 130.f;
	ULocalLightComponent* L = nullptr;
	if (bFloor)
	{
		UPointLightComponent* P = NewObject<UPointLightComponent>(Owner);
		P->SetupAttachment(Shade);
		P->SetWorldLocation(C - FVector(0, 0, E.Z + 4.f));
		P->SetIntensityUnits(ELightUnits::Candelas); P->SetIntensity(4.f); P->SetAttenuationRadius(650.f);
		L = P;
	}
	else
	{
		USpotLightComponent* S = NewObject<USpotLightComponent>(Owner);
		S->SetupAttachment(Shade);
		S->SetWorldLocationAndRotation(C - FVector(0, 0, E.Z * 0.5f), FRotator(-88, 0, 0));
		S->SetIntensityUnits(ELightUnits::Candelas); S->SetIntensity(3.5f); S->SetAttenuationRadius(300.f);
		S->SetInnerConeAngle(30.f); S->SetOuterConeAngle(55.f);
		L = S;
	}
	L->SetLightColor(FLinearColor(1.f, 0.8f, 0.55f));
	L->SetCastShadows(true);
	L->ComponentTags.Add(OwnLight);
	L->ComponentTags.Add(TEXT("HULamp")); // (an office clears these when it re-dresses)
	L->RegisterComponent();
	HUUsables::SetLamp(Shade, L, nullptr, bOn);
}

// ---------- EXIT ----------
void HULamps::ExitSign(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, FVector At, float Yaw)
{
	const FRotator R(0, Yaw, 0);
	const FVector F = R.Vector(), Y = FRotator(0, Yaw + 90.f, 0).Vector(), Z(0, 0, 1);
	auto At2 = [&](float Depth, float U, float V) { return At + F * Depth - Y * U + Z * V; }; // U: left to right as you read it
	Part(Owner, Parent, Out, EShape::Cube, At2(2.5f, 0, 0), FVector(5, 34, 17), FLinearColor(0.06f, 0.06f, 0.06f), R, EHUSurface::PaintedMetal);
	if (UStaticMeshComponent* Face = Part(Owner, Parent, Out, EShape::Cube, At2(5.1f, 0, 0), FVector(0.4f, 31, 14), FLinearColor(0.2f, 0.01f, 0.01f), R))
	{ Glow(Face, FLinearColor(0.18f, 0.01f, 0.008f)); Face->ComponentTags.Add(OwnLight); }
	// the letters, in glowing strokes 1.4 cm wide, 9 tall
	const FLinearColor Red(7.f, 0.35f, 0.2f);
	auto Stroke = [&](float U, float V, float W, float H, float Roll = 0.f)
	{
		if (UStaticMeshComponent* S = Part(Owner, Parent, Out, EShape::Cube, At2(5.6f, U, V), FVector(0.6f, W, H), Red, FRotator(0, Yaw, Roll)))
		{ Glow(S, Red); S->ComponentTags.Add(OwnLight); }
	};
	Stroke(-11.6f, 0, 1.4f, 9.f); Stroke(-9.5f, 3.8f, 5.f, 1.4f); Stroke(-9.8f, 0, 4.2f, 1.3f); Stroke(-9.5f, -3.8f, 5.f, 1.4f); // E
	Stroke(-3.4f, 0, 1.4f, 10.4f, 28.f); Stroke(-3.4f, 0, 1.4f, 10.4f, -28.f);                                       // X
	Stroke(1.9f, 0, 1.4f, 9.f);                                                                                       // I
	Stroke(7.6f, 3.8f, 6.f, 1.4f); Stroke(7.6f, -0.6f, 1.4f, 7.8f);                                                   // T
	UPointLightComponent* L = MakeLight<UPointLightComponent>(Owner, Parent, At2(18.f, 0, -6.f), FRotator::ZeroRotator, 0.22f, FLinearColor(1.f, 0.08f, 0.05f), 230.f, false);
	L->ComponentTags.Add(OwnLight);
}

// ---------- a window with blinds, and the street outside ----------
void HULamps::Window(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, FVector At, float Yaw, float Width, float Height, FLinearColor Street)
{
	const FRotator R(0, Yaw, 0);
	const FVector F = R.Vector(), Y = FRotator(0, Yaw + 90.f, 0).Vector(), Z(0, 0, 1);
	auto P = [&](float D, float U, float V) { return At + F * D + Y * U + Z * V; };
	const FLinearColor Frame(0.7f, 0.68f, 0.62f), Slat(0.78f, 0.76f, 0.68f);
	const float HW = Width / 2, HH = Height / 2;
	// the night through the glass: dark blue, the street's glow low down
	if (UStaticMeshComponent* G = Part(Owner, Parent, Out, EShape::Cube, P(0.5f, 0, 0), FVector(1, Width, Height), FLinearColor(0.02f, 0.03f, 0.05f), R))
	{ Glow(G, FLinearColor(0.012f, 0.018f, 0.035f)); G->ComponentTags.Add(OwnLight); }
	if (UStaticMeshComponent* G = Part(Owner, Parent, Out, EShape::Cube, P(0.8f, 0, -HH * 0.6f), FVector(0.4f, Width, Height * 0.35f), Street * 0.05f, R))
	{ Glow(G, Street * 0.06f); G->ComponentTags.Add(OwnLight); }
	// the frame, a mullion, the sill
	Part(Owner, Parent, Out, EShape::Cube, P(3, 0, HH + 3), FVector(7, Width + 12, 6), Frame, R, EHUSurface::PaintedMetal);
	Part(Owner, Parent, Out, EShape::Cube, P(3, 0, -HH - 2), FVector(7, Width + 12, 4), Frame, R, EHUSurface::PaintedMetal);
	for (float S : { -1.f, 1.f }) Part(Owner, Parent, Out, EShape::Cube, P(3, S * (HW + 3), 0), FVector(7, 6, Height), Frame, R, EHUSurface::PaintedMetal);
	Part(Owner, Parent, Out, EShape::Cube, P(2, 0, 0), FVector(3, 3, Height), Frame, R, EHUSurface::PaintedMetal);
	Part(Owner, Parent, Out, EShape::Cube, P(9, 0, -HH - 5), FVector(16, Width + 18, 3), Frame * 0.9f, R, EHUSurface::Walnut);
	// venetian blinds down to the sill, slats tipped open, their edges catching the street's glow behind them
	Part(Owner, Parent, Out, EShape::Cube, P(9, 0, HH - 1), FVector(6, Width - 2, 4), Slat, R, EHUSurface::PaintedMetal);
	const float Pitch = 4.6f;
	for (float V = HH - 6.f; V > -HH + 1.f; V -= Pitch)
	{
		Part(Owner, Parent, Out, EShape::Cube, P(9, 0, V), FVector(5, Width - 4, 0.35f), Slat, FRotator(-38, Yaw, 0));
		if (UStaticMeshComponent* Edge = Part(Owner, Parent, Out, EShape::Cube, P(7.2f, 0, V + 1.2f), FVector(0.3f, Width - 6, 0.25f), Street * 0.3f, R))
		{ Glow(Edge, Street * 0.22f); Edge->ComponentTags.Add(OwnLight); }
	}
	Part(Owner, Parent, Out, EShape::Cylinder, P(11.5f, HW - 6, -10), FVector(0.4f, 0.4f, Height - 20), Slat * 0.8f, R);
	// THE STREETLIGHT: really six metres out and two and a half up. Its light starts just inside the blinds; a light
	// function (M_HU_BlindsLight, Tools/CreateBlindsLight.py) cuts it to what that far lamp would put through this window
	// and between these slats: a bright window of bars across the floor, the desks, whoever stands in it.
	const FTransform PT = Parent->GetComponentTransform();
	const FVector Far = P(-600.f, 0, 250.f), Blinds = P(9.f, 0, 0);
	const FVector Aim = (Blinds - Far).GetSafeNormal();
	USpotLightComponent* L = MakeLight<USpotLightComponent>(Owner, Parent, P(14.f, 0, 0), PT.InverseTransformVectorNoScale(Aim).Rotation(), 30.f, Street, 1200.f, true);
	L->SetInnerConeAngle(40.f);
	L->SetOuterConeAngle(60.f);
	L->SetSourceRadius(1.f);
	L->ComponentTags.Add(OwnLight);
	static TWeakObjectPtr<UMaterialInterface> Bars;
	static bool bLooked = false;
	if (!bLooked) { bLooked = true; Bars = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_HU_BlindsLight.M_HU_BlindsLight")); }
	if (Bars.IsValid())
	{
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Bars.Get(), L);
		auto V = [&](const TCHAR* Name, FVector X) { D->SetVectorParameterValue(Name, FLinearColor(X.X, X.Y, X.Z, 0.f)); };
		V(TEXT("LP"), PT.TransformPosition(Far));
		V(TEXT("O"), PT.TransformPosition(Blinds));
		V(TEXT("N"), PT.TransformVectorNoScale(F));
		V(TEXT("R"), PT.TransformVectorNoScale(Y));
		V(TEXT("U"), PT.TransformVectorNoScale(Z));
		V(TEXT("Size"), FVector(HW - 2.f, HH - 2.f, 1.5f));
		V(TEXT("Slats"), FVector(Pitch, 0.55f, 0.f));
		L->SetLightFunctionMaterial(D);
		L->SetLightFunctionFadeDistance(4000.f);
		L->SetLightFunctionDisabledBrightness(0.f);
	}
	else L->SetIntensity(6.f); // no bars: just the glow of it
}

// ---------- the flicker and the hum ----------
void UHULampWorld::AddTube(URectLightComponent* L, UPointLightComponent* Up, const TArray<UMaterialInstanceDynamic*>& Glows, FLinearColor GlowColor, float Candela)
{
	FTube T;
	T.Light = L;
	T.Up = Up;
	for (UMaterialInstanceDynamic* G : Glows) T.Glows.Add(G);
	T.GlowColor = GlowColor;
	T.Candela = Candela;
	const FVector P = L->GetComponentLocation();
	T.Seed = HashCombine(GetTypeHash(FMath::RoundToInt(P.X)), GetTypeHash(FMath::RoundToInt(P.Y)));
	T.bTired = T.Seed % 7u == 3u;                          // about one tube in seven is on its way out
	T.NextGlitch = Time + 2.f + (T.Seed % 900u) / 100.f;   // and they don't all go at once
	Tubes.Add(T);
}

namespace
{
	// a second of mains hum: 120 Hz and its harmonics, the ballast's buzz, a little hiss. A whole number of cycles,
	// so it loops without a click.
	TSharedRef<TArray<uint8>, ESPMode::ThreadSafe> HumLoop()
	{
		static TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Loop;
		if (Loop.IsValid()) return Loop.ToSharedRef();
		const int32 Rate = 22050;
		Loop = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
		Loop->SetNumUninitialized(Rate * 2);
		int16* S = (int16*)Loop->GetData();
		FRandomStream Rng(7);
		float Hiss = 0.f;
		for (int32 i = 0; i < Rate; i++)
		{
			const float T = i / (float)Rate, W = 2.f * PI * 120.f * T;
			const float Buzz = FMath::Clamp(FMath::Sin(W) * 4.f, -1.f, 1.f); // a clipped edge: the ballast's buzz
			Hiss = Hiss * 0.85f + Rng.FRandRange(-1.f, 1.f) * 0.15f;
			const float V = 0.42f * FMath::Sin(W) + 0.22f * FMath::Sin(2.f * W + 0.3f) + 0.1f * FMath::Sin(3.f * W + 1.1f) + 0.05f * FMath::Sin(4.f * W) + 0.07f * Buzz + 0.05f * Hiss;
			S[i] = (int16)FMath::Clamp(V * 0.55f * 32767.f, -32767.f, 32767.f);
		}
		return Loop.ToSharedRef();
	}
}

void UHULampWorld::StartHums()
{
	UWorld* W = GetWorld();
	if (!W || !W->IsGameWorld() || W->GetNetMode() == NM_DedicatedServer || HumHolder) return;
	FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
	HumHolder = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SP);
	if (!HumHolder) return;
	USceneComponent* Root = NewObject<USceneComponent>(HumHolder);
	HumHolder->SetRootComponent(Root);
	Root->RegisterComponent();
	const TSharedRef<TArray<uint8>, ESPMode::ThreadSafe> Loop = HumLoop();
	for (int32 i = 0; i < 3; i++)
	{
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(HumHolder);
		Wave->SetSampleRate(22050);
		Wave->NumChannels = 1;
		Wave->Duration = INDEFINITELY_LOOPING_DURATION;
		Wave->bLooping = true;
		Wave->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([Loop](USoundWaveProcedural* Wv, int32) { Wv->QueueAudio(Loop->GetData(), Loop->Num()); });
		UAudioComponent* A = NewObject<UAudioComponent>(HumHolder);
		A->SetupAttachment(Root);
		A->SetUsingAbsoluteLocation(true);
		A->bAutoActivate = false;
		A->bOverrideAttenuation = true;
		A->AttenuationOverrides.bAttenuate = true;
		A->AttenuationOverrides.bSpatialize = true;
		A->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		A->AttenuationOverrides.AttenuationShapeExtents = FVector(60.f, 0.f, 0.f);
		A->AttenuationOverrides.FalloffDistance = 420.f;
		A->bOverridePriority = true;
		A->Priority = 0.1f; // never in the way of a voice
		A->SetSound(Wave);
		A->SetVolumeMultiplier(0.f);
		A->RegisterComponent();
		A->Play();
		Hums.Add(A);
		HumTube.Add(INDEX_NONE);
	}
}

void UHULampWorld::Tick(float Dt)
{
	Time += Dt;
	if (Tubes.Num() == 0) return;
	if (!HumHolder) StartHums();
	for (FTube& T : Tubes)
	{
		URectLightComponent* L = T.Light.Get();
		if (!L || !L->IsVisible()) continue; // gone, or switched off at the wall
		float Want = 1.f;
		if (T.bTired)
		{
			if (T.GlitchEnd < 0.f && Time >= T.NextGlitch) { T.GlitchEnd = Time + FMath::FRandRange(0.35f, 1.6f); T.NextStep = Time; }
			if (T.GlitchEnd >= 0.f)
			{
				if (Time > T.GlitchEnd) { T.GlitchEnd = -1.f; T.NextGlitch = Time + FMath::FRandRange(4.f, 14.f); Want = 1.f; }
				else if (Time >= T.NextStep)
				{
					static const float Steps[] = { 0.04f, 0.04f, 0.3f, 1.f, 1.f, 0.6f };
					Want = Steps[FMath::RandRange(0, 5)];
					T.NextStep = Time + FMath::FRandRange(0.03f, 0.13f);
				}
				else Want = T.Level;
			}
		}
		if (FMath::IsNearlyEqual(Want, T.Level)) continue;
		T.Level = Want;
		L->SetIntensity(T.Candela * Want);
		if (UPointLightComponent* Up = T.Up.Get()) Up->SetIntensity(T.Candela * UpShare * Want);
		for (const TWeakObjectPtr<UMaterialInstanceDynamic>& G : T.Glows) if (G.IsValid()) G->SetVectorParameterValue(TEXT("Color"), T.GlowColor * FMath::Max(0.06f, Want));
	}
	// the hum: the nearest few tubes to whoever's listening, louder from a tired one, crackling as it stutters
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || Hums.Num() == 0) return;
	FVector Ear, Front, Right;
	PC->GetAudioListenerPosition(Ear, Front, Right);
	if ((Reassign -= Dt) <= 0.f)
	{
		Reassign = 0.3f;
		TArray<TPair<float, int32>> Near;
		for (int32 i = 0; i < Tubes.Num(); i++)
			if (const URectLightComponent* L = Tubes[i].Light.Get())
			{
				const float D = FVector::DistSquared(L->GetComponentLocation(), Ear);
				if (D < 900.f * 900.f) Near.Add({ D, i });
			}
		Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
		for (int32 h = 0; h < Hums.Num(); h++)
		{
			HumTube[h] = Near.IsValidIndex(h) ? Near[h].Value : INDEX_NONE;
			if (Tubes.IsValidIndex(HumTube[h]) && Hums[h]) Hums[h]->SetWorldLocation(Tubes[HumTube[h]].Light->GetComponentLocation());
		}
	}
	for (int32 h = 0; h < Hums.Num(); h++)
	{
		if (!Hums[h]) continue;
		float V = 0.f;
		if (Tubes.IsValidIndex(HumTube[h]))
		{
			const FTube& T = Tubes[HumTube[h]];
			const URectLightComponent* L = T.Light.Get();
			if (L && L->IsVisible()) V = (T.bTired ? 0.2f : 0.1f) * (T.GlitchEnd >= 0.f ? FMath::FRandRange(0.3f, 1.8f) : 1.f);
		}
		Hums[h]->SetVolumeMultiplier(V);
	}
}

// -HULampShots on L_Precinct: walk the camera round the precinct's lamps and screenshot each (Saved/Screenshots/.../Lamps_*),
// log the tubes and their stutters, then quit
namespace
{
	struct FLampTour { TWeakObjectPtr<AHUSquadRoom> Room; TWeakObjectPtr<AHUCrimeScene> Scene; float Clock = -1.f; int32 Glitches = 0; };
	FLampTour GTour;
	bool LampTourStep(float Dt)
	{
		UWorld* W = nullptr;
		if (GEngine) for (const FWorldContext& C : GEngine->GetWorldContexts()) if (C.World() && C.World()->IsGameWorld()) W = C.World();
		if (!W) return true;
		APlayerController* PC = W->GetFirstPlayerController();
		APawn* P = PC ? PC->GetPawn() : nullptr;
		if (!GTour.Room.IsValid()) for (TActorIterator<AHUSquadRoom> It(W); It; ++It) if (It->bPrecinct) GTour.Room = *It;
		if (!GTour.Scene.IsValid()) for (TActorIterator<AHUCrimeScene> It(W); It; ++It) GTour.Scene = *It;
		if ((!GTour.Room.IsValid() && !GTour.Scene.IsValid()) || !P) return true;
		const bool bScene = !GTour.Room.IsValid();
		const FTransform RT = bScene ? GTour.Scene->GetActorTransform() : GTour.Room->GetActorTransform();
		const float Was = GTour.Clock < 0.f ? 0.f : GTour.Clock;
		GTour.Clock = Was + Dt;
		auto At = [&](float T) { return Was < T && GTour.Clock >= T; };
		struct FStop { FVector From, To; const TCHAR* Name; };
		// at a scene: three metres inside the front door, looking in, then back at the door (EXIT, the windows)
		const FVector PS = bScene ? GTour.Scene->PlayerStart : FVector::ZeroVector, In = PS + FVector(770, -260, 65);
		const FStop SceneStops[] = {
			{ In, In + FVector(800, 0, -40), TEXT("SceneInside") },
			{ In + FVector(250, 200, 0), In + FVector(-300, 0, 40), TEXT("SceneDoor") },
		};
		static const FStop PrecinctStops[] = {
			{ FVector(-1720, 260, 165), FVector(-2400, -260, 190), TEXT("Lobby") },
			{ FVector(-700, -230, 170), FVector(-760, -1250, 40), TEXT("Bullpen") },
			{ FVector(-1050, -1080, 165), FVector(-1250, -1450, 0), TEXT("BullpenWindow") },
			{ FVector(-250, 980, 165), FVector(-640, 1560, 110), TEXT("Captain") },
			{ FVector(1725, 205, 165), FVector(1725, 430, 110), TEXT("Interview") },
			{ FVector(-1500, 40, 165), FVector(2400, 0, 220), TEXT("Corridor") },
			{ FVector(1450, -250, 165), FVector(1650, -1500, 60), TEXT("Records") },
			{ FVector(-1000, -900, 160), FVector(-1350, -1060, 76), TEXT("DeskLamp") },
		};
		if (UHULampWorld* S = W->GetSubsystem<UHULampWorld>())
		{
			int32 Tired = 0; for (const UHULampWorld::FTube& T : S->Tubes) { Tired += T.bTired; if (T.bTired && T.GlitchEnd >= 0.f && T.NextStep == T.GlitchEnd) {} }
			if (At(1.f)) UE_LOG(LogTemp, Display, TEXT("HULampShots: %d tubes, %d tired"), S->Tubes.Num(), Tired);
			for (const UHULampWorld::FTube& T : S->Tubes) if (T.bTired && T.GlitchEnd >= 0.f && T.Level < 0.5f) { GTour.Glitches++; break; }
		}
		const FStop* Stops = bScene ? SceneStops : PrecinctStops;
		const int32 NumStops = bScene ? UE_ARRAY_COUNT(SceneStops) : UE_ARRAY_COUNT(PrecinctStops);
		const FString Prefix = bScene ? FString::Printf(TEXT("Lamps_%s_"), *GTour.Scene->PlaceName.Replace(TEXT(" "), TEXT(""))) : FString(TEXT("Lamps_"));
		for (int32 i = 0; i < NumStops; i++)
		{
			const float T0 = (bScene ? 12.f : 6.f) + i * 2.5f;
			if (At(T0))
			{
				const FVector F = RT.TransformPosition(Stops[i].From), To = RT.TransformPosition(Stops[i].To);
				P->SetActorLocation(F - FVector(0, 0, 64.f), false, nullptr, ETeleportType::TeleportPhysics);
				PC->SetControlRotation((To - F).Rotation());
			}
			if (At(T0 + 1.8f)) FScreenshotRequest::RequestScreenshot(Prefix + Stops[i].Name, false, false);
		}
		if (At((bScene ? 12.f : 6.f) + NumStops * 2.5f + 1.f))
		{
			UE_LOG(LogTemp, Display, TEXT("HULampShots: done - a tired tube was mid-stutter in %d frames"), GTour.Glitches);
			FPlatformMisc::RequestExit(false);
			return false;
		}
		return true;
	}
	FDelayedAutoRegisterHelper GLampTour(EDelayedRegisterRunPhase::EndOfEngineInit, []()
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("HULampShots"))) FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&LampTourStep), 0.f);
	});
}

void UHULampWorld::Deinitialize()
{
	for (UAudioComponent* A : Hums) if (A) A->Stop();
	Hums.Reset();
	if (HumHolder) HumHolder->Destroy();
	HumHolder = nullptr;
	Tubes.Reset();
	Super::Deinitialize();
}
