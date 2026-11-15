#include "HUVoiceSpace.h"
#include "HUPeople.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ---------- how far ----------
USoundAttenuation* UHUVoiceSpace::Falloff(EFrom From)
{
	TObjectPtr<USoundAttenuation>& A = From == EFrom::Device ? DeviceFalloff : PersonFalloff;
	if (!A)
	{
		const bool bDevice = From == EFrom::Device;
		A = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& S = A->Attenuation;
		S.bAttenuate = true;
		S.bSpatialize = true;
		S.AttenuationShape = EAttenuationShape::Sphere;
		// A person talking: full across an interview table (1.2 m), about a third as loud across a room (5 m), a tenth
		// down the corridor (10 m), gone at 19 m. A tape played to a room is turned up: full to 4 m, carries to 34 m.
		S.AttenuationShapeExtents = FVector(bDevice ? 400.f : 120.f);
		S.FalloffDistance = bDevice ? 3000.f : 1800.f;
		S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; // decibels fall in step with distance, as ears expect
		S.dBAttenuationAtMax = bDevice ? -36.f : -42.f;
		// far off, the top goes first
		S.bAttenuateWithLPF = true;
		S.LPFRadiusMin = 400.f; S.LPFRadiusMax = 2200.f;
		S.LPFFrequencyAtMin = 20000.f; S.LPFFrequencyAtMax = 4500.f;
	}
	return A;
}

float UHUVoiceSpace::LoudnessAt(float Centimetres, EFrom From)
{
	return Falloff(From)->Attenuation.Evaluate(FTransform::Identity, FVector(Centimetres, 0, 0));
}

// ---------- what's between ----------
float UHUVoiceSpace::Dulled(EBetween B, float& OutLowPassHz)
{
	switch (B)
	{
	case EBetween::Glass: OutLowPassHz = 3000.f; return 0.5f; // a pane: half as loud, the edge taken off
	case EBetween::Solid: OutLowPassHz = 900.f; return 0.2f;  // a wall or a shut door: a murmur
	default: OutLowPassHz = 20000.f; return 1.f;
	}
}

// glass: a block given the Glass surface (HUSurfaces: its material comes from MI_Surface_Glass), or tagged HUGlass
static bool IsGlass(const UPrimitiveComponent* C)
{
	if (!C) return false;
	if (C->ComponentHasTag(TEXT("HUGlass"))) return true;
	for (const UMaterialInterface* M = C->GetMaterial(0); M;)
	{
		if (M->GetName().Contains(TEXT("Glass"))) return true;
		const UMaterialInstance* I = Cast<UMaterialInstance>(M);
		M = I ? I->Parent.Get() : nullptr;
	}
	return false;
}

UHUVoiceSpace::EBetween UHUVoiceSpace::Between(const FVector& EarAt, const FVector& Voice, const TArray<TWeakObjectPtr<AActor>>& Through) const
{
	const UWorld* W = GetWorld();
	const FVector To = Voice - EarAt;
	if (!W || To.SizeSquared() < 40.f * 40.f) return EBetween::Nothing; // face to face
	FCollisionQueryParams Base(SCENE_QUERY_STAT(HUVoiceBetween), false);
	for (const TWeakObjectPtr<AActor>& T : Through) if (T.IsValid()) Base.AddIgnoredActor(T.Get());
	if (const APlayerController* PC = W->GetFirstPlayerController()) if (const APawn* Me = PC->GetPawn()) Base.AddIgnoredActor(Me);

	// along one straight line: nothing, glass only, or something solid. People are no wall: capsules and bodies are passed.
	auto Along = [&](const FVector& From, const FVector& Until)
	{
		FCollisionQueryParams Q = Base;
		EBetween Found = EBetween::Nothing;
		for (int32 Step = 0; Step < 8; Step++)
		{
			FHitResult H;
			if (!W->LineTraceSingleByChannel(H, From, Until, ECC_Visibility, Q)) break;
			const UPrimitiveComponent* C = H.GetComponent();
			if (!C) break;
			Q.AddIgnoredComponent(C);
			if (C->IsA<UCapsuleComponent>() || C->IsA<USkeletalMeshComponent>()) continue;
			if (!IsGlass(C)) return EBetween::Solid;
			Found = EBetween::Glass;
		}
		return Found;
	};

	// Sound gets round a chair, a lamp, a monitor: besides the straight line, try three more beside it (above, left,
	// right, 30 cm off at both ends) and take the clearest. A line counts only if both its ends are in the open - in
	// the same space as the ear and as the voice - or a listener leaning on a wall would hear through it.
	const FVector Dir = To.GetSafeNormal();
	FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	if (Side.IsNearlyZero()) Side = FVector::RightVector;
	const FVector Up = FVector::CrossProduct(Side, Dir);
	const FVector Offsets[] = { FVector::ZeroVector, Up * 30.f, Side * 30.f, Side * -30.f };
	EBetween Best = EBetween::Solid;
	for (const FVector& Off : Offsets)
	{
		if (!Off.IsZero() && (Along(EarAt, EarAt + Off) != EBetween::Nothing || Along(Voice, Voice + Off) != EBetween::Nothing)) continue;
		const EBetween Here = Along(EarAt + Off, Voice + Off);
		if (Here < Best) Best = Here;
		if (Best == EBetween::Nothing) break;
	}
	return Best;
}

