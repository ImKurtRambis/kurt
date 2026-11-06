#include "HUSuspectActor.h"
#include "HUBuild.h"
#include "HUCaseSubsystem.h"
#include "HUInteractComponent.h"
#include "HUBrainSubsystem.h"
#include "HUVoiceSubsystem.h"
#include "HUListenSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "HUInterrogation.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"

using namespace HUBuild;

AHUSuspectActor::AHUSuspectActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	NameTag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(Root);
	NameTag->SetRelativeLocation(FVector(0, 0, 205));
	NameTag->SetHorizontalAlignment(EHTA_Center);
	NameTag->SetWorldSize(14.f);
	NameTag->SetTextRenderColor(FColor(232, 193, 90));
}

void AHUSuspectActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Pose();
}

// the figure: arms at the sides, or behind the back in cuffs
void AHUSuspectActor::Pose()
{
	Clear(Blocks);
	const FLinearColor Skin(0.6f, 0.42f, 0.32f), Pants(0.05f, 0.05f, 0.06f), Steel(0.75f, 0.77f, 0.8f);
	Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(0, 0, 45), FVector(40, 46, 90), Pants);
	Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(0, 0, 125), FVector(52, 60, 76), Coat);
	Block(this, RootComponent, Blocks, EShape::Sphere, FVector(0, 0, 178), FVector(26, 24, 30), Skin);
	if (!bCuffed)
	{
		for (float S : { -1.f, 1.f }) Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(0, S * 32, 118), FVector(12, 12, 62), Coat * 0.9f);
	}
	else
	{
		// arms drawn back, wrists together behind, the cuffs on
		for (float S : { -1.f, 1.f }) Block(this, RootComponent, Blocks, EShape::Cylinder, FVector(-18, S * 22, 115), FVector(12, 12, 60), Coat * 0.9f, FRotator(0, 0, S * 22));
		Block(this, RootComponent, Blocks, EShape::Cube, FVector(-30, 0, 90), FVector(6, 14, 5), Steel);
	}
	if (bCuffed) for (UStaticMeshComponent* B : Blocks) if (B) B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // don't trip the officer walking them
}

void AHUSuspectActor::BeginPlay()
{
	Super::BeginPlay();
	NameTag->SetText(FText::FromString(DisplayName().ToUpper()));
}

FString AHUSuspectActor::DisplayName() const
{
	FHUSuspect S;
	const UHUCaseSubsystem* Cases = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr;
	return Cases && Cases->GetSuspect(SuspectId, S) ? S.Name : FString(TEXT("the suspect"));
}

FText AHUSuspectActor::GetPrompt_Implementation(APawn* User) const
{
	FHUSuspect S;
	const UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!Cases || !Cases->GetSuspect(SuspectId, S)) return FText::GetEmpty();
	if (bCuffed)
		return FText::FromString(IsEscortedBy(User) ? FString::Printf(TEXT("[E] Let go of %s  ·  [Q] Take the cuffs off"), *S.Name)
			: FString::Printf(TEXT("[E] Take %s by the arm  ·  [Q] Take the cuffs off"), *S.Name));
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (I && I->IsHolding(EHUTool::Handcuffs))
		return FText::FromString(FString::Printf(TEXT("[E] Arrest %s — put the cuffs on  ·  [Q] Run their record"), *S.Name));
	return FText::FromString(FString::Printf(TEXT("[E] Talk to %s%s  ·  [Q] Run their record"), *S.Name, S.Role.IsEmpty() ? TEXT("") : *(TEXT(", ") + S.Role)));
}

