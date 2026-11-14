#include "HUInterviewBench.h"
#include "HUCaseSubsystem.h"
#include "HUBrainSubsystem.h"
#include "HUInterrogation.h"
#include "HUReaction.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameInstance.h"

static const int32 BenchSeeds[] = { 1101, 2203, 3307, 4409, 5501, 6607, 7703, 8807 };

bool UHUInterviewBench::ShouldCreateSubsystem(UObject* Outer) const
{
	return FParse::Param(FCommandLine::Get(), TEXT("HUInterviewBench"));
}

UHUCaseSubsystem* UHUInterviewBench::Cases() const { return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr; }
UHUBrainSubsystem* UHUInterviewBench::Brain() const { return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUBrainSubsystem>() : nullptr; }

void UHUInterviewBench::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UHUCaseSubsystem>();
	Collection.InitializeDependency<UHUBrainSubsystem>();
	// the script: Tools/InterviewBench/questions.json
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Json, *(FPaths::ProjectDir() / TEXT("Tools/InterviewBench/questions.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid())
		for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("lines")))
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FLine L;
			L.Id = O->GetStringField(TEXT("id"));
			L.Ask = O->GetStringField(TEXT("ask"));
			O->TryGetStringField(TEXT("do"), L.Do);
			O->TryGetStringArrayField(TEXT("kinds"), L.Kinds);
			O->TryGetStringArrayField(TEXT("checks"), L.Checks);
			Lines.Add(L);
		}
	int32 N = 2;
	FParse::Value(FCommandLine::Get(), TEXT("HUBenchCases="), N);
	for (int32 i = 0; i < FMath::Clamp(N, 1, (int32)UE_ARRAY_COUNT(BenchSeeds)); i++) Seeds.Add(BenchSeeds[i]);
	if (!FParse::Value(FCommandLine::Get(), TEXT("HUBenchOut="), OutDir)) OutDir = FPaths::ProjectSavedDir() / TEXT("InterviewBench");
	OutDir = FPaths::ConvertRelativePathToFull(OutDir);
	IFileManager::Get().MakeDirectory(*OutDir, true);
	TurnsFile = OutDir / TEXT("turns.jsonl");
	IFileManager::Get().Delete(*TurnsFile);
	StartedAt = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("HUInterviewBench: %d lines, %d cases -> %s"), Lines.Num(), Seeds.Num(), *OutDir);
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UHUInterviewBench::Tick), 0.2f);
}

