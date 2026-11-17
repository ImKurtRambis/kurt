// The local voice: Supertonic 3, spoken on the processor. Why there is only this one: HUVoiceSubsystem.h.
//
// HOW A LINE IS SAID. sherpa-onnx's own program (AI\voice\bin\sherpa-onnx-offline-tts.exe) is run once for the line:
// it loads the voice, says the line into a .wav file and ends. The file is read, brought down from the 44.1 kHz the
// voice speaks at to the 24 kHz the game plays, and the silence at its ends cut. All of that happens on a thread of
// its own, so the game never waits on it: about a second a line on a fast processor, most of it the program starting.
// No Python, no server, no port. Nothing is left running between lines, and a line that goes wrong is only a caption:
// it can't take the game with it.
//
// WHO GOES FIRST. Lines someone is waiting to hear (a suspect's answer, the 911 call) are said as they come, two at a
// time at most. Lines for later - a suspect's stalling lines, answers said before the question - are said one at a
// time, only while nobody is waiting, and on processor time the game isn't using: they never make an answer late.
//
// WHAT IS KEPT. Every line said is kept in memory, so the same words in the same voice play at once the next time.
// Stalling lines are kept on disk as well (Saved\VoiceCache): there are only ten voices, so after a few sessions
// none of them has to be said again.
#include "HUVoiceSubsystem.h"
#include "HUProcess.h"
#include "Sound/SoundWave.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Async/Async.h"
#include "TimerManager.h"

namespace HUVoiceLocal
{
	constexpr int32 VoiceCount = 10;     // 0-4 women, 5-9 men
	constexpr int32 Steps = 8;           // how carefully a line is made: more is better and slower
	constexpr int32 AtOnce = 2;          // voice programs running at the same time, at most
	constexpr double GiveUpAfter = 45.0; // seconds: a line that has taken this long isn't coming