void AHUSuspectActor::Interact_Implementation(APawn* User, bool bSecondary)
{
	FHUSuspect S;
	const UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!Cases || !Cases->GetSuspect(SuspectId, S) || !I) return;

	if (bInterviewing && Interviewer.Get() == User && (bTyping || bThinking)) return; // typing / waiting on an answer
	if (bSecondary && bInterviewing && Interviewer.Get() == User) { CloseInterview(); return; }
	if (bCuffed)
	{
		if (bSecondary)
		{
			bCuffed = false; Escort = nullptr; Pose();
			I->Say(FString::Printf(TEXT("You take the cuffs off %s. \"About time.\""), *S.Name));
			return;
		}
		if (IsEscortedBy(User)) { Escort = nullptr; I->Say(FString::Printf(TEXT("You let go of %s's arm. They stay put."), *S.Name)); }
		else { Escort = User; I->PlayReach(); I->Say(FString::Printf(TEXT("You take %s by the arm. Walk them out to the van."), *S.Name)); }
		return;
	}
	// Q: run their record - does it fit the profile the evidence is building?
	if (bSecondary)
	{
		const FString* Rec = Cases->SuspectRecord.Find(SuspectId);
		I->Say(FString::Printf(TEXT("RECORD — %s: %s."), *S.Name.ToUpper(), Rec ? **Rec : TEXT("no record")));
		return;
	}
	// holding the cuffs: arrest
	if (I->IsHolding(EHUTool::Handcuffs))
	{
		bCuffed = true;
		Escort = User;
		Pose();
		I->PlayReach();
		const TCHAR* Protest[] = { TEXT("\"You're making a mistake!\""), TEXT("\"I want a lawyer. Now.\""), TEXT("\"Easy — easy! I'm not resisting.\""), TEXT("\"You've got the wrong person, detective.\""), TEXT("\"...\"") };
		I->Say(FString::Printf(TEXT("%s:  %s"), *S.Name.ToUpper(), Protest[FMath::RandRange(0, UE_ARRAY_COUNT(Protest) - 1)]));
		I->Notify(TEXT("ARRESTED"), FString::Printf(TEXT("%s — walk them to the van outside"), *S.Name), FLinearColor(1.f, 0.7f, 0.3f));
		return;
	}
	// otherwise: the interview (opened only on the screen of whoever's doing it)
	if (!User->IsLocallyControlled()) return;
	if (bInterviewing && Interviewer.Get() == User) { SendMove(0); return; } // E in the interview: confront / press
	OpenInterview(User);
	I->Say(FString::Printf(TEXT("%s:  \"%s\""), *S.Name.ToUpper(), *S.Intro));
}

// ---------- the interview ----------
TArray<FString> AHUSuspectActor::BaggedIds() const
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	return C ? C->Found : TArray<FString>();
}

void AHUSuspectActor::OpenInterview(APawn* User)
{
	APlayerController* PC = Cast<APlayerController>(User->GetController());
	if (!PC) return;
	bInterviewing = true; Interviewer = User; StatementSel = 0; EvidenceSel = BaggedIds().Num() > 0 ? 0 : -1; LastReply.Reset();
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	if (UHUInteractComponent* I = User->FindComponentByClass<UHUInteractComponent>()) I->Interviewing = this;
	EnableInput(PC);
	if (InputComponent && InputComponent->KeyBindings.Num() == 0)
	{
		InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AHUSuspectActor::StmtUp);
		InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AHUSuspectActor::StmtUp);
		InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AHUSuspectActor::StmtDown);
		InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AHUSuspectActor::StmtDown);
		InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AHUSuspectActor::EvLeft);
		InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AHUSuspectActor::EvLeft);
		InputComponent->BindKey(EKeys::D, IE_Pressed, this, &AHUSuspectActor::EvRight);
		InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AHUSuspectActor::EvRight);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AHUSuspectActor::RecordKey);
		InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AHUSuspectActor::TypeKey);
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AHUSuspectActor::Q1);
		InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AHUSuspectActor::Q2);
		InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AHUSuspectActor::Q3);
		InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AHUSuspectActor::Q4);
		InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AHUSuspectActor::Q5);
		InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AHUSuspectActor::Q6);
		InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AHUSuspectActor::MicDown);
		InputComponent->BindKey(EKeys::V, IE_Released, this, &AHUSuspectActor::MicUp);
	}
	bTyping = false; Draft.Reset(); bMicOpen = false; Heard.Reset();
	if (UHUBrainSubsystem* B = GetGameInstance()->GetSubsystem<UHUBrainSubsystem>()) B->WarmUp();
}

void AHUSuspectActor::CloseInterview()
{
	if (APawn* U = Interviewer.Get())
	{
		if (APlayerController* PC = Cast<APlayerController>(U->GetController())) { PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); DisableInput(PC); }
		if (UHUInteractComponent* I = U->FindComponentByClass<UHUInteractComponent>()) I->Interviewing = nullptr;
	}
	bInterviewing = false; Interviewer = nullptr;
}

