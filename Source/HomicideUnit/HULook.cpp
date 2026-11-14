#include "HULook.h"
#include "HUBuild.h"
#include "Components/StaticMeshComponent.h"

namespace
{
	const TCHAR* Builds[] = { TEXT("Slim"), TEXT("Average"), TEXT("Broad"), TEXT("Heavy") };
	const float BuildW[] = { 0.85f, 1.f, 1.15f, 1.3f };
	const TCHAR* Skins[] = { TEXT("Fair"), TEXT("Light"), TEXT("Medium"), TEXT("Olive"), TEXT("Brown"), TEXT("Dark") };
	const FLinearColor SkinCol[] = { {0.95f, 0.8f, 0.68f}, {0.87f, 0.68f, 0.53f}, {0.72f, 0.52f, 0.38f}, {0.6f, 0.45f, 0.3f}, {0.42f, 0.28f, 0.18f}, {0.26f, 0.17f, 0.11f} };
	const TCHAR* Hairs[] = { TEXT("Bald"), TEXT("Short"), TEXT("Slicked back"), TEXT("Long"), TEXT("Curly") };
	const TCHAR* HairColors[] = { TEXT("Black"), TEXT("Brown"), TEXT("Blond"), TEXT("Red"), TEXT("Grey") };
	const FLinearColor HairCol[] = { {0.03f, 0.025f, 0.02f}, {0.2f, 0.12f, 0.06f}, {0.75f, 0.6f, 0.3f}, {0.5f, 0.17f, 0.06f}, {0.55f, 0.55f, 0.55f} };
	const TCHAR* Coats[] = { TEXT("Tan trench coat"), TEXT("Charcoal suit"), TEXT("Navy overcoat"), TEXT("Black leather"), TEXT("Brown tweed") };
	const FLinearColor CoatCol[] = { {0.55f, 0.45f, 0.3f}, {0.12f, 0.12f, 0.13f}, {0.08f, 0.1f, 0.2f}, {0.04f, 0.035f, 0.03f}, {0.3f, 0.2f, 0.12f} };
	const TCHAR* Hats[] = { TEXT("No hat"), TEXT("Fedora"), TEXT("Flat cap") };
}

const TCHAR* HULook::FieldName(int32 F)
{
	static const TCHAR* N[] = { TEXT("Build"), TEXT("Height"), TEXT("Skin"), TEXT("Hair"), TEXT("Hair colour"), TEXT("Coat"), TEXT("Hat") };
	return F >= 0 && F < Fields ? N[F] : TEXT("");
}

int32 HULook::Options(int32 F)
{
	static const int32 N[] = { 4, 8, 6, 5, 5, 5, 3 };
	return F >= 0 && F < Fields ? N[F] : 1;
}

float HULook::HeightCm(const FHULook& L) { return 165.f + 5.f * FMath::Min<int32>(L.Height, 7); }

static uint8 Field(const FHULook& L, int32 F)
{
	switch (F) { case 0: return L.Build; case 1: return L.Height; case 2: return L.Skin; case 3: return L.Hair; case 4: return L.HairColor; case 5: return L.Coat; default: return L.Hat; }
}

FString HULook::ValueName(const FHULook& L, int32 F)
{
	const int32 V = FMath::Clamp<int32>(Field(L, F), 0, Options(F) - 1);
	switch (F)
	{
	case 0: return Builds[V];
	case 1: return FString::Printf(TEXT("%d cm"), FMath::RoundToInt(HeightCm(L)));
	case 2: return Skins[V];
	case 3: return Hairs[V];
	case 4: return HairColors[V];
	case 5: return Coats[V];
	default: return Hats[V];
	}
}

void HULook::Step(FHULook& L, int32 F, int32 Dir)
{
	const int32 N = Options(F);
	uint8* P[] = { &L.Build, &L.Height, &L.Skin, &L.Hair, &L.HairColor, &L.Coat, &L.Hat };
	if (F < 0 || F >= Fields) return;
	*P[F] = (uint8)((FMath::Clamp<int32>(*P[F], 0, N - 1) + Dir + N) % N);
}

float HULook::SleeveThickness(const FHULook& L) { return 12.f * BuildW[FMath::Min<int32>(L.Build, 3)]; } // as thick as the body's arms