	static FString VoiceFile(const TCHAR* Rel) { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/voice") / Rel); }
	static FString Program() { return VoiceFile(TEXT("bin/sherpa-onnx-offline-tts.exe")); }
	// where the program writes a line's sound; the file is read and deleted as soon as it's written
	static FString ScratchDir() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Voice")); }

	// a .wav file -> its first channel as 16-bit samples, and the rate it was recorded at
	static bool ReadWav(const TArray<uint8>& Wav, TArray<int16>& Out, int32& OutRate)
	{
		if (Wav.Num() < 44 || FMemory::Memcmp(Wav.GetData(), "RIFF", 4) != 0) return false;
		int32 Channels = 1, Bits = 16, Format = 1;
		OutRate = 0;
		int32 P = 12;
		while (P + 8 <= Wav.Num())
		{
			const int32 Size = *reinterpret_cast<const int32*>(Wav.GetData() + P + 4);
			if (Size < 0) return false;
			if (FMemory::Memcmp(Wav.GetData() + P, "fmt ", 4) == 0 && P + 24 <= Wav.Num())
			{
				Format = *reinterpret_cast<const int16*>(Wav.GetData() + P + 8);
				Channels = FMath::Max<int32>(1, *reinterpret_cast<const int16*>(Wav.GetData() + P + 10));
				OutRate = *reinterpret_cast<const int32*>(Wav.GetData() + P + 12);
				Bits = *reinterpret_cast<const int16*>(Wav.GetData() + P + 22);
			}
			else if (FMemory::Memcmp(Wav.GetData() + P, "data", 4) == 0)
			{
				const bool bFloat = Format == 3 && Bits == 32;
				if ((!bFloat && !(Format == 1 && Bits == 16)) || OutRate < 8000) return false;
				const int32 Len = FMath::Min(Size, Wav.Num() - P - 8);
				const uint8* D = Wav.GetData() + P + 8;
				const int32 Frames = Len / (Bits / 8) / Channels;
				Out.SetNumUninitialized(Frames);
				for (int32 i = 0; i < Frames; i++)
					Out[i] = bFloat ? (int16)FMath::Clamp(reinterpret_cast<const float*>(D)[i * Channels] * 32767.f, -32768.f, 32767.f) : reinterpret_cast<const int16*>(D)[i * Channels];
				return Frames > 0;
			}
			P += 8 + Size + (Size & 1);
		}
		return false;
	}

	// to the rate the game plays at
	static void ToGameRate(const TArray<int16>& In, int32 Rate, TArray<int16>& Out)
	{
		const int32 To = UHUVoiceSubsystem::SampleRate;
		if (Rate == To || In.Num() < 2) { Out = In; return; }
		TArray<float> A;
		A.SetNumUninitialized(In.Num());
		if (To < Rate)
		{
			// keep only what the lower rate can carry, or the rest folds back as hiss: a 65-point low-pass
			// (a sinc under a Hamming window) with its corner just under half the new rate
			const double Cut = 0.45 * To / Rate;
			double H[65], Sum = 0.0;
			for (int32 k = 0; k < 65; k++)
			{
				const double X = 2.0 * Cut * (k - 32) * PI;
				H[k] = 2.0 * Cut * (k == 32 ? 1.0 : FMath::Sin(X) / X) * (0.54 - 0.46 * FMath::Cos(2.0 * PI * k / 64.0));
				Sum += H[k];
			}
			for (double& V : H) V /= Sum;
			const int32 N = In.Num();
			for (int32 i = 0; i < N; i++)
			{
				double Acc = 0.0;
				for (int32 k = FMath::Max(0, 32 - i), End = FMath::Min(64, 32 + N - 1 - i); k <= End; k++) Acc += H[k] * In[i + k - 32];
				A[i] = (float)Acc;
			}
		}
		else for (int32 i = 0; i < In.Num(); i++) A[i] = In[i];
		const int32 N = (int32)((int64)In.Num() * To / Rate);
		const double Step = (double)Rate / To;
		Out.SetNumUninitialized(N);
		for (int32 i = 0; i < N; i++)
		{
			const double T = i * Step;
			const int32 I0 = FMath::Min((int32)T, In.Num() - 1), I1 = FMath::Min(I0 + 1, In.Num() - 1);
			Out[i] = (int16)FMath::Clamp(FMath::Lerp(A[I0], A[I1], (float)(T - I0)), -32768.f, 32767.f);
		}
	}

	// cut the silence the voice leaves at the end (and most of it at the start), keeping a short breath
	static void Trim(TArray<int16>& S)
	{
		const int32 Rate = UHUVoiceSubsystem::SampleRate, Loud = 655; // 2% of full
		int32 First = 0, Last = S.Num() - 1;
		while (First < S.Num() && FMath::Abs((int32)S[First]) <= Loud) First++;
		if (First >= S.Num()) return; // silence all through: leave it
		while (Last > First && FMath::Abs((int32)S[Last]) <= Loud) Last--;
		const int32 From = FMath::Max(0, First - Rate / 20), To = FMath::Min(S.Num(), Last + Rate / 6);
		if (To < S.Num()) S.SetNum(To, EAllowShrinking::No);
		if (From > 0) S.RemoveAt(0, From, EAllowShrinking::No);
	}

	// Runs the voice program for one line and waits for it to end: game-rate 16-bit samples, or false and the code
	// the program ended with (-1 it couldn't start, -2 it took too long). For any thread but the game's.
	static bool Say(const FString& Args, const FString& WavFile, bool bForLater, TArray<uint8>& OutPcm, int32& OutCode)
	{
		const FString Exe = Program();
		OutCode = -1;
		FProcHandle P = HUProcess::Launch(Exe, Args, FPaths::GetPath(Exe), bForLater);
		if (!P.IsValid()) return false;
		const double Started = FPlatformTime::Seconds();
		bool bGaveUp = false;
		while (FPlatformProcess::IsProcRunning(P))
		{
			if (FPlatformTime::Seconds() - Started > GiveUpAfter) { FPlatformProcess::TerminateProc(P, true); bGaveUp = true; break; }
			FPlatformProcess::Sleep(0.004f);
		}
		if (bGaveUp) OutCode = -2; else FPlatformProcess::GetProcReturnCode(P, &OutCode);
		FPlatformProcess::CloseProc(P);
		TArray<uint8> Wav;
		TArray<int16> Said, AtRate;
		int32 Rate = 0;
		const bool bRead = OutCode == 0 && FFileHelper::LoadFileToArray(Wav, *WavFile, FILEREAD_Silent) && ReadWav(Wav, Said, Rate);
		IFileManager::Get().Delete(*WavFile, false, true, true);
		if (!bRead) return false;
		ToGameRate(Said, Rate, AtRate);
		Trim(AtRate);
		OutPcm.SetNumUninitialized(AtRate.Num() * 2);
		FMemory::Memcpy(OutPcm.GetData(), AtRate.GetData(), OutPcm.Num());
		return OutPcm.Num() > 0;
	}
}
namespace HL = HUVoiceLocal;

bool UHUVoiceSubsystem::FLine::Wanted() const { return Asks.ContainsByPredicate([](const FAsk& A) { return A.Need == WantedNow; }); }

bool UHUVoiceSubsystem::HasLocalVoice() const
{
	// looked for once: nothing installs a voice while the game runs. (-HUNoVoice: measuring the game without it.)
	static const bool bInstalled = !FParse::Param(FCommandLine::Get(), TEXT("HUNoVoice"))
		&& IFileManager::Get().FileExists(*HL::Program()) && IFileManager::Get().FileExists(*HL::VoiceFile(TEXT("supertonic/tts.json")));
	return bInstalled && !bBroken;
}

void UHUVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!IsRunningCommandlet() && !FApp::IsUnattended()) WarmUp();
	if (FParse::Param(FCommandLine::Get(), TEXT("HUVoiceTest"))) VoiceTest();
}