void UHUInterviewBench::Deinitialize()
{
	if (Ticker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
	Super::Deinitialize();
}

bool UHUInterviewBench::Tick(float DeltaSeconds)
{
	if (bDone) return false;
	UHUBrainSubsystem* B = Brain();
	if (!B || !Cases()) return true;
	if (Lines.Num() == 0) { Finish(TEXT("Tools/InterviewBench/questions.json is missing or empty")); return false; }
	if (!bBrainUp)
	{
		if (!B->IsInstalled()) { Finish(TEXT("the brain isn't installed (AI\\llama\\llama-server.exe, AI\\models\\suspect.gguf)")); return false; }
		B->WarmUp();
		if (!B->IsReady()) { if (FPlatformTime::Seconds() - StartedAt > 300.0) Finish(TEXT("the brain's server never answered")); return !bDone; }
		bBrainUp = true;
		B->SampleSeed = 42; // the same model gives the same answers run to run
		if (!NextCase()) { Finish(TEXT("no cases")); return false; }
	}
	if (bWaiting)
	{
		if (FPlatformTime::Seconds() - AskedAt > 90.0)
		{
			UE_LOG(LogTemp, Warning, TEXT("HUInterviewBench: no answer in 90 s - moving on"));
			bWaiting = false; LineAt++; ++Turns; // a late answer is ignored (its token is stale)
		}
		return true;
	}
	Step();
	return !bDone;
}

// a new case from the next fixed seed, and one suspect of each kind from it
bool UHUInterviewBench::NextCase()
{
	UHUCaseSubsystem* C = Cases();
	if (++SeedAt >= Seeds.Num()) return false;
	C->ChosenPlace.Reset();
	C->ChosenDifficulty.Reset();
	C->UseSquadSeed(Seeds[SeedAt]);
	C->StartCase(0);
	Brain()->StopPrewrite(); // live answers only
	Brain()->ResetConversations();
	const FHUCase& K = C->Current;
	Runs.Reset();
	RunAt = 0;
	auto HasLie = [](const FHUSuspect& S) { for (const FHUStatement& St : S.Statements) if (!St.Truth) return true; return false; };
	for (const FHUSuspect& S : K.Suspects) if (S.Id == K.Solution.Killer) Runs.Add({ S.Id, TEXT("culprit") });
	for (const FHUSuspect& S : K.Suspects) if (S.Id != K.Solution.Killer && HasLie(S)) { Runs.Add({ S.Id, TEXT("liar") }); break; }
	for (const FHUSuspect& S : K.Suspects) if (S.Id != K.Solution.Killer && !HasLie(S) && S.Statements.Num()) { Runs.Add({ S.Id, TEXT("honest") }); break; }

	// the case as the scorer needs it: everything that's true about it
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("type"), TEXT("case"));
	O->SetNumberField(TEXT("seed"), Seeds[SeedAt]);
	O->SetNumberField(TEXT("number"), K.Number);
	O->SetStringField(TEXT("place"), K.Place);
	O->SetStringField(TEXT("crime"), K.Crime);
	O->SetStringField(TEXT("victim"), K.Victim);
	O->SetStringField(TEXT("time"), K.Time);
	O->SetStringField(TEXT("weapon"), K.Weapon);
	O->SetStringField(TEXT("culprit"), K.Solution.Killer);
	TArray<TSharedPtr<FJsonValue>> Sus, Clues, Picked;
	for (const FHUSuspect& S : K.Suspects)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("id"), S.Id); J->SetStringField(TEXT("name"), S.Name); J->SetStringField(TEXT("role"), S.Role); J->SetStringField(TEXT("intro"), S.Intro);
		TArray<TSharedPtr<FJsonValue>> Sts;
		for (const FHUStatement& St : S.Statements)
		{
			TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
			T->SetStringField(TEXT("text"), St.Text); T->SetBoolField(TEXT("truth"), St.Truth); T->SetStringField(TEXT("onbreak"), St.OnBreak);
			Sts.Add(MakeShared<FJsonValueObject>(T));
		}
		J->SetArrayField(TEXT("statements"), Sts);
		Sus.Add(MakeShared<FJsonValueObject>(J));
	}
	for (const FHUClue& Cl : K.Clues)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("id"), Cl.Id); J->SetStringField(TEXT("name"), Cl.Name); J->SetStringField(TEXT("detail"), Cl.Detail);
		Clues.Add(MakeShared<FJsonValueObject>(J));
	}
	for (const FRun& R : Runs) Picked.Add(MakeShared<FJsonValueString>(R.Kind + TEXT(":") + R.SuspectId));
	O->SetArrayField(TEXT("suspects"), Sus);
	O->SetArrayField(TEXT("clues"), Clues);
	O->SetArrayField(TEXT("interviewed"), Picked);
	Write(O);
	UE_LOG(LogTemp, Display, TEXT("HUInterviewBench: case %d (seed %d) at %s - interviewing %d suspects"), K.Number, Seeds[SeedAt], *K.Place, Runs.Num());
	StartRun();
	return true;
}

void UHUInterviewBench::StartRun()
{
	Script.Reset();
	LineAt = 0;
	LastBroken = INDEX_NONE;
	if (!Runs.IsValidIndex(RunAt)) return;
	const FString& Kind = Runs[RunAt].Kind;
	for (int32 i = 0; i < Lines.Num(); i++)
		if (Lines[i].Kinds.Num() == 0 || Lines[i].Kinds.Contains(TEXT("all")) || Lines[i].Kinds.Contains(Kind)) Script.Add(i);
}

// the placeholders a line can use
FString UHUInterviewBench::Fill(const FString& Text, const FHUSuspect& S, int32 Statement) const
{
	const FHUCase& K = Cases()->Current;
	FString Victim = K.Victim; int32 Comma;
	if (Victim.FindChar(',', Comma)) Victim = Victim.Left(Comma);
	// "THE MIRAMAR DINER" -> "the Miramar Diner", "RAINES STABLES" -> "Raines Stables"
	TArray<FString> Words; K.Place.ToLower().ParseIntoArray(Words, TEXT(" "));
	for (int32 i = 0; i < Words.Num(); i++)
		if (!(i == 0 && Words[i] == TEXT("the")) && Words[i] != TEXT("of") && Words[i] != TEXT("and")) Words[i][0] = FChar::ToUpper(Words[i][0]);
	const FString Place = FString::Join(Words, TEXT(" "));
	const FString Crime = K.Crime.Contains(TEXT("homicide")) ? TEXT("the killing") : K.Crime.Contains(TEXT("assault")) ? TEXT("the assault")
		: K.Crime.Contains(TEXT("robbery")) ? TEXT("the robbery") : K.Crime.Contains(TEXT("arson")) ? TEXT("the fire") : TEXT("what happened to ") + Victim;
	FString Other;
	for (const FHUSuspect& O : K.Suspects) if (O.Id != S.Id) { Other = O.Name; break; }
	const FString St = S.Statements.IsValidIndex(Statement) ? S.Statements[Statement].Text : FString();
	return Text.Replace(TEXT("{victim}"), *Victim).Replace(TEXT("{place}"), *Place).Replace(TEXT("{time}"), *K.Time).Replace(TEXT("{crime}"), *Crime)
		.Replace(TEXT("{other}"), *Other).Replace(TEXT("{name}"), *S.Name).Replace(TEXT("{lie}"), *St).Replace(TEXT("{stmt}"), *St);
}

