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
	// rest positions, for acting out reactions (see Animate)
	BaseXf.Reset();
	for (UStaticMeshComponent* B : Blocks) if (B) BaseXf.Add(B->GetRelativeTransform());
}

void AHUSuspectActor::BeginPlay()
{
	Super::BeginPlay();
	NameTag->SetText(FText::FromString(DisplayName().ToUpper()));
	FaceYaw = GetActorRotation().Yaw;
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
	if (bInterviewing && Interviewer.Get() == User) return; // in the interview you talk (V), not press keys
	OpenInterview(User);
	I->Say(FString::Printf(TEXT("%s:  \"%s\"     (hold V and talk to them)"), *S.Name.ToUpper(), *S.Intro));
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
	bInterviewing = true; Interviewer = User; LastReply.Reset();
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	if (UHUInteractComponent* I = User->FindComponentByClass<UHUInteractComponent>()) I->Interviewing = this;
	EnableInput(PC);
	if (InputComponent && InputComponent->KeyBindings.Num() == 0)
	{
		InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AHUSuspectActor::TypeKey);
		InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AHUSuspectActor::MicDown);
		InputComponent->BindKey(EKeys::V, IE_Released, this, &AHUSuspectActor::MicUp);
	}
	bTyping = false; Draft.Reset(); bMicOpen = false; Heard.Reset();
	bFacing = true; // they turn to face you
	if (UHUListenSubsystem* L = GetGameInstance()->GetSubsystem<UHUListenSubsystem>(); L && !L->IsInstalled())
		if (UHUInteractComponent* I = User->FindComponentByClass<UHUInteractComponent>()) I->Notify(TEXT("NO MICROPHONE TOOLS"), TEXT("Speech-to-text isn't installed (see AI\\README.txt). Press T to type to them instead."), FLinearColor(0.95f, 0.6f, 0.3f));
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
	bFacing = false; Stance = 0; Arms = 0; Gesture = 0; GestureT = -1.f; // back to how they were standing
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
	if (!Typed.IsEmpty()) Handle(Typed);
}

// ---------- what you said ----------
static TArray<FString> WordsOf(const FString& S)
{
	FString Clean;
	for (TCHAR Ch : S.ToLower()) Clean.AppendChar(FChar::IsAlnum(Ch) || Ch == '\'' ? Ch : ' ');
	TArray<FString> W; Clean.ParseIntoArrayWS(W);
	return W;
}
static bool SaysAny(const FString& Said, std::initializer_list<const TCHAR*> Phrases)
{
	const FString L = TEXT(" ") + FString::Join(WordsOf(Said), TEXT(" ")) + TEXT(" ");
	for (const TCHAR* P : Phrases) if (L.Contains(FString(TEXT(" ")) + P + TEXT(" "))) return true;
	return false;
}
static bool Filler(const FString& W)
{
	static const TSet<FString> F = { TEXT("the"), TEXT("this"), TEXT("that"), TEXT("these"), TEXT("those"), TEXT("with"), TEXT("from"), TEXT("your"), TEXT("their"), TEXT("about"), TEXT("what"), TEXT("explain"),
		TEXT("have"), TEXT("there"), TEXT("where"), TEXT("when"), TEXT("near"), TEXT("found"), TEXT("some"), TEXT("piece"), TEXT("left"), TEXT("right"), TEXT("back"), TEXT("room"), TEXT("floor") };
	return W.Len() < 4 || F.Contains(W);
}

// a piece of evidence you've bagged that you named (the most matching words in its name; "" if none)
FString AHUSuspectActor::MatchClue(const FString& Said) const
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!C) return FString();
	const TArray<FString> W = WordsOf(Said);
	FString Best; int32 BestScore = 0;
	for (const FString& Id : BaggedIds())
	{
		FHUClue K; if (!C->GetClue(Id, K)) continue;
		int32 Score = 0;
		for (const FString& N : WordsOf(K.Name))
		{
			if (Filler(N)) continue;
			for (const FString& S : W) if (S == N || (N.Len() > 4 && S.Len() > 4 && (S.StartsWith(N.Left(N.Len() - 1)) || N.StartsWith(S.Left(S.Len() - 1))))) { Score++; break; } // "prints" / "print"
		}
		if (Score > BestScore) { BestScore = Score; Best = Id; }
	}
	return Best;
}

