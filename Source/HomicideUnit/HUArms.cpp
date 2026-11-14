#include "HUArms.h"
#include "HUArmsAnimation.h"
#include "HUBuild.h"
#include "HUInteractComponent.h"
#include "HUCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

using HULook::EHand;

namespace
{
	// where a hand closes on what it holds, in the hand's own frame (wrist at the origin, fingers +X, back of the hand +Z)
	FVector GripOf(EHand S)
	{
		switch (S)
		{
		case EHand::Fist:  return FVector(4.3f, 0, -1.5f); // the handle runs through the fist along X
		case EHand::Pinch: return FVector(10.f, 0, -1.6f); // between the fingertips and the thumb
		default:           return FVector(4.f, 0, 0);      // the middle of the palm
		}
	}
}

void HUArms::FirstPerson(AActor* Owner, USceneComponent* Root, TArray<UStaticMeshComponent*>& Out, const FHULook& L, int32 InHeld, bool bDrag)
{
	// a hand closing on Grip (a point on the tool) turned Rot; bent at the wrist, the sleeve runs back, down and out to
	// its own side so it leaves the view (a touch slimmer than the body's: it's right under your nose)
	auto Arm = [&](bool bLeft, EHand S, FVector Grip, FRotator Rot, FVector Fore = FVector::ZeroVector)
	{
		const FQuat Q = Rot.Quaternion();
		if (Fore.IsNearlyZero()) Fore = bLeft ? FVector(-0.55f, -0.4f, -0.73f) : FVector(-0.72f, 0.42f, -0.55f); // the left one drops away steeply: it's in the middle of the view
		HULook::BuildArm(Owner, Root, Out, L, FTransform(Q, Grip - Q.RotateVector(GripOf(S))), 34.f, bLeft, S, HULook::ESeen::OnlyOwner, Fore, 0.85f);
	};
	// pushing furniture: both palms flat against it, fingers up
	if (bDrag)
	{
		Arm(false, EHand::Flat, FVector(4, 15, 2), FRotator(70, -8, 0), FVector(-0.55f, 0.25f, -0.8f));
		Arm(true, EHand::Flat, FVector(4, -15, 2), FRotator(70, 8, 0), FVector(-0.55f, -0.25f, -0.8f));
		return;
	}
	const int32 Belt = UHUInteractComponent::SlotCount;
	switch (InHeld)
	{
	case 1: // flashlight
	case 2: // UV light: the fist round the barrel
		Arm(false, EHand::Fist, FVector(6, 0, 0), FRotator::ZeroRotator);
		break;
	case 3: // print kit: the case on your left palm, the brush between right finger and thumb
		Arm(true, EHand::Relaxed, FVector(8.5f, 0, -3.3f), FRotator(0, 25, 180));
		Arm(false, EHand::Pinch, FVector(9, 3, 5), FRotator(0, -10, 0));
		break;
	case 4: // crowbar: right fist low on the bar, left fist higher up it
	{
		const FVector Mid(12, 0, 10), Along = FRotator(60, 0, 0).Vector();
		Arm(false, EHand::Fist, Mid - Along * 14.f, FRotator(60, 0, 0));
		Arm(true, EHand::Fist, Mid - Along * 3.f, FRotator(60, 30, 0), FVector(-0.45f, -0.35f, -0.82f));
		break;
	}
	case 5: // scanner: on your palm, screen up
		Arm(false, EHand::Relaxed, FVector(8.9f, 0, -1.1f), FRotator(-20, 0, 180));
		break;
	default:
		if (InHeld == Belt + HUBelt::Cuffs) Arm(false, EHand::Pinch, FVector(10, 0, 2), FRotator::ZeroRotator);       // by the chain
		else if (InHeld == Belt + HUBelt::Pepper) Arm(false, EHand::Fist, FVector(8, 0, 3), FRotator(80, 0, 0));      // round the can
		else if (InHeld == Belt + HUBelt::Taser) Arm(false, EHand::Fist, FVector(4, 0, -2), FRotator(75, 0, 0));      // the pistol grip
		else if (InHeld == Belt + HUBelt::Radio) Arm(false, EHand::Fist, FVector(8, 0, 3), FRotator(80, 0, 0));       // held up
		else Arm(false, EHand::Relaxed, FVector(4, 0, -2.5f), FRotator(8, -6, 0));                                    // empty
		break;
	}
}