// one line of the script: do what the game would do, then ask the brain exactly as the interview does
void UHUInterviewBench::Step()
{
	UHUCaseSubsystem* C = Cases();
	UHUBrainSubsystem* B = Brain();
	// next line, else the next suspect, else the next case
	while (LineAt >= Script.Num())
	{
		if (++RunAt < Runs.Num()) { StartRun(); continue; }
		if (!NextCase()) { Finish(FString()); return; }
	}
	const FRun Run = Runs[RunAt];
	const FLine& L = Lines[Script[LineAt]];
	FHUSuspect S;
	if (!C->GetSuspect(Run.SuspectId, S)) { LineAt++; return; }

	// which statement the line is about: an unbroken lie, or a true statement
	auto FirstLie = [&]() { for (int32 i = 0; i < S.Statements.Num(); i++) if (!S.Statements[i].Truth && !C->IsBroken(S.Id, i)) return i; return (int32)INDEX_NONE; };
	auto FirstTrue = [&]() { for (int32 i = 0; i < S.Statements.Num(); i++) if (S.Statements[i].Truth) return i; return (int32)INDEX_NONE; };
	const bool bAboutLie = L.Do == TEXT("press_lie") || L.Do == TEXT("confront_right") || L.Do == TEXT("confront_wrong") || L.Ask.Contains(TEXT("{lie}"));
	const bool bAboutTrue = L.Do == TEXT("press_true") || L.Do == TEXT("confront_true") || L.Ask.Contains(TEXT("{stmt}"));
	int32 St = bAboutLie || L.Checks.Contains(TEXT("keeps_lie")) ? FirstLie() : bAboutTrue || L.Checks.Contains(TEXT("holds_truth")) ? FirstTrue() : INDEX_NONE;
	if ((bAboutLie || bAboutTrue) && St == INDEX_NONE) { LineAt++; return; } // nothing for this line to be about here

	// the game's move, and the GAME: line it sends with it
	const bool bWasBroken = S.Statements.IsValidIndex(St) && C->IsBroken(S.Id, St);
	FString Situation, GameReply, ClueId;
	if (L.Do.StartsWith(TEXT("press_")))
	{
		GameReply = C->Press(S.Id, St);
		Situation = C->InterrogationSituation(S.Id, 0, St, FString(), bWasBroken, GameReply);
	}
	else if (L.Do.StartsWith(TEXT("confront_")))
	{
		const FHUStatement& T = S.Statements[St];
		if (L.Do == TEXT("confront_right")) { if (T.Evidence.Num()) ClueId = T.Evidence[0]; }
		else for (const FHUClue& K : C->Current.Clues) if (!T.Evidence.Contains(K.Id)) { ClueId = K.Id; break; }
		if (ClueId.IsEmpty()) { LineAt++; return; }
		C->Found.AddUnique(ClueId); // the squad has it in a bag
		GameReply = C->Confront(S.Id, St, ClueId);
		Situation = C->InterrogationSituation(S.Id, 0, St, ClueId, bWasBroken, GameReply);
	}
	else if (L.Do == TEXT("record"))
	{
		GameReply = C->RaiseRecord(S.Id);
		Situation = C->InterrogationSituation(S.Id, 1, INDEX_NONE, FString(), false, GameReply);
	}
	const bool bBroke = !bWasBroken && S.Statements.IsValidIndex(St) && C->IsBroken(S.Id, St);
	if (bBroke) LastBroken = St;

	// the turn as the scorer will see it
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("type"), TEXT("turn"));
	O->SetNumberField(TEXT("seed"), Seeds[SeedAt]);
	O->SetNumberField(TEXT("case"), C->Current.Number);
	O->SetStringField(TEXT("suspect"), S.Id);
	O->SetStringField(TEXT("name"), S.Name);
	O->SetStringField(TEXT("kind"), Run.Kind);
	O->SetStringField(TEXT("line"), L.Id);
	O->SetStringField(TEXT("do"), L.Do);
	TArray<TSharedPtr<FJsonValue>> Checks;
	for (const FString& Ck : L.Checks) Checks.Add(MakeShared<FJsonValueString>(Ck));
	O->SetArrayField(TEXT("checks"), Checks);
	const FString Said = Fill(L.Ask, S, St);
	O->SetStringField(TEXT("said"), Said);
	O->SetStringField(TEXT("situation"), Situation);
	O->SetStringField(TEXT("game_reply"), GameReply);
	if (S.Statements.IsValidIndex(St))
	{
		TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
		T->SetNumberField(TEXT("index"), St); T->SetStringField(TEXT("text"), S.Statements[St].Text);
		T->SetBoolField(TEXT("truth"), S.Statements[St].Truth); T->SetStringField(TEXT("onbreak"), S.Statements[St].OnBreak);
		T->SetBoolField(TEXT("broken"), C->IsBroken(S.Id, St));
		O->SetObjectField(TEXT("statement"), T);
	}
	if (S.Statements.IsValidIndex(LastBroken))
	{
		TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
		T->SetStringField(TEXT("text"), S.Statements[LastBroken].Text); T->SetStringField(TEXT("onbreak"), S.Statements[LastBroken].OnBreak);
		O->SetObjectField(TEXT("last_broken"), T);
	}
	O->SetBoolField(TEXT("broke"), bBroke);
	O->SetBoolField(TEXT("confessed"), C->Confessed == S.Id);
	O->SetBoolField(TEXT("lawyered"), C->Lawyered.Contains(S.Id));
	static const TCHAR* Moods[] = { TEXT("calm"), TEXT("defensive"), TEXT("rattled"), TEXT("cracking"), TEXT("broken"), TEXT("lawyered") };
	O->SetStringField(TEXT("mood"), Moods[FMath::Clamp((int32)C->MoodOf(S.Id), 0, 5)]);
	O->SetNumberField(TEXT("pressure"), C->GetPressure(S.Id));
	O->SetStringField(TEXT("prompt"), B->PromptFor(S.Id));

	bWaiting = true;
	AskedAt = FPlatformTime::Seconds();
	const int32 Token = ++Turns;
	B->React(S.Id, Said, Situation, FHUBrainReact::CreateWeakLambda(this, [this, Token, O](const FHUReaction& R)
	{
		if (Token != Turns || !bWaiting) return; // it already timed out
		O->SetStringField(TEXT("reply"), R.Say);
		O->SetStringField(TEXT("emotion"), HUReact::EmotionName(R.Emotion));
		O->SetStringField(TEXT("action"), HUReact::ActionName(R.Action));
		O->SetNumberField(TEXT("ms"), FMath::RoundToInt((FPlatformTime::Seconds() - AskedAt) * 1000.0));
		Write(O);
		UE_LOG(LogTemp, Display, TEXT("HUInterviewBench %d: %s %s  \"%s\"  ->  \"%s\""), Token, *O->GetStringField(TEXT("kind")), *O->GetStringField(TEXT("line")), *O->GetStringField(TEXT("said")), *R.Say);
		bWaiting = false;
		LineAt++;
	}));
}

