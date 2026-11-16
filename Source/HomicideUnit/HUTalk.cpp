// TALKING TO SOMEONE WITH NO MENU (part of UHUInteractComponent).
// Walk up to a suspect, hold V, speak, let go. What you said is turned into text on this PC (UHUListenSubsystem) and
// goes to ONE person (WhoHears):
//   1. someone in earshot whose name you CALLED - set off by a comma or said alone, after at most a greeting:
//        "Walter, where were you?"   "What about you, Pearl?"   "Mr. Hale?"   "Hey Doris"
//      A name you only mention ("Did you see Walter last night?") is talk ABOUT them: it goes to whoever you face.
//   2. else whoever you're facing, close by
//   3. else whoever you were already talking to, if you're still standing with them
//   4. else someone in earshot whose name you said at all, else someone right beside you
// They hear it as AHUSuspectActor::HearFrom: the same conversation as the sit-down interview (E), without the sheet
// and without being held in place. A tap of V is still the UV light.
//
// -HUTalkTest on L_Scene (not in Shipping) runs it by script: see TalkTest at the end of this file.
#include "HUInteractComponent.h"
#include "HUSuspectActor.h"
#include "HUListenSubsystem.h"
#include "HUBrainSubsystem.h"
#include "HUVoiceSubsystem.h"
#include "HUCaseSubsystem.h"
#include "HUCanned.h"
#include "HUInteractable.h"
#include "Sound/SoundWaveProcedural.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

namespace HUTalk
{
	constexpr float Earshot = 800.f; // a called name carries this far (cm)
	constexpr float Close = 450.f;   // facing them, or still standing with them
	constexpr float Cone = 35.f;     // "facing": this near the middle of your view (degrees)
	constexpr float Beside = 220.f;  // right next to you: no need to look at them
	constexpr float Floor = 300.f;   // further above or below than this is another floor

	// a word of what was said, and whether a comma, full stop or question mark stands beside it
	struct FWord { FString W; bool bBreakBefore = false, bBreakAfter = false; };

	static TArray<FWord> Split(const FString& Said)
	{
		TArray<FWord> Out;
		FString Cur;
		bool bBreak = true; // the start of what was said counts as a break
		auto End = [&]() { if (Cur.Len()) { FWord X; X.W = Cur; X.bBreakBefore = bBreak; Out.Add(X); Cur.Reset(); bBreak = false; } };
		for (const TCHAR C : Said)
		{
			if (FChar::IsAlpha(C) || C == '\'') { Cur.AppendChar(FChar::ToLower(C)); continue; }
			End();
			if (C == ',' || C == '.' || C == '?' || C == '!' || C == ';' || C == ':') { if (Out.Num()) Out.Last().bBreakAfter = true; bBreak = true; }
		}
		End();
		if (Out.Num()) Out.Last().bBreakAfter = true; // and so does the end
		return Out;
	}

	static bool AnyOf(const FString& W, std::initializer_list<const TCHAR*> List) { for (const TCHAR* L : List) if (W == L) return true; return false; }

	// what comes in front of a name when you call it: a title ("Mr. Hale"), a greeting ("hey Walter", "excuse me, Doris")
	static bool Title(const FString& W) { return AnyOf(W, { TEXT("mr"), TEXT("mister"), TEXT("mrs"), TEXT("missus"), TEXT("miss"), TEXT("ms") }); }
	static bool Opener(const FString& W)
	{
		static const TSet<FString> S = { TEXT("sir"), TEXT("ma'am"), TEXT("hey"), TEXT("hi"), TEXT("hello"), TEXT("yo"), TEXT("ok"), TEXT("okay"), TEXT("so"), TEXT("now"), TEXT("well"),
			TEXT("alright"), TEXT("right"), TEXT("listen"), TEXT("look"), TEXT("excuse"), TEXT("me"), TEXT("and"), TEXT("but"), TEXT("oh"), TEXT("say") };
		return Title(W) || S.Contains(W);
	}

	// how many letters must change to turn one word into the other
	static int32 Edits(const FString& A, const FString& B)
	{
		TArray<int32> Row; Row.SetNumUninitialized(B.Len() + 1);
		for (int32 j = 0; j <= B.Len(); j++) Row[j] = j;
		for (int32 i = 1; i <= A.Len(); i++)
		{
			int32 Diag = Row[0];
			Row[0] = i;
			for (int32 j = 1; j <= B.Len(); j++)
			{
				const int32 Up = Row[j];
				Row[j] = FMath::Min3(Up + 1, Row[j - 1] + 1, Diag + (A[i - 1] == B[j - 1] ? 0 : 1));
				Diag = Up;
			}
		}
		return Row[B.Len()];
	}

	// roughly how a word sounds: the vowels after its first letter dropped, look-alike consonants merged, doubles once
	// ("Dunne" and "done", "Pruitt" and "Pruett", "Lindqvist" and "Lindquist" come out the same)
	static FString Sound(const FString& Word)
	{
		FString W = Word;
		static const TCHAR* const Swaps[][2] = { { TEXT("'"), TEXT("") }, { TEXT("ph"), TEXT("f") }, { TEXT("ck"), TEXT("k") }, { TEXT("qu"), TEXT("kw") }, { TEXT("qv"), TEXT("kw") },
			{ TEXT("sz"), TEXT("s") }, { TEXT("wh"), TEXT("w") }, { TEXT("ce"), TEXT("se") }, { TEXT("ci"), TEXT("si") }, { TEXT("cy"), TEXT("si") } };
		for (const auto& S : Swaps) W.ReplaceInline(S[0], S[1]);
		FString Out;
		for (int32 i = 0; i < W.Len(); i++)
		{
			TCHAR C = W[i];
			if (C == 'c' || C == 'q') C = 'k'; else if (C == 'z') C = 's'; else if (C == 'y') C = 'i';
			const bool bVowel = C == 'a' || C == 'e' || C == 'i' || C == 'o' || C == 'u';
			if ((bVowel && i > 0) || (Out.Len() && Out[Out.Len() - 1] == C)) continue;
			Out.AppendChar(C);
		}
		return Out;
	}