// ---------- saying it ----------
bool UHUVoiceSpace::Ear(FVector& OutEar) const
{
	const UWorld* W = GetWorld();
	const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC) return false;
	FRotator Unused;
	PC->GetPlayerViewPoint(OutEar, Unused);
	return true;
}

bool UHUVoiceSpace::Mouth(AActor* Look, USceneComponent*& OutOn, FName& OutBone, FVector& OutAt)
{
	// The body's head bone sits at the top of the neck, and the lips are 8.5 cm in front of it in the bone's own space
	// (measured on five of the cast: within a centimetre and a half on each). The body's bone, not the face's own lip
	// bone: a face is only posed while it is being drawn, so a suspect sat in the next room would speak from where
	// their mouth would be standing up - 40 cm over their head.
	USkeletalMeshComponent* Body = Look ? HUPeople::BodyOf(Look) : nullptr;
	if (!Body || !Body->DoesSocketExist(TEXT("head"))) return false;
	OutOn = Body; OutBone = TEXT("head");
	OutAt = Body->GetSocketTransform(OutBone).TransformPosition(FVector(-0.7f, 8.5f, 0.f));
	return true;
}

UAudioComponent* UHUVoiceSpace::Say(USoundBase* Sound, USceneComponent* On, FName Bone, const FVector& WorldAt, const TArray<AActor*>& Through, EFrom From)
{
	if (!Sound || !On) return nullptr;
	FVoice V;
	for (AActor* T : Through) if (T) V.Through.Add(T);
	// a line that starts behind a wall starts muffled: no fade down from clear
	FVector E;
	const bool bEar = Ear(E);
	const EBetween B = bEar ? Between(E, WorldAt, V.Through) : EBetween::Nothing;
	V.Loud = Dulled(B, V.LowPass);
	UAudioComponent* A = UGameplayStatics::SpawnSoundAttached(Sound, On, Bone, WorldAt, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
		true /*stops if the speaker goes*/, V.Loud, 1.f, 0.f, Falloff(From));
	// (the log has it, for "why can't I hear them": who, how far, what's between)
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit voice: %s said from %s%s, %.1f m from this player, %s between%s"), From == EFrom::Device ? TEXT("a tape") : TEXT("a line"),
		*GetNameSafe(On->GetOwner()), Bone.IsNone() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Bone.ToString()), bEar ? FVector::Dist(E, WorldAt) / 100.f : 0.f,
		B == EBetween::Nothing ? TEXT("nothing") : B == EBetween::Glass ? TEXT("glass") : TEXT("something solid"),
		A ? TEXT("") : TEXT(" - not played: no sound on this PC, or out of earshot"));
	if (!A) return nullptr;
	V.Audio = A;
	A->SetLowPassFilterEnabled(V.LowPass < 19000.f);
	A->SetLowPassFilterFrequency(V.LowPass);
	Voices.Add(MoveTemp(V));
	return A;
}

