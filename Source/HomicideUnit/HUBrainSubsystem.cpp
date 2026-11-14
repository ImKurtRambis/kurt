#include "HUBrainSubsystem.h"
#include "HUCaseSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "HUProcess.h"

FString UHUBrainSubsystem::Root() const { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI")); }

bool UHUBrainSubsystem::IsInstalled() const
{
	if (FParse::Param(FCommandLine::Get(), TEXT("HUNoBrain"))) return false; // measuring the game without the model
	IPlatformFile& F = FPlatformFileManager::Get().GetPlatformFile();
	return F.FileExists(*(Root() / TEXT("llama/llama-server.exe"))) && F.FileExists(*(Root() / TEXT("models/suspect.gguf")));
}

void UHUBrainSubsystem::Deinitialize()
{
	for (FProcHandle* P : { &Server, &Lister }) if (P->IsValid()) { FPlatformProcess::TerminateProc(*P, true); FPlatformProcess::CloseProc(*P); }
	Super::Deinitialize();
}

// ---------- starting the model: on the graphics card, else on the processor ----------
// Ask llama-server which cards it can use (about a second), start it on the first choice, and poll until it answers.
// A choice that fails hands over to the next; with none left, suspects answer with their written lines.
void UHUBrainSubsystem::WarmUp()
{
	if (bReady || bStarting || !IsInstalled()) return;
	// every question calls this: after the card and the processor have both failed, wait five minutes before trying again
	if (GaveUpAt > 0.0 && FPlatformTime::Seconds() - GaveUpAt < 300.0) return;
	bStarting = true;
	StartedAt = FPlatformTime::Seconds();
	Plan.Reset(); Cards.Reset();
	FString Force;
	FParse::Value(FCommandLine::Get(), TEXT("HUBrain="), Force);
	bForced = !Force.IsEmpty();
	if (bForced)
	{
		Plan.Add(!Force.Equals(TEXT("cpu"), ESearchCase::IgnoreCase) && !Force.Equals(TEXT("processor"), ESearchCase::IgnoreCase));
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit brain: -HUBrain=%s - the %s, and nothing else"), *Force, Plan[0] ? TEXT("graphics card") : TEXT("processor"));
		StartNext();
		return;
	}
	// through cmd, so the list lands in a file (HUProcess::Launch ties the program to the game, and has no pipes);
	// a file of this game's own, as two copies on one PC (squad testing) ask at the same moment
	ListFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("BrainCards_%u.txt"), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ListFile), true);
	IFileManager::Get().Delete(*ListFile, false, true, true);
	FString Shell = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
	if (Shell.IsEmpty()) Shell = TEXT("cmd.exe");
	Lister = HUProcess::Launch(Shell, FString::Printf(TEXT("/c \"\"%s\" --list-devices > \"%s\" 2>&1\""), *(Root() / TEXT("llama/llama-server.exe")), *ListFile), Root() / TEXT("llama"));
	ListCards();
}

// wait for the list (20 s at most: a broken driver can hang it), then decide what to try
void UHUBrainSubsystem::ListCards()
{
	const bool bRunning = Lister.IsValid() && FPlatformProcess::IsProcRunning(Lister);
	if (bRunning && FPlatformTime::Seconds() - StartedAt < 20.0)
	{
		FTimerHandle H;
		GetGameInstance()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateUObject(this, &UHUBrainSubsystem::ListCards), 0.1f, false);
		return;
	}
	if (Lister.IsValid()) { if (bRunning) FPlatformProcess::TerminateProc(Lister, true); FPlatformProcess::CloseProc(Lister); }
	// "Available devices:", then a line a card:  "  Vulkan0: AMD Radeon RX 7600 (8176 MiB, 7310 MiB free)"
	FString Out;
	TArray<FString> Lines;
	FFileHelper::LoadFileToString(Out, *ListFile);
	IFileManager::Get().Delete(*ListFile, false, true, true);
	Out.ParseIntoArrayLines(Lines);
	bool bListed = false;
	for (const FString& L : Lines)
	{
		if (L.Contains(TEXT("Available devices"))) { bListed = true; continue; }
		if (bListed && L.Contains(TEXT(": "))) Cards += (Cards.IsEmpty() ? TEXT("") : TEXT(", ")) + L.TrimStartAndEnd();
	}
	// An empty list: no card llama.cpp can use, so straight to the processor. No list at all (it hung, or a newer
	// llama.cpp words it differently): the card is still tried first.
	if (bListed && Cards.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit brain: llama.cpp found no graphics card it can use"));
	}
	else
	{
		Plan.Add(true);
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit brain: graphics cards llama.cpp can use: %s"), Cards.IsEmpty() ? TEXT("(it didn't say)") : *Cards);
	}
	Plan.Add(false);
	StartNext();
}