// One short line as the game starts. The first line of a session costs more than the rest - Windows is reading the
// program and the voice off the disk - and this way no suspect's first answer pays for it. It also shows, before
// anyone is waiting on a line, whether the voice runs on this PC.
void UHUVoiceSubsystem::WarmUp()
{
	if (bWarm || !HasLocalVoice()) return;
	bWarm = true;
	// sound files a session that crashed left behind
	TArray<FString> Old;
	IFileManager::Get().FindFiles(Old, *(HL::ScratchDir() / TEXT("line_*.wav")), true, false);
	for (const FString& F : Old) if (IFileManager::Get().GetFileAgeSeconds(*(HL::ScratchDir() / F)) > 600.0) IFileManager::Get().Delete(*(HL::ScratchDir() / F), false, true, true);
	const double Asked = FPlatformTime::Seconds();
	SynthesizeLocal(TEXT("Okay."), 0, false, Ahead, FHUVoiceReady::CreateLambda([Asked](USoundWave*, float Seconds)
	{
		if (Seconds > 0.f) UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: ready (Supertonic, on the processor) - its first line took %.2f s"), FPlatformTime::Seconds() - Asked);
	}));
}

// The words as the voice program can be given them. It reads them off its command line, which carries plain ASCII
// safely and nothing else: tags the bigger voices used to act on (<sigh>) go, curly quotes and long dashes become
// plain ones, accents come off letters. Nothing left to say (a line that is only "...") comes back empty.
FString UHUVoiceSubsystem::Plain(const FString& Text)
{
	static const TCHAR* Accented = TEXT("àáâãäåçèéêëìíîïñòóôõöøùúûüýÿ")
		TEXT("ÀÁÂÃÄÅÇÈÉÊËÌÍÎÏÑÒÓÔÕÖØÙÚÛÜÝ");
	static const TCHAR* Unaccented = TEXT("aaaaaaceeeeiiiinoooooouuuuyy") TEXT("AAAAAACEEEEIIIINOOOOOOUUUUY");
	FString Out;
	Out.Reserve(Text.Len());
	bool bWord = false;
	for (int32 i = 0; i < Text.Len(); i++)
	{
		const TCHAR Ch = Text[i];
		if (Ch == '<')
		{
			int32 End = i + 1;
			while (End < Text.Len() && Text[End] != '>' && Text[End] != '<') End++;
			if (End < Text.Len() && Text[End] == '>' && End > i + 1) i = End; // a whole <tag>: gone
			continue; // (a stray < goes too)
		}
		const TCHAR* Hit = nullptr;
		if (Ch == 0x2018 || Ch == 0x2019) Out += '\'';
		else if (Ch == 0x2013) Out += '-';
		else if (Ch == 0x2014) Out += TEXT(", ");
		else if (Ch == 0x2026) Out += TEXT("...");
		else if (Ch == '\n' || Ch == '\r' || Ch == '\t') Out += ' ';
		else if (Ch == '"' || Ch == '\\' || Ch == '*') {} // quotes and stage-direction stars aren't said, and both upset a command line
		else if (Ch >= 32 && Ch < 127) { Out += Ch; bWord |= FChar::IsAlnum(Ch); }
		else if (Ch >= 0xc0 && (Hit = FCString::Strchr(Accented, Ch)) != nullptr) { Out += Unaccented[Hit - Accented]; bWord = true; }
	}
	if (!bWord) return FString();
	while (Out.ReplaceInline(TEXT("  "), TEXT(" ")) > 0) {}
	Out.TrimStartAndEndInline();
	// (a line can't start with a dash: the program would take it for one of its settings)
	while (Out.StartsWith(TEXT("-")) || Out.StartsWith(TEXT(",")) || Out.StartsWith(TEXT(" "))) Out.RightChopInline(1);
	return Out;
}