// while a line plays, keep up with the listener: step behind a wall and it muffles, step out and it clears
void UHUVoiceSpace::Tick(float DeltaTime)
{
	FVector E;
	const bool bEar = Ear(E);
	for (int32 i = Voices.Num() - 1; i >= 0; i--)
	{
		FVoice& V = Voices[i];
		UAudioComponent* A = V.Audio.Get();
		if (!A || !A->IsPlaying()) { Voices.RemoveAtSwap(i); continue; }
		if (!bEar) continue;
		float WantLowPass = 20000.f;
		const float WantLoud = Dulled(Between(E, A->GetComponentLocation(), V.Through), WantLowPass);
		V.Loud = FMath::FInterpTo(V.Loud, WantLoud, DeltaTime, 7.f);
		V.LowPass = FMath::FInterpTo(V.LowPass, WantLowPass, DeltaTime, 7.f);
		A->SetVolumeMultiplier(V.Loud);
		A->SetLowPassFilterEnabled(V.LowPass < 19000.f);
		A->SetLowPassFilterFrequency(V.LowPass);
	}
}

void UHUVoiceSpace::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("HUVoiceSpaceTest")))
	{
		// once the level has built its rooms and stood its people up
		FTimerHandle H;
		World.GetTimerManager().SetTimer(H, FTimerDelegate::CreateUObject(this, &UHUVoiceSpace::RunTest), 6.f, false);
	}
#endif
}

#if !UE_BUILD_SHIPPING
#include "HUBuild.h"
#include "HUVoiceSubsystem.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"

