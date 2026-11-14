#include "HUPersonAnimation.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Animation/BoneReference.h"
#include "BonePose.h"

void UHUPersonAnimation::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	TalkLeft = FMath::Max(0.f, TalkLeft - DeltaSeconds);
}

namespace
{
	float Smooth(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
	// a number in 0..1 for this person and this beat: their hands don't repeat, and two people don't move in step
	float Roll(uint32 Seed, int32 Beat, uint32 Salt) { return (HashCombine(HashCombine(Seed, (uint32)Beat * 2654435761u), Salt * 40503u) % 10007u) / 10006.f; }

	// the pose in component space, and the few things done to it
	struct FRig
	{
		FCSPose<FCompactPose>& CS;
		const FBoneContainer& Bones;
		explicit FRig(FCSPose<FCompactPose>& InCS) : CS(InCS), Bones(InCS.GetPose().GetBoneContainer()) {}

		FCompactPoseBoneIndex Find(const TCHAR* Name) const
		{
			FBoneReference R(Name);
			R.Initialize(Bones);
			return R.IsValidToEvaluate(Bones) ? R.GetCompactPoseIndex(Bones) : FCompactPoseBoneIndex(INDEX_NONE);
		}
		static bool Has(FCompactPoseBoneIndex I) { return I.GetInt() != INDEX_NONE; }
		FTransform Get(FCompactPoseBoneIndex I) { return CS.GetComponentSpaceTransform(I); }
		FVector Pos(FCompactPoseBoneIndex I) { return Get(I).GetLocation(); }
		// (children keep their local pose, so they follow)
		void Set(FCompactPoseBoneIndex I, const FTransform& T) { const FBoneTransform B(I, T); CS.SafeSetCSBoneTransforms(MakeArrayView(&B, 1)); }

		// turn a bone about its own joint by Q (component space), W of the way
		void Turn(FCompactPoseBoneIndex I, const FQuat& Q, float W = 1.f)
		{
			if (!Has(I) || W <= 0.001f) return;
			FTransform T = Get(I);
			const FQuat To = (Q * T.GetRotation()).GetNormalized();
			T.SetRotation(W >= 0.999f ? To : FQuat::Slerp(T.GetRotation(), To, W).GetNormalized());
			Set(I, T);
		}
		// turn a bone so its child lies along Dir
		void Aim(FCompactPoseBoneIndex I, FCompactPoseBoneIndex Child, const FVector& Dir, float W = 1.f)
		{
			if (!Has(I) || !Has(Child)) return;
			const FVector Now = (Pos(Child) - Pos(I)).GetSafeNormal();
			const FVector Want = Dir.GetSafeNormal();
			if (Now.IsNearlyZero() || Want.IsNearlyZero()) return;
			Turn(I, FQuat::FindBetweenNormals(Now, Want), W);
		}
		// two-bone IK: A (shoulder / hip), B (elbow / knee), C (wrist / ankle) reaching for Target, bending toward Pole
		void Reach(FCompactPoseBoneIndex A, FCompactPoseBoneIndex B, FCompactPoseBoneIndex C, const FVector& Target, const FVector& Pole, float W = 1.f)
		{
			if (W <= 0.001f || !Has(A) || !Has(B) || !Has(C)) return;
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
			const FVector Mid = PA + Dir * Along + Bend * Out;
			Aim(A, B, Mid - PA, W);
			Aim(B, C, PA + Dir * D - Pos(B), W);
		}
	};

	struct FHUPersonProxy : FAnimSingleNodeInstanceProxy
	{
		explicit FHUPersonProxy(UAnimInstance* Instance) : FAnimSingleNodeInstanceProxy(Instance), Seed(PointerHash(Instance)) {}

		// what the game asked for
		float RightArm = 0.f, HeadPitch = 0.f, HeadYaw = 0.f, SeatHeight = 46.f, TableHeight = 76.f;
		// eased toward it, 0..1
		float Seat = 0.f, Crouch = 0.f, Cuff = 0.f, Talk = 0.f, Crossed = 0.f, Raised = 0.f, Table = 0.f, Aimed = 0.f;
		float Time = 0.f, FlinchT = 99.f, FlinchPower = 1.f;
		int32 SeenFlinch = 0;
		uint32 Seed = 0;

		virtual void PreUpdate(UAnimInstance* Instance, float Dt) override
		{
			FAnimSingleNodeInstanceProxy::PreUpdate(Instance, Dt);
			const UHUPersonAnimation* P = CastChecked<UHUPersonAnimation>(Instance);
			Time += Dt;
			RightArm = P->RightArm; HeadPitch = P->HeadPitch; HeadYaw = P->HeadYaw;
			SeatHeight = P->SeatHeight; TableHeight = P->TableHeight;
			auto Ease = [Dt](float& V, bool bOn, float PerSecond) { V = FMath::FInterpConstantTo(V, bOn ? 1.f : 0.f, Dt, PerSecond); };
			Ease(Seat, P->bSeated, 2.2f);           // down into the chair in about half a second
			Ease(Crouch, P->bCrouched, 4.f);
			Ease(Cuff, P->bCuffed, 3.f);
			Ease(Talk, P->TalkLeft > 0.f, 3.5f);
			Ease(Crossed, P->Arms == 1, 3.f);
			Ease(Raised, P->Arms == 2, 4.f);
			Ease(Table, P->bTable, 3.f);
			Aimed = FMath::FInterpTo(Aimed, Smooth(P->RightArm / 30.f), Dt, 10.f);
			if (P->FlinchCount != SeenFlinch) { SeenFlinch = P->FlinchCount; FlinchT = 0.f; FlinchPower = FMath::Clamp(P->FlinchStrength, 0.3f, 2.f); }
			else FlinchT += Dt;
		}

		// the jolt: in fast, held a moment, out slowly
		float FlinchNow() const
		{
			const float T = FlinchT;
			const float E = T < 0.07f ? T / 0.07f : T < 0.32f ? 1.f : 1.f - Smooth((T - 0.32f) / 0.95f);
			return FMath::Clamp(E, 0.f, 1.f) * FlinchPower;
		}

		virtual bool Evaluate(FPoseContext& Output) override
		{
			FAnimSingleNodeInstanceProxy::Evaluate(Output); // the clip: idle, walk, run
			FCSPose<FCompactPose> CS;
			CS.InitPose(Output.Pose);
			FRig R(CS);
			const FCompactPoseBoneIndex Pelvis = R.Find(TEXT("pelvis")), S2 = R.Find(TEXT("spine_02")), S3 = R.Find(TEXT("spine_03")), S4 = R.Find(TEXT("spine_04")),
				S5 = R.Find(TEXT("spine_05")), Neck = R.Find(TEXT("neck_02")), Head = R.Find(TEXT("head"));
			const FCompactPoseBoneIndex Cla[2] = { R.Find(TEXT("clavicle_l")), R.Find(TEXT("clavicle_r")) };
			const FCompactPoseBoneIndex UpA[2] = { R.Find(TEXT("upperarm_l")), R.Find(TEXT("upperarm_r")) };
			const FCompactPoseBoneIndex LoA[2] = { R.Find(TEXT("lowerarm_l")), R.Find(TEXT("lowerarm_r")) };
			const FCompactPoseBoneIndex Hand[2] = { R.Find(TEXT("hand_l")), R.Find(TEXT("hand_r")) };
			const FCompactPoseBoneIndex Thigh[2] = { R.Find(TEXT("thigh_l")), R.Find(TEXT("thigh_r")) };
			const FCompactPoseBoneIndex Calf[2] = { R.Find(TEXT("calf_l")), R.Find(TEXT("calf_r")) };
			const FCompactPoseBoneIndex Foot[2] = { R.Find(TEXT("foot_l")), R.Find(TEXT("foot_r")) };
			if (!R.Has(Pelvis) || !R.Has(UpA[0]) || !R.Has(UpA[1])) return true;

			// the body's frame, off the clip's shoulders: up, their right, the way they face
			const FVector Up = FVector::UpVector;
			FVector Right = (R.Pos(UpA[1]) - R.Pos(UpA[0])) * FVector(1, 1, 0);
			if (!Right.Normalize()) Right = FVector(-1, 0, 0);
			const FVector Fwd = FVector::CrossProduct(Right, Up);
			auto Between = [](const FVector& A, const FVector& B) { return FQuat::FindBetweenNormals(A.GetSafeNormal(), B.GetSafeNormal()); };
			auto Rad = [](float Deg) { return FMath::DegreesToRadians(Deg); };
			auto Lean = [&](const FVector& Toward, float Deg) { return Between(Up, Up * FMath::Cos(Rad(Deg)) + Toward * FMath::Sin(Rad(Deg))); }; // the top toward Toward
			auto Look = [&](float YawDeg, float PitchDeg) // + right, + down
			{
				const FVector Y = Fwd * FMath::Cos(Rad(YawDeg)) + Right * FMath::Sin(Rad(YawDeg));
				return Between(Fwd, Y * FMath::Cos(Rad(PitchDeg)) - Up * FMath::Sin(Rad(PitchDeg)));
			};

			const float St = Smooth(Seat), Cr = Smooth(Crouch) * (1.f - St), Cu = Smooth(Cuff), Tk = Smooth(Talk), Tb = Smooth(Table) * St;
			const float Xd = Smooth(Crossed) * (1.f - Cu), Up2 = Smooth(Raised) * (1.f - Cu), Fl = FlinchNow();
			const float Free = (1.f - Cu) * (1.f - Xd) * (1.f - Up2); // hands not otherwise busy

			// ---- the hips and legs: sitting, crouching ----
			FTransform FootWas[2];
			for (int32 s = 0; s < 2; s++) if (FRig::Has(Foot[s])) FootWas[s] = R.Get(Foot[s]);
			if (St > 0.001f || Cr > 0.001f)
			{
				FTransform P = R.Get(Pelvis);
				const FVector P0 = P.GetLocation();
				FVector Want = P0;
				Want.Z = FMath::Lerp((float)P0.Z, SeatHeight + 9.f, St) ; // the hip joints just above the seat
				Want -= Fwd * 4.f * St;                                   // back into the chair
				Want.Z = FMath::Lerp((float)Want.Z, (float)P0.Z * 0.55f, Cr);
				Want -= Fwd * 12.f * Cr;                                  // crouching: hips back over the heels
				P.SetLocation(Want);
				R.Set(Pelvis, P);
				for (int32 s = 0; s < 2; s++)
				{
					if (!FRig::Has(Thigh[s]) || !FRig::Has(Calf[s]) || !FRig::Has(Foot[s])) continue;
					const float Side = s == 0 ? -1.f : 1.f;
					const FVector Hip = R.Pos(Thigh[s]);
					const float ThighLen = (R.Pos(Calf[s]) - Hip).Size();
					// sitting: the feet planted ahead, under the knees; crouching: where they stood
					FVector SitFoot = Hip + Fwd * ThighLen * 0.92f + Right * Side * 5.f;
					SitFoot.Z = FootWas[s].GetLocation().Z;
					const FVector Target = FMath::Lerp(FootWas[s].GetLocation(), SitFoot, St);
					R.Reach(Thigh[s], Calf[s], Foot[s], Target, Fwd + Up * 0.4f + Right * Side * 0.15f);
					FTransform F = R.Get(Foot[s]); F.SetRotation(FootWas[s].GetRotation()); R.Set(Foot[s], F); // flat on the floor
				}
			}

			// ---- the back: leaning in at a table, bent over crouching, talking, the jolt of a flinch ----
			R.Turn(S2, Lean(Fwd, 22.f * Cr + 3.f * Tb + 2.f * Tk));
			R.Turn(S4, Lean(Fwd, 16.f * Cr));
			if (Fl > 0.001f)
			{
				const float Shake = 1.f + 0.15f * FMath::Sin(FlinchT * 60.f) * (1.f - Smooth(FlinchT / 0.5f));
				R.Turn(S3, Lean(-Fwd, 8.f * Fl * Shake));
				R.Turn(S5, Lean(-Fwd, 5.f * Fl));
				for (int32 s = 0; s < 2; s++) // the shoulders come up
				{
					const FVector Out = Right * (s == 0 ? -1.f : 1.f);
					R.Turn(Cla[s], Between(Out, Out * FMath::Cos(Rad(11.f * Fl)) + Up * FMath::Sin(Rad(11.f * Fl))));
				}
			}

			// ---- the arms: each layer pulls the hand toward its own spot; the strongest wins ----
			const FVector Chest = R.Has(S5) ? R.Pos(S5) : R.Pos(UpA[0]);
			const FVector Belly = R.Has(S2) ? R.Pos(S2) : R.Pos(Pelvis);
			const FVector Hips = R.Pos(Pelvis);
			const FVector HeadAt = R.Has(Head) ? R.Pos(Head) : Chest + Up * 30.f;
			const float Period = 0.78f + 0.3f * Roll(Seed, 0, 7);
			const float Beat = (Time + 3.f * Roll(Seed, 0, 3)) / Period;
			const int32 B = FMath::FloorToInt(Beat);
			const float Frac = Beat - B;
			for (int32 s = 0; s < 2; s++)
			{
				if (!FRig::Has(LoA[s]) || !FRig::Has(Hand[s])) continue;
				const float Side = s == 0 ? -1.f : 1.f;
				const FVector Out = Right * Side;
				const FVector Sh = R.Pos(UpA[s]);
				const float ArmLen = (R.Pos(LoA[s]) - Sh).Size() + (R.Pos(Hand[s]) - R.Pos(LoA[s])).Size();
				FVector T = R.Pos(Hand[s]), Pole = Out - Up * 0.6f;
				float W = 0.f;
				auto Layer = [&](float Wt, const FVector& At, const FVector& Bend)
				{
					if (Wt <= 0.001f) return;
					T = FMath::Lerp(T, At, Wt); Pole = FMath::Lerp(Pole, Bend, Wt); W = FMath::Max(W, Wt);
				};
				// at rest in the chair: hands in the lap, or on the table
				{
					FVector Lap = (FRig::Has(Calf[s]) ? R.Pos(Calf[s]) : Hips + Fwd * 40.f) - Fwd * 16.f + Up * 13.f - Out * 3.f;
					FVector OnTable = Sh + Fwd * 40.f - Out * 7.f; OnTable.Z = TableHeight + 4.f;
					Layer(St, FMath::Lerp(Lap, OnTable, Smooth(Table)), FMath::Lerp(-Fwd * 0.5f + Out * 0.8f, Out - Up * 0.8f, Smooth(Table)));
				}
				// talking: the hands come up in front and move with the words, a new spot every beat
				if (Tk > 0.001f && Free > 0.001f)
				{
					const bool bActive = Roll(Seed, B, 11 + s) < (s == 1 ? 0.8f : 0.45f);
					auto Spot = [&](int32 Bt)
					{
						FVector Base = Belly + Fwd * 34.f + Out * 16.f + Up * 11.f;
						FVector AtTable = Sh + Fwd * 36.f - Out * 3.f; AtTable.Z = TableHeight + 15.f;
						Base = FMath::Lerp(Base, AtTable, Tb);
						return Base + Out * (Roll(Seed, Bt, 21 + s) * 14.f - 4.f) + Up * (Roll(Seed, Bt, 31 + s) * 14.f - 5.f) + Fwd * (Roll(Seed, Bt, 41 + s) * 10.f - 3.f);
					};
					const FVector Gest = FMath::Lerp(Spot(B - 1), Spot(B), Smooth(Frac / 0.45f)) + (Fwd + Up) * 2.5f * FMath::Sin(PI * FMath::Clamp(Frac / 0.3f, 0.f, 1.f));
					Layer(Tk * Free * (bActive ? 1.f : 0.35f), Gest, Out - Up * 0.7f - Fwd * 0.3f);
				}
				// a partner holding a torch or a tool out where they aim
				if (s == 1 && Aimed > 0.001f)
				{
					const FVector Dir = -Up * FMath::Cos(Rad(RightArm)) + Fwd * FMath::Sin(Rad(RightArm));
					Layer(Aimed, Sh + Dir * ArmLen * 0.9f - Out * 6.f, Out * 0.8f - Up * 0.6f);
				}
				// arms crossed over the chest
				Layer(Xd, Chest + Fwd * (22.f + 2.f * Side) - Out * 12.f - Up * (14.f - 2.f * Side), Out - Up * 0.6f);
				// hands up
				Layer(Up2, Sh + Up * ArmLen * 0.72f + Out * 18.f + Fwd * 8.f, Out - Up * 0.2f - Fwd * 0.3f);
				// the flinch: hands jerk up in front of the chest, palms out
				Layer(FMath::Min(1.f, Fl) * 0.9f * Free, HeadAt + Fwd * 30.f + Out * 15.f - Up * 40.f, Out - Up * 0.8f);
				// cuffed: wrists together behind the back at the belt, elbows back
				Layer(Cu, Hips + Up * 13.f - Fwd * 21.f + Out * 5.f, -Fwd * 0.7f + Out * 0.9f - Up * 0.3f);

				R.Reach(UpA[s], LoA[s], Hand[s], T, Pole, FMath::Min(1.f, W));
				// palms down on the table or the thighs (they hang turned in)
				const float Palm = St * Free * (1.f - Tk * 0.7f);
				if (Palm > 0.001f) R.Turn(Hand[s], Between(-Out, -Up), Palm * 0.8f);
			}

			// ---- the head: where they're told to look, nodding with the words, turning away from the proof ----
			const float Nod = Tk * 4.f * FMath::Sin(2.f * PI * Beat) * (0.5f + 0.5f * Roll(Seed, B, 51));
			const float TalkYaw = Tk * (Roll(Seed, B, 61) * 8.f - 4.f);
			const float Yaw = HeadYaw + TalkYaw - 18.f * Fl, Pitch = HeadPitch + Nod - 7.f * Fl - 18.f * Cr;
			if (!FMath::IsNearlyZero(Yaw, 0.05f) || !FMath::IsNearlyZero(Pitch, 0.05f))
			{
				R.Turn(Neck, Look(Yaw * 0.4f, Pitch * 0.4f));
				R.Turn(Head, Look(Yaw * 0.6f, Pitch * 0.6f));
			}

			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(CS, Output.Pose);
			return true;
		}
	};
}

FAnimInstanceProxy* UHUPersonAnimation::CreateAnimInstanceProxy() { return new FHUPersonProxy(this); }