// typing a question: the other interview keys stand down until Enter / Esc
void AHUSuspectActor::TypeKey(FKey Key)
{
	if (!bInterviewing || bThinking) return;
	if (!bTyping) { if (Key == EKeys::T) { bTyping = true; Draft.Reset(); } return; }
	if (Key == EKeys::Enter) { AskTyped(); return; }
	if (Key == EKeys::Escape) { bTyping = false; Draft.Reset(); return; }
	if (Key == EKeys::BackSpace) { if (Draft.Len()) Draft.LeftChopInline(1); return; }
	if (Draft.Len() >= 140) return;
	const APlayerController* P = Interviewer.IsValid() ? Cast<APlayerController>(Interviewer->GetController()) : nullptr;
	const bool bShift = P && (P->IsInputKeyDown(EKeys::LeftShift) || P->IsInputKeyDown(EKeys::RightShift));
	const FString K = Key.GetFName().ToString();
	static const TMap<FString, FString> Map = {
		{ TEXT("Zero"), TEXT("0") }, { TEXT("One"), TEXT("1") }, { TEXT("Two"), TEXT("2") }, { TEXT("Three"), TEXT("3") }, { TEXT("Four"), TEXT("4") },
		{ TEXT("Five"), TEXT("5") }, { TEXT("Six"), TEXT("6") }, { TEXT("Seven"), TEXT("7") }, { TEXT("Eight"), TEXT("8") }, { TEXT("Nine"), TEXT("9") },
		{ TEXT("SpaceBar"), TEXT(" ") }, { TEXT("Period"), TEXT(".") }, { TEXT("Comma"), TEXT(",") }, { TEXT("Apostrophe"), TEXT("'") }, { TEXT("Hyphen"), TEXT("-") } };
	if (K.Len() == 1 && FChar::IsAlpha(K[0])) Draft += (bShift || Draft.IsEmpty()) ? K.ToUpper() : K.ToLower();
	else if (K == TEXT("Slash")) Draft += bShift ? TEXT("?") : TEXT("/");
	else if (K == TEXT("One") && bShift) Draft += TEXT("!");
	else if (const FString* C = Map.Find(K)) Draft += *C;
}

void AHUSuspectActor::AskTyped()
{
	bTyping = false;
	const FString Typed = Draft.TrimStartAndEnd();
	Draft.Reset();
	if (!Typed.IsEmpty()) Ask(Typed, Typed);
}

void AHUSuspectActor::Ask(const FString& Q, const FString& Prompt)
{
	if (Q.IsEmpty() || bThinking) return;
	UHUBrainSubsystem* B = GetGameInstance()->GetSubsystem<UHUBrainSubsystem>();
	TWeakObjectPtr<AHUSuspectActor> Self(this);
	TWeakObjectPtr<APawn> Asker = Interviewer;
	auto Deliver = [Self, Asker, Q](const FString& Answer)
	{
		if (!Self.IsValid()) return;
		Self->bThinking = false;
		FString A = Answer;
		if (A.IsEmpty())
		{
			// no brain installed (or it failed): a written line for their mood
			const UHUCaseSubsystem* C = Self->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
			const EHUSuspectMood M = C ? C->MoodOf(Self->SuspectId) : EHUSuspectMood::Calm;
			A = M == EHUSuspectMood::Lawyered ? TEXT("I want my lawyer.") : HUInterrogation::PressLine(M, GetTypeHash(Q));
		}
		UHUInteractComponent* I = Asker.IsValid() ? Asker->FindComponentByClass<UHUInteractComponent>() : nullptr;
		if (I) I->SendAnswer(Self->GetPathName(), Q, A);
	};
	if (B && B->IsReady()) { bThinking = true; LastReply = TEXT("..."); B->AskSuspect(SuspectId, Prompt, FHUBrainReply::CreateLambda(Deliver)); }
	else { if (B) B->WarmUp(); Deliver(FString()); }
}