// ---------- your own body's arms ----------
namespace
{
	const FName ArmsTag(TEXT("HUFPArms"));

	// one arm's hold on one thing, in the tools' frame: the point it closes on, its frame there (X along the hand to the
	// knuckles, Z out of the back of it), and which way its elbow points
	struct FArmPose { bool bOn = false; EHand Shape = EHand::Relaxed; FVector Grip = FVector::ZeroVector, X = FVector::ForwardVector, Z = FVector::UpVector, Elbow = FVector(0, 0, -1); };
	struct FTwoArms { FArmPose Arm[2]; }; // left, right
	constexpr int32 PoseCount = 11;        // empty, torch, UV light, print kit, crowbar, scanner, cuffs, pepper, taser, radio, pushing
	struct FTune
	{
		FHUHandShape Shapes[4];   // EHand: Relaxed, Fist, Flat, Pinch
		FVector GripOf[4];        // where each shape closes on what it holds, in a right hand's frame (a left one's mirrors Y)
		FTwoArms Poses[PoseCount];
		FVector HeadFromEyes = FVector(-9, 0, -8); // the head's joint, from your eyes (camera), at 180 cm
		FVector CrowbarAt = FVector(30, 5, -12);   // the tools' frame for the two-handed crowbar (HandAt)
		float ArmCm = 60.f, HandCm = 8.9f;         // the arm (shoulder to wrist) and hand (wrist to knuckle) the grips fit
	};

	FHUHandShape Shape(std::initializer_list<float> F, float Across, float T2, float T3)
	{
		FHUHandShape S;
		const float* V = F.begin();
		for (int32 f = 0; f < 4; f++) for (int32 j = 0; j < 3; j++) S.Finger[f][j] = V[f * 3 + j];
		S.ThumbAcross = Across; S.Thumb[0] = T2; S.Thumb[1] = T3;
		return S;
	}
	FArmPose Hold(EHand S, FVector Grip, FVector X, FVector Z, FVector Elbow)
	{
		FArmPose P; P.bOn = true; P.Shape = S; P.Grip = Grip; P.X = X; P.Z = Z; P.Elbow = Elbow;
		return P;
	}

