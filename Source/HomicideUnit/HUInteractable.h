// Anything the detective can use with E: the laptop, a clue, a suspect, a door, the cruiser.
// Add it in a Blueprint's Class Settings -> Interfaces, then implement the two events.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "HUInteractable.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class UHUInteractable : public UInterface { GENERATED_BODY() };

class HOMICIDEUNIT_API IHUInteractable
{
	GENERATED_BODY()
public:
	// the on-screen prompt, e.g. "[E] Next slide"
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact") FText GetPrompt(APawn* User) const;
	// Primary is E, Secondary is Q
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact") void Interact(APawn* User, bool bSecondary);

	// Which parts light up when you look at Part. By default, the whole actor (right for a single object).
	// Rooms full of things override this to light up just the one thing (this chair, this laptop) - or nothing.
	virtual void HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const
	{
		// (left empty: the caller then lights up the whole actor)
	}
	// false for things you just pick up (evidence): no glow, no spotlight - only the prompt
	virtual bool Highlights() const { return true; }
	// seconds you hold E (or Q) to do it - careful work takes a moment (0 = instant). Coffee makes it quicker.
	virtual float UseTime(APawn* User, bool bSecondary) const { return 0.f; }
};