void AHUSuspectActor::ApplyAnswer(APawn* User, const FString& Question, const FString& Answer)
{
	FHUSuspect S;
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!C || !C->GetSuspect(SuspectId, S)) return;
	if (User && User->IsLocallyControlled()) LastReply = Answer;
	if (UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr)
		I->Say(FString::Printf(TEXT("\"%s\"   %s:  %s"), *Question, *S.Name.ToUpper(), *Answer));
	Speak(Answer);
}

void AHUSuspectActor::StmtUp() { FHUSuspect S; if (bInterviewing &&GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()->GetSuspect(SuspectId, S)) StatementSel = (StatementSel + S.Statements.Num() - 1) % FMath::Max(1, S.Statements.Num()); }
void AHUSuspectActor::StmtDown() { FHUSuspect S; if (bInterviewing && !bTyping && GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()->GetSuspect(SuspectId, S)) StatementSel = (StatementSel + 1) % FMath::Max(1, S.Statements.Num()); }
void AHUSuspectActor::EvLeft() { if (bInterviewing && !bTyping) { const int32 N = BaggedIds().Num(); EvidenceSel = EvidenceSel <= -1 ? N - 1 : EvidenceSel - 1; } }
void AHUSuspectActor::EvRight() { if (bInterviewing && !bTyping) { const int32 N = BaggedIds().Num(); EvidenceSel = EvidenceSel >= N - 1 ? -1 : EvidenceSel + 1; } }
void AHUSuspectActor::RecordKey() { if (bInterviewing && !bTyping && !bThinking) SendMove(1); }

void AHUSuspectActor::SendMove(uint8 Action)
{
	const TArray<FString> Bag = BaggedIds();
	const FString Clue = Action == 0 && Bag.IsValidIndex(EvidenceSel) ? Bag[EvidenceSel] : FString();
	if (UHUInteractComponent* I = Interviewer.IsValid() ? Interviewer->FindComponentByClass<UHUInteractComponent>() : nullptr)
		I->SendInterrogation(GetPathName(), Action, StatementSel, Clue);
}

void AHUSuspectActor::ApplyInterrogation(APawn* User, uint8 Action, int32 Statement, const FString& ClueId)
{
	UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	FHUSuspect S;
	if (!C || !C->GetSuspect(SuspectId, S)) return;
	const int32 BreaksBefore = C->Breaks;
	const bool bConfessedBefore = !C->Confessed.IsEmpty();
	const FString Reply = Action == 1 ? C->RaiseRecord(SuspectId) : !ClueId.IsEmpty() ? C->Confront(SuspectId, Statement, ClueId) : C->Press(SuspectId, Statement);
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return;
	if (User->IsLocallyControlled()) LastReply = Reply;
	I->Say(FString::Printf(TEXT("%s:  %s"), *S.Name.ToUpper(), *Reply));
	Speak(Reply);
	if (C->Breaks > BreaksBefore) I->Notify(TEXT("LIE ON THE RECORD"), FString::Printf(TEXT("%s — %d of %d lies"), *S.Name, C->Breaks, C->Current.Solution.RequiredBreaks), FLinearColor(0.95f, 0.35f, 0.3f));
	if (!bConfessedBefore && !C->Confessed.IsEmpty()) I->Notify(TEXT("CONFESSION"), FString::Printf(TEXT("%s confessed. Cuff them, van, book, submit."), *S.Name), FLinearColor(0.95f, 0.76f, 0.32f));
}

void AHUSuspectActor::EndPlay(const EEndPlayReason::Type Reason)
{
	CloseInterview();
	Super::EndPlay(Reason);
}

void AHUSuspectActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bInterviewing) { const APawn* V = Interviewer.Get(); if (!V || FVector::Dist2D(V->GetActorLocation(), GetActorLocation()) > 450.f) CloseInterview(); }
	// sprayed / tased: bent forward, then slowly straightening up
	if (StunLeft > 0.f)
	{
		StunLeft -= DeltaSeconds;
		const float Bend = StunLeft > 1.f ? -26.f : 0.f;
		const FRotator Now = GetActorRotation();
		SetActorRotation(StunLeft > 0.f ? FMath::RInterpTo(Now, FRotator(Bend, Now.Yaw, 0), DeltaSeconds, StunLeft > 1.f ? 12.f : 4.f) : FRotator(0, Now.Yaw, 0));
	}
	const APawn* E = Escort.Get();
	if (!bCuffed || !E) return;
	// a step behind and to the side of whoever's walking them, facing the way they're going
	const FVector Behind = E->GetActorLocation() - E->GetActorForwardVector() * 95.f + E->GetActorRightVector() * 40.f;
	FVector Target(Behind.X, Behind.Y, GetActorLocation().Z);
	const FVector Now = GetActorLocation();
	const FVector Next = FMath::VInterpTo(Now, Target, DeltaSeconds, 5.f);
	SetActorLocation(Next);
	if (FVector::Dist2D(Now, Next) > 0.5f) SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0, (Next - Now).Rotation().Yaw, 0), DeltaSeconds, 8.f));
}

