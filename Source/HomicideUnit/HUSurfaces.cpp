#include "HUSurfaces.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PrimitiveComponent.h"

namespace
{
	struct FInfo { const TCHAR* Name; FLinearColor Avg; bool bKeepHue; };
	// Avg: each base colour map's average in linear colour (Content/Art/Textures/Source/surface_averages.json for the
	// generated ones; plaster, linoleum and walnut measured the same way). Re-run make_surfaces.py -> update these.
	const FInfo Info[] = {
		{ TEXT("None"),         FLinearColor(1.f, 1.f, 1.f),         false },
		{ TEXT("Plaster"),      FLinearColor(0.420f, 0.442f, 0.326f), false },
		{ TEXT("Linoleum"),     FLinearColor(0.214f, 0.186f, 0.137f), false },
		{ TEXT("Walnut"),       FLinearColor(0.072f, 0.019f, 0.005f), false }, // so red that brightening it turns it orange: take the colour
		{ TEXT("Brick"),        FLinearColor(0.295f, 0.129f, 0.093f), true },
		{ TEXT("Concrete"),     FLinearColor(0.310f, 0.301f, 0.271f), false },
		{ TEXT("Asphalt"),      FLinearColor(0.054f, 0.052f, 0.051f), true },
		{ TEXT("Tile"),         FLinearColor(0.684f, 0.670f, 0.617f), false },
		{ TEXT("Carpet"),       FLinearColor(0.124f, 0.033f, 0.014f), true },
		{ TEXT("WoodFloor"),    FLinearColor(0.253f, 0.110f, 0.037f), true },
		{ TEXT("CeilingTile"),  FLinearColor(0.687f, 0.668f, 0.597f), false },
		{ TEXT("PaintedMetal"), FLinearColor(0.506f, 0.506f, 0.474f), false },
		{ TEXT("Glass"),        FLinearColor(0.531f, 0.633f, 0.606f), false },
		{ TEXT("Fabric"),       FLinearColor(0.329f, 0.288f, 0.228f), false },
		{ TEXT("Leather"),      FLinearColor(0.074f, 0.028f, 0.013f), true },
	};
	static_assert(UE_ARRAY_COUNT(Info) == (int32)EHUSurface::Count, "one entry per surface");

	// loaded on first use; weak, so a level change can let them go (they're loaded again next time)
	TWeakObjectPtr<UMaterialInterface> Loaded[(int32)EHUSurface::Count];
	bool Missing[(int32)EHUSurface::Count] = {};
}

const TCHAR* HUSurfaces::Name(EHUSurface S) { return Info[FMath::Clamp((int32)S, 0, (int32)EHUSurface::Count - 1)].Name; }

UMaterialInterface* HUSurfaces::Material(EHUSurface S)
{
	const int32 I = (int32)S;
	if (S == EHUSurface::None || I >= (int32)EHUSurface::Count || Missing[I]) return nullptr;
	if (UMaterialInterface* M = Loaded[I].Get()) return M;
	const FString Path = FString::Printf(TEXT("/Game/Art/Materials/Surfaces/MI_Surface_%s.MI_Surface_%s"), Info[I].Name, Info[I].Name);
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *Path);
	if (!M) { Missing[I] = true; UE_LOG(LogTemp, Warning, TEXT("HomicideUnit: surface %s isn't in the project (run Tools/Surfaces/ImportSurfaces.py) - flat colour instead"), Info[I].Name); }
	Loaded[I] = M;
	return M;
}

FLinearColor HUSurfaces::Natural(EHUSurface S) { return Info[FMath::Clamp((int32)S, 0, (int32)EHUSurface::Count - 1)].Avg; }

bool HUSurfaces::Apply(UPrimitiveComponent* C, EHUSurface S, const FLinearColor& Want)
{
	UMaterialInterface* M = Material(S);
	if (!C || !M) return false;
	const FInfo& In = Info[(int32)S];
	FLinearColor Tint;
	if (In.bKeepHue)
	{
		const float L = FMath::Clamp(Want.GetLuminance() / FMath::Max(0.001f, In.Avg.GetLuminance()), 0.f, 4.f);
		Tint = FLinearColor(L, L, L);
	}
	else Tint = FLinearColor(FMath::Clamp(Want.R / FMath::Max(0.001f, In.Avg.R), 0.f, 4.f), FMath::Clamp(Want.G / FMath::Max(0.001f, In.Avg.G), 0.f, 4.f),
		FMath::Clamp(Want.B / FMath::Max(0.001f, In.Avg.B), 0.f, 4.f));
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, C);
	D->SetVectorParameterValue(TEXT("Tint"), Tint);
	C->SetMaterial(0, D);
	return true;
}
