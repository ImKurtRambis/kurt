#include "HUNotebook.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "HUDraw.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/DateTime.h"

UHUNotebook* UHUNotebook::Of(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UHUNotebook>() : nullptr;
}

FString UHUNotebook::WhoOf(const APawn* P)
{
	const APlayerState* PS = P ? P->GetPlayerState() : nullptr;
	const FString Name = PS ? PS->GetPlayerName().Replace(TEXT("_"), TEXT(" ")).TrimStartAndEnd() : FString();
	return Name.IsEmpty() ? FString(TEXT("A detective")) : Name;
}

const TCHAR* UHUNotebook::KindName(EHUNote K)
{
	switch (K)
	{
	case EHUNote::Evidence:   return TEXT("BAGGED");
	case EHUNote::Heard:      return TEXT("HEARD");
	case EHUNote::Lie:        return TEXT("LIE ON THE RECORD");
	case EHUNote::Confession: return TEXT("CONFESSION");
	case EHUNote::Lookup:     return TEXT("LOOKED UP");
	case EHUNote::Tape:       return TEXT("ON THE TAPE");
	case EHUNote::Body:       return TEXT("THE BODY");
	default:                  return TEXT("NOTE");
	}
}

FLinearColor UHUNotebook::KindColor(EHUNote K)
{
	switch (K)
	{
	case EHUNote::Evidence:   return HUDraw::Blue;
	case EHUNote::Lie:        return HUDraw::Red;
	case EHUNote::Confession: return HUDraw::Gold;
	case EHUNote::Lookup:     return HUDraw::Green;
	case EHUNote::Tape:       return FLinearColor(0.6f, 0.85f, 1.f);
	case EHUNote::Body:       return HUDraw::Violet;
	default:                  return FLinearColor(0.75f, 0.8f, 0.86f);
	}
}

void UHUNotebook::Sync()
{
	const UHUCaseSubsystem* C = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	// (by the case, not whether it's still open: after the Captain's verdict the notebook is still there to read)
	const FString Key = C && C->Current.Suspects.Num() ? FString::Printf(TEXT("%d|%s"), C->Current.Number, *C->Current.Title) : FString();
	if (Key == CaseKey) return;
	CaseKey = Key;
	Entries.Reset(); FoundBy.Reset(); CaughtBy.Reset(); Seen = 0;
}

void UHUNotebook::Note(EHUNote Kind, const APawn* By, const FString& Text, const FString& Ref, const TCHAR* Tell, const FString& Short)
{
	Sync();
	FEntry E;
	E.Kind = Kind; E.Who = WhoOf(By); E.Text = Text.TrimStartAndEnd(); E.Ref = Ref;
	E.Clock = FDateTime::Now().ToString(TEXT("%H:%M"));
	Entries.Add(E);
	if (Entries.Num() > 500) { Entries.RemoveAt(0); Seen = FMath::Max(0, Seen - 1); }
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit notebook: %s %s: %s"), *E.Who, KindName(Kind), *E.Text.Left(160));
	if (!Tell) return;
	// a card for everyone else in the squad (whoever did it has had their own already)
	UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	APawn* Me = PC && PC->IsLocalController() ? PC->GetPawn() : nullptr;
	if (!Me || Me == By) return;
	if (UHUInteractComponent* I = Me->FindComponentByClass<UHUInteractComponent>())
		I->Notify(Tell, FString::Printf(TEXT("%s: %s"), *E.Who, *(Short.IsEmpty() ? E.Text : Short)), KindColor(Kind));
}

TSet<FString> UHUNotebook::LiesNow(const UHUCaseSubsystem* C)
{
	TSet<FString> Out;
	if (C) for (const FHUSuspect& S : C->Current.Suspects)
		for (int32 i = 0; i < S.Statements.Num(); i++) if (C->IsBroken(S.Id, i)) Out.Add(FString::Printf(TEXT("%s|%d"), *S.Id, i));
	return Out;
}

void UHUNotebook::NoteNewLies(const UHUCaseSubsystem* C, const APawn* By, const TSet<FString>& Before, bool bTell)
{
	if (!C) return;
	Sync();
	for (const FHUSuspect& S : C->Current.Suspects)
		for (int32 i = 0; i < S.Statements.Num(); i++)
		{
			const FString Key = FString::Printf(TEXT("%s|%d"), *S.Id, i);
			if (Before.Contains(Key) || !C->IsBroken(S.Id, i)) continue;
			Note(EHUNote::Lie, By, FString::Printf(TEXT("%s: \"%s\" - a lie, and it's on the record."), *S.Name.ToUpper(), *S.Statements[i].Text), S.Id,
				bTell ? TEXT("LIE ON THE RECORD") : nullptr, FString::Printf(TEXT("caught %s in a lie"), *S.Name));
			CaughtBy.Add(Key, WhoOf(By));
		}
}

// ---------- on screen ----------
using namespace HUDraw;

// a line in rows that fit a width
static TArray<FString> WrapRows(const FString& Line, const FSlateFontInfo& F, float MaxW)
{
	TArray<FString> Rows, Words;
	Line.ParseIntoArray(Words, TEXT(" "));
	FString Row;
	for (const FString& Wd : Words)
	{
		const FString Try = Row.IsEmpty() ? Wd : Row + TEXT(" ") + Wd;
		if (!Row.IsEmpty() && Measure(Try, F).X > MaxW) { Rows.Add(Row); Row = Wd; }
		else Row = Try;
	}
	if (!Row.IsEmpty()) Rows.Add(Row);
	return Rows;
}

void UHUNotebookWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

