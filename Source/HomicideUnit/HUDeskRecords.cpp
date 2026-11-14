#include "HUDeskRecords.h"
#include "HUCaseSubsystem.h"
#include "HUPhoneData.h"
#include "HAL/PlatformTime.h"

static uint32 Hash(const FString& S) { return GetTypeHash(S); }
static bool HasWord(const FHUClue& Cl, std::initializer_list<const TCHAR*> Words)
{
	const FString T = (Cl.Name + TEXT(" ") + Cl.Detail).ToLower();
	for (const TCHAR* W : Words) if (T.Contains(W)) return true;
	return false;
}
// people on file who have nothing to do with the case (what a cold hit comes back as)
static const TCHAR* Strangers[] = { TEXT("Darnell Hayes"), TEXT("Marisol Vega"), TEXT("Pete Kowalczyk"), TEXT("Ruth Abernathy"), TEXT("Tommy Lee Briggs"), TEXT("Anita Fairweather") };
static const TCHAR* Streets[] = { TEXT("Halsey St"), TEXT("Orchard Ave"), TEXT("Canal Row"), TEXT("Wexford Ct"), TEXT("Pine Hollow Rd"), TEXT("Delancey Pl") };

void UHUDeskRecords::Sync(const UHUCaseSubsystem* C)
{
	const FString Key = C && C->bCaseActive ? FString::Printf(TEXT("%d|%s"), C->Current.Number, *C->Current.Title) : FString();
	if (Key == CaseKey) return;
	CaseKey = Key;
	Prints.Reset(); Plates.Reset(); LabIn.Reset(); LabBy.Reset(); Backgrounds.Reset(); Warrants.Reset();
}

const UHUDeskRecords::FWarrant* UHUDeskRecords::WarrantFor(const FString& Suspect, bool bArrest) const
{
	for (int32 i = Warrants.Num() - 1; i >= 0; i--) if (Warrants[i].Suspect == Suspect && Warrants[i].bArrest == bArrest) return &Warrants[i];
	return nullptr;
}

void UHUDeskRecords::Apply(const UHUCaseSubsystem* C, EHUDeskJob Job, const FString& Arg, const FString& Who)
{
	Sync(C);
	switch (Job)
	{
	case EHUDeskJob::Print: Prints.Add(Arg, Who); break;
	case EHUDeskJob::Plate: Plates.Add(Arg, Who); break;
	case EHUDeskJob::Lab: if (!LabIn.Contains(Arg)) { LabIn.Add(Arg, FPlatformTime::Seconds()); LabBy.Add(Arg, Who); } break;
	case EHUDeskJob::Background: Backgrounds.Add(Arg); break;
	case EHUDeskJob::ArrestWarrant:
	case EHUDeskJob::SearchWarrant:
	{
		FWarrant W; W.Suspect = Arg; W.bArrest = Job == EHUDeskJob::ArrestWarrant; W.Who = Who;
		W.bApproved = Judge(C, this, Arg, W.bArrest, W.Reason);
		Warrants.Add(W);
		break;
	}
	}
}

bool UHUDeskRecords::IsPrint(const UHUCaseSubsystem* C, const FString& Clue)
{
	FHUClue Cl; return C && C->GetClue(Clue, Cl) && HasWord(Cl, { TEXT("print"), TEXT("smudge"), TEXT("latent"), TEXT("palm") });
}
bool UHUDeskRecords::IsPlate(const UHUCaseSubsystem* C, const FString& Clue)
{
	FHUClue Cl; return C && C->GetClue(Clue, Cl) && HasWord(Cl, { TEXT(" car"), TEXT("car "), TEXT("plate"), TEXT("vehicle"), TEXT("tire"), TEXT("tyre"), TEXT("parking"), TEXT("taxi"), TEXT("truck"), TEXT("sedan"), TEXT("license") });
}

