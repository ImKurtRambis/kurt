#include "HUUsables.h"
#include "HUInteractable.h"
#include "HUInteractComponent.h"
#include "HUCharacter.h"
#include "HUBuild.h"
#include "HUProps.h"
#include "HUSound.h"
#include "HUDeskMenu.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Pawn.h"
#include "UObject/UObjectHash.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

namespace
{
	const TCHAR* KindName(EHUUse K)
	{
		static const TCHAR* N[] = { TEXT("None"), TEXT("Drawer"), TEXT("Files"), TEXT("Locker"), TEXT("Phone"), TEXT("Seat"), TEXT("Bench"), TEXT("Switch"),
			TEXT("Screen"), TEXT("Lamp"), TEXT("Drink"), TEXT("Clock"), TEXT("Read") };
		return N[FMath::Clamp((int32)K, 0, (int32)UE_ARRAY_COUNT(N) - 1)];
	}
	// a line by number (the use count is the same on every machine, so everyone's copy agrees)
	FString Pick(std::initializer_list<const TCHAR*> Lines, uint32 N) { return *(Lines.begin() + N % (uint32)Lines.size()); }

	FString DrawerLine(uint32 N)
	{
		return Pick({ TEXT("Pens, a stapler, a roll of antacids."), TEXT("Take-out menus and a lighter that doesn't light."), TEXT("A dog-eared copy of the penal code."),
			TEXT("Old case notes in a rubber band. Nobody's coming back for them."), TEXT("A spare tie, still in its plastic."), TEXT("Paper clips, a dead pager and a bus schedule from 1998."),
			TEXT("Somebody's reading glasses and half a sandwich in wax paper.") }, N);
	}
	FString OldFile(uint32 N)
	{
		return Pick({ TEXT("CASE 1986-0231 - a liquor store holdup on Elm. Closed: the clerk's cousin pleaded guilty."),
			TEXT("CASE 1991-1177 - a dockworker found in the river. Open. Someone's pencilled 'ask about the union' in the margin."),
			TEXT("CASE 1979-0544 - a jeweller's safe, emptied overnight. Closed: an inside job, the night watchman."),
			TEXT("CASE 1995-0802 - a hit-and-run outside the Majestic. Open. Green sedan, no plate."),
			TEXT("CASE 1983-0310 - a bookkeeper gone missing from Pierce & Sons. Closed: found in Reno, alive, with the money."),
			TEXT("CASE 2001-0067 - a fire at a laundromat on 9th. Closed: the owner, for the insurance."),
			TEXT("CASE 1974-0919 - a body in a hotel boiler room. Open. The file's thick and nobody's touched it in years."),
			TEXT("CASE 1998-1450 - a stabbing outside the Velvet Lounge. Closed: self-defence, no charges.") }, N);
	}
	FString LockerLine(uint32 N)
	{
		return Pick({ TEXT("A spare shirt, deodorant, and a photo of somebody's kids taped inside the door."), TEXT("A gym bag, running shoes, a paperback with a cracked spine."),
			TEXT("Empty, except a padlock with no key."), TEXT("A raincoat, a box of sunflower seeds, and a bowling trophy nobody wanted at home."),
			TEXT("Uniform trousers on a hanger and somebody's lunch, two days old.") }, N);
	}
	FString PhoneLine(uint32 N)
	{
		return Pick({ TEXT("Dial tone. The switchboard picks up: \"Fourteenth Precinct, who d'you want?\" Nobody. You hang up."),
			TEXT("You catch the tail of a call on the shared line: \"...tell him the lab's backed up till Thursday.\" Click."),
			TEXT("Dial tone. You think about calling home. You put it back."),
			TEXT("The desk sergeant: \"If that's about the coffee fund, I don't want to hear it.\" You hang up.") }, N);
	}
	FString NewsLine(uint32 N)
	{
		return Pick({ TEXT("Channel 6, the late news: \"...police are asking anyone with information to call the tip line...\""),
			TEXT("A rerun of a cop show. The detective on screen solves it in forty minutes, with ads."),
			TEXT("The weather: rain all night, rain tomorrow, then more rain."),
			TEXT("Late-night ball game, extra innings. Nobody here's watching.") }, N);
	}