void UHUBrainSubsystem::StartNext()
{
	if (Plan.Num() == 0)
	{
		bStarting = false;
		GaveUpAt = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Warning, TEXT("HomicideUnit brain: the model wouldn't run on this PC - suspects answer with their written lines"));
		return;
	}
	bOnCard = Plan[0];
	Plan.RemoveAt(0);
	AttemptAt = FPlatformTime::Seconds();
	const FString Exe = Root() / TEXT("llama/llama-server.exe");
	FString Args = FString::Printf(TEXT("-m \"%s\" --host 127.0.0.1 --port %d -c 4096"), *(Root() / TEXT("models/suspect.gguf")), Port);
	// the card: every layer on it. The processor: no card at all, and under the game's priority, so frames stay smooth
	Args += bOnCard ? TEXT(" -ngl 99") : TEXT(" --device none -ngl 0 --prio -1");
#if !UE_BUILD_SHIPPING
	if (bOnCard && FParse::Param(FCommandLine::Get(), TEXT("HUBrainBreakCard"))) Args += TEXT(" --device NoSuchCard"); // to test the hand-over
#endif
	Server = HUProcess::Launch(Exe, Args, Root() / TEXT("llama")); // ends with the game, even after a crash
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit brain: starting the model on the %s (%s)"), bOnCard ? TEXT("graphics card") : TEXT("processor"), Server.IsValid() ? TEXT("ok") : TEXT("failed"));
	Ping();
}

void UHUBrainSubsystem::GiveUpOn(const FString& Why)
{
	UE_LOG(LogTemp, Warning, TEXT("HomicideUnit brain: %s on the %s%s"), *Why, bOnCard ? TEXT("graphics card") : TEXT("processor"), Plan.Num() ? TEXT(" - trying the processor") : TEXT(""));
	if (Server.IsValid()) { FPlatformProcess::TerminateProc(Server, true); FPlatformProcess::CloseProc(Server); }
	// a moment for Windows to free the port before the next server takes it
	FTimerHandle H;
	if (UGameInstance* GI = GetGameInstance()) GI->GetTimerManager().SetTimer(H, FTimerDelegate::CreateUObject(this, &UHUBrainSubsystem::StartNext), 0.5f, false);
}

void UHUBrainSubsystem::Ping()
{
	if (bReady || !bStarting) return;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/health"), Port));
	Req->SetVerb(TEXT("GET"));
	Req->SetTimeout(2.f);
	TWeakObjectPtr<UHUBrainSubsystem> Self(this);
	Req->OnProcessRequestComplete().BindLambda([Self](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (!Self.IsValid() || Self->bReady || !Self->bStarting) return;
		const int32 Code = bOk && Resp.IsValid() ? Resp->GetResponseCode() : 0;
		if (Code == 200)
		{
			// a card that has the processor to fall back on must first show it's the faster of the two
			if (Self->bOnCard && !Self->bForced && Self->Plan.Num() > 0) Self->CheckSpeed(); else Self->NowReady(0.0);
			return;
		}
		// Nobody on the port and our server gone: it died (no usable card, a driver fault). Somebody answering "not
		// yet" is a server still loading the model: ours, or another copy of the game's on this PC, which we share.
		if (Code == 0 && (!Self->Server.IsValid() || !FPlatformProcess::IsProcRunning(Self->Server))) { Self->GiveUpOn(TEXT("the model's server stopped")); return; }
		if (FPlatformTime::Seconds() - Self->AttemptAt > 120.0) { Self->GiveUpOn(TEXT("the model never answered")); return; }
		FTimerHandle H;
		if (UGameInstance* GI = Self->GetGameInstance()) GI->GetTimerManager().SetTimer(H, FTimerDelegate::CreateUObject(Self.Get(), &UHUBrainSubsystem::Ping), 1.f, false);
	});
	Req->ProcessRequest();
}

