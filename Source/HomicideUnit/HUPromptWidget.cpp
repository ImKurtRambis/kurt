#include "HUPromptWidget.h"
#include "HUInteractComponent.h"
#include "HUCaseSubsystem.h"
#include "HUDraw.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

using namespace HUDraw;

// split a line into rows that fit a width
static TArray<FString> Wrap(const FString& Line, const FSlateFontInfo& F, float MaxW)
{
	TArray<FString> Rows, Words;
	Line.ParseIntoArray(Words, TEXT(" "));
	FString Row;
	for (const FString& W : Words)
	{
		const FString Try = Row.IsEmpty() ? W : Row + TEXT(" ") + W;
		if (!Row.IsEmpty() && Measure(Try, F).X > MaxW) { Rows.Add(Row); Row = W; }
		else Row = Try;
	}
	if (!Row.IsEmpty()) Rows.Add(Row);
	return Rows;
}

int32 UHUPromptWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, G, Cull, Out, LayerId, Style, bParentEnabled);
	if (!Owner || !Owner->GetWorld()) return LayerId;
	const FVector2D L = G.GetLocalSize();
	const int32 Ly = LayerId + 1;
	const float Now = Owner->GetWorld()->GetRealTimeSeconds();
	const UHUCaseSubsystem* Cases = Owner->GetWorld()->GetGameInstance() ? Owner->GetWorld()->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	const FString PromptText = Owner->Prompt.ToString();
	const float In = FMath::Clamp(Owner->PromptAge * 5.f, 0.f, 1.f), Ease = 1.f - FMath::Square(1.f - In);

	// ---- crosshair: a dot, and four ticks that close in when you're looking at something usable ----
	const FVector2D C = L * 0.5f;
	const bool bAim = !PromptText.IsEmpty();
	Box(Out, Ly, G, C - FVector2D(2.5f, 2.5f), FVector2D(5, 5), bAim ? Gold : FLinearColor(1, 1, 1, 0.75f), 3.f);
	if (bAim)
	{
		const float R = 16.f - 6.f * Ease;
		for (const FVector2D& D : { FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1) })
		{
			const FVector2D P = C + D * R;
			Box(Out, Ly, G, P - (D.X != 0 ? FVector2D(3, 1) : FVector2D(1, 3)), D.X != 0 ? FVector2D(6, 2) : FVector2D(2, 6), Gold * FLinearColor(1, 1, 1, Ease), 1.f);
		}
	}

	// ---- the prompt, with key caps, easing in under the crosshair ----
	if (bAim)
	{
		const FSlateFontInfo F = Font("Bold", 13);
		const float W = Prompt(Out, Ly, G, PromptText, F, FVector2D(0, 0), Ink, 1.f, false);
		const FVector2D Pos(C.X - W * 0.5f, C.Y + 44.f + (1.f - Ease) * 12.f);
		Panel(Out, Ly, G, Pos - FVector2D(14, 10), FVector2D(W + 28, 38), Ease, Blue, 9.f);
		Prompt(Out, Ly + 1, G, PromptText, F, Pos, Ink, Ease, true);
	}

	// ---- the case card, top left, while you're working a scene ----
	if (Cases && Cases->bCaseActive && Cases->InScene.Num() > 0)
	{
		int32 Bagged, Placed, KeyBagged, KeyPlaced;
		Cases->GetEvidenceCount(Bagged, Placed, KeyBagged, KeyPlaced);
		TArray<int32> Counts;
		const TArray<EHUProfile> Leads = Cases->GetProfileLeads(Counts);
		const FVector2D P(24, 24);
		const float W = 340.f, H = 168.f + (Leads.Num() > 0 ? 28.f : 0.f);
		Panel(Out, Ly, G, P, FVector2D(W, H), 1.f, Gold);
		Text(Out, Ly + 2, G, FString::Printf(TEXT("CASE #%d  ·  %s"), Cases->Current.Number, *Cases->PlayDifficulty.ToUpper()), Font("Regular", 9, 120), P + FVector2D(18, 14), Dim);
		Text(Out, Ly + 2, G, Cases->Current.Place, Heading(13), P + FVector2D(18, 30), Ink);
		auto Row = [&](float Y, const TCHAR* Label, int32 A, int32 B, FLinearColor Col)
		{
			Text(Out, Ly + 2, G, Label, Font("Bold", 9, 100), P + FVector2D(18, Y), Dim);
			Text(Out, Ly + 2, G, FString::Printf(TEXT("%d / %d"), A, B), Font("Bold", 10), P + FVector2D(W - 60, Y - 1), Ink);
			Bar(Out, Ly + 2, G, P + FVector2D(18, Y + 16), FVector2D(W - 36, 5), B > 0 ? (float)A / B : 0.f, Col);
		};
		Row(62, TEXT("EVIDENCE BAGGED"), Bagged, Placed, Blue);
		Row(96, TEXT("PROVES A LIE"), KeyBagged, KeyPlaced, KeyBagged >= Cases->Current.Solution.RequiredBreaks ? Green : Amber);
		// lies on the record: one pip each
		Text(Out, Ly + 2, G, TEXT("LIES ON THE RECORD"), Font("Bold", 9, 100), P + FVector2D(18, 132), Dim);
		for (int32 i = 0; i < Cases->Current.Solution.RequiredBreaks; i++)
			Box(Out, Ly + 2, G, P + FVector2D(W - 30 - i * 18, 132), FVector2D(12, 12), i < Cases->Breaks ? Red : FLinearColor(1, 1, 1, 0.15f), 6.f);
		// what the evidence says you're dealing with
		if (Leads.Num() > 0)
		{
			float X = P.X + 18;
			for (int32 i = 0; i < Leads.Num() && i < 3; i++)
			{
				const FString Chip = FString::Printf(TEXT("%s  ×%d"), HUProfiles::Name(Leads[i]), Counts[i]);
				const FSlateFontInfo CF = Font("Bold", 8, 60);
				const float CW = Measure(Chip, CF).X + 16;
				if (X + CW > P.X + W - 12) break;
				Box(Out, Ly + 2, G, FVector2D(X, P.Y + 158), FVector2D(CW, 20), i == 0 ? Violet * FLinearColor(1, 1, 1, 0.9f) : FLinearColor(1, 1, 1, 0.12f), 10.f);
				Text(Out, Ly + 3, G, Chip, CF, FVector2D(X + 8, P.Y + 161), i == 0 ? FLinearColor(0.08f, 0.04f, 0.1f) : Ink);
				X += CW + 6;
			}
		}
	}

	// ---- notification cards, top right: slide in, hold, fade ----
	{
		float Y = 24.f;
		for (int32 i = Owner->Notes.Num() - 1; i >= 0; i--)
		{
			const UHUInteractComponent::FNote& N = Owner->Notes[i];
			const float Age = Now - N.Born;
			if (Age > 5.f) continue;
			const float A = FMath::Clamp(FMath::Min(Age * 6.f, (5.f - Age) * 2.f), 0.f, 1.f);
			const float Slide = (1.f - FMath::Clamp(Age * 6.f, 0.f, 1.f)) * 60.f;
			const float W = 330.f;
			const TArray<FString> Rows = Wrap(N.Body, Font("Regular", 11), W - 36);
			const float H = 36.f + Rows.Num() * 17.f;
			const FVector2D P(L.X - W - 24 + Slide, Y);
			Panel(Out, Ly, G, P, FVector2D(W, H), A, N.Color);
			Text(Out, Ly + 2, G, N.Title, Font("Bold", 9, 140), P + FVector2D(18, 10), N.Color * FLinearColor(1, 1, 1, A));
			for (int32 r = 0; r < Rows.Num(); r++) Text(Out, Ly + 2, G, Rows[r], Font("Regular", 11), P + FVector2D(18, 28 + r * 17), Ink * FLinearColor(1, 1, 1, A));
			Y += H + 10.f;
		}
	}

	// ---- the hotbar, bottom centre ----
	const float SlotW = 74.f, SlotH = 58.f, Gap = 8.f;
	const int32 N = UHUInteractComponent::SlotCount;
	const float Total = N * SlotW + (N - 1) * Gap, X0 = (L.X - Total) * 0.5f, Y0 = L.Y - SlotH - 18.f;
	for (int32 i = 0; i < N; i++)
	{
		const FVector2D P(X0 + i * (SlotW + Gap), Y0);
		const bool bHeld = Owner->Held == i, bHave = Owner->SlotUsable(i);
		if (bHeld) Box(Out, Ly, G, P - FVector2D(2, 2), FVector2D(SlotW + 4, SlotH + 4), Gold, 11.f);
		Box(Out, Ly + 1, G, P, FVector2D(SlotW, SlotH), bHeld ? FLinearColor(0.12f, 0.09f, 0.03f, 0.95f) : PanelFill(bHave ? 0.82f : 0.45f), 9.f);
		const FLinearColor Col = bHeld ? Gold : bHave ? Ink : FLinearColor(0.4f, 0.42f, 0.45f, 0.8f);
		Text(Out, Ly + 2, G, FString::FromInt(i + 1), Font("Bold", 9), P + FVector2D(8, 5), Col * FLinearColor(1, 1, 1, 0.8f));
		FString Name = i == 0 ? FString(TEXT("Hands")) : FString(HUTools::Name((EHUTool)(i - 1))).Replace(TEXT("Evidence "), TEXT(""));
		TextCentered(Out, Ly + 2, G, bHave ? Name : FString(TEXT("—")), Font(bHeld ? "Bold" : "Regular", 10), P + FVector2D(SlotW * 0.5f, SlotH * 0.58f), Col);
		const bool bOn = (i == (int32)EHUTool::Flashlight + 1 && Owner->bFlashlightOn) || (i == (int32)EHUTool::UVLight + 1 && Owner->bUVOn);
		if (bOn) Box(Out, Ly + 2, G, P + FVector2D(SlotW - 14, 7), FVector2D(7, 7), Green, 4.f);
	}
	if (Cases)
	{
		const FString Cash = FString::Printf(TEXT("$%d"), Cases->Cash);
		const FVector2D S = Measure(Cash, Font("Bold", 14));
		Panel(Out, Ly, G, FVector2D(X0 - S.X - 44, Y0 + 12), FVector2D(S.X + 26, 34), 1.f, FLinearColor::Transparent, 9.f);
		Text(Out, Ly + 2, G, Cash, Font("Bold", 14), FVector2D(X0 - S.X - 31, Y0 + 19), Green);
	}

	// ---- subtitles: the speaker's name, then the line, wrapped, above the hotbar ----
	float Bottom = Y0 - 22.f;
	auto Caption = [&](const FString& Line, FLinearColor NameCol, bool bItalic, const TCHAR* Tag)
	{
		if (Line.IsEmpty()) return;
		FString Who, Said = Line;
		int32 Colon = Line.Find(TEXT(":  "));
		if (Colon > 0 && Colon < 40 && Line.Left(Colon) == Line.Left(Colon).ToUpper()) { Who = Line.Left(Colon); Said = Line.Mid(Colon + 3); }
		const FSlateFontInfo F = Font(bItalic ? "Italic" : "Regular", 15);
		const TArray<FString> Rows = Wrap(Said, F, FMath::Min(860.f, L.X - 160.f));
		float W = 0; for (const FString& R : Rows) W = FMath::Max(W, (float)Measure(R, F).X);
		if (!Who.IsEmpty()) W = FMath::Max(W, (float)Measure(Who, Font("Bold", 9, 140)).X + (Tag ? 50.f : 0.f));
		const float H = Rows.Num() * 22.f + (Who.IsEmpty() ? 18.f : 36.f);
		const FVector2D P((L.X - W) * 0.5f - 20.f, Bottom - H);
		Panel(Out, Ly, G, P, FVector2D(W + 40.f, H), 1.f, NameCol);
		float Y = P.Y + 10.f;
		if (!Who.IsEmpty())
		{
			float X = P.X + 20.f;
			if (Tag) { Box(Out, Ly + 2, G, FVector2D(X, Y), FVector2D(40, 16), Red, 8.f); TextCentered(Out, Ly + 3, G, Tag, Font("Bold", 8, 80), FVector2D(X + 20, Y + 8), FLinearColor::White); X += 48.f; }
			Text(Out, Ly + 2, G, Who, Font("Bold", 9, 140), FVector2D(X, Y + 1), NameCol);
			Y += 20.f;
		}
		for (const FString& R : Rows) { Text(Out, Ly + 2, G, R, F, FVector2D(P.X + 20.f, Y), Ink); Y += 22.f; }
		Bottom = P.Y - 10.f;
	};
	Caption(Owner->Subtitle.ToString(), Gold, false, nullptr);
	{
		// the 911 call: "911 CALL  ▸  NAME:  "line""
		FString Call = Owner->CallLine.ToString();
		Call.ReplaceInline(TEXT("911 CALL  ▸  "), TEXT(""));
		Call.ReplaceInline(TEXT("911  ▸  "), TEXT("911:  "));
		Caption(Call, Amber, true, TEXT("911"));
	}
	return Ly + 4;
}