// which of their statements you mean: the one your words match, else the one this evidence would catch, else the first
int32 AHUSuspectActor::MatchStatement(const FString& Said, const FString& ClueId) const
{
	FHUSuspect S;
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!C || !C->GetSuspect(SuspectId, S)) return 0;
	const TArray<FString> W = WordsOf(Said);
	int32 Best = -1, BestScore = 1; // at least two words in common
	for (int32 i = 0; i < S.Statements.Num(); i++)
	{
		int32 Score = 0;
		for (const FString& N : WordsOf(S.Statements[i].Text)) if (!Filler(N) && W.Contains(N)) Score++;
		if (Score > BestScore) { BestScore = Score; Best = i; }
	}
	if (Best >= 0) return Best;
	if (!ClueId.IsEmpty()) for (int32 i = 0; i < S.Statements.Num(); i++) if (!C->IsBroken(SuspectId, i) && S.Statements[i].Evidence.Contains(ClueId)) return i;
	for (int32 i = 0; i < S.Statements.Num(); i++) if (!C->IsBroken(SuspectId, i)) return i;
	return 0;
}

// the game's part first (it decides the facts), then the thinking brain answers
void AHUSuspectActor::Handle(const FString& Said)
{
	if (!bInterviewing || bThinking || Said.IsEmpty()) return;
	LastSaid = Said;
	Heard = Said;
	UHUInteractComponent* I = Interviewer.IsValid() ? Interviewer->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return;
	if (SaysAny(Said, { TEXT("that's all"), TEXT("that is all"), TEXT("we're done"), TEXT("we are done"), TEXT("you can go"), TEXT("that'll be all"), TEXT("goodbye") }))
	{
		I->Say(FString::Printf(TEXT("You end the interview with %s."), *DisplayName()));
		CloseInterview();
		return;
	}
	const FString Clue = MatchClue(Said);
	const bool bRecord = SaysAny(Said, { TEXT("record"), TEXT("rap sheet"), TEXT("priors"), TEXT("your history"), TEXT("arrested before"), TEXT("been arrested"), TEXT("criminal") });
	const bool bLie = SaysAny(Said, { TEXT("lying"), TEXT("liar"), TEXT("lie"), TEXT("lied"), TEXT("the truth"), TEXT("bullshit"), TEXT("don't believe"), TEXT("not true"), TEXT("come clean") });
	if (!Clue.IsEmpty() || bRecord || bLie)
	{
		// a move on the record: every machine applies it; this machine's brain then answers (0x80 = said out loud)
		bThinking = true; LastReply = TEXT("...");
		I->SendInterrogation(GetPathName(), (uint8)((bRecord && Clue.IsEmpty() ? 1 : 0) | 0x80), MatchStatement(Said, Clue), Clue);
		return;
	}
	Think(Said, FString(), FString());
}