// A card can run the model and still be the wrong place for it: an old integrated chip, or a card so short of memory
// that the model spills into shared memory, writes slower than the processor would. So ask for a few words and time
// them. (-HUBrainMinSpeed=<tokens a second> moves the bar: 9999 sends any card to the processor, to test this.)
void UHUBrainSubsystem::CheckSpeed()
{
	float MinSpeed = 6.f; // tokens a second: a desktop processor on its own writes 8 to 16
	FParse::Value(FCommandLine::Get(), TEXT("HUBrainMinSpeed="), MinSpeed);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/v1/chat/completions"), Port));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(TEXT("{\"messages\": [{\"role\": \"user\", \"content\": \"Count from one to thirty, in words.\"}], \"max_tokens\": 24, \"temperature\": 0}"));
	Req->SetTimeout(30.f);
	TWeakObjectPtr<UHUBrainSubsystem> Self(this);
	Req->OnProcessRequestComplete().BindLambda([Self, MinSpeed](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (!Self.IsValid() || Self->bReady || !Self->bStarting) return;
		double Speed = 0.0;
		int32 Wrote = 0;
		TSharedPtr<FJsonObject> Obj;
		const bool bAnswered = bOk && Resp.IsValid() && Resp->GetResponseCode() == 200 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Obj) && Obj.IsValid();
		if (const TSharedPtr<FJsonObject>* T; bAnswered && Obj->TryGetObjectField(TEXT("timings"), T)) { (*T)->TryGetNumberField(TEXT("predicted_per_second"), Speed); (*T)->TryGetNumberField(TEXT("predicted_n"), Wrote); }
		if (!bAnswered) Self->GiveUpOn(TEXT("the model didn't answer a test question in 30 s"));
		else if (Wrote >= 8 && Speed < MinSpeed) Self->GiveUpOn(FString::Printf(TEXT("the model writes only %.1f tokens a second"), Speed));
		else Self->NowReady(Speed); // (too few words to time, or a server that doesn't report timings: keep the card)
	});
	Req->ProcessRequest();
}

void UHUBrainSubsystem::NowReady(double TokensPerSecond)
{
	bReady = true; bStarting = false;
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit brain: ready on the %s after %.1f s%s"), bOnCard ? TEXT("graphics card") : TEXT("processor"), FPlatformTime::Seconds() - StartedAt,
		TokensPerSecond > 0.0 ? *FString::Printf(TEXT(" - it writes %.0f tokens a second"), TokensPerSecond) : TEXT(""));
}

// what the suspect knows, what they're hiding, how they're holding up - and the rules
FString UHUBrainSubsystem::SystemPrompt(const FString& Id) const
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	FHUSuspect S;
	if (!C || !C->GetSuspect(Id, S)) return TEXT("");
	const FHUCase& K = C->Current;
	const bool bCulprit = Id == K.Solution.Killer;
	const bool bConfessed = C->Confessed == Id;
	const EHUSuspectMood Mood = C->MoodOf(Id);
	FString P = FString::Printf(TEXT("You are %s, %s. Tonight at %s there was a %s: %s, around %s. The police are holding everyone who was there, and a detective is interviewing you.\n\n"),
		*S.Name, *S.Role, *K.Place, *K.Crime, *K.Victim, *K.Time);
	P += TEXT("What you have already told the detective:\n");
	for (int32 i = 0; i < S.Statements.Num(); i++) P += FString::Printf(TEXT("%d. \"%s\"\n"), i + 1, *S.Statements[i].Text);
	P += TEXT("\nWhat only you know:\n");
	for (int32 i = 0; i < S.Statements.Num(); i++)
	{
		const FHUStatement& St = S.Statements[i];
		if (St.Truth) continue;
		if (C->IsBroken(Id, i)) P += FString::Printf(TEXT("- Statement %d was a lie and the detective has PROVEN it. The truth: %s You may admit this, grudgingly.\n"), i + 1, *St.OnBreak);
		else P += FString::Printf(TEXT("- Statement %d is a lie you are protecting. Do not admit it. If pushed, deflect or repeat it.\n"), i + 1);
	}
	if (bConfessed) P += TEXT("- You have confessed to the crime. You may talk about it, with regret.\n");
	else if (bCulprit) P += TEXT("- You committed the crime. You will NOT admit it, no matter what.\n");
	else P += TEXT("- You did not commit the crime, and you don't know who did.\n");
	const TCHAR* How = Mood == EHUSuspectMood::Calm ? TEXT("composed and a little annoyed; short, steady answers")
		: Mood == EHUSuspectMood::Defensive ? TEXT("curt and evasive; you resent the questions")
		: Mood == EHUSuspectMood::Rattled ? TEXT("nervous: you hesitate, stammer a little, lose your train of thought")
		: Mood == EHUSuspectMood::Cracking ? TEXT("close to breaking: frantic, broken sentences, pleading")
		: Mood == EHUSuspectMood::Lawyered ? TEXT("done talking: you only say you want your lawyer")
		: TEXT("broken: exhausted, quiet, honest about what has been proven");
	P += FString::Printf(TEXT("\nHow you are right now: %s.\n\n"), How);
	P += TEXT("Rules: answer as spoken words only, in 1 to 3 short sentences. No actions, no narration, no asterisks. Never say you are an AI. Never invent evidence, people or events that are not above. If you don't know something, say so.");
	return P;
}

