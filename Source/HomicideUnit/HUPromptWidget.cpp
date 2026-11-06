#include "HUPromptWidget.h"
#include "HUInteractComponent.h"
#include "HUCaseSubsystem.h"
#include "HUDraw.h"
#include "HUSecurityDesk.h"
#include "HUCharacter.h"
#include "Engine/TextureRenderTarget2D.h"
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
	// ---- watching the security tapes: the feed fills the screen ----
	if (const AHUSecurityDesk* Desk = Owner->Watching.Get())
	{
		const float VH = L.Y * 0.78f, VW = VH * 4.f / 3.f;
		const FVector2D P((L.X - VW) * 0.5f, (L.Y - VH) * 0.42f);
		Box(Out, Ly, G, FVector2D(0, 0), L, FLinearColor(0, 0, 0, 0.92f), 0.f);
		Box(Out, Ly + 1, G, P - FVector2D(10, 10), FVector2D(VW + 20, VH + 20), FLinearColor(0.12f, 0.12f, 0.11f), 14.f); // the monitor's bezel
		if (Desk->FrameLost())
		{
			// the recorder was off: snow and a message
			Box(Out, Ly + 2, G, P, FVector2D(VW, VH), FLinearColor(0.06f, 0.06f, 0.06f), 4.f);
			FRandomStream Snow(FMath::FloorToInt(Now * 20.f));
			for (int32 s = 0; s < 260; s++) Box(Out, Ly + 3, G, P + FVector2D(Snow.FRand() * (VW - 4), Snow.FRand() * (VH - 3)), FVector2D(Snow.FRandRange(2.f, 9.f), 2.f), FLinearColor(1, 1, 1, Snow.FRandRange(0.1f, 0.5f)), 0.f);
			TextCentered(Out, Ly + 4, G, TEXT("SIGNAL LOST"), Heading(26), P + FVector2D(VW, VH) * 0.5f - FVector2D(0, 16), Ink);
			TextCentered(Out, Ly + 4, G, TEXT("The recorder was switched off around here. Somebody didn't want this on tape."), Font("Regular", 12), P + FVector2D(VW, VH) * 0.5f + FVector2D(0, 22), Dim);
		}
		else if (Desk->Feed)
		{
			FeedBrush.SetResourceObject(Desk->Feed);
			FeedBrush.ImageSize = FVector2D(640, 480);
			FeedBrush.DrawAs = ESlateBrushDrawType::Image;
			FSlateDrawElement::MakeBox(Out, Ly + 2, G.ToPaintGeometry(FVector2D(VW, VH), FSlateLayoutTransform(P)), &FeedBrush, ESlateDrawEffect::None, FLinearColor(0.95f, 1.f, 0.95f));
			// scanlines
			for (float Y = 0; Y < VH; Y += 4.f) Box(Out, Ly + 3, G, P + FVector2D(0, Y), FVector2D(VW, 1.f), FLinearColor(0, 0, 0, 0.18f), 0.f);
		}
		// the tape's own overlay: REC, camera, time
		const FSlateFontInfo Mono = Font("Bold", 14, 60);
		if (FMath::Fmod(Now, 1.f) < 0.6f) { Box(Out, Ly + 4, G, P + FVector2D(18, 20), FVector2D(12, 12), Red, 6.f); }
		Text(Out, Ly + 4, G, TEXT("REC"), Mono, P + FVector2D(36, 15), FLinearColor(1, 1, 1, 0.9f));
		Text(Out, Ly + 4, G, Desk->CamLabel(), Mono, P + FVector2D(18, 42), FLinearColor(1, 1, 1, 0.85f));
		if (Desk->Frames.IsValidIndex(Desk->Frame))
		{
			const FString Clock = Desk->Frames[Desk->Frame].Clock;
			Text(Out, Ly + 4, G, Clock, Font("Bold", 22, 80), P + FVector2D(VW - Measure(Clock, Font("Bold", 22, 80)).X - 22, VH - 46), FLinearColor(1, 1, 1, 0.92f));
			Text(Out, Ly + 4, G, FString::Printf(TEXT("TAPE  %d / %d"), Desk->Frame + 1, Desk->Frames.Num()), Font("Bold", 11, 80), P + FVector2D(18, VH - 34), FLinearColor(1, 1, 1, 0.7f));
			// the tape's timeline: a tick for each moment, the current one lit
			const float TW = 220.f; const FVector2D TP(P.X + (VW - TW) * 0.5f, P.Y + VH + 26);
			for (int32 k = 0; k < Desk->Frames.Num(); k++)
			{
				const float X = TP.X + TW * k / FMath::Max(1, Desk->Frames.Num() - 1);
				Box(Out, Ly + 4, G, FVector2D(X - 5, TP.Y - 5), FVector2D(10, 10), k == Desk->Frame ? Gold : FLinearColor(1, 1, 1, Desk->Frames[k].bLost ? 0.1f : 0.3f), 5.f);
			}
		}
		const FString Keys = TEXT("[A] [D] camera     [W] [S] earlier / later     [Q] step away");
		const float KW = Prompt(Out, Ly, G, Keys, Font("Regular", 12), FVector2D(0, 0), Dim, 1.f, false);
		Prompt(Out, Ly + 4, G, Keys, Font("Regular", 12), FVector2D((L.X - KW) * 0.5f, L.Y - 46), Dim, 1.f, true);
		return Ly + 6;
	}

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

	// ---- holding E on a timed task: a ring of dots filling around the crosshair ----
	if (Owner->bHolding)
	{
		const int32 Dots = 28;
		for (int32 k = 0; k < Dots; k++)
		{
			const float A = 2.f * PI * k / Dots - PI * 0.5f;
			const bool bDone = (float)k / Dots < Owner->HoldProgress;
			Box(Out, Ly, G, C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 26.f - FVector2D(2.5f, 2.5f), FVector2D(5, 5), bDone ? Gold : FLinearColor(1, 1, 1, 0.18f), 3.f);
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
	// ---- coffee: how long it lasts, beside the hotbar ----
	if (const AHUCharacter* Ch = Cast<AHUCharacter>(Owner->GetOwner()); Ch && Ch->IsCaffeinated())
	{
		const float Left = Ch->CoffeeLeft();
		const FVector2D P(X0 + Total + 18, Y0 + 6);
		Panel(Out, Ly, G, P, FVector2D(150, 46), 1.f, FLinearColor(0.85f, 0.6f, 0.35f), 9.f);
		Text(Out, Ly + 2, G, FString::Printf(TEXT("COFFEE  %d:%02d"), FMath::FloorToInt(Left) / 60, FMath::FloorToInt(Left) % 60), Font("Bold", 10, 100), P + FVector2D(14, 8), FLinearColor(0.95f, 0.75f, 0.5f));
		Bar(Out, Ly + 2, G, P + FVector2D(14, 30), FVector2D(122, 4), Left / FMath::Max(1.f, Ch->CoffeeLength), FLinearColor(0.85f, 0.6f, 0.35f));
	}	if (Cases)
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
