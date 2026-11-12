#include "HUBuild.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"

namespace HUBuild
{
	static UStaticMesh* Mesh(EShape S)
	{
		static const TCHAR* Paths[] = { TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("/Engine/BasicShapes/Sphere.Sphere") };
		return LoadObject<UStaticMesh>(nullptr, Paths[(int32)S]);
	}

	UStaticMeshComponent* Block(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out,
		EShape Shape, FVector Pos, FVector SizeCm, FLinearColor Color, FRotator Rot, bool bCollide)
	{
		UStaticMesh* M = Mesh(Shape);
		if (!M || !Owner) return nullptr;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(M);
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(Pos);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(SizeCm / 100.f);
		if (!bCollide) C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		static UMaterialInterface* Base = nullptr;
		if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (Base)
		{
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, C);
			D->SetVectorParameterValue(TEXT("Color"), Color);
			C->SetMaterial(0, D);
		}
		C->RegisterComponent();
		Out.Add(C);
		return C;
	}

	void Clear(TArray<UStaticMeshComponent*>& Out)
	{
		for (UStaticMeshComponent* C : Out) if (C) C->DestroyComponent();
		Out.Reset();
	}

	UMaterialInterface* PropSurface(AActor* Owner, UMaterialInterface* Parent,
		const TCHAR* BaseColorPath, FLinearColor Tint, float TileSizeCm, float RoughnessScale)
	{
		if (!Owner || !Parent || !BaseColorPath) return nullptr;
		const FString BasePath(BaseColorPath);
		const FString NormalPath = BasePath.Replace(TEXT("_BaseColor"), TEXT("_Normal"));
		const FString RoughnessPath = BasePath.Replace(TEXT("_BaseColor"), TEXT("_Roughness"));
		UTexture* BaseColor = LoadObject<UTexture>(nullptr, *BasePath);
		UTexture* Normal = LoadObject<UTexture>(nullptr, *NormalPath);
		UTexture* Roughness = LoadObject<UTexture>(nullptr, *RoughnessPath);
		if (!BaseColor || !Normal || !Roughness) return nullptr;
		UMaterialInstanceDynamic* Surface = UMaterialInstanceDynamic::Create(Parent, Owner);
		if (!Surface) return nullptr;
		Surface->SetTextureParameterValue(TEXT("BaseColor"), BaseColor);
		Surface->SetTextureParameterValue(TEXT("NormalMap"), Normal);
		Surface->SetTextureParameterValue(TEXT("RoughnessMap"), Roughness);
		Surface->SetScalarParameterValue(TEXT("TileSizeCm"), TileSizeCm);
		Surface->SetScalarParameterValue(TEXT("RoughnessScale"), RoughnessScale);
		Surface->SetVectorParameterValue(TEXT("SurfaceTint"), Tint);
		return Surface;
	}

	void WallWithDoor(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out,
		bool bAlongY, FVector Center, float Length, float Height, float Thick, float DoorAt, float DoorW, float DoorH, FLinearColor Color)
	{
		const float Lo = -Length / 2, Hi = Length / 2, D0 = DoorAt - DoorW / 2, D1 = DoorAt + DoorW / 2;
		auto Seg = [&](float A, float B, float Z0, float Z1)
		{
			if (B - A < 1.f || Z1 - Z0 < 1.f) return;
			const float Mid = (A + B) / 2, Len = B - A;
			const FVector P = bAlongY ? Center + FVector(0, Mid, (Z0 + Z1) / 2 - Height / 2) : Center + FVector(Mid, 0, (Z0 + Z1) / 2 - Height / 2);
			const FVector S = bAlongY ? FVector(Thick, Len, Z1 - Z0) : FVector(Len, Thick, Z1 - Z0);
			Block(Owner, Parent, Out, EShape::Cube, P, S, Color);
		};
		Seg(Lo, D0, 0, Height);
		Seg(D1, Hi, 0, Height);
		Seg(D0, D1, DoorH, Height); // above the door
	}
}