// ---------- prewritten answers ----------
namespace
{
	struct FTopic { const TCHAR* Ask; const TCHAR* Keys[6]; };
	// the questions every detective asks; a question matches a topic if it contains one of its keys
	const FTopic Topics[] = {
		{ TEXT("Did you do it?"), { TEXT("did you do it"), TEXT("did you kill"), TEXT("you did it"), TEXT("was it you"), TEXT("did you hurt"), TEXT("are you responsible") } },
		{ TEXT("Who do you think did this?"), { TEXT("who did"), TEXT("who do you think"), TEXT("who would"), TEXT("anyone want"), TEXT("any idea who"), nullptr } },
		{ TEXT("Can anyone vouch for you?"), { TEXT("vouch"), TEXT("alibi"), TEXT("prove it"), TEXT("anyone see you"), TEXT("who were you with"), TEXT("witness") } },
		{ TEXT("Did you see or hear anything tonight?"), { TEXT("see anything"), TEXT("hear anything"), TEXT("notice anything"), TEXT("did you see"), TEXT("did you hear"), TEXT("what did you see") } },
		{ TEXT("How did you know the victim?"), { TEXT("know the victim"), TEXT("know him"), TEXT("know her"), TEXT("relationship"), TEXT("how do you know"), TEXT("how did you know") } },
		{ TEXT("Where were you when it happened?"), { TEXT("where were you"), TEXT("where was you"), TEXT("when it happened"), TEXT("at the time"), TEXT("what were you doing"), nullptr } },
	};
	int32 TopicOf(const FString& Said)
	{
		const FString L = Said.ToLower();
		for (int32 t = 0; t < UE_ARRAY_COUNT(Topics); t++)
			for (const TCHAR* K : Topics[t].Keys) if (K && L.Contains(K)) return t;
		return INDEX_NONE;
	}
}

// what the answer depends on: their mood, which of their lies are broken, a confession
uint32 UHUBrainSubsystem::StateOf(const FString& Id) const
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	FHUSuspect S;
	if (!C || !C->GetSuspect(Id, S)) return 0;
	uint32 H = (uint32)C->MoodOf(Id) * 7 + (C->Confessed == Id ? 1000 : 0);
	for (int32 i = 0; i < S.Statements.Num(); i++) if (C->IsBroken(Id, i)) H += 1u << (i + 4);
	return H;
}

void UHUBrainSubsystem::StartPrewrite()
{
	Pre.Reset(); PreQueue.Reset();
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!C || !IsInstalled()) return;
	// topic by topic across everyone, so the likeliest questions are ready for everybody first
	for (int32 t = UE_ARRAY_COUNT(Topics) - 1; t >= 0; t--)
		for (const FHUSuspect& S : C->Current.Suspects) PreQueue.Add({ S.Id, t });
	WarmUp();
	GetGameInstance()->GetTimerManager().SetTimer(PumpTimer, FTimerDelegate::CreateUObject(this, &UHUBrainSubsystem::Pump), 0.5f, true);
}

void UHUBrainSubsystem::StopPrewrite()
{
	PreQueue.Reset(); Pre.Reset();
	if (UGameInstance* GI = GetGameInstance()) GI->GetTimerManager().ClearTimer(PumpTimer);
}