	FTune Defaults()
	{
		FTune T;
		//                                     index        middle       ring         pinky      thumb: across, joints
		T.Shapes[(int32)EHand::Relaxed] = Shape({ 20, 30, 15,  22, 32, 15,  25, 34, 16,  28, 36, 18 }, 15, 10, 10);
		T.Shapes[(int32)EHand::Fist]    = Shape({ 62, 72, 40,  66, 75, 40,  70, 76, 40,  74, 76, 40 }, 45, 20, 30);
		T.Shapes[(int32)EHand::Flat]    = Shape({ 0, 3, 0,     0, 3, 0,     0, 3, 0,     0, 3, 0 }, 0, 0, 0);
		T.Shapes[(int32)EHand::Pinch]   = Shape({ 38, 52, 30,  44, 58, 30,  52, 62, 30,  58, 62, 30 }, 38, 12, 18);
		T.GripOf[(int32)EHand::Relaxed] = FVector(5.5f, 0, -3.5f); // the middle of the palm
		T.GripOf[(int32)EHand::Fist]    = FVector(7.5f, 0.5f, -3.8f); // the handle across the closed fingers
		T.GripOf[(int32)EHand::Flat]    = FVector(5.5f, 0, -2.5f);
		T.GripOf[(int32)EHand::Pinch]   = FVector(13.f, -3.f, -3.f); // between the fingertips and the thumb
		const FVector ElbowR(-0.3f, 0.7f, -0.65f), ElbowL(-0.3f, -0.7f, -0.65f);
		FTwoArms* P = T.Poses;
		P[0].Arm[1] = Hold(EHand::Relaxed, FVector(3, -1, -6), FVector(0.9f, -0.2f, -0.25f), FVector(0.25f, 0.7f, 0.65f), ElbowR); // empty
		// the torch and the UV light: a fist round the barrel, the barrel along the palm and out past the thumb
		P[1].Arm[1] = Hold(EHand::Fist, FVector(7, 0, 0), FVector(0.82f, 0, -0.57f), FVector(0, 1, 0), ElbowR);
		P[2] = P[1];
		// print kit: the case on your left palm, the brush between right finger and thumb
		P[3].Arm[0] = Hold(EHand::Relaxed, FVector(8.5f, 0, -2.5f), FVector(0.6f, 0.8f, 0), FVector(0, 0, -1), ElbowL);
		P[3].Arm[1] = Hold(EHand::Pinch, FVector(10, 3, 5), FVector(0.8f, -0.3f, -0.35f), FVector(0.35f, 0.45f, 0.8f), FVector(-0.2f, 0.55f, -0.8f));
		// crowbar: both fists round the bar (along (0.5, 0, 0.87) through (12, 0, 10)), thumbs up it: the right low, the
		// left above it, reaching across with the back of the hand turned out to its side
		P[4].Arm[1] = Hold(EHand::Fist, FVector(6, 0, -0.4f), FVector(0.87f, 0, -0.5f), FVector(0, 1, 0), ElbowR);
		P[4].Arm[0] = Hold(EHand::Fist, FVector(10, 0, 6.5f), FVector(0.7f, 0.52f, -0.4f), FVector(0.6f, -0.8f, 0), ElbowL);
		// scanner: on your palm, screen up
		P[5].Arm[1] = Hold(EHand::Relaxed, FVector(9.1f, 0, -0.5f), FVector(0.8f, -0.45f, -0.3f), FVector(-0.34f, 0, -0.94f), ElbowR);
		// off the belt: the cuffs by the chain, a fist round the pepper can, the taser's grip and the radio
		P[6].Arm[1] = Hold(EHand::Pinch, FVector(10, 0, 2), FVector(0.6f, -0.2f, -0.75f), FVector(0.45f, 0.5f, 0.7f), ElbowR);
		P[7].Arm[1] = Hold(EHand::Fist, FVector(8, 0, 2.5f), FVector(0.9f, -0.4f, 0), FVector(0.4f, 0.9f, 0), ElbowR);
		P[8].Arm[1] = Hold(EHand::Fist, FVector(4, 0, -1), FVector(0.9f, -0.4f, 0.1f), FVector(0.4f, 0.9f, 0), ElbowR);
		P[9].Arm[1] = Hold(EHand::Fist, FVector(8, 0, 3), FVector(0.9f, -0.4f, 0), FVector(0.4f, 0.9f, 0), ElbowR);
		// pushing furniture: both palms flat against it, fingers up
		P[10].Arm[1] = Hold(EHand::Flat, FVector(4, 15, 2), FVector(0.25f, 0.1f, 0.96f), FVector(-1, 0, 0), ElbowR);
		P[10].Arm[0] = Hold(EHand::Flat, FVector(4, -15, 2), FVector(0.25f, -0.1f, 0.96f), FVector(-1, 0, 0), ElbowL);
		return T;
	}

#if !UE_BUILD_SHIPPING
	// Saved/HUArmsTune.txt (development builds), read at startup - any of these lines, # for comments:
	//   shape <Relaxed|Fist|Flat|Pinch> i1 i2 i3 m1 m2 m3 r1 r2 r3 p1 p2 p3 across t2 t3
	//   gripof <shape> x y z
	//   pose <0..10> <L|R> <off|shape> gripx gripy gripz  xx xy xz  zx zy zz  elbowx elbowy elbowz
	//   head x y z
	//   crowbarat x y z
	void ReadTune(FTune& T)
	{
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *(FPaths::ProjectSavedDir() / TEXT("HUArmsTune.txt")))) return;
		static const TCHAR* Names[4] = { TEXT("Relaxed"), TEXT("Fist"), TEXT("Flat"), TEXT("Pinch") };
		auto ShapeOf = [](const FString& N) { for (int32 i = 0; i < 4; i++) if (N.Equals(Names[i], ESearchCase::IgnoreCase)) return i; return -1; };
		for (const FString& Line : Lines)
		{
			TArray<FString> W; Line.ParseIntoArrayWS(W);
			if (W.Num() == 0 || W[0].StartsWith(TEXT("#"))) continue;
			auto F = [&](int32 i) { return W.IsValidIndex(i) ? FCString::Atof(*W[i]) : 0.f; };
			auto V = [&](int32 i) { return FVector(F(i), F(i + 1), F(i + 2)); };
			if (W[0] == TEXT("shape") && W.Num() >= 17 && ShapeOf(W[1]) >= 0)
			{
				FHUHandShape& S = T.Shapes[ShapeOf(W[1])];
				for (int32 k = 0; k < 12; k++) S.Finger[k / 3][k % 3] = F(2 + k);
				S.ThumbAcross = F(14); S.Thumb[0] = F(15); S.Thumb[1] = F(16);
			}
			else if (W[0] == TEXT("gripof") && W.Num() >= 5 && ShapeOf(W[1]) >= 0) T.GripOf[ShapeOf(W[1])] = V(2);
			else if (W[0] == TEXT("head") && W.Num() >= 4) T.HeadFromEyes = V(1);
			else if (W[0] == TEXT("crowbarat") && W.Num() >= 4) T.CrowbarAt = V(1);
			else if (W[0] == TEXT("pose") && W.Num() >= 4)
			{
				const int32 I = FCString::Atoi(*W[1]);
				if (I < 0 || I >= PoseCount) continue;
				FArmPose& A = T.Poses[I].Arm[W[2] == TEXT("L") ? 0 : 1];
				if (ShapeOf(W[3]) < 0) { A.bOn = false; continue; }
				if (W.Num() >= 16) A = Hold((EHand)ShapeOf(W[3]), V(4), V(7), V(10), V(13));
			}
		}
		UE_LOG(LogTemp, Display, TEXT("HUArms: grips from Saved/HUArmsTune.txt (%d lines)"), Lines.Num());
	}
