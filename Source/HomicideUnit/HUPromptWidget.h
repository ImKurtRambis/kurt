// The on-screen prompt ("[E] Next slide") and subtitle bar, drawn in code so no widget asset is needed.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUPromptWidget.generated.h"

UCLASS()
class HOMICIDEUNIT_API UHUPromptWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY() class UHUInteractComponent* Owner = nullptr;

private:
	mutable FSlateBrush FeedBrush; // the security tape on screen

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
