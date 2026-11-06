#include "HUBrainSubsystem.h"
#include "HUCaseSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

FString UHUBrainSubsystem::Root() const { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI")); }

bool UHUBrainSubsystem::IsInstalled() const
{
	IPlatformFile& F = FPlatformFileManager::Get().GetPlatformFile();
	return F.FileExists(*(Root() / TEXT("llama/llama-server.exe"))) && F.FileExists(*(Root() / TEXT("models/suspect.gguf")));
}

void UHUBrainSubsystem::Deinitialize()
{
	if (Server.IsValid()) { FPlatformProcess::TerminateProc(Server, true); FPlatformProcess::CloseProc(Server); }
	Super::Deinitialize();
}

// start the local server in the background (once), then poll until it answers
void UHUBrainSubsystem::WarmUp()
{
	if (bReady || bStarting || !IsInstalled()) return;
	bStarting = true;
	StartedAt = FPlatformTime::Seconds();
	const FString Exe = Root() / TEXT("llama/llama-server.exe");
	const FString Args = FString::Printf(TEXT("-m \"%s\" --host 127.0.0.1 --port %d -c 4096 -ngl 99"), *(Root() / TEXT("models/suspect.gguf")), Port);
	Server = FPlatformProcess::CreateProc(*Exe, *Args, false, true, true, nullptr, 0, *(Root() / TEXT("llama")), nullptr);
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit brain: starting local model server (%s)"), Server.IsValid() ? TEXT("ok") : TEXT("failed"));
	Ping();
}

void UHUBrainSubsystem::Ping()
{
	if (bReady) return;
	if (FPlatformTime::Seconds() - StartedAt > 120.0) { bStarting = false; UE_LOG(LogTemp, Warning, TEXT("HomicideUnit brain: server never answered")); return; }
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/health"), Port));
	Req->SetVerb(TEXT("GET"));
	Req->SetTimeout(2.f);
	TWeakObjectPtr<UHUBrainSubsystem> Self(this);
	Req->OnProcessRequestComplete().BindLambda([Self](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (!Self.IsValid()) return;
		if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200) { Self->bReady = true; Self->bStarting = false; UE_LOG(LogTemp, Log, TEXT("HomicideUnit brain: ready")); return; }
		FTimerHandle H;
		if (UGameInstance* GI = Self->GetGameInstance()) GI->GetTimerManager().SetTimer(H, FTimerDelegate::CreateUObject(Self.Get(), &UHUBrainSubsystem::Ping), 1.5f, false);
	});
	Req->ProcessRequest();
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