#endif

	const FTune& Tune()
	{
		static FTune T = []() { FTune D = Defaults();
#if !UE_BUILD_SHIPPING
			ReadTune(D);
#endif
			return D; }();
		return T;
	}

	int32 PoseOf(int32 InHeld, bool bDrag)
	{
		const int32 Belt = UHUInteractComponent::SlotCount;
		if (bDrag) return 10;
		if (InHeld >= 0 && InHeld <= 5) return InHeld;
		if (InHeld == Belt + HUBelt::Cuffs) return 6;
		if (InHeld == Belt + HUBelt::Pepper) return 7;
		if (InHeld == Belt + HUBelt::Taser) return 8;
		if (InHeld == Belt + HUBelt::Radio) return 9;
		return 0;
	}

	// the MetaHuman the squad sees you as (HUPeople::Detective): attached to you, owned by you, tagged HUPerson
	USkeletalMeshComponent* SourceBody(const AActor* Owner)
	{
		if (!Owner) return nullptr;
		TArray<AActor*> Attached;
		Owner->GetAttachedActors(Attached);
		for (AActor* A : Attached)
		{
			if (!A || A->GetOwner() != Owner || !A->ActorHasTag(TEXT("HUPerson"))) continue;
			TInlineComponentArray<USkeletalMeshComponent*> Meshes(A);
			for (USkeletalMeshComponent* M : Meshes) if (M && M->GetFName() == TEXT("Body") && M->GetSkeletalMeshAsset()) return M;
		}
		return nullptr;
	}
	USkeletalMeshComponent* Viewmodel(const AActor* Owner)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
		for (USkeletalMeshComponent* M : Meshes) if (M && M->ComponentHasTag(ArmsTag) && Cast<UHUArmsAnimation>(M->GetAnimInstance())) return M;
		return nullptr;
	}
	void RemoveArms(AActor* Owner)
	{
		TInlineComponentArray<UPrimitiveComponent*> Parts(Owner);
		for (UPrimitiveComponent* P : Parts) if (P && P->ComponentHasTag(ArmsTag)) P->DestroyComponent();
	}
	float HeightOf(const USkeletalMeshComponent* Src) { return Src->GetOwner() ? FMath::Max(0.5f, (float)Src->GetOwner()->GetActorRelativeScale3D().Z) : 1.f; }

	// only you see them, and they cast nothing (your body, which casts your shadow, is the squad's copy)
	void OnlyYou(UPrimitiveComponent* C)
	{
		C->ComponentTags.Add(ArmsTag);
		C->SetOnlyOwnerSee(true);
		C->SetCastShadow(false);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetReceivesDecals(false);
	}

	USkeletalMeshComponent* MakeArms(AActor* Owner, USceneComponent* Camera, USkeletalMeshComponent* Src)
	{
		USkeletalMeshComponent* Arms = NewObject<USkeletalMeshComponent>(Owner);
		OnlyYou(Arms);
		Arms->SetSkeletalMesh(Src->GetSkeletalMeshAsset());
		for (int32 i = 0; i < Src->GetNumMaterials(); i++) Arms->SetMaterial(i, Src->GetMaterial(i));
		Arms->SetAnimInstanceClass(UHUArmsAnimation::StaticClass());
		Arms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Arms->SetupAttachment(Camera);
		Arms->RegisterComponent();
		// what it's wearing, worn the same way: each piece follows the copy's bones
		TInlineComponentArray<USkinnedMeshComponent*> Pieces(Src->GetOwner());
		for (USkinnedMeshComponent* P : Pieces)
		{
			if (!P || P == Src || !P->GetSkinnedAsset() || P->GetFName().ToString().Contains(TEXT("Face"))) continue;
			USkinnedMeshComponent* C = NewObject<USkinnedMeshComponent>(Owner, P->GetClass());
			OnlyYou(C);
			C->SetSkinnedAssetAndUpdate(P->GetSkinnedAsset(), false);
			for (int32 i = 0; i < P->GetNumMaterials(); i++) C->SetMaterial(i, P->GetMaterial(i));
			C->bUseBoundsFromLeaderPoseComponent = true;
			C->SetupAttachment(Arms);
			C->SetLeaderPoseComponent(Arms);
			C->RegisterComponent();
			UE_LOG(LogTemp, Log, TEXT("HUArms: wearing %s (%s, %s)"), *P->GetName(), *P->GetClass()->GetName(), *P->GetSkinnedAsset()->GetName());
		}
		UE_LOG(LogTemp, Display, TEXT("HUArms: your arms are %s's own (%s)"), *Src->GetOwner()->GetName(), *Src->GetSkeletalMeshAsset()->GetName());
		return Arms;
	}

	// your eyes on the camera: the copy stands so its head's joint is just behind and below them, facing the way you look.
	// And how long its arms and hands are, next to the grips' own (Bruce's, at 180 cm): a shorter reach holds things
	// closer, a smaller hand closes nearer its wrist.
	void Place(USkeletalMeshComponent* Arms, const USkeletalMeshComponent* Src, UHUArmsAnimation* A)
	{
		const float S = HeightOf(Src);
		const FReferenceSkeleton& Ref = Arms->GetSkeletalMeshAsset()->GetRefSkeleton();
		auto At = [&Ref](const TCHAR* Bone, FVector Else)
		{
			const int32 I = Ref.FindBoneIndex(Bone);
			if (I == INDEX_NONE) return Else;
			FTransform T = FTransform::Identity;
			for (int32 B = I; B != INDEX_NONE; B = Ref.GetParentIndex(B)) T = T * Ref.GetRefBonePose()[B];
			return T.GetLocation();
		};
		const FVector Head = At(TEXT("head"), FVector(0, 0, 160));
		const FRotator Turn(0, -90, 0); // a MetaHuman faces its own +Y
		Arms->SetRelativeScale3D(FVector(S));
		Arms->SetRelativeRotation(Turn);
		Arms->SetRelativeLocation(Tune().HeadFromEyes * S - Turn.RotateVector(Head * S));
		const FVector Up = At(TEXT("upperarm_r"), FVector::ZeroVector), Lo = At(TEXT("lowerarm_r"), FVector::ZeroVector), Wrist = At(TEXT("hand_r"), FVector::ZeroVector);
		const float ArmCm = ((Lo - Up).Size() + (Wrist - Lo).Size()) * S, HandCm = (At(TEXT("middle_01_r"), Wrist) - Wrist).Size() * S;
		A->Reach = ArmCm > 1.f ? FMath::Clamp(ArmCm / Tune().ArmCm, 0.7f, 1.2f) : 1.f;
		A->HandScale = HandCm > 1.f ? FMath::Clamp(HandCm / Tune().HandCm, 0.75f, 1.25f) : 1.f;
		UE_LOG(LogTemp, Verbose, TEXT("HUArms: arm %.1f cm, hand %.1f cm: reach x%.2f, hand x%.2f"), ArmCm, HandCm, A->Reach, A->HandScale);
	}
}

