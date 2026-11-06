// A small drawing kit for the game's 2D screens, so the HUD and the menus share one look:
// dark rounded panels, gold accents, letter-spaced headings, key caps, progress bars.
#pragma once

#include "CoreMinimal.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

namespace HUDraw
{
	// the palette
	inline const FLinearColor Ink(0.93f, 0.94f, 0.96f);
	inline const FLinearColor Dim(0.55f, 0.6f, 0.66f);
	inline const FLinearColor Gold(0.95f, 0.76f, 0.32f);
	inline const FLinearColor Blue(0.36f, 0.76f, 1.f);
	inline const FLinearColor Green(0.42f, 0.9f, 0.6f);
	inline const FLinearColor Red(0.95f, 0.35f, 0.3f);
	inline const FLinearColor Violet(0.8f, 0.55f, 0.98f);
	inline const FLinearColor Amber(1.f, 0.7f, 0.3f);
	inline FLinearColor PanelFill(float A = 0.86f) { return FLinearColor(0.025f, 0.03f, 0.045f, A); }

	inline FSlateFontInfo Font(const char* Style, int32 Size, int32 Spacing = 0)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(Style, Size);
		F.LetterSpacing = Spacing;
		return F;
	}
	inline FSlateFontInfo Heading(int32 Size) { return Font("Bold", Size, 180); }

	inline FVector2D Measure(const FString& T, const FSlateFontInfo& F)
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, F);
	}

	inline void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& T, const FSlateFontInfo& F, FVector2D Pos, FLinearColor Col)
	{
		if (T.IsEmpty()) return;
		FSlateDrawElement::MakeText(Out, Layer, G.ToPaintGeometry(Measure(T, F), FSlateLayoutTransform(Pos)), T, F, ESlateDrawEffect::None, Col);
	}
	inline void TextCentered(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& T, const FSlateFontInfo& F, FVector2D Center, FLinearColor Col)
	{
		Text(Out, Layer, G, T, F, Center - Measure(T, F) * 0.5f, Col);
	}

	// a rounded box (cached brushes, one per radius)
	inline const FSlateBrush* Rounded(float Radius)
	{
		static TMap<int32, FSlateRoundedBoxBrush*> Cache;
		const int32 K = FMath::RoundToInt(Radius);
		if (FSlateRoundedBoxBrush** B = Cache.Find(K)) return *B;
		FSlateRoundedBoxBrush* B = new FSlateRoundedBoxBrush(FLinearColor::White, (float)K);
		Cache.Add(K, B);
		return B;
	}
	inline void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2D Pos, FVector2D Size, FLinearColor Col, float Radius = 8.f)
	{
		if (Size.X <= 0 || Size.Y <= 0) return;
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Rounded(FMath::Min(Radius, (float)FMath::Min(Size.X, Size.Y) * 0.5f)), ESlateDrawEffect::None, Col);
	}
	// a panel: a thin edge, the dark fill, and an optional accent stripe on the left
	inline void Panel(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2D Pos, FVector2D Size, float Alpha = 1.f, FLinearColor Accent = FLinearColor::Transparent, float Radius = 10.f)
	{
		Box(Out, Layer, G, Pos - FVector2D(1, 1), Size + FVector2D(2, 2), FLinearColor(1, 1, 1, 0.08f * Alpha), Radius + 1);
		Box(Out, Layer, G, Pos, Size, PanelFill(0.86f * Alpha), Radius);
		if (Accent.A > 0.f) Box(Out, Layer + 1, G, Pos + FVector2D(0, 6), FVector2D(3, Size.Y - 12), Accent * FLinearColor(1, 1, 1, Alpha), 2.f);
	}
	// a key cap, e.g. [E], returns its width
	inline float KeyCap(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& Key, FVector2D Pos, float Alpha = 1.f, FLinearColor Col = Gold)
	{
		const FSlateFontInfo F = Font("Bold", 11);
		const FVector2D S = Measure(Key, F);
		const FVector2D Size(FMath::Max(24.f, S.X + 14.f), 24.f);
		Box(Out, Layer, G, Pos, Size, Col * FLinearColor(1, 1, 1, 0.9f * Alpha), 5.f);
		Box(Out, Layer + 1, G, Pos + FVector2D(1.5f, 1.5f), Size - FVector2D(3, 4.5f), FLinearColor(0.06f, 0.05f, 0.03f, 0.92f * Alpha), 4.f);
		Text(Out, Layer + 2, G, Key, F, Pos + (Size - S) * 0.5f - FVector2D(0, 1), Col * FLinearColor(1, 1, 1, Alpha));
		return Size.X;
	}
	// a progress bar
	inline void Bar(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2D Pos, FVector2D Size, float Frac, FLinearColor Col)
	{
		Box(Out, Layer, G, Pos, Size, FLinearColor(1, 1, 1, 0.08f), Size.Y * 0.5f);
		Box(Out, Layer + 1, G, Pos, FVector2D(Size.X * FMath::Clamp(Frac, 0.f, 1.f), Size.Y), Col, Size.Y * 0.5f);
	}
	// "[E] Do the thing  ·  [Q] Other thing": key caps for anything in [brackets], text for the rest. Returns width.
	inline float Prompt(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& Line, const FSlateFontInfo& F, FVector2D Pos, FLinearColor Col, float Alpha, bool bDraw)
	{
		float X = Pos.X;
		FString Rest = Line;
		while (!Rest.IsEmpty())
		{
			int32 Open = Rest.Find(TEXT("["));
			int32 Close = Open >= 0 ? Rest.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Open) : -1;
			const FString Before = Open >= 0 && Close > Open ? Rest.Left(Open) : Rest;
			if (!Before.IsEmpty())
			{
				if (bDraw) Text(Out, Layer + 2, G, Before, F, FVector2D(X, Pos.Y), Col * FLinearColor(1, 1, 1, Alpha));
				X += Measure(Before, F).X;
			}
			if (Open < 0 || Close <= Open) break;
			const FString Key = Rest.Mid(Open + 1, Close - Open - 1);
			X += (bDraw ? KeyCap(Out, Layer, G, Key, FVector2D(X, Pos.Y - 3), Alpha) : FMath::Max(24.f, Measure(Key, Font("Bold", 11)).X + 14.f)) + 6.f;
			Rest = Rest.Mid(Close + 1).TrimStart();
		}
		return X - Pos.X;
	}
}