	// do these two words name the same person? bLoose: also when they only look or sound alike - the ears mishear names
	static bool Same(const FString& Heard, const FString& Name, bool bLoose)
	{
		if (Heard == Name) return true;
		if (!bLoose || Heard.Len() < 3 || Heard.Len() > Name.Len() + 2) return false;
		if (Name.Len() >= 5 && Edits(Heard, Name) <= (Name.Len() >= 8 ? 2 : 1)) return true;
		const FString A = Sound(Heard);
		return A.Len() >= 2 && A == Sound(Name);
	}

	// the words of a name worth listening for: "Walter Hale" -> walter, hale
	static TArray<FString> NameWords(const FString& FullName)
	{
		TArray<FString> Out;
		for (const FWord& W : Split(FullName)) if (W.W.Len() >= 3) Out.Add(W.W);
		return Out;
	}

	// Is this name being CALLED, not just mentioned? Set off by a comma or standing alone, after at most a greeting.
	// When the ears leave the comma out: the name at the start of a phrase, then an order, a question or "you".
	static bool Calls(const TArray<FWord>& W, const TArray<FString>& Names)
	{
		auto IsName = [&](const FString& X) { if (Opener(X)) return false; for (const FString& N : Names) if (Same(X, N, true)) return true; return false; };
		for (int32 i = 0; i < W.Num(); i++)
		{
			if (!IsName(W[i].W)) continue;
			int32 Last = i;  // first and last name together are one call: "Walter Hale, sit down"
			while (Last + 1 < W.Num() && !W[Last].bBreakAfter && IsName(W[Last + 1].W)) Last++;
			int32 First = i; // what belongs in front of it: a title ("Mr. Hale"), a greeting with no pause after it ("hey Walter")
			while (First > 0 && (Title(W[First - 1].W) || (Opener(W[First - 1].W) && !W[First - 1].bBreakAfter))) First--;
			const bool bStarts = W[First].bBreakBefore, bEnds = W[Last].bBreakAfter; // a pause (or the start / the end) on that side
			if (bStarts && bEnds) return true; // "Walter, where were you?"  "What about you, Pearl?"  "Tell me, Walter, ..."  "Mr. Hale?"
			if (bStarts && Last + 1 < W.Num())
			{
				const FString& Next = W[Last + 1].W;
				if (AnyOf(Next, { TEXT("you"), TEXT("you're"), TEXT("your"), TEXT("where"), TEXT("what"), TEXT("when"), TEXT("why"), TEXT("how"), TEXT("sit"), TEXT("stand"), TEXT("look"),
					TEXT("come"), TEXT("stop"), TEXT("listen"), TEXT("tell"), TEXT("show"), TEXT("explain"), TEXT("give"), TEXT("put"), TEXT("turn"), TEXT("calm"), TEXT("wait"), TEXT("hold"),
					TEXT("get"), TEXT("step"), TEXT("shut"), TEXT("answer"), TEXT("talk"), TEXT("stay"), TEXT("drop"), TEXT("open") })) return true; // "Walter where were you"
				if (Last + 2 < W.Num() && W[Last + 2].W == TEXT("you") && AnyOf(Next, { TEXT("did"), TEXT("do"), TEXT("were"), TEXT("are"), TEXT("can"), TEXT("could"), TEXT("would"),
					TEXT("will"), TEXT("have"), TEXT("don't"), TEXT("didn't"), TEXT("aren't"), TEXT("weren't"), TEXT("won't") })) return true; // "Walter did you see it"
			}
			i = Last;
		}
		return false;
	}

	// Said their name at all. Only asked when nobody else could be meant, and not for names that are everyday words.
	static bool Mentions(const TArray<FWord>& W, const TArray<FString>& Names)
	{
		static const TSet<FString> Everyday = { TEXT("ray"), TEXT("june"), TEXT("frank"), TEXT("earl"), TEXT("pearl"), TEXT("hale"), TEXT("marsh"), TEXT("pierce"), TEXT("sal"), TEXT("will"),
			TEXT("bill"), TEXT("mark"), TEXT("rose"), TEXT("grace"), TEXT("hope"), TEXT("joy"), TEXT("may"), TEXT("art"), TEXT("pat"), TEXT("sue"), TEXT("dawn"), TEXT("jack"), TEXT("rob"),
			TEXT("rich"), TEXT("dean"), TEXT("cliff"), TEXT("bob"), TEXT("don"), TEXT("guy"), TEXT("penny"), TEXT("young"), TEXT("king"), TEXT("stone"), TEXT("wood"), TEXT("brown"),
			TEXT("black"), TEXT("white"), TEXT("green"), TEXT("gray"), TEXT("long"), TEXT("bell"), TEXT("fox"), TEXT("hill"), TEXT("lane"), TEXT("price") };
		for (const FWord& X : W)
			if (!Opener(X.W)) for (const FString& N : Names) if (!Everyday.Contains(N) && Same(X.W, N, N.Len() >= 6)) return true;
		return false;
	}
}