bool HUArms::RealArms(AActor* Owner, USceneComponent* Frame, int32 InHeld, bool bDrag)
{
	USkeletalMeshComponent* Src = SourceBody(Owner);
	USceneComponent* Camera = Frame ? Frame->GetAttachParent() : nullptr;
	if (!Src || !Camera) { RemoveArms(Owner); return false; }
	USkeletalMeshComponent* Arms = Viewmodel(Owner);
	UHUArmsAnimation* A = Arms ? Cast<UHUArmsAnimation>(Arms->GetAnimInstance()) : nullptr;
	if (!A || A->Source.Get() != Src->GetOwner() || Arms->GetSkeletalMeshAsset() != Src->GetSkeletalMeshAsset())
	{
		RemoveArms(Owner);
		Arms = MakeArms(Owner, Camera, Src);
		A = Cast<UHUArmsAnimation>(Arms->GetAnimInstance());
		if (!A) { RemoveArms(Owner); return false; }
		A->Source = Src->GetOwner();
	}
	Place(Arms, Src, A);
	A->Frame = Frame;
	const FTune& T = Tune();
	const FTwoArms& P = T.Poses[PoseOf(InHeld, bDrag)];
	for (int32 s = 0; s < 2; s++)
	{
		const FArmPose& In = P.Arm[s];
		FHUArmGrip& G = A->Grip[s];
		G.bOn = In.bOn;
		if (!In.bOn) continue;
		const FQuat Q = FRotationMatrix::MakeFromXZ(In.X, In.Z).ToQuat();
		FVector Off = T.GripOf[(int32)In.Shape] * A->HandScale;
		if (s == 0) Off.Y = -Off.Y; // a left hand's thumb is on its +Y
		G.Hand = FTransform(Q, In.Grip - Q.RotateVector(Off));
		G.Shape = T.Shapes[(int32)In.Shape];
		G.Elbow = In.Elbow;
	}
	return true;
}