// one answer at a time, never while a detective is waiting on a live one
void UHUBrainSubsystem::Pump()
{
	if (!bReady || bPreBusy || LiveBusy > 0) return;
	while (PreQueue.Num())
	{
		const TPair<FString, int32> Job = PreQueue[0];
		PreQueue.RemoveAt(0);
		const FString Key = FString::Printf(TEXT("%s|%d"), *Job.Key, Job.Value);
		const uint32 State = StateOf(Job.Key);
		if (const FPre* Have = Pre.Find(Key); Have && Have->State == State) continue; // still good
		bPreBusy = true;
		TWeakObjectPtr<UHUBrainSubsystem> Self(this);
		Send(Job.Key, Topics[Job.Value].Ask, FString(), false, FHUBrainReact::CreateLambda([Self, Key, State](const FHUReaction& R)
		{
			if (!Self.IsValid()) return;
			Self->bPreBusy = false;
			if (!R.Say.IsEmpty()) Self->Pre.Add(Key, { R, State });
			UE_LOG(LogTemp, Log, TEXT("HomicideUnit brain: prewrote %s (%d ready, %d to go): %s"), *Key, Self->Pre.Num(), Self->PreQueue.Num(), *R.Say);
		}));
		return;
	}
}

bool UHUBrainSubsystem::TakeReady(const FString& SuspectId, const FString& Said, FHUReaction& Out)
{
	const int32 T = TopicOf(Said);
	if (T == INDEX_NONE) return false;
	const FString Key = FString::Printf(TEXT("%s|%d"), *SuspectId, T);
	FPre Got;
	const bool bHave = Pre.RemoveAndCopyValue(Key, Got);
	PreQueue.Insert({ SuspectId, T }, 0); // used or stale: write the next one (it'll know this conversation)
	if (!bHave || Got.State != StateOf(SuspectId)) return false;
	Out = Got.R;
	TArray<TPair<FString, FString>>& H = History.FindOrAdd(SuspectId);
	H.Add({ TEXT("The detective says: \"") + Said + TEXT("\""), Out.Say });
	if (H.Num() > 6) H.RemoveAt(0);
	return true;
}

// ---------- the thinking brain: words + emotion + body action, as JSON ----------
void UHUBrainSubsystem::React(const FString& SuspectId, const FString& Said, const FString& Situation, FHUBrainReact Done)
{
	if (!bReady) { WarmUp(); Done.ExecuteIfBound(FHUReaction()); return; }
	LiveBusy++;
	TWeakObjectPtr<UHUBrainSubsystem> Self(this);
	Send(SuspectId, Said, Situation, true, FHUBrainReact::CreateLambda([Self, Done](const FHUReaction& R)
	{
		if (Self.IsValid()) Self->LiveBusy = FMath::Max(0, Self->LiveBusy - 1);
		Done.ExecuteIfBound(R);
	}));
}

