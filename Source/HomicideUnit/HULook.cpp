#include "HULook.h"
#include "HUBuild.h"
#include "HUPeople.h"
#include "Components/StaticMeshComponent.h"

namespace
{
	const TCHAR* Builds[] = { TEXT("Slim"), TEXT("Average"), TEXT("Broad"), TEXT("Heavy") };
	const float BuildW[] = { 0.85f, 1.f, 1.15f, 1.3f };
	const TCHAR* Skins[] = { TEXT("Fair"), TEXT("Light"), TEXT("Medium"), TEXT("Olive"), TEXT("Brown"), TEXT("Dark") };
	const FLinearColor SkinCol[] = { {0.95f, 0.8f, 0.68f}, {0.87f, 0.68f, 0.53f}, {0.72f, 0.52f, 0.38f}, {0.6f, 0.45f, 0.3f}, {0.42f, 0.28f, 0.18f}, {0.26f, 0.17f, 0.11f} };
	const TCHAR* Coats[] = { TEXT("Tan"), TEXT("Charcoal"), TEXT("Navy"), TEXT("Black"), TEXT("Brown") };
	const FLinearColor CoatCol[] = { {0.55f, 0.45f, 0.3f}, {0.12f, 0.12f, 0.13f}, {0.08f, 0.1f, 0.2f}, {0.04f, 0.035f, 0.03f}, {0.3f, 0.2f, 0.12f} };
}

const TCHAR* HULook::FieldName(int32 F)
{
	static const TCHAR* Names[] = { TEXT("Model"), TEXT("Build"), TEXT("Height"), TEXT("Hand skin"), TEXT("Sleeve") };
	return F >= 0 && F < Fields ? Names[F] : TEXT("");
}
int32 HULook::Options(int32 F)
{
	static const int32 Counts[] = { HUPeople::CastCount, 4, 8, 6, 5 };
	return F >= 0 && F < Fields ? Counts[F] : 1;
}
float HULook::HeightCm(const FHULook& L) { return 165.f + 5.f * FMath::Min<int32>(L.Height, 7); }
FString HULook::ValueName(const FHULook& L, int32 F)
{
	switch (F)
	{
	case 0: return HUPeople::CastFace(L.Model);
	case 1: return Builds[FMath::Min<int32>(L.Build, 3)];
	case 2: return FString::Printf(TEXT("%d cm"), FMath::RoundToInt(HeightCm(L)));
	case 3: return Skins[FMath::Min<int32>(L.Skin, 5)];
	case 4: return Coats[FMath::Min<int32>(L.Coat, 4)];
	default: return FString();
	}
}
void HULook::Step(FHULook& L, int32 F, int32 Dir)
{
	if (F < 0 || F >= Fields) return;
	uint8* Values[] = { &L.Model, &L.Build, &L.Height, &L.Skin, &L.Coat };
	const int32 Count = Options(F);
	*Values[F] = (uint8)(((FMath::Clamp<int32>(*Values[F], 0, Count - 1) + Dir) % Count + Count) % Count);
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