// ---------- lines kept, lines made ahead ----------
// A line takes a second or so to say. What a suspect is likely to say next can be said ahead of time and its sound
// kept: asked for later, the same words in the same voice play at once.
FString UHUVoiceSubsystem::KeptKey(const FString& Text, int32 Voice, bool bPhone)
{
	return FString::Printf(TEXT("%d%s|%s"), FMath::Abs(Voice) % HL::VoiceCount, bPhone ? TEXT("p") : TEXT(""), *Plain(Text));
}

FString UHUVoiceSubsystem::DiskFile(const FString& Key)
{
	// (named for the voice and how it was made, so a change to either never plays an old recording)
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VoiceCache") / FString::Printf(TEXT("supertonic3-%d-%s.pcm"), HL::Steps, *FMD5::HashAnsiString(*Key)));
}

void UHUVoiceSubsystem::KeepSound(const FString& Key, const TArray<uint8>& Pcm)
{
	constexpr int64 Cap = 64 * 1024 * 1024; // about 22 minutes of speech; the oldest goes first
	if (Pcm.Num() == 0 || Kept.Contains(Key)) return;
	Kept.Add(Key, Pcm);
	KeptOrder.Add(Key);
	KeptBytes += Pcm.Num();
	while (KeptBytes > Cap && KeptOrder.Num() > 1)
	{
		if (const TArray<uint8>* Old = Kept.Find(KeptOrder[0])) KeptBytes -= Old->Num();
		Kept.Remove(KeptOrder[0]);
		KeptOrder.RemoveAt(0);
	}
}

bool UHUVoiceSubsystem::HasAhead(const FString& Text, int32 Voice) const { return Kept.Contains(KeptKey(Text, Voice, false)); }

USoundWave* UHUVoiceSubsystem::FromKept(const FString& Text, int32 Voice, float& OutSeconds)
{
	const TArray<uint8>* Have = Kept.Find(KeptKey(Text, Voice, false));
	if (!Have) return nullptr;
	TArray<uint8> Pcm = *Have; // a sound uses its samples up as it plays: every play gets its own
	return MakeSound(Pcm, false, OutSeconds);
}

void UHUVoiceSubsystem::MakeAhead(const FString& Text, int32 Voice)
{
	if (HasLocalVoice()) SynthesizeLocal(Text, Voice, false, Ahead, FHUVoiceReady());
}