void UHUInterviewBench::Write(const TSharedRef<FJsonObject>& Obj)
{
	FString S;
	FJsonSerializer::Serialize(Obj, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&S));
	FFileHelper::SaveStringToFile(S + TEXT("\n"), *TurnsFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

void UHUInterviewBench::Finish(const FString& Why)
{
	if (bDone) return;
	bDone = true;
	// which model answered - so a report always says what it measured
	const FString Model = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/models/suspect.gguf"));
	TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
	M->SetStringField(TEXT("error"), Why);
	M->SetNumberField(TEXT("turns"), Turns);
	M->SetNumberField(TEXT("seconds"), FMath::RoundToInt(FPlatformTime::Seconds() - StartedAt));
	M->SetNumberField(TEXT("cases"), FMath::Max(0, FMath::Min(SeedAt + 1, Seeds.Num())));
	M->SetStringField(TEXT("model"), FPaths::GetCleanFilename(Model));
	M->SetNumberField(TEXT("model_bytes"), (double)IFileManager::Get().FileSize(*Model));
	M->SetStringField(TEXT("model_date"), IFileManager::Get().GetTimeStamp(*Model).ToIso8601());
	M->SetNumberField(TEXT("lines"), Lines.Num());
	FString S;
	FJsonSerializer::Serialize(M, TJsonWriterFactory<>::Create(&S));
	FFileHelper::SaveStringToFile(S, *(OutDir / TEXT("meta.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display, TEXT("HUInterviewBench: done%s - %d turns in %.0f s"), Why.IsEmpty() ? TEXT("") : *(TEXT(" (") + Why + TEXT(")")), Turns, FPlatformTime::Seconds() - StartedAt);
	FPlatformMisc::RequestExit(false);
}