FVector HUArms::HandAt(const AActor* Owner, int32 InHeld)
{
	// out to the right, at your belt line's height in view; the crowbar comes in closer and to the middle, so both hands
	// reach it and you see them on it. Shorter arms hold it all nearer your eyes.
	const FVector At = PoseOf(InHeld, false) == 4 ? Tune().CrowbarAt : FVector(36, 16, -18);
	const USkeletalMeshComponent* Arms = Viewmodel(Owner);
	const UHUArmsAnimation* A = Arms ? Cast<UHUArmsAnimation>(Arms->GetAnimInstance()) : nullptr;
	return A ? At * A->Reach : At;
}

bool HUArms::Stale(const AActor* Owner)
{
	const USkeletalMeshComponent* Src = SourceBody(Owner);
	const USkeletalMeshComponent* Arms = Viewmodel(Owner);
	if (!Src || !Arms) return (Src != nullptr) != (Arms != nullptr);
	const UHUArmsAnimation* A = Cast<UHUArmsAnimation>(Arms->GetAnimInstance());
	return !A || A->Source.Get() != Src->GetOwner() || Arms->GetSkeletalMeshAsset() != Src->GetSkeletalMeshAsset()
		|| !FMath::IsNearlyEqual((float)Arms->GetRelativeScale3D().Z, HeightOf(Src), 0.001f);
}