// ---------- saying lines ----------
// Done gets the sound and its length. A line made Ahead is made to be kept, not played: its Done gets no sound, and
// the length (0 if it couldn't be said).
void UHUVoiceSubsystem::SynthesizeLocal(const FString& Text, int32 Voice, bool bPhone, uint8 Need, FHUVoiceReady Done)
{
	const FString Words = Plain(Text);
	if (Words.IsEmpty()) { Done.ExecuteIfBound(nullptr, 0.f); return; }
	const int32 Who = FMath::Abs(Voice) % HL::VoiceCount;
	const FString Key = KeptKey(Words, Who, bPhone);
	const TArray<uint8>* Have = Kept.Find(Key);
	// a stalling line said in an earlier session is on disk
	if (!Have && Need == Premade)
		if (TArray<uint8> Disk; FFileHelper::LoadFileToArray(Disk, *DiskFile(Key), FILEREAD_Silent) && Disk.Num() > 2000 && Disk.Num() % 2 == 0)
		{
			KeepSound(Key, Disk);
			Have = Kept.Find(Key);
		}
	// already made: its sound is here, no waiting
	if (Have)
	{
		if (Need == Ahead) { Done.ExecuteIfBound(nullptr, Have->Num() / 2.f / SampleRate); return; }
		TArray<uint8> Pcm = *Have;
		float Seconds = 0.f;
		USoundWave* S = MakeSound(Pcm, bPhone, Seconds);
		if (Need == WantedNow) { ServedKept++; UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: already made, no wait: \"%s\""), *Words.Left(60)); }
		Done.ExecuteIfBound(S, Seconds);
		return;
	}
	// the same words in the same voice are already being said, or waiting to be: this asker shares that saying of them
	// (an answer made ahead and asked for before it's finished comes back as soon as it is, not after a second go)
	FLine* Line = Queue.FindByPredicate([&Key](const FLine& L) { return L.Key == Key; });
	if (!Line)
	{
		Line = &Queue.AddDefaulted_GetRef();
		Line->Text = Words; Line->Key = Key; Line->Voice = Who; Line->bPhone = bPhone; Line->AskedAt = FPlatformTime::Seconds();
	}
	if (Need != Ahead || Done.IsBound() || Line->Asks.Num() == 0) Line->Asks.Add({ Need, Done });
	SpeakNext();
}

// Start whatever can start now: lines someone is waiting on first, in the order asked, then lines for later.
void UHUVoiceSubsystem::SpeakNext()
{
	for (;;)
	{
		int32 Speaking = 0, Next = INDEX_NONE;
		for (int32 i = 0; i < Queue.Num(); i++)
		{
			if (Queue[i].bSpeaking) Speaking++;
			else if (Next == INDEX_NONE || (Queue[i].Wanted() && !Queue[Next].Wanted())) Next = i;
		}
		if (Next == INDEX_NONE || Speaking >= HL::AtOnce) return;
		FLine& L = Queue[Next];
		const bool bForLater = !L.Wanted();
		if (bForLater && Speaking > 0) return; // lines for later: one at a time, and never beside a line someone is waiting on
		L.bSpeaking = true;
		IFileManager::Get().MakeDirectory(*HL::ScratchDir(), true);
		const FString Wav = HL::ScratchDir() / FString::Printf(TEXT("line_%u_%d.wav"), FPlatformProcess::GetCurrentProcessId(), ++LineCount);
		const FString M = HL::VoiceFile(TEXT("supertonic"));
		const FString Args = FString::Printf(TEXT("--supertonic-duration-predictor=\"%s/duration_predictor.int8.onnx\" --supertonic-text-encoder=\"%s/text_encoder.int8.onnx\" ")
			TEXT("--supertonic-vector-estimator=\"%s/vector_estimator.int8.onnx\" --supertonic-vocoder=\"%s/vocoder.int8.onnx\" --supertonic-tts-json=\"%s/tts.json\" ")
			TEXT("--supertonic-unicode-indexer=\"%s/unicode_indexer.bin\" --supertonic-voice-style=\"%s/voice.bin\" --lang=en --num-steps=%d --num-threads=2 --sid=%d ")
			TEXT("--output-filename=\"%s\" \"%s\""), *M, *M, *M, *M, *M, *M, *M, HL::Steps, L.Voice, *Wav, *L.Text);
		TWeakObjectPtr<UHUVoiceSubsystem> Self(this);
		Async(EAsyncExecution::Thread, [Self, Args, Wav, bForLater, Key = L.Key]()
		{
			const double Started = FPlatformTime::Seconds();
			TArray<uint8> Pcm;
			int32 Code = 0;
			HL::Say(Args, Wav, bForLater, Pcm, Code);
			const float Took = (float)(FPlatformTime::Seconds() - Started);
			AsyncTask(ENamedThreads::GameThread, [Self, Key, Code, Took, Pcm = MoveTemp(Pcm)]() mutable { if (UHUVoiceSubsystem* V = Self.Get()) V->Spoke(Key, Pcm, Code, Took); });
		});
	}
}