void AHUSuspectActor::Subdue(APawn* User, bool bTaser)
{
	if (bCuffed || IsHidden()) return;
	if (bInterviewing) CloseInterview();
	StunLeft = bTaser ? 5.f : 7.f;
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return;
	const TCHAR* Pepper[] = { TEXT("\"Agh — my eyes! My EYES!\""), TEXT("*coughing* \"What the hell, man?!\""), TEXT("\"I can't see! I can't — \"") };
	const TCHAR* Taser[] = { TEXT("\"AAGH—\""), TEXT("*drops to a knee, shaking*"), TEXT("\"Okay! Okay! I'm down!\"") };
	I->Say(FString::Printf(TEXT("%s:  %s   (Subdued a few seconds — cuff them now.)"), *DisplayName().ToUpper(),
		bTaser ? Taser[FMath::RandRange(0, 2)] : Pepper[FMath::RandRange(0, 2)]));
}

void AHUSuspectActor::LoadIntoVan()
{
	if (UHUCaseSubsystem* Cases = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) Cases->Arrested.AddUnique(SuspectId);
	Escort = nullptr;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

// say it out loud, here in the room: every player''s PC voices it locally (free). Nerves speed them up, breaking slows them.
void AHUSuspectActor::Speak(const FString& Said)
{
	UHUVoiceSubsystem* V = GetGameInstance()->GetSubsystem<UHUVoiceSubsystem>();
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!V || !C || !V->HasNeuralVoice() || GetNetMode() == NM_DedicatedServer) return;
	const EHUSuspectMood M = C->MoodOf(SuspectId);
	const float Speed = M == EHUSuspectMood::Rattled ? 1.1f : M == EHUSuspectMood::Cracking ? 1.2f : M == EHUSuspectMood::Broken ? 0.85f : M == EHUSuspectMood::Defensive ? 1.05f : 1.f;
	// a voice per suspect (the 911 caller and the captain use others)
	const float Emotion = M == EHUSuspectMood::Calm ? 0.35f : M == EHUSuspectMood::Defensive ? 0.5f : M == EHUSuspectMood::Rattled ? 0.65f : M == EHUSuspectMood::Cracking ? 0.85f : 0.5f;
	const int32 VoiceNum = 1 + (int32)(GetTypeHash(SuspectId) % 10);
	TWeakObjectPtr<AHUSuspectActor> Self(this);
	V->Synthesize(Said, VoiceNum, FString(), false, FHUVoiceReady::CreateLambda([Self](USoundWave* S, float)
	{
		if (!Self.IsValid() || !S) return;
		if (Self->VoiceAudio.IsValid()) Self->VoiceAudio->Stop();
		Self->VoiceAudio = UGameplayStatics::SpawnSoundAtLocation(Self.Get(), S, Self->GetActorLocation() + FVector(0, 0, 170));
	}), Speed, Emotion);
}
// ---------- the question list ----------
static const TCHAR* QShort[] = { TEXT("Where were you when it happened?"), TEXT("How did you know the victim?"), TEXT("Did you see or hear anything?"),
	TEXT("Who else was there?"), TEXT("Tell me more about what you said."), TEXT("Why did you lie to me?") };

TArray<FString> AHUSuspectActor::QuestionLabels() const
{
	TArray<FString> L;
	for (int32 i = 0; i < 6; i++) L.Add(QShort[i]);
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	FHUSuspect S;
	bool bAnyLie = false;
	if (C && C->GetSuspect(SuspectId, S)) for (int32 i = 0; i < S.Statements.Num(); i++) bAnyLie |= C->IsBroken(SuspectId, i);
	if (!bAnyLie) L[5] = TEXT("Is there anything you're not telling me?");
	return L;
}