AHUSuspectActor* UHUInteractComponent::WhoHears(const FString& Said, FString* OutHow) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	auto Pick = [OutHow](AHUSuspectActor* S, const TCHAR* How) { if (OutHow) *OutHow = How; return S; };
	if (!Pawn || !GetWorld()) return Pick(nullptr, TEXT("nobody is close enough"));
	const FVector From = Pawn->GetActorLocation();
	const FVector Fwd = FRotator(0.f, Pawn->GetControlRotation().Yaw, 0.f).Vector();
	const TArray<HUTalk::FWord> Words = HUTalk::Split(Said);
	// within reach: able to hear, this near, and on your floor
	auto Within = [&From](const AHUSuspectActor* S, float Range)
	{
		const FVector To = S->GetActorLocation() - From;
		return S->CanHear() && To.Size2D() <= Range && FMath::Abs(To.Z) <= HUTalk::Floor;
	};
	AHUSuspectActor *Called = nullptr, *Faced = nullptr, *Mentioned = nullptr, *Beside = nullptr;
	TArray<AHUSuspectActor*> AllCalled;
	float CalledD = HUTalk::Earshot, MentionedD = HUTalk::Earshot, BesideD = HUTalk::Beside, FacedDot = FMath::Cos(FMath::DegreesToRadians(HUTalk::Cone));
	for (TActorIterator<AHUSuspectActor> It(GetWorld()); It; ++It)
	{
		AHUSuspectActor* S = *It;
		if (!Within(S, HUTalk::Earshot)) continue;
		const FVector To = S->GetActorLocation() - From;
		const float D = To.Size2D();
		const TArray<FString> Names = HUTalk::NameWords(S->DisplayName());
		if (HUTalk::Calls(Words, Names)) { AllCalled.Add(S); if (D <= CalledD) { Called = S; CalledD = D; } }
		if (D <= MentionedD && HUTalk::Mentions(Words, Names)) { Mentioned = S; MentionedD = D; }
		if (D <= BesideD) { Beside = S; BesideD = D; }
		if (D <= HUTalk::Close)
		{
			const float Dot = D < 60.f ? 1.f : FVector::DotProduct(FVector(To.X, To.Y, 0.f) / D, Fwd);
			if (Dot > FacedDot) { Faced = S; FacedDot = Dot; }
		}
	}
	if (AHUSuspectActor* F = Cast<AHUSuspectActor>(Focus); F && Within(F, HUTalk::Close)) Faced = F; // the crosshair is on them
	AHUSuspectActor* Still = TalkingTo.Get();
	if (Still && (!Still->bWalkUp || !Within(Still, HUTalk::Close))) Still = nullptr;
	// two people here share that name: the one you're facing, else the nearer
	if (Called) return Pick(AllCalled.Contains(Faced) ? Faced : Called, TEXT("you called their name"));
	if (Faced) return Pick(Faced, TEXT("you're facing them"));
	if (Still) return Pick(Still, TEXT("you were already talking to them"));
	if (Mentioned) return Pick(Mentioned, TEXT("you said their name and faced nobody else"));
	if (Beside) return Pick(Beside, TEXT("they're right beside you"));
	return Pick(nullptr, TEXT("nobody is close enough"));
}

void UHUInteractComponent::SayTo(const FString& Said)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	const FString Line = Said.TrimStartAndEnd();
	if (!Pawn || Line.IsEmpty()) return;
	FString How;
	AHUSuspectActor* S = WhoHears(Line, &How);
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit talk: \"%s\" -> %s (%s)"), *Line, S ? *S->DisplayName() : TEXT("nobody"), *How);
	if (!S)
	{
		TalkLine = FText::GetEmpty();
		Say(FString::Printf(TEXT("YOU:  \"%s\"     (nobody is close enough to hear that - walk up to them, or call their name)"), *Line));
		return;
	}
	TalkingTo = S;
	TalkLine = FText::FromString(FString::Printf(TEXT("YOU, TO %s:  \"%s\""), *S->DisplayName().ToUpper(), *Line)); // on screen until they answer
	S->HearFrom(Pawn, Line);
}

bool UHUInteractComponent::TalkBusy() const { return bMenuOpen || Interviewing.IsValid() || Watching.IsValid() || Reading.IsValid(); }

// V: held for a moment near someone = talk; let go at once = the UV light, as it always was
void UHUInteractComponent::TalkDown()
{
	bTalkKey = true; bTalkHot = false; bTalkNoMic = false;
	TalkKeyAt = FPlatformTime::Seconds();
	// someone could hear you: the microphone opens now, so your first word isn't clipped (a tap throws the sound away)
	UHUListenSubsystem* L = GetWorld()->GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
	if (L && bInEarshot && !TalkBusy()) { bTalkHot = L->StartListening(); bTalkNoMic = !bTalkHot && !L->IsWorking(); }
}

void UHUInteractComponent::TalkUp()
{
	const bool bWasDown = bTalkKey, bWasHot = bTalkHot, bWasOpen = bTalkOpen;
	bTalkKey = bTalkHot = bTalkOpen = false;
	UHUListenSubsystem* L = GetWorld()->GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
	if (!bWasOpen)
	{
		if (bWasHot && L) L->StopListening(FHUHeard()); // a tap: nothing was said
		if (bWasDown) ToggleUV();
		return;
	}
	if (!L) return;
	TalkLine = FText::FromString(TEXT("YOU:  ..."));
	TWeakObjectPtr<UHUInteractComponent> Self(this);
	TWeakObjectPtr<UHUListenSubsystem> Ears(L);
	L->StopListening(FHUHeard::CreateLambda([Self, Ears](const FString& Text)
	{
		if (!Self.IsValid()) return;
		if (!Text.IsEmpty()) { Self->SayTo(Text); return; }
		Self->TalkLine = FText::GetEmpty();
		Self->Say(Ears.IsValid() && Ears->LastWasSilent()
			? FString::Printf(TEXT("(your microphone - %s - heard nothing. Checking your other microphones... hold V and try again)"), *Ears->MicLabel())
			: FString(TEXT("(didn't catch that - hold V a little longer and speak up)")));
	}));
}