// a line is back from the voice program (Pcm empty: it couldn't be said)
void UHUVoiceSubsystem::Spoke(const FString& Key, TArray<uint8>& Pcm, int32 Code, float Took)
{
	const int32 At = Queue.IndexOfByPredicate([&Key](const FLine& L) { return L.bSpeaking && L.Key == Key; });
	if (At == INDEX_NONE) return;
	// (out of the list before anyone is told: being told, they often ask for their next line)
	const FLine Line = MoveTemp(Queue[At]);
	Queue.RemoveAt(At);
	const float Seconds = Pcm.Num() / 2.f / SampleRate;
	if (Pcm.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("HomicideUnit voice: couldn't say a line (%s) - it shows as a caption: \"%s\""),
			Code == -1 ? TEXT("the voice program didn't start") : Code == -2 ? TEXT("it took too long") : *FString::Printf(TEXT("the voice program ended with code %d"), Code), *Line.Text.Left(60));
		// The same line failing again says little; three different ones in a row, with none said in between, and the
		// voice doesn't run on this PC. Stop trying: every try is a program started for nothing.
		if (LastFailed != Line.Text) { LastFailed = Line.Text; Failures++; }
		if (Failures >= 3 && !bBroken)
		{
			bBroken = true;
			UE_LOG(LogTemp, Error, TEXT("HomicideUnit voice: the local voice doesn't run on this PC - lines show as captions for the rest of this session"));
			TArray<FLine> Dropped;
			for (int32 i = Queue.Num() - 1; i >= 0; i--) if (!Queue[i].bSpeaking) { Dropped.Add(MoveTemp(Queue[i])); Queue.RemoveAt(i); }
			for (const FLine& D : Dropped) for (const FAsk& A : D.Asks) A.Done.ExecuteIfBound(nullptr, 0.f);
		}
	}
	else
	{
		Failures = 0; LastFailed.Reset();
		// Every line is kept (up to the cap), not only the ones made ahead: written lines come round again - "I want my
		// lawyer", an introduction - and the second time they cost nothing. (Before MakeSound: a phone line's samples
		// get changed.)
		KeepSound(Key, Pcm);
		if (Line.Asks.ContainsByPredicate([](const FAsk& A) { return A.Need == Premade; }))
			Async(EAsyncExecution::ThreadPool, [File = DiskFile(Key), Copy = Pcm]()
			{
				// (under another name until it's whole, so a crash half way never leaves half a line to be played)
				if (FFileHelper::SaveArrayToFile(Copy, *(File + TEXT(".part")))) IFileManager::Get().Move(*File, *(File + TEXT(".part")), true, true);
			});
		if (Line.Wanted()) { UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: said in %.2f s (%.1f s of speech, voice %d%s): \"%s\""), Took, Seconds, Line.Voice, Line.bPhone ? TEXT(", phone") : TEXT(""), *Line.Text.Left(60)); }
		else { UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: made ahead in %.2f s (%d more to make): \"%s\""), Took, Queue.Num(), *Line.Text.Left(60)); }
	}
	for (const FAsk& A : Line.Asks)
	{
		if (Pcm.Num() == 0 || A.Need == Ahead) { A.Done.ExecuteIfBound(nullptr, Seconds); continue; }
		TArray<uint8> Copy = Pcm; // a sound uses its samples up as it plays: every asker gets their own
		float Length = 0.f;
		USoundWave* S = MakeSound(Copy, Line.bPhone, Length);
		A.Done.ExecuteIfBound(S, Length);
	}
	SpeakNext();
}