// -HUVoiceSpaceTest: the numbers behind "quieter across the room and through the glass", in the log
void UHUVoiceSpace::RunTest()
{
	UWorld* W = GetWorld();
	if (!W) return;
	int32 Wrong = 0;
	auto Check = [&Wrong](bool bOk, const FString& What) { if (!bOk) Wrong++; UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: %s  %s"), bOk ? TEXT("ok   ") : TEXT("WRONG"), *What); };

	// 1. how far
	for (const EFrom From : { EFrom::Person, EFrom::Device })
	{
		FString Line;
		for (const float Cm : { 100.f, 200.f, 300.f, 500.f, 1000.f, 1500.f, 2500.f })
			Line += FString::Printf(TEXT("%s%.0f m %.0f%%"), Line.IsEmpty() ? TEXT("") : TEXT(", "), Cm / 100.f, LoudnessAt(Cm, From) * 100.f);
		UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: %s, loudness by distance: %s"), From == EFrom::Person ? TEXT("a person") : TEXT("a tape played to a room"), *Line);
	}
	Check(LoudnessAt(110.f) > 0.99f, TEXT("across an interview table (1.1 m) a voice is at full loudness"));
	Check(LoudnessAt(500.f) > 0.25f && LoudnessAt(500.f) < 0.6f, FString::Printf(TEXT("across a room (5 m) it is clearly quieter but easy to hear: %.0f%%"), LoudnessAt(500.f) * 100.f));
	Check(LoudnessAt(1000.f) < LoudnessAt(500.f) * 0.5f && LoudnessAt(1000.f) > 0.03f, FString::Printf(TEXT("down the corridor (10 m) it is faint: %.0f%%"), LoudnessAt(1000.f) * 100.f));
	Check(LoudnessAt(2500.f) < 0.01f, FString::Printf(TEXT("at 25 m it is gone: %.1f%%"), LoudnessAt(2500.f) * 100.f));
	Check(LoudnessAt(500.f, EFrom::Device) > LoudnessAt(500.f, EFrom::Person), TEXT("a tape played to the room carries further than a person talking"));

	// 2. what's between: a rig high above the level - open air, a wall, a pane of glass, a pane then a wall, a monitor
	FActorSpawnParameters P;
	AActor* Rig = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(0, 0, 60000)), P);
	USceneComponent* Root = NewObject<USceneComponent>(Rig);
	Rig->SetRootComponent(Root);
	Root->RegisterComponent();
	Rig->SetActorLocation(FVector(0, 0, 60000));
	TArray<UStaticMeshComponent*> Blocks;
	const FLinearColor Grey(0.5f, 0.5f, 0.5f);
	HUBuild::Block(Rig, Root, Blocks, HUBuild::EShape::Cube, FVector(0, 1000, 150), FVector(20, 600, 300), Grey, FRotator::ZeroRotator, true, EHUSurface::Plaster);    // a wall
	UStaticMeshComponent* Pane = HUBuild::Block(Rig, Root, Blocks, HUBuild::EShape::Cube, FVector(0, 2000, 150), FVector(3, 600, 300), Grey, FRotator::ZeroRotator, true, EHUSurface::Glass); // a pane
	UStaticMeshComponent* Pane2 = HUBuild::Block(Rig, Root, Blocks, HUBuild::EShape::Cube, FVector(-60, 3000, 150), FVector(3, 600, 300), Grey, FRotator::ZeroRotator, true, EHUSurface::Glass);
	HUBuild::Block(Rig, Root, Blocks, HUBuild::EShape::Cube, FVector(60, 3000, 150), FVector(20, 600, 300), Grey, FRotator::ZeroRotator, true, EHUSurface::Plaster);   // pane then wall
	HUBuild::Block(Rig, Root, Blocks, HUBuild::EShape::Cube, FVector(0, 4000, 150), FVector(40, 40, 40), Grey);                                                        // a monitor at head height
	HUBuild::Block(Rig, Root, Blocks, HUBuild::EShape::Cube, FVector(10, 5000, 150), FVector(20, 600, 300), Grey, FRotator::ZeroRotator, true, EHUSurface::Plaster);   // a wall the listener leans on
	// (the surface library may not be imported on this PC: then nothing looks like glass, so say so with a tag)
	const bool bGlassArt = Pane && IsGlass(Pane);
	if (!bGlassArt) for (UStaticMeshComponent* G : { Pane, Pane2 }) if (G) G->ComponentTags.Add(TEXT("HUGlass"));
	UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: glass is recognised by %s"), bGlassArt ? TEXT("its surface (MI_Surface_Glass)") : TEXT("the HUGlass tag - the surface library isn't imported here"));
	const FVector O = Rig->GetActorLocation();
	const TArray<TWeakObjectPtr<AActor>> None;
	auto Name = [](EBetween B) { return B == EBetween::Nothing ? TEXT("nothing") : B == EBetween::Glass ? TEXT("glass") : TEXT("something solid"); };
	auto Expect = [&](float Y, float EarX, EBetween Want, const TCHAR* What)
	{
		const EBetween Got = Between(O + FVector(EarX, Y, 150), O + FVector(200, Y, 150), None);
		float Hz; const float Left = Dulled(Got, Hz);
		Check(Got == Want, FString::Printf(TEXT("%s: %s between -> %.0f%% as loud, nothing above %.0f Hz"), What, Name(Got), Left * 100.f, Hz));
	};
	Expect(0.f, -200.f, EBetween::Nothing, TEXT("open air, 4 m apart"));
	Expect(1000.f, -200.f, EBetween::Solid, TEXT("a wall between"));
	Expect(2000.f, -200.f, EBetween::Glass, TEXT("a pane of glass between"));
	Expect(3000.f, -200.f, EBetween::Solid, TEXT("a pane of glass and then a wall"));
	Expect(4000.f, -200.f, EBetween::Nothing, TEXT("a monitor at head height between (sound gets round it)"));
	Expect(5000.f, -12.f, EBetween::Solid, TEXT("the listener leaning on a wall, the voice the other side"));
	Rig->Destroy();

	// 3. the phone call on tape: what's left of each pitch (a 911 line through UHUVoiceSubsystem::PhoneOnTape)
	{
		FString Line;
		TMap<int32, float> Db;
		for (const int32 Hz : { 100, 200, 300, 500, 1000, 2000, 3000, 3400, 5000, 8000 })
		{
			const int32 N = 24000;
			TArray<int16> Tone; Tone.SetNumUninitialized(N);
			for (int32 i = 0; i < N; i++) Tone[i] = (int16)(8000.f * FMath::Sin(2.f * PI * Hz * i / 24000.f));
			double In = 0.0, Out = 0.0;
			for (int32 i = 2400; i < N - 2400; i++) In += (double)Tone[i] * Tone[i];
			UHUVoiceSubsystem::PhoneOnTape(Tone.GetData(), N);
			for (int32 i = 2400; i < N - 2400; i++) Out += (double)Tone[i] * Tone[i];
			const float D = 10.f * FMath::LogX(10.f, (float)(Out / FMath::Max(1.0, In)));
			Db.Add(Hz, D);
			Line += FString::Printf(TEXT("%s%d Hz %+.0f dB"), Line.IsEmpty() ? TEXT("") : TEXT(", "), Hz, D);
		}
		UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: a phone call on tape, by pitch: %s"), *Line);
		Check(Db[1000] - Db[100] > 25.f, FString::Printf(TEXT("the bass is gone, as on a phone line (100 Hz is %.0f dB under 1 kHz)"), Db[1000] - Db[100]));
		Check(Db[1000] - Db[8000] > 20.f, FString::Printf(TEXT("the top is gone, as on a phone line (8 kHz is %.0f dB under 1 kHz)"), Db[1000] - Db[8000]));
		Check(Db[1000] - Db[300] < 8.f && Db[1000] - Db[3400] < 8.f, FString::Printf(TEXT("the band a phone carries is kept (300 Hz is %.0f dB under 1 kHz, 3400 Hz is %.0f)"), Db[1000] - Db[300], Db[1000] - Db[3400]));
		Check(FMath::Abs(Db[1000] - Db[2000]) < 4.f, TEXT("the middle, where speech is, comes through evenly"));
		// and the tape under it: silence in, hiss out
		TArray<int16> Quiet; Quiet.SetNumZeroed(24000);
		UHUVoiceSubsystem::PhoneOnTape(Quiet.GetData(), Quiet.Num());
		double Hiss = 0.0;
		for (int32 i = 2400; i < 21600; i++) Hiss += (double)Quiet[i] * Quiet[i];
		const float HissDb = 10.f * FMath::LogX(10.f, (float)(Hiss / 19200.0 / (32768.0 * 32768.0)) + 1e-12f);
		Check(HissDb > -60.f && HissDb < -36.f, FString::Printf(TEXT("tape hiss under the call, quiet: %.0f dB below full scale"), HissDb));
	}
	// -HUVoiceSpaceTape=<a 24 kHz 16-bit mono .wav>: write what the game would play for it beside it, to listen to
	FString TapeIn;
	if (FParse::Value(FCommandLine::Get(), TEXT("HUVoiceSpaceTape="), TapeIn))
	{
		TArray<uint8> Wav;
		int32 At = 12, DataAt = -1, DataLen = 0;
		if (FFileHelper::LoadFileToArray(Wav, *TapeIn) && Wav.Num() > 44)
			for (; At + 8 <= Wav.Num();)
			{
				int32 Len; FMemory::Memcpy(&Len, Wav.GetData() + At + 4, 4);
				if (!FMemory::Memcmp(Wav.GetData() + At, "data", 4)) { DataAt = At + 8; DataLen = FMath::Min(Len, Wav.Num() - DataAt); break; }
				At += 8 + Len + (Len & 1);
			}
		if (DataAt > 0)
		{
			UHUVoiceSubsystem::PhoneOnTape(reinterpret_cast<int16*>(Wav.GetData() + DataAt), DataLen / 2);
			const FString Out = FPaths::ChangeExtension(TapeIn, TEXT("")) + TEXT("_on_tape.wav");
			UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: %s -> %s (%s)"), *TapeIn, *Out, FFileHelper::SaveArrayToFile(Wav, *Out) ? TEXT("written") : TEXT("couldn't write it"));
		}
		else UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: couldn't read %s as a .wav"), *TapeIn);
	}

	// 4. where people's voices come from: each person's mouth, against their head and their feet
	int32 People = 0;
	TWeakObjectPtr<AActor> First;
	for (TActorIterator<AActor> It(W); It && People < 4; ++It)
	{
		bool bFace = false;
		for (const FName& T : It->Tags) if (T.ToString().StartsWith(TEXT("HUFace:"))) bFace = true;
		if (!bFace) continue;
		USceneComponent* On = nullptr; FName Bone; FVector At;
		if (!Mouth(*It, On, Bone, At)) { Check(false, FString::Printf(TEXT("%s has no head to speak from"), *It->GetName())); continue; }
		People++;
		if (!First.IsValid()) First = *It;
		const FVector Head = On->GetSocketLocation(Bone);
		const float FromHead = FVector::Dist(At, Head), Tall = At.Z - It->GetActorLocation().Z;
		// against the face's own upper lip - while the face is being drawn. A face that isn't (out of sight, or a run
		// with no window) isn't posed: its bones stay where they'd be standing still, so there's only the head to go by.
		FVector Lip = FVector::ZeroVector; bool bLip = false, bDrawn = false;
		TArray<USkeletalMeshComponent*> Meshes;
		It->GetComponents(Meshes);
		for (const USkeletalMeshComponent* M : Meshes)
			if (M && M->GetBoneIndex(TEXT("FACIAL_C_MouthUpper")) != INDEX_NONE) { Lip = M->GetSocketLocation(TEXT("FACIAL_C_MouthUpper")); bLip = true; bDrawn = M->WasRecentlyRendered(1.f); break; }
		if (bLip && bDrawn)
			Check(FVector::Dist(At, Lip) < 1.5f, FString::Printf(TEXT("%s speaks from %.0f cm in front of their head bone, %.0f cm above their feet: %.1f cm from the upper lip of their face"),
				*It->GetName(), FromHead, Tall, FVector::Dist(At, Lip)));
		else
			Check(FromHead > 5.f && FromHead < 12.f, FString::Printf(TEXT("%s speaks from %.0f cm in front of their head bone, %.0f cm above their feet (%s)"), *It->GetName(), FromHead, Tall,
				!bLip ? TEXT("their face has no lip bone to compare with")
				: *FString::Printf(TEXT("their face isn't being drawn, so it isn't posed: its lip bone is %.0f cm from where their mouth is"), FVector::Dist(At, Lip))));
	}
	if (People == 0) UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: nobody is standing in this level (try L_Lobby or L_Scene): mouths not checked"));

	// 5. a line said out loud, when this PC has sound. The player is stood a metre and a half in front of the first
	// person here; a moment later (their ears have to arrive too) that person says a line too quiet to hear.
	APlayerController* PC = W->GetFirstPlayerController();
	if (APawn* Me = PC ? PC->GetPawn() : nullptr; Me && First.IsValid())
	{
		USceneComponent* On = nullptr; FName Bone; FVector At;
		if (Mouth(First.Get(), On, Bone, At)) Me->SetActorLocation(At + First->GetActorRightVector().GetSafeNormal2D() * 150.f - FVector(0, 0, 60.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	TWeakObjectPtr<UHUVoiceSpace> Self(this);
	FTimerHandle Later;
	W->GetTimerManager().SetTimer(Later, FTimerDelegate::CreateLambda([Self, First, Wrong]() mutable
	{
		UHUVoiceSpace* Space = Self.Get();
		UWorld* World = Space ? Space->GetWorld() : nullptr;
		if (!World) return;
		UHUVoiceSubsystem* Voice = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UHUVoiceSubsystem>() : nullptr;
		APlayerController* Player = World->GetFirstPlayerController();
		USceneComponent* Anchor = Player && Player->GetPawn() ? Player->GetPawn()->GetRootComponent() : nullptr;
		FVector E; FRotator Facing;
		if (Voice && Anchor && Space->Ear(E))
		{
			Player->GetPlayerViewPoint(E, Facing);
			TArray<uint8> Pcm; Pcm.SetNumUninitialized(24000 * 2);
			int16* Tone = reinterpret_cast<int16*>(Pcm.GetData());
			for (int32 i = 0; i < 24000; i++) Tone[i] = (int16)(24.f * FMath::Sin(2.f * PI * 440.f * i / 24000.f)); // 62 dB under full scale
			float Seconds = 0.f;
			USoundWave* Quiet = Voice->MakeSound(Pcm, false, Seconds);
			// from the person's mouth if they're within earshot, else from 2 m in front of the player
			USceneComponent* On = Anchor; FName Bone; FVector At = E + Facing.Vector() * 200.f;
			TArray<AActor*> Through; TArray<TWeakObjectPtr<AActor>> ThroughToo;
			AActor* Who = First.Get();
			FVector Lips;
			if (Who && Mouth(Who, On, Bone, Lips) && FVector::Dist(Lips, E) < 1200.f) { At = Lips; Through = { Who, Who->GetOwner() }; ThroughToo = { Who, Who->GetOwner() }; }
			else { Who = nullptr; On = Anchor; Bone = NAME_None; }
			const EBetween B = Space->Between(E, At, ThroughToo);
			UAudioComponent* A = Space->Say(Quiet, On, Bone, At, Through);
			if (!A)
			{
				UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: this run has no sound (-nosound), so no line was played"));
			}
			else
			{
				const bool bOk = A->IsPlaying() && A->AttenuationSettings == Space->PersonFalloff && A->GetAttachParent() == On && A->GetAttachSocketName() == Bone
					&& FVector::Dist(A->GetComponentLocation(), At) < 2.f && Space->Voices.Num() == 1 && B == EBetween::Nothing;
				if (!bOk) Wrong++;
				UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: %s  a line is playing from %s%s, %.1f m from the player, with %s between (their own body doesn't count), a person's falloff on it, followed as it plays"),
					bOk ? TEXT("ok   ") : TEXT("WRONG"), Who ? *Who->GetName() : TEXT("a spot in front of the player"), Bone.IsNone() ? TEXT("") : *FString::Printf(TEXT(", attached to %s"), *Bone.ToString()),
					FVector::Dist(E, At) / 100.f, B == EBetween::Nothing ? TEXT("nothing") : B == EBetween::Glass ? TEXT("glass") : TEXT("something solid"));
				// and a tape played to the room, as the 911 call is from the briefing lectern: the same tone as a phone call on tape
				TArray<uint8> Reel; Reel.SetNumUninitialized(24000 * 2);
				int16* Taped = reinterpret_cast<int16*>(Reel.GetData());
				for (int32 i = 0; i < 24000; i++) Taped[i] = (int16)(24.f * FMath::Sin(2.f * PI * 440.f * i / 24000.f));
				const FVector TapeAt = E + Facing.Vector() * 200.f;
				UAudioComponent* T = Space->Say(Voice->MakeSound(Reel, true, Seconds), Anchor, NAME_None, TapeAt, {}, EFrom::Device);
				const bool bTape = T && T->IsPlaying() && T->AttenuationSettings == Space->DeviceFalloff && FVector::Dist(T->GetComponentLocation(), TapeAt) < 2.f && Space->Voices.Num() == 2;
				if (!bTape) Wrong++;
				UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: %s  a tape is playing 2 m in front of the player: a phone call on tape, a tape's falloff on it (it carries further), followed as it plays"),
					bTape ? TEXT("ok   ") : TEXT("WRONG"));
			}
		}
		UE_LOG(LogTemp, Display, TEXT("HUVoiceSpaceTest: %s"), Wrong ? *FString::Printf(TEXT("%d CHECKS WRONG"), Wrong) : TEXT("all checks passed"));
		if (!FParse::Param(FCommandLine::Get(), TEXT("HUVoiceSpaceStay")))
		{
			FTimerHandle Quit;
			World->GetTimerManager().SetTimer(Quit, FTimerDelegate::CreateLambda([]() { FPlatformMisc::RequestExit(false); }), 2.f, false);
		}
	}), 1.f, false);
}
#endif