void UHUInteractComponent::TickTalk(float Dt)
{
	UHUListenSubsystem* L = GetWorld()->GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const bool bBusy = TalkBusy();
	// Who's about? Twice a second is plenty.
	if ((EarsClock -= Dt) <= 0.f && Pawn)
	{
		EarsClock = 0.5f;
		bool bSomeone = false;
		const FVector From = Pawn->GetActorLocation();
		const FVector Fwd = FRotator(0.f, Pawn->GetControlRotation().Yaw, 0.f).Vector();
		for (TActorIterator<AHUSuspectActor> It(GetWorld()); It; ++It)
		{
			const FVector To = It->GetActorLocation() - From;
			const float D = To.Size2D();
			if (!It->CanHear() || D > HUTalk::Earshot || FMath::Abs(To.Z) > HUTalk::Floor) continue;
			bSomeone = true;
			// Walking up to this one (close and ahead of you, or right beside you): their voice gets ready before a
			// word is said. Not everyone in earshot - a voice takes seconds a line, and a room can hold eight people.
			if (D <= HUTalk::Beside || (D <= HUTalk::Close && FVector::DotProduct(FVector(To.X, To.Y, 0.f) / D, Fwd) > 0.64f)) It->GetReady();
		}
		bInEarshot = bSomeone && L && L->IsInstalled(); // (the microphone only opens for someone who could hear you)
		// The first time someone is in earshot, the ears and the brain get ready: both take seconds to load, and the
		// right microphone has to be found - before your first word, not after it.
		if (bSomeone && !bEarsWarm && !bBusy)
		{
			bEarsWarm = true;
			if (L && L->IsInstalled()) L->AutoPickMic(false);
			if (UHUBrainSubsystem* B = GetWorld()->GetGameInstance()->GetSubsystem<UHUBrainSubsystem>()) B->WarmUp();
		}
	}
	// V still down after a quarter of a second: this is talking, not a tap
	if (bTalkKey && L && FPlatformTime::Seconds() - TalkKeyAt > 0.25)
	{
		// (V was pressed before anyone was in earshot, or while your last words were still being made out)
		if (!bTalkHot && !bTalkNoMic && bInEarshot && !bBusy) { bTalkHot = L->StartListening(); bTalkNoMic = !bTalkHot && !L->IsWorking(); }
		if (bTalkHot && !bTalkOpen) { bTalkOpen = true; TalkLine = FText::GetEmpty(); }
		else if (bTalkNoMic)
		{
			bTalkKey = false; // say so once, not every frame
			Say(TEXT("No microphone answered. Pick one in the menu (Esc) - or press E on them, then T, to type instead."));
		}
	}
	if (bTalkHot && bBusy) { bTalkKey = bTalkHot = bTalkOpen = false; if (L) L->StopListening(FHUHeard()); } // a menu opened over it: never mind
	// what you said stays up while it's being made out and while they think; their answer then takes the subtitle line
	if (!bTalkOpen && !TalkLine.IsEmpty() && !(L && L->IsWorking()) && !(TalkingTo.IsValid() && TalkingTo->bThinking)) TalkLine = FText::GetEmpty();
#if !UE_BUILD_SHIPPING
	TalkTest(Dt);
#endif
}

#if UE_BUILD_SHIPPING
void UHUInteractComponent::TalkTest(float) {}
#else
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "UnrealClient.h"