void HULook::BuildArm(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, const FHULook& L,
	const FTransform& Xf, float SleeveLen, bool bLeft, EHand Shape, ESeen Seen, FVector ForearmDir, float Thickness)
{
	using namespace HUBuild;
	const FLinearColor Skin = SkinCol[FMath::Min<int32>(L.Skin, 5)], Coat = CoatCol[FMath::Min<int32>(L.Coat, 4)], Shirt(0.75f, 0.74f, 0.7f);
	const float T = SleeveThickness(L) * Thickness;
	const float Th = bLeft ? 1.f : -1.f; // the thumb's side
	auto Put = [&](const FTransform& F, FVector Pos, FVector Size, FLinearColor Col, FRotator Rot)
	{
		UStaticMeshComponent* M = Block(Owner, Parent, Out, EShape::Cube, F.TransformPosition(Pos), Size, Col, (F.GetRotation() * Rot.Quaternion()).Rotator(), false);
		if (!M) return;
		if (Seen == ESeen::OnlyOwner) { M->SetOnlyOwnerSee(true); M->SetCastShadow(false); } // a first-person arm
		else { M->SetOwnerNoSee(Seen == ESeen::NotByOwner); M->SetCastShadow(true); }
	};
	auto P = [&](FVector Pos, FVector Size, FLinearColor Col, FRotator Rot = FRotator::ZeroRotator) { Put(Xf, Pos, Size, Col, Rot); };
	// the coat sleeve, and the shirt cuff showing at the wrist: straight back from the hand, or bent at the wrist
	const FTransform Fore = ForearmDir.IsNearlyZero() ? Xf
		: FTransform(FRotationMatrix::MakeFromXZ(-ForearmDir.GetSafeNormal(), Xf.GetRotation().GetUpVector()).ToQuat(), Xf.GetLocation());
	Put(Fore, FVector(-SleeveLen / 2 - 2.5f, 0, 0), FVector(SleeveLen, T, T), Coat, FRotator::ZeroRotator);
	Put(Fore, FVector(-1.6f, 0, 0), FVector(3.2f, T * 0.8f, T * 0.8f), Shirt, FRotator::ZeroRotator);
	// the hand
	switch (Shape)
	{
	case EHand::Fist: // closed round a handle that runs through it along X (at z -1.5)
		P(FVector(4.3f, 0, -1.5f), FVector(8.6f, 8.6f, 7.4f), Skin);
		P(FVector(8.1f, 0, 0.6f), FVector(2.2f, 8.2f, 3.2f), Skin * 0.92f);                   // the knuckles
		P(FVector(6.2f, Th * 2.6f, 2.6f), FVector(4.6f, 3.2f, 2.4f), Skin, FRotator(0, -Th * 20, 0)); // the thumb, over the top
		break;
	case EHand::Flat: // palm out, fingers straight, thumb spread
		P(FVector(4.f, 0, 0), FVector(8.f, 8.6f, 3.f), Skin);
		P(FVector(11.f, 0, 0), FVector(6.f, 8.f, 2.6f), Skin);
		P(FVector(3.5f, Th * 5.6f, 0), FVector(5.f, 2.4f, 2.4f), Skin, FRotator(0, Th * 30, 0));
		break;
	case EHand::Pinch: // fingers loosely forward, thumb meeting them
		P(FVector(4.f, 0, 0), FVector(8.f, 8.6f, 3.4f), Skin);
		P(FVector(10.f, -Th * 0.6f, -1.f), FVector(5.f, 7.2f, 3.f), Skin, FRotator(-12, 0, 0));
		P(FVector(8.2f, Th * 3.4f, -1.6f), FVector(5.f, 2.3f, 2.3f), Skin, FRotator(0, -Th * 25, 0));
		break;
	default: // relaxed: fingers curled a little, thumb alongside
		P(FVector(4.f, 0, 0), FVector(8.f, 8.6f, 3.6f), Skin);
		P(FVector(10.2f, 0, -1.4f), FVector(5.f, 8.f, 3.f), Skin, FRotator(-25, 0, 0));
		P(FVector(3.6f, Th * 5.2f, -1.2f), FVector(5.f, 2.4f, 2.4f), Skin, FRotator(0, Th * 12, 0));
		break;
	}
}