void UHUBrainSubsystem::Send(const FString& SuspectId, const FString& Said, const FString& Situation, bool bRemember, FHUBrainReact Done)
{
	FString Emotions, Actions;
	for (uint8 i = 0; i < HUReact::EmotionCount; i++) Emotions += (i ? TEXT(", ") : TEXT("")) + FString(HUReact::EmotionName(i));
	for (uint8 i = 0; i < HUReact::ActionCount; i++) Actions += (i ? TEXT(", ") : TEXT("")) + FString(HUReact::ActionName(i));
	FString Sys = SystemPrompt(SuspectId);
	Sys += FString::Printf(TEXT("\n\nYou are in the room with the detective, and they talk to you out loud. Reply ONLY with a JSON object:\n")
		TEXT("{\"say\": \"your spoken words, 1 to 3 short sentences\", \"emotion\": one of [%s], \"action\": one of [%s]}\n")
		TEXT("\"action\" is what your body does as you answer. If the detective gives you an order (sit down, stand up, show me your hands, look at me, calm down), ")
		TEXT("decide whether you obey from how you are right now: if you obey, do that action (sit, stand, hands_up, face_you...); if you refuse, say so and pick another action. ")
		TEXT("When a line starts with GAME:, it is what really just happened - it is true, react to it, and keep to its meaning."), *Emotions, *Actions);

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Msgs;
	auto Msg = [&Msgs](const TCHAR* Role, const FString& Text) { TSharedRef<FJsonObject> M = MakeShared<FJsonObject>(); M->SetStringField(TEXT("role"), Role); M->SetStringField(TEXT("content"), Text); Msgs.Add(MakeShared<FJsonValueObject>(M)); };
	Msg(TEXT("system"), Sys);
	for (const TPair<FString, FString>& QA : History.FindRef(SuspectId)) { Msg(TEXT("user"), QA.Key); Msg(TEXT("assistant"), FString::Printf(TEXT("{\"say\": \"%s\", \"emotion\": \"calm\", \"action\": \"none\"}"), *QA.Value.Replace(TEXT("\""), TEXT("'")))); }
	const FString User = (Situation.IsEmpty() ? FString() : TEXT("GAME: ") + Situation + TEXT("\n")) + TEXT("The detective says: \"") + Said + TEXT("\"");
	Msg(TEXT("user"), User);
	Body->SetArrayField(TEXT("messages"), Msgs);
	Body->SetNumberField(TEXT("max_tokens"), 160);
	Body->SetNumberField(TEXT("temperature"), 0.8);
	if (SampleSeed >= 0) Body->SetNumberField(TEXT("seed"), SampleSeed);
	{ TSharedRef<FJsonObject> RF = MakeShared<FJsonObject>(); RF->SetStringField(TEXT("type"), TEXT("json_object")); Body->SetObjectField(TEXT("response_format"), RF); }
	FString Json;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/v1/chat/completions"), Port));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(Json);
	Req->SetTimeout(40.f);
	Req->SetActivityTimeout(40.f); // the answer arrives all at once: without this Unreal gives up after 30 s of silence, which a slow processor can need
	TWeakObjectPtr<UHUBrainSubsystem> Self(this);
	const FString Key = Said;
	const double Asked = FPlatformTime::Seconds();
	Req->OnProcessRequestComplete().BindLambda([Self, SuspectId, Key, Done, bRemember, Asked](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		FHUReaction R;
		FString Content;
		TSharedPtr<FJsonObject> Obj;
		if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Obj) && Obj.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Choices;
			if (Obj->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
				if (const TSharedPtr<FJsonObject>* M; (*Choices)[0]->AsObject()->TryGetObjectField(TEXT("message"), M)) Content = (*M)->GetStringField(TEXT("content"));
			// how fast this PC answers, in the log: the number to look at when a player says suspects are slow
			double Speed = 0.0; int32 Wrote = 0;
			if (const TSharedPtr<FJsonObject>* T; Obj->TryGetObjectField(TEXT("timings"), T)) { (*T)->TryGetNumberField(TEXT("predicted_per_second"), Speed); (*T)->TryGetNumberField(TEXT("predicted_n"), Wrote); }
			if (Self.IsValid()) UE_LOG(LogTemp, Log, TEXT("HomicideUnit brain: answered in %.2f s (%d tokens at %.0f a second, on the %s)"), FPlatformTime::Seconds() - Asked, Wrote, Speed, *Self->RunsOn());
		}
		// the model's JSON (if it wrapped it in anything, cut to the braces)
		const int32 A = Content.Find(TEXT("{")), B = Content.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		TSharedPtr<FJsonObject> J;
		if (A >= 0 && B > A && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Content.Mid(A, B - A + 1)), J) && J.IsValid())
		{
			R.Say = J->GetStringField(TEXT("say"));
			R.Emotion = HUReact::EmotionFrom(J->GetStringField(TEXT("emotion")));
			R.Action = HUReact::ActionFrom(J->GetStringField(TEXT("action")));
		}
		else R.Say = Content; // plain words: still usable
		int32 X = 0, Y = 0;
		while (R.Say.FindChar('*', X) && R.Say.Mid(X + 1).FindChar('*', Y)) R.Say.RemoveAt(X, Y + 2);
		R.Say.ReplaceInline(TEXT("\""), TEXT(""));
		R.Say.TrimStartAndEndInline();
		if (R.Say.Len() > 320) R.Say = R.Say.Left(320) + TEXT("...");
		if (bRemember && Self.IsValid() && !R.Say.IsEmpty())
		{
			TArray<TPair<FString, FString>>& H = Self->History.FindOrAdd(SuspectId);
			H.Add({ TEXT("The detective says: \"") + Key + TEXT("\""), R.Say });
			if (H.Num() > 6) H.RemoveAt(0);
		}
		Done.ExecuteIfBound(R);
	});
	Req->ProcessRequest();
}

