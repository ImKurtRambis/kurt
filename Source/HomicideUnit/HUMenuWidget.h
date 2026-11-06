// The desk menu's 2D layer: the title, the label for what you're pointing at, and the panels. Drawn in code.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUMenuWidget.generated.h"

UCLASS()
class HOMICIDEUNIT_API UHUMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY() class AHUDeskMenu* Owner = nullptr;

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