FString UHUDeskRecords::Implicates(const UHUCaseSubsystem* C, const FString& Clue)
{
	if (!C) return FString();
	for (const FHUSuspect& S : C->Current.Suspects)
		for (const FHUStatement& St : S.Statements) if (!St.Truth && St.Evidence.Contains(Clue)) return S.Id;
	return FString();
}

static FString NameOf(const UHUCaseSubsystem* C, const FString& Id) { FHUSuspect S; return C && C->GetSuspect(Id, S) ? S.Name : Id; }

FString UHUDeskRecords::PrintResult(const UHUCaseSubsystem* C, const FString& Clue)
{
	const FString Who = Implicates(C, Clue);
	if (!Who.IsEmpty())
	{
		const bool bPriors = !C->SuspectRecord.FindRef(Who).IsEmpty();
		return FString::Printf(TEXT("AFIS MATCH (12 points): %s - on file from %s."), *NameOf(C, Who), bPriors ? TEXT("a prior arrest") : TEXT("a licensing background check"));
	}
	const uint32 H = Hash(Clue);
	if (H % 3 == 0) return FString::Printf(TEXT("AFIS hit: %s - on file, but nowhere near the scene that night. Dead end."), Strangers[H / 3 % UE_ARRAY_COUNT(Strangers)]);
	return TEXT("No match in AFIS. Too smudged, or not on file.");
}

FString UHUDeskRecords::PlateNumber(const FString& Clue)
{
	const uint32 H = Hash(Clue + TEXT("plate"));
	return FString::Printf(TEXT("%d%c%c%c%03d"), 1 + H % 9, TCHAR('A' + H / 9 % 26), TCHAR('A' + H / 234 % 26), TCHAR('A' + H / 6084 % 26), H / 158184 % 1000);
}

FString UHUDeskRecords::PlateResult(const UHUCaseSubsystem* C, const FString& Clue)
{
	const FString Who = Implicates(C, Clue);
	if (!Who.IsEmpty()) return FString::Printf(TEXT("DMV: plate %s is registered to %s. No stolen report on it."), *PlateNumber(Clue), *NameOf(C, Who));
	const uint32 H = Hash(Clue);
	if (H % 2) return FString::Printf(TEXT("DMV: plate %s - a rental, out to a tourist with a receipt across town. Dead end."), *PlateNumber(Clue));
	return FString::Printf(TEXT("DMV: plate %s is registered to %s, who has nothing to do with the case."), *PlateNumber(Clue), Strangers[H / 2 % UE_ARRAY_COUNT(Strangers)]);
}

FString UHUDeskRecords::LabResult(const UHUCaseSubsystem* C, const FString& Clue)
{
	FHUClue Cl;
	if (!C || !C->GetClue(Clue, Cl)) return FString();
	FString Out;
	const FString Sign = C->ClueSign.FindRef(Clue);
	if (!Sign.IsEmpty()) Out = Sign + TEXT(" ");
	const FString Who = Implicates(C, Clue);
	if (!Who.IsEmpty()) Out += FString::Printf(TEXT("Trace on it ties it to %s."), *NameOf(C, Who));
	else if (!C->Current.Weapon.IsEmpty() && (Cl.Name + Cl.Detail).ToLower().Contains(C->Current.Weapon.ToLower().Left(6))) Out += TEXT("Consistent with the murder weapon - but nothing on it names anyone.");
	else Out += TEXT("Nothing on it ties it to anyone in the case.");
	return Out;
}