void UHUBrainSubsystem::AskSuspect(const FString& SuspectId, const FString& Question, FHUBrainReply Done)
{
	if (!bReady) { WarmUp(); Done.ExecuteIfBound(FString()); return; }
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Msgs;
	auto Msg = [&Msgs](const TCHAR* Role, const FString& Text) { TSharedRef<FJsonObject> M = MakeShared<FJsonObject>(); M->SetStringField(TEXT("role"), Role); M->SetStringField(TEXT("content"), Text); Msgs.Add(MakeShared<FJsonValueObject>(M)); };
	Msg(TEXT("system"), SystemPrompt(SuspectId));
	for (const TPair<FString, FString>& QA : History.FindRef(SuspectId)) { Msg(TEXT("user"), QA.Key); Msg(TEXT("assistant"), QA.Value); }
	Msg(TEXT("user"), Question);
	Body->SetArrayField(TEXT("messages"), Msgs);
	Body->SetNumberField(TEXT("max_tokens"), 110);
	Body->SetNumberField(TEXT("temperature"), 0.8);
	FString Json;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/v1/chat/completions"), Port));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(Json);
	Req->SetTimeout(40.f);
	Req->SetActivityTimeout(40.f);
	TWeakObjectPtr<UHUBrainSubsystem> Self(this);
	Req->OnProcessRequestComplete().BindLambda([Self, SuspectId, Question, Done](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		FString Reply;
		TSharedPtr<FJsonObject> Obj;
		if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Obj) && Obj.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Choices;
			if (Obj->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
				if (const TSharedPtr<FJsonObject>* M; (*Choices)[0]->AsObject()->TryGetObjectField(TEXT("message"), M)) Reply = (*M)->GetStringField(TEXT("content"));
		}
		// tidy it: no stage directions, no wrapping quotes, not too long
		int32 A = 0, B = 0;
		while (Reply.FindChar('*', A) && Reply.Mid(A + 1).FindChar('*', B)) Reply.RemoveAt(A, B + 2);
		Reply.TrimStartAndEndInline();
		Reply.ReplaceInline(TEXT("\""), TEXT(""));
		if (Reply.Len() > 320) Reply = Reply.Left(320) + TEXT("...");
		if (Self.IsValid() && !Reply.IsEmpty())
		{
			TArray<TPair<FString, FString>>& H = Self->History.FindOrAdd(SuspectId);
			H.Add({ Question, Reply });
			if (H.Num() > 6) H.RemoveAt(0);
		}
		Done.ExecuteIfBound(Reply);
	});
	Req->ProcessRequest();
}

#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"
// HU.Perf: every 5 seconds, log the frame times and whether the brain was writing - for the system requirements
static FTSTicker::FDelegateHandle GHUPerf;
static FAutoConsoleCommand GHUPerfCmd(TEXT("HU.Perf"), TEXT("Toggle a frame-time log every 5 s (with the brain's state)"), FConsoleCommandDelegate::CreateLambda([]()
{
	if (GHUPerf.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(GHUPerf); GHUPerf.Reset(); return; }
	struct FAcc { double Sum = 0, Worst = 0, T = 0; int32 N = 0, Slow = 0, Busy = 0; };
	TSharedRef<FAcc> A = MakeShared<FAcc>();
	GHUPerf = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([A](float Dt)
	{
		const UHUBrainSubsystem* B = nullptr;
		if (GEngine) for (const FWorldContext& Ctx : GEngine->GetWorldContexts()) if (Ctx.OwningGameInstance) { B = Ctx.OwningGameInstance->GetSubsystem<UHUBrainSubsystem>(); break; }
		const double Ms = FApp::GetDeltaTime() * 1000.0;
		A->Sum += Ms; A->N++; A->Worst = FMath::Max(A->Worst, Ms); A->T += Dt;
		if (Ms > 33.4) A->Slow++;
		if (B && B->IsWriting()) A->Busy++;
		if (A->T >= 5.0 && A->N > 0)
		{
			const double Avg = A->Sum / A->N;
			UE_LOG(LogTemp, Display, TEXT("HU.Perf: avg %.1f ms (%.0f fps), worst %.1f ms, %d of %d frames over 33 ms, brain writing %d%% of the time, %d answers ready"),
				Avg, 1000.0 / Avg, A->Worst, A->Slow, A->N, A->Busy * 100 / A->N, B ? B->ReadyCount() : 0);
			*A = FAcc();
		}
		return true;
	}));
}));
#endif
