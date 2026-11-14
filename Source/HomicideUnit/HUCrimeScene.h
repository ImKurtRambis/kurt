// The crime scene. Drop one in L_Scene and press Play: it reads the case and builds a location to match.
//
// Every place has its own theme (a diner has a storeroom loft and a grease vent, the stables a hayloft and
// a drainage culvert, the bank a manager's gallery and a vault vent...), and the case number shuffles the
// layout: which side the stairs and ladder are on, where the crawlspace opens, which furniture hides what.
//
// Every scene has the same kinds of hard-to-reach places, and the case's evidence is spread across them:
//   STAIRS up to a loft / balcony           - evidence up top
//   a LADDER up a tall cabinet or rack       - evidence on top
//   a NARROW PASSAGE between partitions      - evidence at the dead end
//   a CRAWLSPACE through the far wall        - crouch through it to a back room with more evidence
//   heavy FURNITURE to push and pull         - evidence hidden underneath
// Suspects stand around the main floor.
//
// Fab / Quixel: set the mesh and material slots below and the scene uses them instead of plain blocks.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "HUSurfaces.h"
#include "HUCrimeScene.generated.h"

class AHUEvidence;

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUCrimeScene : public AActor
{
	GENERATED_BODY()
public:
	AHUCrimeScene();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) class UTextRenderComponent* Preview;

	// -1: use the case in play (or a random one if you open this level directly)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene") int32 ForceCase = -1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene") bool bPlacePlayer = true;

	// ---- Fab / Quixel slots (all optional). Empty: each place uses the surface library (HUSurfaces.h) - carpet in the
	// hotel, tile in the hospital, brick and concrete in the warehouse...; set a material here to override it ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fab Assets") UMaterialInterface* FloorMaterial = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fab Assets") UMaterialInterface* WallMaterial = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fab Assets") UMaterialInterface* WoodMaterial = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fab Assets") UStaticMesh* FurnitureMesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fab Assets") UStaticMesh* EvidenceMesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fab Assets") UStaticMesh* LadderMesh = nullptr;

	// what got built, for Blueprints / the HUD
	UPROPERTY(BlueprintReadOnly, Category = "Scene") FString PlaceName;
	UPROPERTY(BlueprintReadOnly, Category = "Scene") FString LoftName;
	UPROPERTY(BlueprintReadOnly, Category = "Scene") FString HighName;
	UPROPERTY(BlueprintReadOnly, Category = "Scene") FString CrawlName;
	UPROPERTY(BlueprintReadOnly, Category = "Scene") FString BackName;
	// the generated floor plan's style ("an open hall", "a warren of small rooms"...)
	UPROPERTY(BlueprintReadOnly, Category = "Scene") FString LayoutName;
	UPROPERTY(BlueprintReadOnly, Category = "Scene") FVector PlayerStart = FVector(0, 0, 100);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
	UPROPERTY() class UBoxComponent* CrawlZone = nullptr;
	bool bPlayerReady = false;
	FString Arrival;

	void Build();
	// spawn with a fixed name (class, case, count) so every player's copy has the same name - squad interactions find things by it
	TMap<UClass*, int32> SpawnCounts;
	template <class T> T* SpawnNamed(const FTransform& Where)
	{
		FActorSpawnParameters P;
		const int32 N = SpawnCounts.FindOrAdd(T::StaticClass())++;
		P.Name = FName(*FString::Printf(TEXT("HU_%s_%d"), *T::StaticClass()->GetName(), N));
		P.bDeferConstruction = true;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return GetWorld()->SpawnActor<T>(T::StaticClass(), Where, P);
	}
	UStaticMeshComponent* Box(FVector Pos, FVector Size, FLinearColor Color, UMaterialInterface* Override = nullptr, bool bCollide = true);
	// a box made of a surface (tinted to Color); a designer's Floor/Wall/Wood material, if set, wins for those roles
	UStaticMeshComponent* Box(FVector Pos, FVector Size, FLinearColor Color, EHUSurface Surface, bool bCollide = true);
	// what this place is made of (set from the place at the start of Build)
	EHUSurface FloorS, WallS, WoodS, CeilingS;
	void Sign(const FString& Text, FVector Pos, float Yaw, float Size, FColor Color);

	UFUNCTION() void OnCrawlEnter(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Body, bool bSweep, const FHitResult& Hit);
	UFUNCTION() void OnCrawlLeave(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Body);
};
