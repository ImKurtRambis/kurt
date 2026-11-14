#include "HUMenuWidget.h"
#include "HUDeskMenu.h"
#include "HUDraw.h"
#include "HUVersion.h"
#include "Engine/World.h"

using namespace HUDraw;

int32 UHUMenuWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, G, Cull, Out, LayerId, Style, bParentEnabled);
	if (!Owner) return LayerId;
	const FVector2D L = G.GetLocalSize();
	const int32 Ly = LayerId + 1;

	// a soft dark vignette at the top and bottom, so the text always reads over the desk
	Box(Out, Ly, G, FVector2D(0, 0), FVector2D(L.X, 150), FLinearColor(0, 0, 0, 0.35f), 0.f);
	Box(Out, Ly, G, FVector2D(0, L.Y - 190), FVector2D(L.X, 190), FLinearColor(0, 0, 0, 0.35f), 0.f);

	// the title
	Text(Out, Ly + 1, G, TEXT("HOMICIDE UNIT"), Heading(30), FVector2D(44, 34), Ink);
	Box(Out, Ly + 1, G, FVector2D(46, 84), FVector2D(60, 3), Gold, 1.5f);
	Text(Out, Ly + 1, G, TEXT("METRO PD  ·  NIGHT SHIFT"), Font("Bold", 10, 220), FVector2D(118, 78), Gold);
	// which build this is, small in the corner: what a tester quotes in a bug report (HUVersion)
	{
		const FString Build = HUVersion::Text(); // as written in the log and crash reports (the hash stays lower-case)
		const FSlateFontInfo VF = Font("Regular", 10, 60);
		Text(Out, Ly + 1, G, Build, VF, FVector2D(L.X - Measure(Build, VF).X - 22.f, L.Y - 26.f), Dim);
	}

	if (Owner->Panel == EHUMenuPanel::None)
	{
		// the card for what you're pointing at
		const EHUDeskItem It = (EHUDeskItem)Owner->Pick;
		const FString Name = AHUDeskMenu::ItemName(It), Does = Owner->ItemDoes(It);
		const FSlateFontInfo NF = Heading(16), DF = Font("Regular", 13);
		const FVector2D NS = Measure(Name, NF), DS = Measure(Does, DF);
		const float W = FMath::Max(NS.X, DS.X) + 64.f, H = 84.f;
		const FVector2D P((L.X - W) * 0.5f, L.Y - H - 74.f);
		Panel(Out, Ly + 1, G, P, FVector2D(W, H), 1.f, Gold, 12.f);
		TextCentered(Out, Ly + 3, G, Name, NF, FVector2D(L.X * 0.5f, P.Y + 28), Gold);
		TextCentered(Out, Ly + 3, G, Does, DF, FVector2D(L.X * 0.5f, P.Y + 58), Ink);
		// the controls, as key caps
		const FString Hint = TEXT("[Click] use   [←] [→] choose   [Enter] use   [Esc] go home");
		const FSlateFontInfo HF = Font("Regular", 11);
		const float HW = Prompt(Out, Ly, G, Hint, HF, FVector2D(0, 0), Dim, 1.f, false);
		Prompt(Out, Ly + 1, G, Hint, HF, FVector2D((L.X - HW) * 0.5f, L.Y - 40), Dim, 1.f, true);
		return Ly + 5;
	}

	// a panel over the desk
	FString Title; TArray<FString> Lines;
	Owner->PanelText(Title, Lines);
	const FSlateFontInfo TF = Heading(17), LF = Font("Regular", 13);
	float W = Measure(Title, TF).X;
	for (const FString& Ln : Lines) W = FMath::Max(W, (float)Measure(Ln, LF).X);
	W = FMath::Clamp(W + 80.f, 520.f, L.X - 120.f);
	const float RowH = 30.f, H = 96.f + Lines.Num() * RowH + 20.f;
	const FVector2D P((L.X - W) * 0.5f, (L.Y - H) * 0.5f);
	Box(Out, Ly, G, FVector2D(0, 0), L, FLinearColor(0, 0, 0, 0.55f), 0.f);
	Panel(Out, Ly + 1, G, P, FVector2D(W, H), 1.f, Gold, 14.f);
	Text(Out, Ly + 3, G, Title, TF, P + FVector2D(36, 28), Gold);
	Box(Out, Ly + 3, G, P + FVector2D(36, 60), FVector2D(48, 2), Gold * FLinearColor(1, 1, 1, 0.6f), 1.f);
	for (int32 i = 0; i < Lines.Num(); i++)
	{
		const FVector2D RP = P + FVector2D(36, 80 + i * RowH);
		FString Ln = Lines[i];
		const bool bSelected = Ln.StartsWith(TEXT("▸"));
		if (bSelected)
		{
			// the selected row: a soft gold bar behind it
			Box(Out, Ly + 2, G, RP - FVector2D(14, 5), FVector2D(W - 44, RowH - 2), Gold * FLinearColor(1, 1, 1, 0.14f), 7.f);
			Box(Out, Ly + 3, G, RP - FVector2D(14, 3), FVector2D(3, RowH - 6), Gold, 1.5f);
			Ln = Ln.RightChop(1).TrimStart();
		}
		else Ln = Ln.TrimStart();
		Text(Out, Ly + 3, G, Ln, bSelected ? Font("Bold", 13) : LF, RP, bSelected ? Ink : (Ln.StartsWith(TEXT("↑")) || Ln.StartsWith(TEXT("Type")) || Ln.StartsWith(TEXT("ENTER")) || Ln.StartsWith(TEXT("ESC")) ? Dim : Ink * FLinearColor(1, 1, 1, 0.85f)));
	}
	return Ly + 5;
}