#if !UE_BUILD_SHIPPING
// -HUHandShots: hold each thing in turn (whether you own it or not), and the push pose, and screenshot each
void UHUInteractComponent::HandShots(float Dt)
{
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("HUHandShots"));
	if (!bOn || ShotStep < 0) return;
	// -HUHandLook=Model=3,Height=6,Coat=2 (any of your look's fields, by name): dress yourself first - once your save's
	// look is in - to see the arms follow (your body is rebuilt from it, and the tick rebuilds your arms)
	FString Dress;
	static bool bDressed = false;
	if (!bDressed && ShotClock > 2.f && FParse::Value(FCommandLine::Get(), TEXT("HUHandLook="), Dress, false))
		if (AHUCharacter* Ch = Cast<AHUCharacter>(GetOwner()))
		{
			bDressed = true;
			FHULook L = Ch->Look;
			TArray<FString> Pairs; Dress.ParseIntoArray(Pairs, TEXT(","));
			for (const FString& Pair : Pairs)
			{
				FString K, V;
				if (!Pair.Split(TEXT("="), &K, &V)) continue;
				if (const FByteProperty* P = CastField<FByteProperty>(FHULook::StaticStruct()->FindPropertyByName(*K))) P->SetPropertyValue_InContainer(&L, (uint8)FCString::Atoi(*V));
				else UE_LOG(LogTemp, Warning, TEXT("HUHandShots: your look has no %s"), *K);
			}
			Ch->ServerSetLook(L);
		}
	// a steady view for every shot, a little below level (-HUHandPitch=degrees), whatever the mouse does
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			static float Yaw = TNumericLimits<float>::Max(), Pitch = -5.f;
			if (Yaw == TNumericLimits<float>::Max()) { Yaw = PC->GetControlRotation().Yaw; FParse::Value(FCommandLine::Get(), TEXT("HUHandPitch="), Pitch); }
			PC->SetIgnoreLookInput(true);
			PC->SetControlRotation(FRotator(Pitch, Yaw, 0));
		}
	const int32 Poses[] = { 0, 1, 2, 3, 4, 5, SlotCount + HUBelt::Cuffs, SlotCount + HUBelt::Pepper, SlotCount + HUBelt::Taser, SlotCount + HUBelt::Radio, -1 };
	const int32 N = UE_ARRAY_COUNT(Poses);
	// -HUHandOnly=4: just that one
	static int32 Only = -1;
	static const bool bOnly = FParse::Value(FCommandLine::Get(), TEXT("HUHandOnly="), Only);
	if (bOnly && ShotStep < N && ShotStep != Only && !bShotPosed) { ShotStep++; return; }
	ShotClock += Dt;
	const float At = 5.f + ShotStep * 1.5f;
	if (ShotStep >= N) { if (ShotClock > At) { ShotStep = -1; FPlatformMisc::RequestExit(false); } return; }
	if (!bShotPosed && ShotClock >= At)
	{
		const int32 H = Poses[ShotStep];
		Held = H < 0 ? 0 : H;
		if (H < 0) // the push pose, built straight in (there's no furniture in your hands)
		{
			HUBuild::Clear(HandParts);
			if (!HUArms::RealArms(GetOwner(), Hand, 0, true)) HUArms::FirstPerson(GetOwner(), Hand, HandParts, HandLook, 0, true);
		}
		else BuildHand();
		bShotPosed = true;
	}
	if (bShotPosed && ShotClock >= At + 0.7f)
	{
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Hands_%02d"), ShotStep), false, false);
		ShotStep++; bShotPosed = false;
	}
}
#else
void UHUInteractComponent::HandShots(float) {}
#endif