// -HUVoiceTest: says lines the way the game does, checks what comes back and how long each took, and quits.
//   1 six lines one after another (the 911 call's two voices on the phone, four suspects)
//   2 the same six again: kept, so each must be back before the call returns
//   3 two asked for at the same moment: both come back
//   4 an awkward line (curly quotes, a long dash, accents, a <tag>, stars) and one with nothing to say
//   5 three made ahead, then one of them asked for: no wait
//   6 a stalling line: kept on disk, and found there once it's forgotten from memory
void UHUVoiceSubsystem::VoiceTest()
{
#if !UE_BUILD_SHIPPING
	struct FTest
	{
		int32 Step = 0, Wrong = 0, Back = 0, Lines = 0;
		bool bAsked = false;
		double Asked = 0.0, Work = 0.0, Speech = 0.0;
		FTimerHandle Tick;
		void Check(bool bOk, const FString& What) { if (!bOk) Wrong++; UE_LOG(LogTemp, Display, TEXT("HUVoiceTest: %s  %s"), bOk ? TEXT("ok   ") : TEXT("WRONG"), *What); }
	};
	struct FSay { const TCHAR* Text; int32 Voice; bool bPhone; };
	static const FSay Six[] = {
		{ TEXT("Nine one one, what is the address of your emergency?"), 0, true },
		{ TEXT("There's a man on the floor, he's not moving, please, you have to send someone right now!"), 3, true },
		{ TEXT("I was by the coat check most of the night."), 6, false },
		{ TEXT("Fine. Yes. But that's got nothing to do with this."), 2, false },
		{ TEXT("Look, I've told you everything I know. I never went near the office."), 8, false },
		{ TEXT("I don't know what you want me to say. I heard a door slam around ten, and that's all I heard."), 5, false },
	};
	static const TCHAR* Awkward = TEXT("—Wait. “José’s café”… <sigh> it's *closed*, isn't it?");
	static const TCHAR* AheadLines[] = { TEXT("I never touched the safe."), TEXT("Ask the night manager. He has the keys."), TEXT("We argued. That's all it was.") };
	static const TCHAR* Stall = TEXT("Give me a second to think about that.");
	TSharedRef<FTest> T = MakeShared<FTest>();
	TWeakObjectPtr<UHUVoiceSubsystem> Self(this);
	UE_LOG(LogTemp, Display, TEXT("HUVoiceTest: starting (%s)"), HasLocalVoice() ? TEXT("the local voice is installed") : TEXT("NO LOCAL VOICE"));
	GetGameInstance()->GetTimerManager().SetTimer(T->Tick, FTimerDelegate::CreateLambda([Self, T]()
	{
		UHUVoiceSubsystem* V = Self.Get();
		if (!V) return;
		auto Finish = [&]()
		{
			if (T->Lines > 0) UE_LOG(LogTemp, Display, TEXT("HUVoiceTest: %d lines said one at a time: %.2f s a line on average, %.2f s of work for each second of speech"), T->Lines, T->Work / T->Lines, T->Work / FMath::Max(0.1, T->Speech));
			if (T->Wrong == 0) { UE_LOG(LogTemp, Display, TEXT("HUVoiceTest: all checks passed")); }
			else { UE_LOG(LogTemp, Error, TEXT("HUVoiceTest: %d CHECKS WRONG"), T->Wrong); }
			V->GetGameInstance()->GetTimerManager().ClearTimer(T->Tick);
			FPlatformMisc::RequestExit(false);
		};
		if (!V->HasLocalVoice()) { T->Check(false, TEXT("the local voice is installed and runs (AI\\voice)")); Finish(); return; }
		const double Now = FPlatformTime::Seconds();
		if (T->bAsked && Now - T->Asked > 120.0) { T->Check(false, FString::Printf(TEXT("step %d came back within two minutes"), T->Step)); Finish(); return; }
		if (T->Step < 6) // 1: one after another
		{
			if (T->bAsked) return;
			const FSay& L = Six[T->Step];
			T->bAsked = true; T->Asked = Now;
			V->Synthesize(L.Text, L.Voice, FString(), L.bPhone, FHUVoiceReady::CreateLambda([T, &L](USoundWave* S, float Seconds)
			{
				const double Took = FPlatformTime::Seconds() - T->Asked;
				T->Check(S && Seconds > 1.f, FString::Printf(TEXT("%s line, voice %d: %.1f s of speech in %.2f s"), L.bPhone ? TEXT("phone") : TEXT("suspect"), L.Voice, Seconds, Took));
				T->Lines++; T->Work += Took; T->Speech += Seconds;
				T->bAsked = false; T->Step++;
			}));
		}
		else if (T->Step == 6) // 2: kept
		{
			const int32 Before = V->ServedFromKept();
			T->Back = 0;
			for (const FSay& L : Six) V->Synthesize(L.Text, L.Voice, FString(), L.bPhone, FHUVoiceReady::CreateLambda([T](USoundWave* S, float) { if (S) T->Back++; }));
			T->Check(T->Back == 6 && V->ServedFromKept() == Before + 6, FString::Printf(TEXT("the same six asked for again are back at once, from the lines kept (%d of 6)"), T->Back));
			T->Step++;
		}
		else if (T->Step == 7) // 3: two at the same moment
		{
			if (T->bAsked) { if (T->Back < 2) return; T->Check(true, FString::Printf(TEXT("two lines asked for together are both back, %.2f s after asking"), Now - T->Asked)); T->bAsked = false; T->Step++; return; }
			T->bAsked = true; T->Asked = Now; T->Back = 0;
			V->Synthesize(TEXT("You can't keep me here all night. I know my rights."), 7, FString(), false, FHUVoiceReady::CreateLambda([T](USoundWave* S, float) { if (S) T->Back++; }));
			V->Synthesize(TEXT("I told the other officer already. Why am I telling you again?"), 1, FString(), false, FHUVoiceReady::CreateLambda([T](USoundWave* S, float) { if (S) T->Back++; }));
		}
		else if (T->Step == 8) // 4: awkward, and nothing to say
		{
			if (T->bAsked) return;
			T->Back = 0;
			V->Synthesize(TEXT("… <sigh> *...*"), 4, FString(), false, FHUVoiceReady::CreateLambda([T](USoundWave* S, float) { T->Back = S ? 1 : -1; }));
			T->Check(T->Back == -1, TEXT("a line with nothing to say comes back at once with no sound (caption only)"));
			T->Check(Plain(Awkward) == TEXT("Wait. Jose's cafe... it's closed, isn't it?"), FString::Printf(TEXT("an awkward line is handed to the voice as plain words: \"%s\""), *Plain(Awkward)));
			T->bAsked = true; T->Asked = Now;
			V->Synthesize(Awkward, 9, FString(), false, FHUVoiceReady::CreateLambda([T](USoundWave* S, float Seconds)
			{
				T->Check(S && Seconds > 1.f, FString::Printf(TEXT("and it is said: %.1f s of speech in %.2f s"), Seconds, FPlatformTime::Seconds() - T->Asked));
				T->bAsked = false; T->Step++;
			}));
		}
		else if (T->Step == 9) // 5: made ahead
		{
			if (!T->bAsked) { T->bAsked = true; T->Asked = Now; for (const TCHAR* L : AheadLines) V->MakeAhead(L, 6); return; }
			for (const TCHAR* L : AheadLines) if (!V->HasAhead(L, 6)) return;
			T->Check(true, FString::Printf(TEXT("three lines made ahead are all made, %.2f s after asking"), Now - T->Asked));
			const int32 Before = V->ServedFromKept();
			T->Back = 0;
			V->Synthesize(AheadLines[1], 6, FString(), false, FHUVoiceReady::CreateLambda([T](USoundWave* S, float) { if (S) T->Back = 1; }));
			T->Check(T->Back == 1 && V->ServedFromKept() == Before + 1, TEXT("asked for now, one of them is spoken with no wait"));
			T->bAsked = false; T->Step++;
		}
		else if (T->Step == 10) // 6: a stalling line, kept on disk
		{
			const FString Key = KeptKey(Stall, 3, false), File = DiskFile(Key);
			if (!T->bAsked)
			{
				IFileManager::Get().Delete(*File, false, true, true);
				T->bAsked = true; T->Asked = Now; T->Back = 0;
				V->Synthesize(Stall, 3, FString(), false, FHUVoiceReady::CreateLambda([T](USoundWave* S, float) { T->Back = S ? 1 : -1; }), 1.f, 0.5f, true);
				return;
			}
			if (T->Back == 0 || (T->Back > 0 && !IFileManager::Get().FileExists(*File) && Now - T->Asked < 30.0)) return; // (the file is written off the game's thread)
			T->Check(T->Back > 0 && IFileManager::Get().FileExists(*File), TEXT("a stalling line is said, and kept on disk (Saved\\VoiceCache)"));
			if (const TArray<uint8>* Have = V->Kept.Find(Key)) { V->KeptBytes -= Have->Num(); V->Kept.Remove(Key); V->KeptOrder.Remove(Key); } // as a new session would find it
			T->Back = 0;
			V->Synthesize(Stall, 3, FString(), false, FHUVoiceReady::CreateLambda([T](USoundWave* S, float) { if (S) T->Back = 1; }), 1.f, 0.5f, true);
			T->Check(T->Back == 1, TEXT("forgotten from memory, it comes back at once from the disk"));
			Finish();
		}
	}), 0.05f, true);
#endif
}