TArray<FString> UHUDeskRecords::Background(const UHUCaseSubsystem* C, const FString& Id)
{
	TArray<FString> L;
	FHUSuspect S;
	if (!C || !C->GetSuspect(Id, S)) return L;
	const uint32 H = Hash(S.Name);
	L.Add(FString::Printf(TEXT("DOB %02d/%02d/19%02d   ·   %d %s"), 1 + H % 12, 1 + H / 12 % 28, 55 + H / 336 % 40, 10 + H / 13440 % 890, Streets[H / 7 % UE_ARRAY_COUNT(Streets)]));
	L.Add(TEXT("Employment: ") + S.Role);
	const FString Rec = C->SuspectRecord.FindRef(Id);
	L.Add(TEXT("Priors: ") + (Rec.IsEmpty() ? FString(TEXT("none on file")) : Rec));
	L.Add(FString::Printf(TEXT("Vehicle: plate %s"), *PlateNumber(S.Id)));
	// known associates: whoever else in the case they turn up with in the phone company's records
	TArray<FString> Assoc;
	for (const FHUSuspect& O : C->Current.Suspects) if (O.Id != Id && (Hash(O.Id + Id) % 3 == 0 || Hash(Id + O.Id) % 3 == 0)) Assoc.Add(O.Name);
	L.Add(TEXT("Known associates: ") + (Assoc.Num() ? FString::Join(Assoc, TEXT(", ")) : FString(TEXT("none in the case"))));
	return L;
}

bool UHUDeskRecords::Judge(const UHUCaseSubsystem* C, const UHUDeskRecords* R, const FString& Id, bool bArrest, FString& Reason)
{
	if (!C) { Reason = TEXT("No open case."); return false; }
	int32 Broken = 0;
	FHUSuspect S;
	if (C->GetSuspect(Id, S)) for (int32 i = 0; i < S.Statements.Num(); i++) if (C->IsBroken(Id, i)) Broken++;
	if (bArrest)
	{
		if (C->CanAccuse()) { Reason = FString::Printf(TEXT("Signed. %d lies on the record - enough to charge."), C->Breaks); return true; }
		Reason = FString::Printf(TEXT("Denied: %d of %d lies on the record. Break more stories first."), C->Breaks, C->Current.Solution.RequiredBreaks);
		return false;
	}
	// a search: one of their stories broken, or a print / plate that came back to them
	bool bHit = false;
	if (R)
	{
		for (const TPair<FString, FString>& P : R->Prints) if (Implicates(C, P.Key) == Id) bHit = true;
		for (const TPair<FString, FString>& P : R->Plates) if (Implicates(C, P.Key) == Id) bHit = true;
	}
	if (Broken > 0 || bHit || HUPhone::HasWarrant(C, Id)) { Reason = bHit && Broken == 0 ? TEXT("Signed. The AFIS / DMV hit is probable cause.") : TEXT("Signed. Their story's broken - that's probable cause."); return true; }
	Reason = TEXT("Denied: no probable cause. Break one of their stories, or get a print or plate that comes back to them.");
	return false;
}

FString UHUDeskRecords::Summary(const UHUCaseSubsystem* C, EHUDeskJob Job, const FString& Arg)
{
	FHUClue Cl; const bool bClue = C && C->GetClue(Arg, Cl);
	switch (Job)
	{
	case EHUDeskJob::Print: return FString::Printf(TEXT("ran prints on %s: %s"), bClue ? *Cl.Name : *Arg, *PrintResult(C, Arg));
	case EHUDeskJob::Plate: return FString::Printf(TEXT("ran a plate: %s"), *PlateResult(C, Arg));
	case EHUDeskJob::Lab: return FString::Printf(TEXT("sent %s to the lab (results in %d s)"), bClue ? *Cl.Name : *Arg, (int32)LabSeconds);
	case EHUDeskJob::Background: return FString::Printf(TEXT("pulled %s's full background"), *NameOf(C, Arg));
	case EHUDeskJob::ArrestWarrant:
	case EHUDeskJob::SearchWarrant:
	{
		FString Reason;
		const bool bOk = Judge(C, nullptr, Arg, Job == EHUDeskJob::ArrestWarrant, Reason);
		return FString::Printf(TEXT("filed a%s warrant for %s - %s"), Job == EHUDeskJob::ArrestWarrant ? TEXT("n arrest") : TEXT(" search"), *NameOf(C, Arg), bOk ? TEXT("SIGNED") : TEXT("denied"));
	}
	}
	return FString();
}