	// the wall clock reads your own PC's time, on a 12-hour face ("2:07")
	FString ClockTime() { const FDateTime Now = FDateTime::Now(); return FString::Printf(TEXT("%d:%02d"), Now.GetHour12(), Now.GetMinute()); }

	UHUUsableWorld* World(const UObject* C) { UWorld* W = C ? C->GetWorld() : nullptr; return W ? W->GetSubsystem<UHUUsableWorld>() : nullptr; }

	// the part's box in its own frame (unscaled transform + scaled half sizes)
	void LocalBox(const UPrimitiveComponent* C, FVector& Center, FVector& Half)
	{
		FBox B(FVector(-50.f), FVector(50.f)); // an engine cube
		if (const UStaticMeshComponent* M = Cast<UStaticMeshComponent>(C)) if (M->GetStaticMesh()) B = M->GetStaticMesh()->GetBoundingBox();
		const FVector S = C->GetComponentScale().GetAbs();
		Center = B.GetCenter() * S;
		Half = B.GetExtent() * S;
	}
}

// ---------- marking ----------
void HUUsables::Mark(UPrimitiveComponent* C, EHUUse Kind, const FString& Words, float FrontYaw, float Extra)
{
	UHUUsableWorld* S = World(C);
	if (!C || Kind == EHUUse::None || !S) return;
	// the same name on every machine - its kind and where it stands - so a squadmate's use finds this one
	const FVector P = C->GetRelativeLocation();
	FString Base = FString::Printf(TEXT("Use%s_%d_%d_%d"), KindName(Kind), FMath::RoundToInt(P.X), FMath::RoundToInt(P.Y), FMath::RoundToInt(P.Z)).Replace(TEXT("-"), TEXT("m"));
	FString Name = Base;
	for (int32 N = 2; ; N++)
	{
		UObject* Was = StaticFindObjectFast(nullptr, C->GetOuter(), FName(*Name));
		if (!Was || Was == C) break;
		// a part from an earlier build of the room, on its way out: move it aside rather than shift every name
		const UActorComponent* Old = Cast<UActorComponent>(Was);
		if (Old && (!Old->IsRegistered() || Old->IsBeingDestroyed() || !IsValid(Old))) { Was->Rename(nullptr, nullptr, REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty); break; }
		Name = FString::Printf(TEXT("%s_%d"), *Base, N);
	}
	if (C->GetFName() != FName(*Name)) C->Rename(*Name, nullptr, REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
	C->ComponentTags.AddUnique(TEXT("HUUse"));
	// something you walk through (a phone on a desk, a clock on a wall) still has to catch your eye: the use trace
	// hits it, nothing else does
	if (C->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	UHUUsableWorld::FUse& U = S->Parts.FindOrAdd(TObjectKey<UPrimitiveComponent>(C));
	U = UHUUsableWorld::FUse();
	U.Kind = Kind; U.Words = Words; U.FrontYaw = FrontYaw; U.Extra = Extra;
}

void HUUsables::MarkProp(UPrimitiveComponent* C, const TCHAR* PropName)
{
	// the desk menu is clicked with the mouse: its desk, lamp, phone and cabinet are menu items, left as they are
	if (!C || Cast<AHUDeskMenu>(C->GetOwner())) return;
	const FString N(PropName);
	if (N == TEXT("Desk")) Mark(C, EHUUse::Drawer, FString(), 0.f, 0.45f); // the drawers on the sitter's side, to the right
	else if (N == TEXT("FilingCabinet")) Mark(C, EHUUse::Files);
	else if (N == TEXT("Locker") || N == TEXT("LockerOpen")) { Mark(C, EHUUse::Locker); if (UHUUsableWorld::FUse* U = World(C) ? World(C)->Find(C) : nullptr) U->bOn = N == TEXT("LockerOpen"); }
	else if (N == TEXT("DeskPhone")) Mark(C, EHUUse::Phone);
	else if (N == TEXT("OfficeChair")) Mark(C, EHUUse::Seat, FString(), 0.f, 0.47f);
	else if (N == TEXT("StackChair")) Mark(C, EHUUse::Seat, FString(), 0.f, 0.46f);
	else if (N == TEXT("RollCallBench")) Mark(C, EHUUse::Bench, FString(), 0.f, 0.5f);          // a back: you face the way it does
	else if (N == TEXT("Bench") || N == TEXT("BenchLong")) Mark(C, EHUUse::Bench, FString(), 999.f, 0.9f); // no back: face the way you were looking
	else if (N == TEXT("WaterCooler")) Mark(C, EHUUse::Drink);
	else if (N == TEXT("WallClock")) Mark(C, EHUUse::Clock);
	else if (N == TEXT("CRTTV")) Mark(C, EHUUse::Screen);
	else if (N == TEXT("DeskLamp")) Mark(C, EHUUse::Lamp);
	else if (N.StartsWith(TEXT("CorkBoard"))) Mark(C, EHUUse::Read);
	// CRTMonitor and Keyboard belong to the desk computers (PrecinctOS): not touched here
}

bool HUUsables::IsMarked(const UPrimitiveComponent* C) { return C && C->ComponentHasTag(TEXT("HUUse")); }

bool HUUsables::Handles(AActor* Target, UPrimitiveComponent* Part, APawn* User)
{
	if (!Target || !IsMarked(Part)) return false;
	if (!Target->GetClass()->ImplementsInterface(UHUInteractable::StaticClass())) return true;
	return IHUInteractable::Execute_GetPrompt(Target, User).IsEmpty(); // what the room does with it comes first
}

// ---------- what E says ----------
FText HUUsables::Prompt(UPrimitiveComponent* C, APawn* User)
{
	UHUUsableWorld* S = World(C);
	const UHUUsableWorld::FUse* U = S ? S->Find(C) : nullptr;
	if (!U) return FText::GetEmpty();
	const AHUCharacter* Ch = Cast<AHUCharacter>(User);
	const TCHAR* T = TEXT("");
	switch (U->Kind)
	{
	case EHUUse::Drawer: T = U->bOn ? TEXT("[E] Shut the drawer") : TEXT("[E] Open the drawer"); break;
	case EHUUse::Files:  T = U->bOn ? TEXT("[E] Shut the drawer  ·  [Q] Pull a file") : TEXT("[E] Open the drawer  ·  [Q] Pull a file"); break;
	case EHUUse::Locker: T = U->bOn ? TEXT("[E] Shut the locker") : TEXT("[E] Open the locker"); break;
	case EHUUse::Phone:  T = TEXT("[E] Pick up the phone"); break;
	case EHUUse::Seat: case EHUUse::Bench: T = Ch && Ch->bSeated ? TEXT("[E] Stand up") : TEXT("[E] Sit down"); break;
	case EHUUse::Switch: T = U->bOn ? TEXT("[E] Lights on") : TEXT("[E] Lights off"); break;
	case EHUUse::Screen: T = U->bOn ? TEXT("[E] Turn the TV off") : TEXT("[E] Turn the TV on"); break;
	case EHUUse::Lamp:   T = U->bOn ? TEXT("[E] Turn the lamp off") : TEXT("[E] Turn the lamp on"); break;
	case EHUUse::Drink:  T = TEXT("[E] A cup of water"); break;
	case EHUUse::Clock:  return FText::FromString(TEXT("The clock says ") + ClockTime());
	case EHUUse::Read:   T = TEXT("[E] Read it"); break;
	default: break;
	}
	return FText::FromString(T);
}

// ---------- using it (every machine) ----------
void HUUsables::Use(UPrimitiveComponent* C, APawn* User, bool bSecondary)
{
	UHUUsableWorld* S = World(C);
	UHUUsableWorld::FUse* U = S ? S->Find(C) : nullptr;
	if (!U) return;
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	const bool bMine = User && User->IsLocallyControlled();
	auto Line = [&](const FString& L) { if (bMine && I && !L.IsEmpty()) { I->Say(L); UE_LOG(LogTemp, Log, TEXT("HUUsables: %s"), *L); } };
	auto Sound = [&](const TCHAR* Name, float Vol) { if (C->GetOwner() && C->GetOwner()->HasAuthority()) HUSound::Play(C, Name, C->GetComponentLocation(), Vol); }; // once, from the host
	auto Reach = [&]() { if (I) I->PlayReach(); };
	if (bSecondary && U->Kind != EHUUse::Files) return;
	const int32 N = U->Uses++;
	switch (U->Kind)
	{
	case EHUUse::Drawer:
		U->bOn = !U->bOn; Reach(); Sound(TEXT("SFX_Click"), 0.5f);
		if (U->bOn) Line(U->Words.IsEmpty() ? DrawerLine(N + GetTypeHash(C->GetFName())) : U->Words);
		break;
	case EHUUse::Files:
		Reach();
		if (bSecondary) { Line(OldFile(N + GetTypeHash(C->GetFName()))); break; }
		U->bOn = !U->bOn; Sound(TEXT("SFX_Click"), 0.5f);
		if (U->bOn) Line(TEXT("Hanging folders, A to F. Every one of them somebody's worst night. [Q] pulls one."));
		break;
	case EHUUse::Locker:
		U->bOn = !U->bOn; Reach(); Sound(TEXT("SFX_Door"), 0.6f);
		if (UStaticMeshComponent* M = Cast<UStaticMeshComponent>(C))
			if (UStaticMesh* To = HUBuild::PropMesh(U->bOn ? TEXT("LockerOpen") : TEXT("Locker"))) M->SetStaticMesh(To); // same scale: both are modelled at real size
		if (U->bOn) Line(U->Words.IsEmpty() ? LockerLine(N + GetTypeHash(C->GetFName())) : U->Words);
		break;
	case EHUUse::Phone:
		Reach(); Sound(TEXT("SFX_Click"), 0.6f); Line(U->Words.IsEmpty() ? PhoneLine(N) : U->Words);
		break;
	case EHUUse::Seat: case EHUUse::Bench:
		if (AHUCharacter* Ch = Cast<AHUCharacter>(User))
		{
			if (Ch->bSeated) { Ch->StandUp(); break; }
			FVector Mid, Half; LocalBox(C, Mid, Half);
			const FTransform T(C->GetComponentQuat(), C->GetComponentLocation());
			FVector Seat = T.TransformPosition(Mid);
			if (U->Kind == EHUUse::Bench)
			{
				// along the bench to where you're standing
				const FVector Along = C->GetRightVector();
				const float A = FMath::Clamp(FVector::DotProduct(User->GetActorLocation() - Seat, Along), -FMath::Max(0.f, Half.Y - 30.f), FMath::Max(0.f, Half.Y - 30.f));
				Seat += Along * A;
			}
			Seat.Z = T.TransformPosition(Mid - FVector(0, 0, Half.Z)).Z + 2.f * Half.Z * U->Extra;
			const float Yaw = U->FrontYaw > 900.f ? User->GetActorRotation().Yaw : C->GetComponentRotation().Yaw + U->FrontYaw;
			Ch->SitDown(Seat, Yaw);
		}
		break;
	case EHUUse::Switch:
	{
		U->bOn = !U->bOn; Reach(); Sound(TEXT("SFX_Click"), 0.7f);
		AActor* O = C->GetOwner();
		if (!O) break;
		const FTransform OT = O->GetActorTransform();
		TArray<UActorComponent*> Comps; O->GetComponents(Comps);
		for (UActorComponent* AC : Comps)
		{
			const USceneComponent* SC = Cast<USceneComponent>(AC);
			if (!SC || SC == C) continue;
			const FVector L = OT.InverseTransformPosition(SC->GetComponentLocation());
			if (!U->Room.IsInside(FVector2D(L.X, L.Y))) continue;
			if (ULightComponent* Lt = Cast<ULightComponent>(AC)) { Lt->SetVisibility(!U->bOn); continue; }
			// the light panels themselves: the self-lit blocks (colour brighter than white) go dark too
			if (UStaticMeshComponent* M = Cast<UStaticMeshComponent>(AC))
				if (UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
				{
					if (U->bOn)
					{
						FLinearColor Col;
						if (D->GetVectorParameterValue(TEXT("Color"), Col) && Col.GetMax() > 1.5f) { U->Dimmed.Add(M, Col); D->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.05f, 0.05f)); }
					}
					else if (const FLinearColor* Was = U->Dimmed.Find(M)) D->SetVectorParameterValue(TEXT("Color"), *Was);
				}
		}
		if (!U->bOn) U->Dimmed.Reset();
		break;
	}
	case EHUUse::Screen:
	{
		U->bOn = !U->bOn; Reach(); Sound(TEXT("SFX_Click"), 0.5f);
		if (!U->Glow.IsValid() && C->GetOwner())
		{
			// a lit picture on the front of the set
			FVector Mid, Half; LocalBox(C, Mid, Half);
			TArray<UStaticMeshComponent*> Tmp;
			UStaticMeshComponent* G = HUBuild::Block(C->GetOwner(), C->GetAttachParent(), Tmp, HUBuild::EShape::Cube, FVector::ZeroVector,
				FVector(0.6f, Half.Y * 1.3f, Half.Z * 1.1f), FLinearColor(0.5f, 0.7f, 1.4f), FRotator::ZeroRotator, false);
			if (G) { const FTransform T(C->GetComponentQuat(), C->GetComponentLocation()); G->SetWorldLocationAndRotation(T.TransformPosition(Mid + FVector(Half.X + 0.5f, 0, Half.Z * 0.1f)), C->GetComponentQuat()); }
			U->Glow = G;
		}
		if (U->Glow.IsValid()) U->Glow->SetVisibility(U->bOn);
		if (U->bOn) Line(NewsLine(N));
		break;
	}
	case EHUUse::Lamp:
	{
		U->bOn = !U->bOn; Reach(); Sound(TEXT("SFX_Click"), 0.5f);
		if (!U->Light.IsValid() && C->GetOwner())
		{
			FVector Mid, Half; LocalBox(C, Mid, Half);
			UPointLightComponent* L = NewObject<UPointLightComponent>(C->GetOwner());
			L->SetupAttachment(C->GetAttachParent());
			L->RegisterComponent();
			const FTransform T(C->GetComponentQuat(), C->GetComponentLocation());
			L->SetWorldLocation(T.TransformPosition(Mid + FVector(0, 0, Half.Z * 0.7f)));
			L->SetIntensity(1500.f); L->SetAttenuationRadius(450.f); L->SetLightColor(FLinearColor(1.f, 0.8f, 0.55f));
			U->Light = L;
		}
		if (U->Light.IsValid()) U->Light->SetVisibility(U->bOn);
		break;
	}
	case EHUUse::Drink:
		Reach(); Line(Pick({ TEXT("You fill a paper cone at the cooler. Cold, and it tastes faintly of plastic."), TEXT("The cooler glugs. One big bubble climbs the bottle.") }, N));
		break;
	case EHUUse::Clock:
		Line(ClockTime() + TEXT(". A long night, and it isn't over."));
		break;
	case EHUUse::Read:
		Line(U->Words.IsEmpty() ? FString(TEXT("Notices: overtime slips, a bowling night, a lost badge, and a wanted poster gone yellow.")) : U->Words);
		break;
	default: break;
	}
}

// ---------- a light switch ----------
UStaticMeshComponent* HUUsables::AddSwitch(AActor* Owner, USceneComponent* Parent, TArray<UStaticMeshComponent*>& Out, FVector Pos, float Yaw, FBox2D Room)
{
	const FRotator R(0, Yaw, 0);
	UStaticMeshComponent* Plate = HUBuild::Block(Owner, Parent, Out, HUBuild::EShape::Cube, Pos, FVector(1.2f, 8, 12), FLinearColor(0.78f, 0.76f, 0.68f), R, true);
	HUBuild::Block(Owner, Parent, Out, HUBuild::EShape::Cube, Pos + R.Vector() * 1.f, FVector(1.5f, 1.6f, 3.6f), FLinearColor(0.08f, 0.08f, 0.08f), R, false);
	if (Plate) { Mark(Plate, EHUUse::Switch); if (UHUUsableWorld::FUse* U = World(Plate) ? World(Plate)->Find(Plate) : nullptr) U->Room = Room; }
	return Plate;
}

// ---------- drawers sliding, screens flickering ----------
void UHUUsableWorld::PlaceTray(UPrimitiveComponent* C, FUse& U)
{
	const bool bFiles = U.Kind == EHUUse::Files;
	if (!U.Tray.IsValid())
	{
		// the drawer: its box, its front panel (with a pull), and what's in it - folders' tabs, or papers
		if (!C->GetOwner()) return;
		TArray<UStaticMeshComponent*> Tmp;
		auto Part = [&](FLinearColor Col, EHUSurface S) { return HUBuild::Block(C->GetOwner(), C->GetAttachParent(), Tmp, HUBuild::EShape::Cube, FVector::ZeroVector, FVector(1, 1, 1), Col, FRotator::ZeroRotator, false, S); };
		U.Tray = Part(bFiles ? FLinearColor(0.2f, 0.21f, 0.2f) : FLinearColor(0.06f, 0.035f, 0.02f), bFiles ? EHUSurface::PaintedMetal : EHUSurface::Walnut);
		U.Front = Part(bFiles ? FLinearColor(0.42f, 0.43f, 0.4f) : FLinearColor(0.12f, 0.07f, 0.04f), bFiles ? EHUSurface::PaintedMetal : EHUSurface::Walnut);
		U.Fill = Part(bFiles ? FLinearColor(0.72f, 0.58f, 0.32f) : FLinearColor(0.78f, 0.76f, 0.68f), EHUSurface::None);
		if (!U.Tray.IsValid()) return;
	}
	// on the front face (FrontYaw), near the top, slid out along the front
	FVector Mid, Half; LocalBox(C, Mid, Half);
	const FVector D = FRotator(0, U.FrontYaw, 0).Vector(), Side(-D.Y, D.X, 0);
	const float Face = FMath::Abs(D.X) * Half.X + FMath::Abs(D.Y) * Half.Y, SideHalf = FMath::Abs(Side.X) * Half.X + FMath::Abs(Side.Y) * Half.Y;
	const float Depth = FMath::Min(bFiles ? 55.f : 42.f, Face * 1.6f), Width = FMath::Min(bFiles ? SideHalf * 1.6f : 46.f, SideHalf * 1.6f), Height = bFiles ? 26.f : 13.f;
	const FVector Shut = Mid + D * (Face - Depth / 2 - 0.5f) + Side * (SideHalf * U.Extra) + FVector(0, 0, Half.Z * (bFiles ? 0.78f : 0.55f) - Height / 2);
	const FVector Local = Shut + D * (U.Slide * Depth * 0.85f);
	const FTransform T(C->GetComponentQuat(), C->GetComponentLocation());
	const FQuat Q = (FRotator(C->GetComponentQuat()) + FRotator(0, U.FrontYaw, 0)).Quaternion();
	const bool bShow = U.Slide > 0.02f;
	auto Put = [&](const TWeakObjectPtr<UStaticMeshComponent>& M, FVector At, FVector Size)
	{
		if (!M.IsValid()) return;
		M->SetWorldScale3D(Size / 100.f);
		M->SetWorldLocationAndRotation(T.TransformPosition(At), Q);
		M->SetVisibility(bShow);
	};
	Put(U.Tray, Local, FVector(Depth, Width, Height - 2.f));
	Put(U.Front, Local + D * (Depth / 2 + 0.8f), FVector(1.6f, Width + 3.f, Height + 2.f));
	Put(U.Fill, Local + FVector(0, 0, Height / 2 - 0.5f), bFiles ? FVector(Depth * 0.85f, Width * 0.8f, 3.f) : FVector(Depth * 0.6f, Width * 0.6f, 1.f));
}

// -HUUseTest (on L_Precinct or L_Lobby): count what's usable, then go to one of each kind, use it, and screenshot it
void UHUUsableWorld::TestStep(float DeltaTime)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) return;
	const float Was = TestClock;
	TestClock += DeltaTime;
	auto Passed = [&](float T) { return Was < T && TestClock >= T; };
	static const EHUUse Order[] = { EHUUse::Drawer, EHUUse::Files, EHUUse::Locker, EHUUse::Screen, EHUUse::Phone, EHUUse::Seat, EHUUse::Bench, EHUUse::Lamp, EHUUse::Drink, EHUUse::Clock, EHUUse::Read, EHUUse::Switch };
	if (Passed(3.f))
	{
		TMap<EHUUse, int32> Count;
		for (const auto& It : Parts) if (It.Key.ResolveObjectPtr()) Count.FindOrAdd(It.Value.Kind)++;
		for (const auto& C : Count) UE_LOG(LogTemp, Display, TEXT("HUUseTest: %d usable %s"), C.Value, KindName(C.Key));
	}
	for (int32 i = 0; i < UE_ARRAY_COUNT(Order); i++)
	{
		const float T0 = 4.f + i * 3.5f;
		if (!(Passed(T0) || Passed(T0 + 0.6f) || Passed(T0 + 2.f) || Passed(T0 + 2.2f) || Passed(T0 + 2.8f))) continue;
		UPrimitiveComponent* C = nullptr;
		FUse* U = nullptr;
		for (auto& It : Parts) if (It.Value.Kind == Order[i]) if (UPrimitiveComponent* Cc = It.Key.ResolveObjectPtr()) { C = Cc; U = &It.Value; break; }
		if (!C) { if (Passed(T0)) UE_LOG(LogTemp, Display, TEXT("HUUseTest: no %s here"), KindName(Order[i])); continue; }
		const FVector F = (FRotator(C->GetComponentQuat()) + FRotator(0, U->FrontYaw > 900.f ? 0.f : U->FrontYaw, 0)).Vector().GetSafeNormal2D();
		// look at the drawer if there is one, else the middle of the thing
		const FVector At = U->Tray.IsValid() ? U->Tray->GetComponentLocation() + F * 40.f : C->Bounds.Origin;
		if (Passed(T0))
		{
			const float Back = Order[i] == EHUUse::Switch ? 120.f : 170.f;
			P->SetActorLocation(FVector(At.X + F.X * Back, At.Y + F.Y * Back, C->Bounds.Origin.Z - C->Bounds.BoxExtent.Z + 100.f), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation((At - (P->GetActorLocation() + FVector(0, 0, 64))).Rotation());
		}
		if (Passed(T0 + 0.6f))
		{
			UE_LOG(LogTemp, Display, TEXT("HUUseTest: %s %s - prompt \"%s\""), KindName(Order[i]), *C->GetName(), *HUUsables::Prompt(C, P).ToString());
			HUUsables::Use(C, P, false);
			if (Order[i] == EHUUse::Files) HUUsables::Use(C, P, true); // and pull a file
		}
		// aim again once you've landed: a TV or a clock high on the wall is above where you were dropped from
		if (Passed(T0 + 2.f) && Order[i] != EHUUse::Seat && Order[i] != EHUUse::Bench) PC->SetControlRotation((At - P->GetPawnViewLocation()).Rotation());
		if (Passed(T0 + 2.2f)) FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Use_%s"), KindName(Order[i])), false, false);
		if (Passed(T0 + 2.8f) && (Order[i] == EHUUse::Seat || Order[i] == EHUUse::Bench || Order[i] == EHUUse::Switch)) HUUsables::Use(C, P, false); // stand up / lights back on
	}
	if (Passed(4.f + UE_ARRAY_COUNT(Order) * 3.5f + 1.f)) FPlatformMisc::RequestExit(false);
}