// what the brain is actually asked (the selected statement is spelled out for "tell me more")
FString AHUSuspectActor::QuestionText(int32 Index) const
{
	const TArray<FString> L = QuestionLabels();
	if (Index != 4) return L.IsValidIndex(Index) ? L[Index] : FString();
	FHUSuspect S;
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (C && C->GetSuspect(SuspectId, S) && S.Statements.IsValidIndex(StatementSel))
		return FString::Printf(TEXT("You told me: \"%s\" Tell me more about that."), *S.Statements[StatementSel].Text);
	return L[4];
}

void AHUSuspectActor::AskPreset(int32 Index)
{
	if (!bInterviewing || bTyping || bThinking || bMicOpen) return;
	const TArray<FString> L = QuestionLabels();
	if (L.IsValidIndex(Index)) Ask(L[Index], QuestionText(Index));
}
void AHUSuspectActor::Q1() { AskPreset(0); } void AHUSuspectActor::Q2() { AskPreset(1); } void AHUSuspectActor::Q3() { AskPreset(2); }
void AHUSuspectActor::Q4() { AskPreset(3); } void AHUSuspectActor::Q5() { AskPreset(4); } void AHUSuspectActor::Q6() { AskPreset(5); }

// the closest preset to what was said: key words for each question (-1: nothing close)
int32 AHUSuspectActor::MatchQuestion(const FString& Said) const
{
	static const TArray<TArray<FString>> Keys = {
		{ TEXT("where"), TEXT("were"), TEXT("alibi"), TEXT("night"), TEXT("happened"), TEXT("when"), TEXT("time"), TEXT("doing") },
		{ TEXT("know"), TEXT("knew"), TEXT("victim"), TEXT("relationship"), TEXT("friend"), TEXT("meet"), TEXT("met") },
		{ TEXT("see"), TEXT("saw"), TEXT("hear"), TEXT("heard"), TEXT("notice"), TEXT("noticed"), TEXT("anything") },
		{ TEXT("who"), TEXT("else"), TEXT("anyone"), TEXT("somebody"), TEXT("someone"), TEXT("people") },
		{ TEXT("more"), TEXT("tell"), TEXT("explain"), TEXT("said"), TEXT("statement"), TEXT("story"), TEXT("details") },
		{ TEXT("lie"), TEXT("lied"), TEXT("lying"), TEXT("truth"), TEXT("hiding"), TEXT("honest"), TEXT("not") } };
	TArray<FString> Words;
	FString Clean;
	for (TCHAR Ch : Said.ToLower()) Clean.AppendChar(FChar::IsAlpha(Ch) || Ch == '\'' ? Ch : ' ');
	Clean.ParseIntoArrayWS(Words);
	int32 Best = -1, BestScore = 0;
	for (int32 q = 0; q < Keys.Num(); q++)
	{
		int32 Score = 0;
		for (const FString& W : Words) if (Keys[q].Contains(W)) Score++;
		if (Score > BestScore) { Best = q; BestScore = Score; }
	}
	return Best;
}

void AHUSuspectActor::MicDown()
{
	if (!bInterviewing || bTyping || bThinking || bMicOpen) return;
	UHUListenSubsystem* L = GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
	if (!L || !L->IsEnabled()) return;
	if (L->StartListening()) { bMicOpen = true; Heard.Reset(); }
}

void AHUSuspectActor::MicUp()
{
	if (!bMicOpen) return;
	bMicOpen = false;
	UHUListenSubsystem* L = GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
	if (!L) return;
	Heard = TEXT("...");
	TWeakObjectPtr<AHUSuspectActor> Self(this);
	L->StopListening(FHUHeard::CreateLambda([Self](const FString& Text)
	{
		if (!Self.IsValid() || !Self->bInterviewing) return;
		const int32 Q = Self->MatchQuestion(Text);
		Self->Heard = Text.IsEmpty() ? FString(TEXT("(didn't catch that)")) : Q < 0 ? FString::Printf(TEXT("\"%s\" — not one of the questions"), *Text) : Text;
		if (Q >= 0) Self->AskPreset(Q);
	}));
}