// the thinking brain: words, emotion, body - shared with the squad, and spoken by the voice on every machine
void AHUSuspectActor::Think(const FString& Said, const FString& Situation, const FString& Fallback)
{
	UHUBrainSubsystem* B = GetGameInstance()->GetSubsystem<UHUBrainSubsystem>();
	TWeakObjectPtr<AHUSuspectActor> Self(this);
	TWeakObjectPtr<APawn> Asker = Interviewer.IsValid() ? Interviewer : TWeakObjectPtr<APawn>();
	auto Deliver = [Self, Asker, Said, Fallback](const FHUReaction& R)
	{
		if (!Self.IsValid()) return;
		Self->bThinking = false;
		FHUReaction Out = R;
		if (Out.Say.IsEmpty())
		{
			// no brain (or it failed): the game's written line, or one for their mood
			const UHUCaseSubsystem* C = Self->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
			const EHUSuspectMood M = C ? C->MoodOf(Self->SuspectId) : EHUSuspectMood::Calm;
			Out.Say = !Fallback.IsEmpty() ? Fallback : M == EHUSuspectMood::Lawyered ? FString(TEXT("I want my lawyer.")) : HUInterrogation::PressLine(M, GetTypeHash(Said));
			Out.Emotion = 255; Out.Action = HUReact::None;
		}
		UHUInteractComponent* I = Asker.IsValid() ? Asker->FindComponentByClass<UHUInteractComponent>() : nullptr;
		if (I) I->SendAnswer(Self->GetPathName(), Said, Out.Say, Out.Emotion, Out.Action);
	};
	bThinking = true; LastReply = TEXT("...");
	if (B && B->IsReady()) B->React(SuspectId, Said, Situation, FHUBrainReact::CreateLambda(Deliver));
	else { if (B) B->WarmUp(); Deliver(FHUReaction()); }
}

void AHUSuspectActor::ApplyAnswer(APawn* User, const FString& Question, const FString& Answer, uint8 InEmotion, uint8 InAction)
{
	FHUSuspect S;
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!C || !C->GetSuspect(SuspectId, S)) return;
	bThinking = false;
	if (User && User->IsLocallyControlled()) LastReply = Answer;
	Emotion = InEmotion < HUReact::EmotionCount ? InEmotion : (uint8)HUReact::Calm;
	Act(InAction);
	if (UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr)
	{
		const FString Body = InAction != HUReact::None ? FString::Printf(TEXT("  (%s)"), HUReact::ActionLabel(InAction)) : FString();
		I->Say(FString::Printf(TEXT("\"%s\"   %s:  %s%s"), *Question, *S.Name.ToUpper(), *Answer, *Body));
	}
	Speak(Answer, InEmotion);
}