void HULook::BuildFigure(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, const FHULook& L, bool bOwnerNoSee)
{
	using namespace HUBuild;
	Clear(Out);
	const float W = BuildW[FMath::Min<int32>(L.Build, 3)], S = HeightCm(L) / 180.f;
	const FLinearColor Skin = SkinCol[FMath::Min<int32>(L.Skin, 5)], Hair = HairCol[FMath::Min<int32>(L.HairColor, 4)], Coat = CoatCol[FMath::Min<int32>(L.Coat, 4)];
	const FLinearColor Trousers(0.06f, 0.06f, 0.07f), Shoe(0.02f, 0.02f, 0.02f), Shirt(0.75f, 0.74f, 0.7f);
	auto P = [&](EShape Sh, FVector Pos, FVector Size, FLinearColor Col)
	{
		UStaticMeshComponent* M = Block(Owner, Parent, Out, Sh, Pos, Size, Col, FRotator::ZeroRotator, false);
		if (M) { M->SetOwnerNoSee(bOwnerNoSee); M->SetCastShadow(true); }
	};
	// legs and shoes
	for (float Sd : { -1.f, 1.f })
	{
		P(EShape::Cube, FVector(0, Sd * 10 * W, 44 * S), FVector(16 * W, 15 * W, 80 * S), Trousers);
		P(EShape::Cube, FVector(5, Sd * 10 * W, 4), FVector(28, 13 * W, 8), Shoe);
	}
	// the coat: body, and the skirt of it over the thighs
	P(EShape::Cube, FVector(0, 0, 116 * S), FVector(28 * W, 46 * W, 62 * S), Coat);
	P(EShape::Cube, FVector(0, 0, 72 * S), FVector(30 * W, 44 * W, 30 * S), Coat * 0.95f);
	P(EShape::Cube, FVector(14.5f * W, 0, 138 * S), FVector(1, 10, 16 * S), Shirt); // shirt collar at the front
	// arms and hands
	for (float Sd : { -1.f, 1.f })
	{
		P(EShape::Cube, FVector(0, Sd * (23 * W + 7), 115 * S), FVector(12 * W, 12 * W, 58 * S), Coat);
		P(EShape::Cube, FVector(0, Sd * (23 * W + 7), 82 * S), FVector(9, 9, 10), Skin);
	}
	// neck and head
	P(EShape::Cylinder, FVector(0, 0, 151 * S), FVector(11, 11, 10), Skin);
	const float HZ = 165 * S;
	P(EShape::Sphere, FVector(0, 0, HZ), FVector(21, 19, 25), Skin);
	// hair
	switch (L.Hair)
	{
	case 1: P(EShape::Sphere, FVector(-1.5f, 0, HZ + 4), FVector(22, 20, 21), Hair); break;                                      // short
	case 2: P(EShape::Sphere, FVector(-3, 0, HZ + 5), FVector(22, 19.5f, 18), Hair); P(EShape::Cube, FVector(-9, 0, HZ - 2), FVector(6, 16, 10), Hair); break; // slicked back
	case 3: P(EShape::Sphere, FVector(-1.5f, 0, HZ + 4), FVector(22, 21, 21), Hair); P(EShape::Cube, FVector(-8, 0, HZ - 12), FVector(8, 21, 26), Hair); break; // long
	case 4: P(EShape::Sphere, FVector(-1, 0, HZ + 6), FVector(26, 25, 22), Hair); break;                                         // curly
	default: break;                                                                                                              // bald
	}
	// hat
	if (L.Hat == 1) // fedora
	{
		P(EShape::Cylinder, FVector(0, 0, HZ + 10), FVector(36, 34, 1.5f), Coat * 0.5f);
		P(EShape::Cylinder, FVector(0, 0, HZ + 16), FVector(21, 20, 12), Coat * 0.5f);
		P(EShape::Cylinder, FVector(0, 0, HZ + 12), FVector(21.5f, 20.5f, 2.5f), FLinearColor(0.02f, 0.02f, 0.02f)); // the band
	}
	else if (L.Hat == 2) // flat cap
	{
		P(EShape::Sphere, FVector(-1, 0, HZ + 10), FVector(24, 22, 9), Coat * 0.7f);
		P(EShape::Cube, FVector(11, 0, HZ + 8), FVector(9, 18, 1.5f), Coat * 0.7f);
	}
}
