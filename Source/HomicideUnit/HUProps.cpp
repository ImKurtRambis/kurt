#include "HUProps.h"
#include "HUUsables.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"

namespace HUBuild
{
	UStaticMesh* PropMesh(const TCHAR* Name)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("HUNoProps")); // the old blocks, for comparison
		if (bOff) return nullptr;
		static TMap<FString, TWeakObjectPtr<UStaticMesh>> Found;
		static TSet<FString> Missing;
		const FString Key(Name);
		if (const TWeakObjectPtr<UStaticMesh>* M = Found.Find(Key)) if (M->IsValid()) return M->Get();
		if (Missing.Contains(Key)) return nullptr;
		const FString Path = FString::Printf(TEXT("/Game/Art/Props/SM_%s.SM_%s"), Name, Name);
		// asked quietly: a build without the art just keeps its blocks, no warning per piece of furniture
		UStaticMesh* M = FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)) ? LoadObject<UStaticMesh>(nullptr, *Path) : nullptr;
		if (M) Found.Add(Key, M); else Missing.Add(Key);
		return M;
	}

	UStaticMeshComponent* Prop(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out,
		const TCHAR* Name, FVector Center, FVector SizeCm, FRotator Rot, bool bCollide, const TCHAR* FitAs)
	{
		UStaticMesh* M = Owner ? PropMesh(Name) : nullptr;
		UStaticMesh* Ref = FitAs ? PropMesh(FitAs) : M;
		if (!M || !Ref) return nullptr;
		// fit the mesh's bounds into the blocks' box, axis by axis in the prop's own frame (before Rot). An axis given
		// as 0 keeps the prop's proportions: it takes the average scale of the others (all 0 = real size).
		const FBox B = Ref->GetBoundingBox();
		const FVector Ext = B.GetSize();
		FVector Scale(1.f);
		float Sum = 0.f; int32 N = 0;
		for (int32 a = 0; a < 3; a++)
			if (SizeCm[a] > 0.f && Ext[a] > 0.1) { Scale[a] = SizeCm[a] / Ext[a]; Sum += Scale[a]; N++; }
		for (int32 a = 0; a < 3; a++)
			if (!(SizeCm[a] > 0.f && Ext[a] > 0.1)) Scale[a] = N ? Sum / N : 1.f;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(M);
		C->SetupAttachment(Parent);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeRotation(Rot);
		C->SetRelativeLocation(Center - Rot.RotateVector(B.GetCenter() * Scale)); // the bounds' centre lands on Center
		if (!bCollide) C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		Out.Add(C);
		HUUsables::MarkProp(C, Name); // a desk has drawers, a phone a dial tone (HUUsables.h)
		return C;
	}
}