void AHUSuspectActor::ApplyInterrogation(APawn* User, uint8 Action, int32 Statement, const FString& ClueId)
{
	UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	FHUSuspect S;
	if (!C || !C->GetSuspect(SuspectId, S)) return;
	const bool bVoice = (Action & 0x80) != 0;
	Action &= 0x7F;
	const int32 BreaksBefore = C->Breaks;
	const bool bConfessedBefore = !C->Confessed.IsEmpty();
	const bool bWasBroken = S.Statements.IsValidIndex(Statement) && C->IsBroken(SuspectId, Statement);
	const FString Reply = Action == 1 ? C->RaiseRecord(SuspectId) : !ClueId.IsEmpty() ? C->Confront(SuspectId, Statement, ClueId) : C->Press(SuspectId, Statement);
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return;
	if (bVoice)
	{
		// said out loud: the asker's thinking brain answers, told exactly what the game decided
		if (User->IsLocallyControlled())
		{
			FString Sit;
			const FString St = S.Statements.IsValidIndex(Statement) ? S.Statements[Statement].Text : FString();
			if (Action == 1) { const FString* Rec = C->SuspectRecord.Find(SuspectId); Sit = FString::Printf(TEXT("The detective brings up your criminal record (%s)."), Rec ? **Rec : TEXT("nothing on it")); }
			else if (!ClueId.IsEmpty())
			{
				FHUClue K; C->GetClue(ClueId, K);
				const bool bCaught = !bWasBroken && C->IsBroken(SuspectId, Statement);
				Sit = FString::Printf(TEXT("The detective shows you evidence: %s - %s. "), *K.Name, *K.Detail)
					+ (bCaught ? FString::Printf(TEXT("It PROVES you lied when you said \"%s\". You're caught: admit what the truth was."), *St)
						: TEXT("It does not prove anything you said is false. Stay with your story."));
			}
			else Sit = FString::Printf(TEXT("The detective calls you a liar about \"%s\" - with no evidence to show."), *St);
			Sit += FString::Printf(TEXT(" Your answer must mean this: \"%s\""), *Reply);
			Think(LastSaid, Sit, Reply);
		}
	}
	else
	{
		if (User->IsLocallyControlled()) LastReply = Reply;
		I->Say(FString::Printf(TEXT("%s:  %s"), *S.Name.ToUpper(), *Reply));
		Speak(Reply);
	}
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
	Animate(DeltaSeconds);
	const APawn* E = Escort.Get();
	if (!bCuffed || !E) return;
	// walked in front of whoever has them by the arm: an arm's length ahead, back to the officer
	// (so the cuffs are in view), facing the way the officer faces. When the officer turns, the
	// suspect swings around them instead of being dragged through them.
	constexpr float Ahead = 100.f;
	const FVector Center = E->GetActorLocation();
	const FVector Now = GetActorLocation();
	const float OfficerYaw = E->GetActorRotation().Yaw;
	FVector Rel = (Now - Center) * FVector(1, 1, 0);
	const float CurYaw = Rel.SizeSquared() > 1.f ? Rel.Rotation().Yaw : OfficerYaw;
	const float NewYaw = FMath::RInterpTo(FRotator(0, CurYaw, 0), FRotator(0, OfficerYaw, 0), DeltaSeconds, 7.f).Yaw;
	const float NewDist = FMath::FInterpTo(Rel.Size(), Ahead, DeltaSeconds, 6.f);
	const FVector Next = Center + FRotator(0, NewYaw, 0).Vector() * NewDist;
	SetActorLocation(FVector(Next.X, Next.Y, Now.Z));
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0, OfficerYaw, 0), DeltaSeconds, 10.f));
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
void AHUSuspectActor::Speak(const FString& Said, uint8 InEmotion)
{
	UHUVoiceSubsystem* V = GetGameInstance()->GetSubsystem<UHUVoiceSubsystem>();
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	if (!V || !C || !V->HasNeuralVoice() || GetNetMode() == NM_DedicatedServer) return;
	// THE VOICE: the emotion the thinking brain picked, or (written lines) one from their mood
	const EHUSuspectMood M = C->MoodOf(SuspectId);
	float Speed = M == EHUSuspectMood::Rattled ? 1.1f : M == EHUSuspectMood::Cracking ? 1.2f : M == EHUSuspectMood::Broken ? 0.85f : M == EHUSuspectMood::Defensive ? 1.05f : 1.f;
	float Excite = M == EHUSuspectMood::Calm ? 0.35f : M == EHUSuspectMood::Defensive ? 0.5f : M == EHUSuspectMood::Rattled ? 0.65f : M == EHUSuspectMood::Cracking ? 0.85f : 0.5f;
	if (InEmotion < HUReact::EmotionCount) HUReact::Delivery(InEmotion, Excite, Speed);
	// a voice per suspect (the 911 caller and the captain use others)
	const int32 VoiceNum = 1 + (int32)(GetTypeHash(SuspectId) % 10);
	TWeakObjectPtr<AHUSuspectActor> Self(this);
	V->Synthesize(Said, VoiceNum, FString(), false, FHUVoiceReady::CreateLambda([Self](USoundWave* S, float)
	{
		if (!Self.IsValid() || !S) return;
		if (Self->VoiceAudio.IsValid()) Self->VoiceAudio->Stop();
		Self->VoiceAudio = UGameplayStatics::SpawnSoundAtLocation(Self.Get(), S, Self->GetActorLocation() + FVector(0, 0, 170));
	}), Speed, Excite);
}
// ---------- the microphone: hold V, talk, let go ----------
void AHUSuspectActor::MicDown()
{
	if (!bInterviewing || bTyping || bThinking || bMicOpen) return;
	UHUListenSubsystem* L = GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
	if (!L || !L->IsInstalled()) return;
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
		if (Text.IsEmpty()) { Self->Heard = TEXT("(didn't catch that - hold V a little longer and speak up)"); return; }
		Self->Handle(Text);
	}));
}

