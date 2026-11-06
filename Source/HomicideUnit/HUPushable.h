// Heavy furniture you can grab and push or pull across the floor (E to grab, E again to let go).
// Evidence can be hidden underneath: it appears once the piece has been moved off it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.h"
#include "HUPushable.generated.h"

class AHUEvidence;

UCLASS(Blueprintable)
class HOMICIDEUNIT_API AHUPushable : public AActor, public IHUInteractable
{
	GENERATED_BODY()
public:
	AHUPushable();

	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Body;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pushable") FString Label = TEXT("Crate");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pushable") FVector SizeCm = FVector(120, 120, 100);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pushable") FLinearColor Color = FLinearColor(0.35f, 0.22f, 0.1f);
	// optional: a real mesh from Fab, fitted to SizeCm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pushable") UStaticMesh* Mesh = nullptr;
	// how far it has to move before what's under it shows
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pushable") float RevealDistance = 90.f;
	// evidence hidden underneath
	UPROPERTY(BlueprintReadWrite, Category = "Pushable") TArray<AHUEvidence*> Hidden;

	UFUNCTION(BlueprintCallable, Category = "Pushable") void Apply();
	UFUNCTION(BlueprintCallable, Category = "Pushable") void Release();

	virtual FText GetPrompt_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User, bool bSecondary) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override { Super::OnConstruction(Transform); Apply(); }

protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<APawn> Holder;
	FVector Offset, Start;
	float SavedSpeed = 0.f;
	float GroundZ = 0.f;
};