int32 UHUNotebookWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, G, Cull, Out, LayerId, Style, bParentEnabled);
	const UHUNotebook* NB = Owner ? UHUNotebook::Of(Owner) : nullptr;
	if (!NB) return LayerId;
	const FVector2D L = G.GetLocalSize();
	const int32 Ly = LayerId + 1;
	const FString Me = UHUNotebook::WhoOf(Cast<APawn>(Owner->GetOwner()));

	// closed: a tab under the case panel when there's something new in it
	if (!Owner->bNotebookOpen)
	{
		if (NB->Unread() == 0) return LayerId;
		const FVector2D P(16, 140);
		const FString T = FString::Printf(TEXT("SQUAD NOTEBOOK  ·  %d new"), NB->Unread());
		const FSlateFontInfo F = Font("Bold", 10, 100);
		Panel(Out, Ly, G, P, FVector2D(52 + Measure(T, F).X + 16, 34), 1.f, Gold, 8.f);
		KeyCap(Out, Ly + 2, G, TEXT("N"), P + FVector2D(12, 5));
		Text(Out, Ly + 2, G, T, F, P + FVector2D(48, 9), Gold);
		return Ly + 4;
	}

	// open: on the left, newest first
	const float W = FMath::Min(620.f, L.X * 0.46f), Top = 140.f, H = FMath::Max(220.f, L.Y - Top - 130.f);
	const FVector2D P(16, Top);
	Panel(Out, Ly, G, P, FVector2D(W, H), 1.f, Gold, 12.f);
	Text(Out, Ly + 2, G, TEXT("SQUAD NOTEBOOK"), Heading(13), P + FVector2D(22, 14), Ink);
	const UHUCaseSubsystem* C = Owner->GetWorld()->GetGameInstance() ? Owner->GetWorld()->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	FString Case = C && C->Current.Suspects.Num() ? C->Current.Title : FString(TEXT("No case yet"));
	while (Case.Len() > 12 && Measure(Case, Font("Regular", 10, 60)).X > W - 60) Case = Case.LeftChop(4) + TEXT("…");
	Text(Out, Ly + 2, G, Case.ToUpper(), Font("Regular", 10, 60), P + FVector2D(22, 38), Dim);
	const FSlateFontInfo HintF = Font("Regular", 10);
	const FString Hint = TEXT("[N] Close   ·   wheel scrolls");
	Prompt(Out, Ly + 1, G, Hint, HintF, P + FVector2D(W - 22 - Prompt(Out, Ly + 1, G, Hint, HintF, FVector2D::ZeroVector, Dim, 1.f, false), 16), Dim, 1.f, true);
	Box(Out, Ly + 1, G, P + FVector2D(22, 60), FVector2D(W - 44, 1), FLinearColor(1, 1, 1, 0.08f), 0.f);

	const FSlateFontInfo BodyF = Font("Regular", 11), WhoF = Font("Bold", 10, 60), KindF = Font("Bold", 9, 120), ClockF = Font("Regular", 9);
	const float TextW = W - 44, Bottom = P.Y + H - 30;
	float Y = P.Y + 72;
	if (NB->Entries.Num() == 0)
	{
		const TArray<FString> Rows = WrapRows(TEXT("Nothing yet. Whatever anyone in the squad bags, hears or looks up goes in here as it happens, with who it was."), Font("Italic", 11), TextW);
		for (int32 r = 0; r < Rows.Num(); r++) Text(Out, Ly + 2, G, Rows[r], Font("Italic", 11), FVector2D(P.X + 22, Y + r * 17), Dim);
		return Ly + 4;
	}
	const int32 Skip = FMath::Clamp(Owner->NotebookScroll, 0, NB->Entries.Num() - 1);
	if (Skip > 0) { Text(Out, Ly + 2, G, FString::Printf(TEXT("%d newer above"), Skip), Font("Italic", 9), FVector2D(P.X + W - 130, P.Y + 44), Dim); }
	int32 Shown = 0;
	for (int32 i = NB->Entries.Num() - 1 - Skip; i >= 0; i--)
	{
		const UHUNotebook::FEntry& E = NB->Entries[i];
		TArray<FString> Rows = WrapRows(E.Text, BodyF, TextW);
		if (Rows.Num() > 4) { Rows.SetNum(4); Rows[3] += TEXT(" …"); }
		const float Need = 20.f + Rows.Num() * 17.f + 10.f;
		if (Y + Need > Bottom && Shown > 0) break;
		const bool bMine = E.Who == Me;
		float X = P.X + 22;
		Text(Out, Ly + 2, G, E.Clock, ClockF, FVector2D(X, Y + 1), Dim);
		X += 42;
		const FString Who = bMine ? FString(TEXT("YOU")) : E.Who.ToUpper();
		Text(Out, Ly + 2, G, Who, WhoF, FVector2D(X, Y), bMine ? Blue : Gold);
		X += Measure(Who, WhoF).X + 10;
		Text(Out, Ly + 2, G, UHUNotebook::KindName(E.Kind), KindF, FVector2D(X, Y + 1), UHUNotebook::KindColor(E.Kind));
		Y += 20;
		for (const FString& R : Rows) { Text(Out, Ly + 2, G, R, BodyF, FVector2D(P.X + 22, Y), Ink); Y += 17; }
		Y += 10;
		Shown++;
	}
	const int32 Older = NB->Entries.Num() - Skip - Shown;
	if (Older > 0) Text(Out, Ly + 2, G, FString::Printf(TEXT("%d earlier - scroll down"), Older), Font("Italic", 9), FVector2D(P.X + 22, P.Y + H - 24), Dim);
	return Ly + 4;
}