// -HUTalkTest on L_Scene: talks to the suspects with no menu, by script, and checks who answers. The log has a line
// for each check ("HUTalkTest: ok ..." or "WRONG ..."), then "all checks passed" or how many were wrong; the game quits.
// It says three lines OUT LOUD - the small voice speaks them, the ears make them out - to check the whole way from
// sound to who answers (skipped where the ears or the small voice aren't installed). Along the way it checks who
// answered (by hand when there is no brain: HUCanned), and it ends, when the run has a voice, by checking that their
// introduction was spoken ahead of time and that a stalling line can be said twice.
//   -HUTalkShots: also saves pictures of what's on screen (Saved\Screenshots\Talk_*.png; needs -RenderOffscreen or a window).
void UHUInteractComponent::TalkTest(float Dt)
{
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("HUTalkTest"));
	if (!bOn || TalkStep >= 100) return;
	FApp::SetVolumeMultiplier(0.f); // a test run makes no noise, even with sound on
	// -HUTalkShots: pictures of what's on screen along the way (needs a window, or -RenderOffscreen)
	static const bool bShots = FParse::Param(FCommandLine::Get(), TEXT("HUTalkShots"));
	auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name, true, false); };
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* W = GetWorld();
	if (!PC || !W) return;
	auto Check = [this](bool bOk, const FString& What) { if (!bOk) TalkWrong++; UE_LOG(LogTemp, Display, TEXT("HUTalkTest: %s  %s"), bOk ? TEXT("ok   ") : TEXT("WRONG"), *What); };
	auto Finish = [this, W]()
	{
		UE_LOG(LogTemp, Display, TEXT("HUTalkTest: %s"), TalkWrong ? *FString::Printf(TEXT("%d CHECKS WRONG"), TalkWrong) : TEXT("all checks passed"));
		TalkStep = 100;
		FTimerHandle Quit;
		W->GetTimerManager().SetTimer(Quit, FTimerDelegate::CreateLambda([]() { FPlatformMisc::RequestExit(false); }), 2.f, false);
	};
	TArray<AHUSuspectActor*> All;
	for (TActorIterator<AHUSuspectActor> It(W); It; ++It) if (It->CanHear()) All.Add(*It);
	TalkClock += Dt;
	AHUSuspectActor* A = All.IsValidIndex(0) ? All[0] : nullptr;
	AHUSuspectActor* B = All.IsValidIndex(1) ? All[1] : nullptr;
	// stand the player 1.5 m in front of someone, looking at them (or straight away from them)
	auto StandBy = [&](const AHUSuspectActor* S, bool bFacing)
	{
		const FVector At = S->GetActorLocation() + S->GetActorForwardVector().GetSafeNormal2D() * 150.f + FVector(0, 0, 95.f);
		Pawn->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
		const float Yaw = (S->GetActorLocation() - At).Rotation().Yaw;
		PC->SetControlRotation(FRotator(0.f, bFacing ? Yaw : Yaw + 180.f, 0.f));
	};
	auto Hears = [&](const TCHAR* Said, const AHUSuspectActor* Want, const TCHAR* Why)
	{
		FString How;
		const AHUSuspectActor* Got = WhoHears(Said, &How);
		Check(Got == Want, FString::Printf(TEXT("%s: \"%s\" -> %s (%s)"), Why, Said, Got ? *Got->DisplayName() : TEXT("nobody"), *How));
	};
	auto Next = [this]() { TalkStep++; TalkClock = 0.f; };
	// turn, where you stand, to look at nobody (false if someone is in view whichever way you turn)
	auto LookAtNobody = [&]()
	{
		const FVector From = Pawn->GetActorLocation();
		for (float Yaw = 0.f; Yaw < 360.f; Yaw += 15.f)
		{
			const FVector F = FRotator(0.f, Yaw, 0.f).Vector();
			bool bClear = true;
			for (const AHUSuspectActor* S : All)
			{
				const FVector To = (S->GetActorLocation() - From) * FVector(1, 1, 0);
				const float D = To.Size();
				if (D < HUTalk::Close + 50.f && (D < 60.f || FVector::DotProduct(To / D, F) > FMath::Cos(FMath::DegreesToRadians(HUTalk::Cone + 12.f)))) bClear = false;
			}
			if (bClear) { PC->SetControlRotation(FRotator(0.f, Yaw, 0.f)); return true; }
		}
		return false;
	};
	// someone other than the first suspect, and a word of their name that nobody else here shares or sounds like
	static TWeakObjectPtr<AHUSuspectActor> Second;
	static FString SecondName;
	static bool bLookedAway = false;
	auto PickSecond = [&]()
	{
		for (AHUSuspectActor* S : All)
			if (S != A) for (const FString& Word : HUTalk::NameWords(S->DisplayName()))
			{
				bool bOnlyTheirs = true;
				for (const AHUSuspectActor* O : All) if (O != S) for (const FString& Theirs : HUTalk::NameWords(O->DisplayName())) if (HUTalk::Same(Word, Theirs, true) || HUTalk::Same(Theirs, Word, true)) bOnlyTheirs = false;
				if (bOnlyTheirs) { Second = S; SecondName = Word.Left(1).ToUpper() + Word.Mid(1); return; }
			}
	};

	switch (TalkStep)
	{
	case -1: // the scene has to build itself and stand its people up
		if (W->GetTimeSeconds() < 8.f) return;
		if (!A) { if (W->GetTimeSeconds() > 40.f) { Check(false, TEXT("nobody at this scene to talk to (run it on L_Scene)")); Finish(); } return; }
		Next();
		return;
	case 0: // who a name calls, with no world at all
	{
		struct FCase { const TCHAR* Name; const TCHAR* Said; bool bCalls; };
		const FCase Cases[] = {
			{ TEXT("Walter Hale"), TEXT("Walter, where were you last night?"), true }, { TEXT("Walter Hale"), TEXT("What about you, Walter?"), true },
			{ TEXT("Walter Hale"), TEXT("Mr. Hale?"), true }, { TEXT("Walter Hale"), TEXT("Hey Walter where were you"), true },
			{ TEXT("Walter Hale"), TEXT("Walter Hale, sit down."), true }, { TEXT("Walter Hale"), TEXT("Tell me, Walter, where were you?"), true },
			{ TEXT("Walter Hale"), TEXT("Walter did you see anything"), true },
			{ TEXT("Walter Hale"), TEXT("Did you see Walter last night?"), false }, { TEXT("Walter Hale"), TEXT("Where's Walter?"), false },
			{ TEXT("Walter Hale"), TEXT("Walter did it, didn't he?"), false }, { TEXT("Walter Hale"), TEXT("I spoke to Mr. Hale."), false },
			{ TEXT("Walter Hale"), TEXT("Walter was with you last night"), false }, { TEXT("Walter Hale"), TEXT("Hello, where were you?"), false },
			{ TEXT("Edie Dunne"), TEXT("Done, where were you last night?"), true }, { TEXT("Edie Dunne"), TEXT("What have you done?"), false },
			{ TEXT("Pearl Lindqvist"), TEXT("Miss Lindquist, a word."), true }, { TEXT("Pearl Lindqvist"), TEXT("Pearl, look at me."), true },
			{ TEXT("Pearl Lindqvist"), TEXT("Tell me about the pearl necklace."), false },
			{ TEXT("Frank Pruitt"), TEXT("Mr. Pruett, sit down."), true }, { TEXT("Frank Pruitt"), TEXT("To be frank, I don't believe you."), false } };
		for (const FCase& C : Cases)
		{
			const bool bGot = HUTalk::Calls(HUTalk::Split(C.Said), HUTalk::NameWords(C.Name));
			Check(bGot == C.bCalls, FString::Printf(TEXT("\"%s\" %s %s"), C.Said, bGot ? TEXT("calls") : TEXT("doesn't call"), C.Name));
		}
		// what counts as a hello (it gets their introduction, by hand - HUCanned)
		struct FHello { const TCHAR* Said; bool bHello; };
		const FHello Hellos[] = { { TEXT("Hello."), true }, { TEXT("Hey Walter"), true }, { TEXT("Good evening, Mr. Hale."), true }, { TEXT("Who are you?"), true },
			{ TEXT("What's your name?"), true }, { TEXT("Excuse me."), true }, { TEXT("Hey Walter where were you"), false }, { TEXT("Where were you last night?"), false },
			{ TEXT("Hi, did you see anything?"), false } };
		for (const FHello& H : Hellos)
		{
			const bool bGot = HUCanned::Greeting(H.Said, TEXT("Walter Hale"));
			Check(bGot == H.bHello, FString::Printf(TEXT("\"%s\" %s a hello"), H.Said, bGot ? TEXT("is") : TEXT("isn't")));
		}
		FString Names;
		for (const AHUSuspectActor* S : All) Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + S->DisplayName();
		UE_LOG(LogTemp, Display, TEXT("HUTalkTest: at this scene: %s"), *Names);
		StandBy(A, true);
		TalkStep = bShots ? 20 : 1; TalkClock = 0.f;
		return;
	}
	case 20: // (pictures) looking at them: the prompt under the crosshair
		if (TalkClock < 1.5f) return;
		Shot(TEXT("Talk_1_prompt.png"));
		Next();
		return;
	case 21: // (pictures) the microphone open
		bTalkOpen = TalkClock < 1.2f;
		if (TalkClock > 0.6f && TalkClock < 0.6f + Dt * 1.5f) Shot(TEXT("Talk_2_listening.png"));
		if (!bTalkOpen) Next();
		return;
	case 22: // (pictures) what you said, while they think
		TalkLine = FText::FromString(FString::Printf(TEXT("YOU, TO %s:  \"Where were you last night?\""), *A->DisplayName().ToUpper()));
		if (TalkClock > 0.6f && TalkClock < 0.6f + Dt * 1.5f) Shot(TEXT("Talk_3_asked.png"));
		if (TalkClock > 1.2f) { TalkStep = 1; TalkClock = 0.f; }
		return;
	case 1: // facing someone: they hear it, nothing is locked, and they answer
		if (TalkClock < 0.5f) return;
		Hears(TEXT("Where were you last night?"), A, TEXT("standing in front of them"));
		SayTo(TEXT("Where were you last night?"));
		Check(A->bInterviewing && A->bWalkUp && TalkingTo.Get() == A, FString::Printf(TEXT("%s is in conversation with you"), *A->DisplayName()));
		Check(!PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !Interviewing.IsValid(), TEXT("you can still walk and look around, and no interview sheet is up"));
		Next();
		return;
	case 2:
	{
		if (A->bThinking && TalkClock < 60.f) return;
		static float AnsweredAt = -1.f;
		if (AnsweredAt < 0.f)
		{
			AnsweredAt = TalkClock;
			Check(!A->bThinking && !A->LastReply.IsEmpty() && A->LastReply != TEXT("..."), FString::Printf(TEXT("%s answered after %.1f s: %s"), *A->DisplayName(), TalkClock, *A->LastReply.Left(110)));
			if (bShots) Shot(TEXT("Talk_4_answer.png"));
			// who answered that: with no brain, the hand-written answer built from what they've already told the detective
			const UHUCaseSubsystem* Cs = W->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
			const UHUBrainSubsystem* Br = W->GetGameInstance()->GetSubsystem<UHUBrainSubsystem>();
			if (Cs && Br && !Br->IsInstalled())
			{
				const FString Want = HUCanned::AnswerTopic(*Cs, A->SuspectId, UHUBrainSubsystem::WhereWereYou);
				Check(A->LastReply == Want && Br->Tallied(UHUBrainSubsystem::EAnsweredBy::Hand) == 1, FString::Printf(TEXT("no brain here: that was answered by hand, from what they've told the detective: %s"), *Want));
			}
			else if (Br)
			{
				UE_LOG(LogTemp, Display, TEXT("HUTalkTest: the brain is installed: that answer was %s"), Br->Tallied(UHUBrainSubsystem::EAnsweredBy::Ahead) ? TEXT("one it wrote ahead of time")
					: Br->Tallied(UHUBrainSubsystem::EAnsweredBy::Live) ? TEXT("thought up live") : TEXT("written by hand (the brain wasn't up yet)"));
			}
		}
		if (bShots && TalkClock < AnsweredAt + 0.8f) return; // hold the view for the picture
		// a hello gets their introduction, there and then, brain or no brain
		if (const UHUCaseSubsystem* Cs = W->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>())
		{
			SayTo(TEXT("Hello."));
			Check(!A->bThinking && A->LastReply == HUCanned::Introduction(*Cs, A->SuspectId), FString::Printf(TEXT("\"Hello.\" -> their introduction, with nothing to wait for: %s"), *A->LastReply));
		}
		bLookedAway = LookAtNobody(); // turn away from them (and from everyone else)
		Next();
		return;
	}
	case 3:
		if (TalkClock < 0.5f) return;
		if (bLookedAway) Hears(TEXT("And what did you see?"), A, TEXT("looking at nobody, still standing with them"));
		else UE_LOG(LogTemp, Display, TEXT("HUTalkTest: someone is in view whichever way you turn here: carrying on while looking away not checked"));
		PickSecond();
		B = Second.Get();
		if (!B) { UE_LOG(LogTemp, Display, TEXT("HUTalkTest: nobody else here with a name of their own: calling a second person by name not checked")); TalkStep = 5; TalkClock = 0.f; return; }
		// bring them over: 3 m to the side of the first, where you can't be facing both
		B->SetActorLocation(A->GetActorLocation() + A->GetActorRightVector().GetSafeNormal2D() * 300.f);
		StandBy(A, true);
		Next();
		return;
	case 4:
	{
		B = Second.Get();
		if (TalkClock < 0.5f || !B) return;
		Hears(TEXT("Where were you last night?"), A, TEXT("two in earshot, facing the first"));
		Hears(*FString::Printf(TEXT("%s, where were you last night?"), *SecondName), B, TEXT("facing the first, calling the second"));
		Hears(*FString::Printf(TEXT("What about you, %s?"), *SecondName), B, TEXT("facing the first, calling the second at the end"));
		Hears(*FString::Printf(TEXT("Did you see %s last night?"), *SecondName), A, TEXT("facing the first, only mentioning the second"));
		SayTo(FString::Printf(TEXT("%s, where were you last night?"), *SecondName));
		Check(B->bInterviewing && B->bWalkUp && TalkingTo.Get() == B, FString::Printf(TEXT("%s, called by name, is now the one in conversation with you"), *B->DisplayName()));
		Next();
		return;
	}
	case 5: // out of earshot: nobody
	{
		if (TalkClock < 0.5f) return;
		for (const FVector Dir : { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) })
		{
			const FVector At = A->GetActorLocation() + Dir * 3000.f + FVector(0, 0, 95.f);
			bool bClear = true;
			for (const AHUSuspectActor* S : All) if (FVector::Dist2D(S->GetActorLocation(), At) < HUTalk::Earshot + 100.f) bClear = false;
			if (!bClear) continue;
			Pawn->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			const FString Cap = HUTalk::NameWords(A->DisplayName())[0];
			Hears(TEXT("Hello? Anybody?"), nullptr, TEXT("30 m from anyone"));
			Hears(*FString::Printf(TEXT("%s, come here."), *Cap), nullptr, TEXT("30 m from anyone, calling a name"));
			break;
		}
		// right over their heads, on another floor
		Pawn->SetActorLocation(A->GetActorLocation() + FVector(0, 0, 95.f + 600.f), false, nullptr, ETeleportType::TeleportPhysics);
		Hears(TEXT("Where were you last night?"), nullptr, TEXT("on the floor above them"));
		StandBy(A, true);
		Next();
		return;
	}
	case 6: // "that's all" ends it; E still opens the sit-down interview and Q still leaves it
		if (TalkClock < 0.5f || A->bThinking) return;
		SayTo(TEXT("Where were you last night?"));
		Next();
		return;
	case 7:
		if (A->bThinking && TalkClock < 60.f) return;
		IHUInteractable::Execute_Interact(A, Pawn, false); // E
		Check(Interviewing.Get() == A && A->bInterviewing && !A->bWalkUp && PC->IsMoveInputIgnored(), TEXT("E on someone you're talking to opens the sit-down interview (held in place, sheet up)"));
		LeaveInterview();                                  // Q
		Check(!Interviewing.IsValid() && !A->bInterviewing && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored(), TEXT("Q leaves it: free to move again"));
		SayTo(TEXT("One more thing. Where were you last night?"));
		Next();
		return;
	case 8:
		if (A->bThinking && TalkClock < 60.f) return;
		SayTo(TEXT("That's all."));
		Check(!A->bInterviewing && !TalkingTo.IsValid(), FString::Printf(TEXT("\"That's all.\" ends it: %s"), *Subtitle.ToString()));
		Next();
		return;
	case 9: // the V key: a tap is the UV light
	{
		const int32 HeldBefore = Held; const bool bLightBefore = bLightOn;
		Subtitle = FText::GetEmpty();
		TalkDown(); TalkUp();
		const bool bUV = Held != HeldBefore || bLightOn != bLightBefore || Subtitle.ToString().Contains(TEXT("UV light"));
		Check(bUV && !bTalkOpen, TEXT("a tap of V is still the UV light"));
		EarsClock = 0.f; // look again for who's in earshot, now
		TalkDown();      // and hold it
		Next();
		return;
	}
	case 10:
	{
		if (TalkClock < 0.8f) return;
		UHUListenSubsystem* L = W->GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
		if (!L || !L->IsInstalled()) { UE_LOG(LogTemp, Display, TEXT("HUTalkTest: speech-to-text isn't installed here: holding V not checked")); }
		else if (bTalkOpen) { UE_LOG(LogTemp, Display, TEXT("HUTalkTest: holding V beside %s opened the microphone (%s)"), *A->DisplayName(), *L->MicLabel()); }
		else if (FParse::Param(FCommandLine::Get(), TEXT("nosound"))) { UE_LOG(LogTemp, Display, TEXT("HUTalkTest: this run has no sound (-nosound), so holding V couldn't open a microphone: not checked")); }
		else { UE_LOG(LogTemp, Display, TEXT("HUTalkTest: holding V beside %s did NOT open a microphone on this PC (none plugged in, or it is in use)"), *A->DisplayName()); }
		TalkUp();
		Next();
		return;
	}
	case 11: // out loud: the small voice says a line, the ears make it out, and it goes to whoever it's for
	{
		if (TalkClock < 1.5f) return;
		UHUListenSubsystem* L = W->GetGameInstance()->GetSubsystem<UHUListenSubsystem>();
		const FString AI = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI"));
		const FString Voice = AI / TEXT("voice/bin/sherpa-onnx-offline-tts.exe"), M = AI / TEXT("voice/supertonic");
		B = Second.Get();
		if (!L || !L->IsInstalled() || !B || !FPaths::FileExists(Voice) || !FPaths::FileExists(M / TEXT("tts.json")))
		{
			UE_LOG(LogTemp, Display, TEXT("HUTalkTest: the ears or the small voice aren't installed here (or there's nobody to call): saying it out loud not checked"));
			TalkStep = 13; TalkClock = 0.f;
			return;
		}
		// you face the first suspect; the second stands 3 m to the side
		struct FSpoken { FString Line; AHUSuspectActor* Want; const TCHAR* Why; };
		const TArray<FSpoken> Lines = {
			{ FString::Printf(TEXT("%s, where were you last night?"), *SecondName), B, TEXT("a name called") },
			{ TEXT("Where were you last night?"), A, TEXT("no name") },
			{ FString::Printf(TEXT("Did you see %s last night?"), *SecondName), A, TEXT("a name only mentioned") } };
		static int32 SpokenAt = 0;
		if (!Lines.IsValidIndex(SpokenAt)) { TalkStep = 13; TalkClock = 0.f; return; }
		const FSpoken Now = Lines[SpokenAt++];
		const FString Wav = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("TalkTest.wav"));
		IFileManager::Get().Delete(*Wav);
		const FString Args = FString::Printf(TEXT("--supertonic-duration-predictor=\"%s/duration_predictor.int8.onnx\" --supertonic-text-encoder=\"%s/text_encoder.int8.onnx\" ")
			TEXT("--supertonic-vector-estimator=\"%s/vector_estimator.int8.onnx\" --supertonic-vocoder=\"%s/vocoder.int8.onnx\" --supertonic-tts-json=\"%s/tts.json\" ")
			TEXT("--supertonic-unicode-indexer=\"%s/unicode_indexer.bin\" --supertonic-voice-style=\"%s/voice.bin\" --lang=en --num-steps=8 --num-threads=2 --sid=6 ")
			TEXT("--output-filename=\"%s\" \"%s\""), *M, *M, *M, *M, *M, *M, *M, *Wav, *Now.Line);
		int32 Code = -1;
		FPlatformProcess::ExecProcess(*Voice, *Args, &Code, nullptr, nullptr);
		TArray<uint8> Bytes;
		int32 Rate = 16000, DataAt = -1, DataLen = 0;
		if (FFileHelper::LoadFileToArray(Bytes, *Wav) && Bytes.Num() > 44)
		{
			FMemory::Memcpy(&Rate, Bytes.GetData() + 24, 4);
			for (int32 At = 12; At + 8 <= Bytes.Num();)
			{
				int32 Len; FMemory::Memcpy(&Len, Bytes.GetData() + At + 4, 4);
				if (!FMemory::Memcmp(Bytes.GetData() + At, "data", 4)) { DataAt = At + 8; DataLen = FMath::Min(Len, Bytes.Num() - DataAt); break; }
				At += 8 + Len + (Len & 1);
			}
		}
		if (DataAt < 0) { Check(false, FString::Printf(TEXT("the small voice didn't say \"%s\" (exit code %d)"), *Now.Line, Code)); TalkClock = 1.f; return; }
		TArray<float> Mono; Mono.SetNumUninitialized(DataLen / 2);
		const int16* S16 = reinterpret_cast<const int16*>(Bytes.GetData() + DataAt);
		for (int32 i = 0; i < Mono.Num(); i++) Mono[i] = S16[i] / 32768.f;
		StandBy(A, true);
		TalkStep = 12; TalkClock = 0.f;
		TWeakObjectPtr<UHUInteractComponent> Self(this);
		L->HearSamples(Mono, Rate, FHUHeard::CreateLambda([Self, Said = Now.Line, Want = TWeakObjectPtr<AHUSuspectActor>(Now.Want), Why = FString(Now.Why)](const FString& Text)
		{
			UHUInteractComponent* Me = Self.Get();
			if (!Me) return;
			FString How;
			const AHUSuspectActor* Got = Me->WhoHears(Text, &How);
			const bool bOk = !Text.IsEmpty() && Got && Got == Want.Get();
			// The ears got the name itself wrong (short names are the weak spot: "Sal" has come back as "Saw"). What was
			// heard then calls nobody, and it went where that pointed: a miss by the ears, noted, not a wrong turn here.
			const bool bMisheard = !bOk && !Text.IsEmpty() && Want.IsValid() && Why == TEXT("a name called") && !HUTalk::Calls(HUTalk::Split(Text), HUTalk::NameWords(Want->DisplayName()));
			if (!bOk && !bMisheard) Me->TalkWrong++;
			UE_LOG(LogTemp, Display, TEXT("HUTalkTest: %s  said out loud (%s): \"%s\" was heard as \"%s\" -> %s (%s)%s"), bOk ? TEXT("ok   ") : bMisheard ? TEXT("note ") : TEXT("WRONG"), *Why, *Said, *Text,
				Got ? *Got->DisplayName() : TEXT("nobody"), *How, bMisheard ? TEXT(" - the ears misheard the name") : TEXT(""));
			Me->TalkStep = 11; Me->TalkClock = 1.f; // on to the next line
		}));
		return;
	}
	case 12:
		if (TalkClock > 60.f) { Check(false, TEXT("the ears never answered")); TalkStep = 11; TalkClock = 1.f; }
		return;
	case 13: // voices made ahead: you walked up to them, so what they're likeliest to say has been spoken already
	{
		UHUVoiceSubsystem* V = W->GetGameInstance()->GetSubsystem<UHUVoiceSubsystem>();
		const UHUCaseSubsystem* Cs = W->GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
		const UHUBrainSubsystem* Br = W->GetGameInstance()->GetSubsystem<UHUBrainSubsystem>();
		if (!V || !Cs || !V->HasLocalVoice()) { UE_LOG(LogTemp, Display, TEXT("HUTalkTest: this run has no local voice (-HUNoVoice): voices made ahead not checked")); Finish(); return; }
		if (TalkClock < 0.3f) { StandBy(A, true); return; }
		// With no brain it is the hand-written answers that are made ahead - and nobody has asked this one yet, so if
		// its sound is here it was made ahead. With a brain: their introduction (said once already, so kept).
		const bool bByHand = Br && !Br->IsInstalled();
		const FString Line = bByHand ? HUCanned::AnswerTopic(*Cs, A->SuspectId, UHUBrainSubsystem::SawAnything) : HUCanned::Introduction(*Cs, A->SuspectId);
		if (!V->HasAhead(Line, A->VoiceNum()) && TalkClock < 120.f) return;
		Check(V->HasAhead(Line, A->VoiceNum()), FString::Printf(TEXT("%s's \"%s\" %s"), *A->DisplayName(), *Line.Left(70),
			bByHand ? TEXT("was spoken and kept before anyone asked the question") : TEXT("is kept from the first time it was said")));
		const int32 Before = V->ServedFromKept();
		SayTo(bByHand ? TEXT("Did you see or hear anything tonight?") : TEXT("Hello."));
		Check(V->ServedFromKept() == Before + 1 && A->LastReply == Line, TEXT("asked now, it is spoken with no wait: the sound was already made"));
		TalkStep = bByHand || !Br || !Br->IsInstalled() ? 14 : 15; TalkClock = 0.f;
		return;
	}
	case 15: // with a brain: an answer it wrote ahead for them has had its voice made ahead as well
	{
		UHUVoiceSubsystem* V = W->GetGameInstance()->GetSubsystem<UHUVoiceSubsystem>();
		UHUBrainSubsystem* Br = W->GetGameInstance()->GetSubsystem<UHUBrainSubsystem>();
		if (A->bThinking) return;
		int32 Topic = INDEX_NONE;
		FString Say;
		for (int32 T = UHUBrainSubsystem::TopicCount - 1; T >= 0 && Topic == INDEX_NONE; T--) if (Br->PeekReady(A->SuspectId, T, Say) && V->HasAhead(Say, A->VoiceNum())) Topic = T;
		if (Topic == INDEX_NONE)
		{
			if (TalkClock < 150.f) return;
			Check(false, FString::Printf(TEXT("nothing the brain wrote ahead for %s had its voice made ahead within 150 s"), *A->DisplayName()));
			TalkStep = 14; TalkClock = 0.f;
			return;
		}
		const int32 Kept = V->ServedFromKept(), Ahead = Br->Tallied(UHUBrainSubsystem::EAnsweredBy::Ahead);
		SayTo(UHUBrainSubsystem::TopicQuestion(Topic));
		Check(V->ServedFromKept() == Kept + 1 && Br->Tallied(UHUBrainSubsystem::EAnsweredBy::Ahead) == Ahead + 1 && A->LastReply == Say,
			FString::Printf(TEXT("\"%s\" -> written ahead by the brain AND spoken ahead (%.0f s after walking up): no wait for words or voice: %s"), UHUBrainSubsystem::TopicQuestion(Topic), TalkClock, *Say.Left(80)));
		TalkStep = 14; TalkClock = 0.f;
		return;
	}
	case 14: // a stalling line said twice is heard twice: every play gets a sound of its own
	{
		UHUVoiceSubsystem* V = W->GetGameInstance()->GetSubsystem<UHUVoiceSubsystem>();
		const FString Stall = HUFiller::Lines(HUFiller::Question)[0];
		if (!V->HasAhead(Stall, A->VoiceNum()) && TalkClock < 90.f) return;
		float S1 = 0.f, S2 = 0.f;
		USoundWaveProcedural* One = Cast<USoundWaveProcedural>(V->FromKept(Stall, A->VoiceNum(), S1));
		USoundWaveProcedural* Two = Cast<USoundWaveProcedural>(V->FromKept(Stall, A->VoiceNum(), S2));
		if (!One) { UE_LOG(LogTemp, Display, TEXT("HUTalkTest: their stalling lines weren't recorded in time: saying one twice not checked")); }
		else Check(Two && One != Two && One->GetAvailableAudioByteCount() > 0 && Two->GetAvailableAudioByteCount() > 0 && S1 > 0.2f,
			FString::Printf(TEXT("\"%s\" said twice is two sounds, each %.1f s long with its samples still to play"), *Stall, S1));
		Finish();
		return;
	}
	default:
		Finish();
		return;
	}
}
#endif