// ---------- acting it out ----------
// Stance (sitting) and arms (crossed / hands up) stay until something changes them; the rest are quick gestures.
void AHUSuspectActor::Act(uint8 A)
{
	LastAction = A;
	switch (A)
	{
	case HUReact::Sit: Stance = 1; break;
	case HUReact::Stand: Stance = 0; break;
	case HUReact::CrossArms: Arms = 1; break;
	case HUReact::HandsUp: Arms = 2; ArmsLeft = 4.f; break;
	case HUReact::FaceYou: bFacing = true; Gesture = 0; GestureT = -1.f; break;
	case HUReact::None: break;
	default: Gesture = A; GestureT = 0.f; if (Arms == 1 && (A == HUReact::LeanIn || A == HUReact::HeadDown)) Arms = 0; break;
	}
}

void AHUSuspectActor::Animate(float Dt)
{
	if (bCuffed || StunLeft > 0.f || Blocks.Num() < 5 || BaseXf.Num() < 5) return;
	// facing whoever's interviewing them
	if (bFacing && Interviewer.IsValid()) FaceYaw = (Interviewer->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
	// the gesture's curve: in, hold, out over ~2.5 s
	float G = 0.f, Tm = 0.f;
	if (GestureT >= 0.f)
	{
		GestureT += Dt; Tm = GestureT;
		G = FMath::Clamp(FMath::Min(GestureT / 0.35f, (2.6f - GestureT) / 0.5f), 0.f, 1.f);
		if (GestureT > 2.6f) { GestureT = -1.f; Gesture = 0; }
	}
	if (Arms == 2 && (ArmsLeft -= Dt) <= 0.f) Arms = 0;
	float YawOff = 0.f; FVector All(0, 0, 0); FVector Head(0, 0, 0);
	switch (Gesture)
	{
	case HUReact::LookAway:  YawOff = 55.f * G; break;
	case HUReact::ShakeHead: YawOff = 12.f * G * FMath::Sin(Tm * 14.f); break;
	case HUReact::Fidget:    YawOff = 5.f * G * FMath::Sin(Tm * 23.f); All.Y = 3.f * G * FMath::Sin(Tm * 9.f); break;
	case HUReact::Nod:       Head = FVector(4, 0, -7) * G * FMath::Abs(FMath::Sin(Tm * 9.f)); break;
	case HUReact::HeadDown:  Head = FVector(10, 0, -16) * G; break;
	case HUReact::LeanIn:    All.X = 14.f * G; Head.X = 10.f * G; break;
	case HUReact::StepBack:  All.X = -35.f * G; break;
	default: break;
	}
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0, FaceYaw + YawOff, 0), Dt, 8.f));
	// the parts: 0 legs, 1 body, 2 head, 3-4 arms
	const float Drop = Stance == 1 ? 44.f : 0.f;
	for (int32 i = 0; i < 5; i++)
	{
		FTransform X = BaseXf[i];
		FVector L = X.GetLocation() + All;
		FRotator R = X.Rotator();
		FVector Sc = X.GetScale3D();
		if (i == 0 && Stance == 1) { Sc.Z *= 0.5f; L.Z = 22.f; L.X += 10.f; } // crouched on a seat that isn't there yet
		if (i > 0) L.Z -= Drop;
		if (i == 2) L += Head;
		if (i >= 3)
		{
			const float Side = i == 3 ? -1.f : 1.f;
			if (Arms == 1) { L = FVector(16, Side * 6, 118 + (i == 3 ? 0.f : 8.f) - Drop) + All; R = FRotator(0, 0, 90); Sc.Z *= 0.85f; }
			else if (Arms == 2) { L = FVector(4, Side * 30, 182 - Drop) + All; }
		}
		Blocks[i]->SetRelativeLocation(FMath::VInterpTo(Blocks[i]->GetRelativeLocation(), L, Dt, 10.f));
		Blocks[i]->SetRelativeRotation(FMath::RInterpTo(Blocks[i]->GetRelativeRotation(), R, Dt, 10.f));
		Blocks[i]->SetRelativeScale3D(FMath::VInterpTo(Blocks[i]->GetRelativeScale3D(), Sc, Dt, 10.f));
	}
}