void UHUUsableWorld::Tick(float DeltaTime)
{
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("HUUseTest"));
	if (bTest && GetWorld() && GetWorld()->IsGameWorld()) TestStep(DeltaTime);
	for (auto It = Parts.CreateIterator(); It; ++It)
	{
		UPrimitiveComponent* C = It.Key().ResolveObjectPtr();
		FUse& U = It.Value();
		if (!C)
		{
			// the room was rebuilt (an office redecorated): what this part added goes with it - a lit lamp mustn't stay lit
			for (UPrimitiveComponent* Extra : { (UPrimitiveComponent*)U.Tray.Get(), (UPrimitiveComponent*)U.Front.Get(), (UPrimitiveComponent*)U.Fill.Get(), (UPrimitiveComponent*)U.Glow.Get() })
				if (Extra) Extra->DestroyComponent();
			if (U.Light.IsValid()) U.Light->DestroyComponent();
			It.RemoveCurrent();
			continue;
		}
		if (U.Kind == EHUUse::Drawer || U.Kind == EHUUse::Files)
		{
			const float Want = U.bOn ? 1.f : 0.f;
			if (!FMath::IsNearlyEqual(U.Slide, Want) || (U.bOn && !U.Tray.IsValid()))
			{
				U.Slide = FMath::FInterpConstantTo(U.Slide, Want, DeltaTime, 3.5f);
				PlaceTray(C, U);
			}
		}
		else if (U.Kind == EHUUse::Screen && U.bOn && U.Glow.IsValid())
			if (UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(U.Glow->GetMaterial(0)))
				D->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.7f, 1.4f) * FMath::FRandRange(0.8f, 1.25f)); // the picture flickers
	}
}
