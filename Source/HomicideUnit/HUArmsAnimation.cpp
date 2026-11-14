#include "HUArmsAnimation.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Animation/BoneReference.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	// the pose in component space, and the few things done to it
	struct FArmsRig
	{
		FCSPose<FCompactPose>& CS;
		const FBoneContainer& Bones;
		explicit FArmsRig(FCSPose<FCompactPose>& InCS) : CS(InCS), Bones(InCS.GetPose().GetBoneContainer()) {}

		FCompactPoseBoneIndex Find(const FString& Name) const
		{
			FBoneReference R(*Name);
			R.Initialize(Bones);
			return R.IsValidToEvaluate(Bones) ? R.GetCompactPoseIndex(Bones) : FCompactPoseBoneIndex(INDEX_NONE);
		}
		static bool Has(FCompactPoseBoneIndex I) { return I.GetInt() != INDEX_NONE; }
		FTransform Get(FCompactPoseBoneIndex I) { return CS.GetComponentSpaceTransform(I); }
		FVector Pos(FCompactPoseBoneIndex I) { return Get(I).GetLocation(); }
		// (children keep their local pose, so they follow)
		void Set(FCompactPoseBoneIndex I, const FTransform& T) { const FBoneTransform B(I, T); CS.SafeSetCSBoneTransforms(MakeArrayView(&B, 1)); }
		// turn a bone about its own joint by Q (component space)
		void Turn(FCompactPoseBoneIndex I, const FQuat& Q)
		{
			if (!Has(I)) return;
			FTransform T = Get(I);
			T.SetRotation((Q * T.GetRotation()).GetNormalized());
			Set(I, T);
		}
		// turn a bone so its child lies along Dir
		void Aim(FCompactPoseBoneIndex I, FCompactPoseBoneIndex Child, const FVector& Dir)
		{
			if (!Has(I) || !Has(Child)) return;
			const FVector Now = (Pos(Child) - Pos(I)).GetSafeNormal(), Want = Dir.GetSafeNormal();
			if (!Now.IsNearlyZero() && !Want.IsNearlyZero()) Turn(I, FQuat::FindBetweenNormals(Now, Want));
		}
		// two-bone IK: shoulder A, elbow B, wrist C reaching for Target, the elbow bending toward Pole
		void Reach(FCompactPoseBoneIndex A, FCompactPoseBoneIndex B, FCompactPoseBoneIndex C, const FVector& Target, const FVector& Pole)
		{
			if (!Has(A) || !Has(B) || !Has(C)) return;
			const FVector PA = Pos(A);
			const float L1 = (Pos(B) - PA).Size(), L2 = (Pos(C) - Pos(B)).Size();
			if (L1 < 1.f || L2 < 1.f) return;
			const FVector ToT = Target - PA;
			const float D = FMath::Clamp((float)ToT.Size(), FMath::Abs(L1 - L2) + 0.5f, (L1 + L2) * 0.998f);
			const FVector Dir = ToT.GetSafeNormal();
			FVector Bend = Pole - Dir * FVector::DotProduct(Pole, Dir);
			if (!Bend.Normalize()) Bend = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
			const float Along = (L1 * L1 - L2 * L2 + D * D) / (2.f * D);
			const float Out = FMath::Sqrt(FMath::Max(0.f, L1 * L1 - Along * Along));
			Aim(A, B, PA + Dir * Along + Bend * Out - PA);
			Aim(B, C, PA + Dir * D - Pos(B));
		}
	};

	struct FHUArmsProxy : FAnimSingleNodeInstanceProxy
	{
		explicit FHUArmsProxy(UAnimInstance* Instance) : FAnimSingleNodeInstanceProxy(Instance) {}

		FHUArmGrip Grip[2];
		FTransform FrameToComp;
		bool bFrame = false;

		virtual void PreUpdate(UAnimInstance* Instance, float Dt) override
		{
			FAnimSingleNodeInstanceProxy::PreUpdate(Instance, Dt);
			const UHUArmsAnimation* A = CastChecked<UHUArmsAnimation>(Instance);
			Grip[0] = A->Grip[0];
			Grip[1] = A->Grip[1];
			// where the tools' frame is from the body, this frame (both ride on the camera; the tools' frame sways)
			const USkeletalMeshComponent* C = Instance->GetSkelMeshComponent();
			bFrame = A->Frame.IsValid() && C;
			if (bFrame) FrameToComp = A->Frame->GetComponentTransform().GetRelativeTransform(C->GetComponentTransform());
		}

		virtual bool Evaluate(FPoseContext& Output) override
		{
			Output.ResetToRefPose();
			FCSPose<FCompactPose> CS;
			CS.InitPose(Output.Pose);
			FArmsRig R(CS);

			// the head folds away to nothing: the camera is where its eyes were
			const FCompactPoseBoneIndex Neck = R.Find(TEXT("neck_01"));
			if (R.Has(Neck)) { FTransform T = R.Get(Neck); T.SetScale3D(FVector(0.001f)); R.Set(Neck, T); }

			const TCHAR* Sfx[2] = { TEXT("_l"), TEXT("_r") };
			auto Bone = [&](const TCHAR* Name, int32 s) { return R.Find(FString(Name) + Sfx[s]); };
			const FCompactPoseBoneIndex UpL = Bone(TEXT("upperarm"), 0), UpR = Bone(TEXT("upperarm"), 1);
			if (!R.Has(UpL) || !R.Has(UpR)) return true;
			// the body's frame, off its shoulders: up, its right, the way it faces
			const FVector Up = FVector::UpVector;
			FVector Right = (R.Pos(UpR) - R.Pos(UpL)) * FVector(1, 1, 0);
			if (!Right.Normalize()) Right = FVector(-1, 0, 0);
			const FVector Fwd = FVector::CrossProduct(Right, Up);
			static const TCHAR* Fingers[4] = { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };

			for (int32 s = 0; s < 2; s++)
			{
				const FCompactPoseBoneIndex UpA = Bone(TEXT("upperarm"), s), LoA = Bone(TEXT("lowerarm"), s), Hand = Bone(TEXT("hand"), s);
				if (!R.Has(LoA) || !R.Has(Hand)) continue;
				const FHUArmGrip& G = Grip[s];
				const FVector Out = Right * (s == 0 ? -1.f : 1.f);
				if (!bFrame || !G.bOn)
				{
					// nothing in this hand: it hangs by your side, under the view
					const FVector Sh = R.Pos(UpA);
					const float Len = (R.Pos(LoA) - Sh).Size() + (R.Pos(Hand) - R.Pos(LoA)).Size();
					R.Reach(UpA, LoA, Hand, Sh - Up * Len * 0.96f - Fwd * 4.f + Out * 4.f, Out - Fwd);
					continue;
				}

				// the wrist to its grip, the elbow out toward its pole
				const FTransform Want = G.Hand * FrameToComp;
				R.Reach(UpA, LoA, Hand, Want.GetLocation(), FrameToComp.TransformVectorNoScale(G.Elbow));

				// the hand turns to the grip's frame, measured off the hand itself: along it to the middle knuckle, across
				// it to the thumb side
				const FCompactPoseBoneIndex Mid = Bone(TEXT("middle_01"), s), Index = Bone(TEXT("index_01"), s), Pinky = Bone(TEXT("pinky_01"), s);
				if (!R.Has(Mid) || !R.Has(Index) || !R.Has(Pinky)) continue;
				const FVector Wrist = R.Pos(Hand);
				const FVector Thumbward = R.Pos(Index) - R.Pos(Pinky);
				const FQuat Now = FRotationMatrix::MakeFromXY(R.Pos(Mid) - Wrist, s == 1 ? -Thumbward : Thumbward).ToQuat();
				const FQuat To = Want.GetRotation().GetNormalized();
				const FQuat Delta = (To * Now.Inverse()).GetNormalized();
				// the forearm's twist bones take their share of the roll, so the wrist doesn't pinch
				const FVector Fore = (Wrist - R.Pos(LoA)).GetSafeNormal();
				FQuat Swing, Twist;
				Delta.ToSwingTwist(Fore, Swing, Twist);
				const float ForeLen = FMath::Max(1.f, (float)(Wrist - R.Pos(LoA)).Size());
				for (const TCHAR* T : { TEXT("lowerarm_twist_01"), TEXT("lowerarm_twist_02") })
				{
					const FCompactPoseBoneIndex Tw = Bone(T, s);
					if (!R.Has(Tw)) continue;
					const float Share = FMath::Clamp((float)(R.Pos(Tw) - R.Pos(LoA)).Size() / ForeLen, 0.f, 1.f);
					R.Turn(Tw, FQuat::Slerp(FQuat::Identity, Twist, Share));
				}
				R.Turn(Hand, Delta);

				// the fingers close toward the palm, each joint about the line of the knuckles (+ turns along-the-hand
				// toward the palm, -Z, for either hand: the frame is right-handed)
				const FVector Xh = To.GetAxisX(), Yh = To.GetAxisY(), Zh = To.GetAxisZ();
				const FHUHandShape& Sh = G.Shape;
				for (int32 f = 0; f < 4; f++)
					for (int32 j = 0; j < 3; j++)
						if (!FMath::IsNearlyZero(Sh.Finger[f][j]))
							R.Turn(Bone(*FString::Printf(TEXT("%s_%02d"), Fingers[f], j + 1), s), FQuat(Yh, FMath::DegreesToRadians(Sh.Finger[f][j])));
				// the thumb swings in under the palm (about the hand's length: + takes a right thumb, on -Y, toward -Z)
				const FCompactPoseBoneIndex T1 = Bone(TEXT("thumb_01"), s), T2 = Bone(TEXT("thumb_02"), s), T3 = Bone(TEXT("thumb_03"), s);
				R.Turn(T1, FQuat(Xh, FMath::DegreesToRadians(Sh.ThumbAcross) * (s == 1 ? 1.f : -1.f)));
				// and its outer joints bend toward the palm (about the line across the thumb's last bone and the palm)
				for (int32 j = 0; j < 2; j++)
				{
					const FCompactPoseBoneIndex J = j == 0 ? T2 : T3;
					if (!R.Has(T2) || !R.Has(T3) || FMath::IsNearlyZero(Sh.Thumb[j])) continue;
					FVector Axis = FVector::CrossProduct(R.Pos(T3) - R.Pos(T2), -Zh);
					if (Axis.Normalize()) R.Turn(J, FQuat(Axis, FMath::DegreesToRadians(Sh.Thumb[j])));
				}
			}

			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(CS, Output.Pose);
			return true;
		}
	};
}

FAnimInstanceProxy* UHUArmsAnimation::CreateAnimInstanceProxy() { return new FHUArmsProxy(this); }
