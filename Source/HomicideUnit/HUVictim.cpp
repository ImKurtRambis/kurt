#include "HUVictim.h"
#include "HUBuild.h"
#include "HUPeople.h"
#include "HUCaseSubsystem.h"
#include "HUEvidence.h"
#include "HUInteractComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"

using namespace HUBuild;

AHUVictim::AHUVictim()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	const TCHAR* Names[] = { TEXT("Head"), TEXT("Neck"), TEXT("LeftHand"), TEXT("RightHand"), TEXT("LeftArm"), TEXT("RightArm"), TEXT("Torso"), TEXT("LeftThigh"), TEXT("RightThigh"), TEXT("LeftCalf"), TEXT("RightCalf") };
	const float Radii[] = { 12, 8, 10, 10, 13, 13, 25, 18, 18, 16, 16 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		USphereComponent* Zone = CreateDefaultSubobject<USphereComponent>(FName(Names[Index]));
		Zone->SetupAttachment(Root);
		Zone->InitSphereRadius(Radii[Index]);
		Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
		Zone->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Zone->SetGenerateOverlapEvents(false);
		BodyZones.Add(Zone);
	}
}

UHUCaseSubsystem* AHUVictim::Cases() const { return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHUCaseSubsystem>() : nullptr; }

void AHUVictim::BeginPlay()
{
	Super::BeginPlay();
	const UHUCaseSubsystem* C = Cases();
	if (Kind != EHUVictimKind::Body || !C) return;
	FString Name = C->Current.Victim; int32 Comma; if (Name.FindChar(',', Comma)) Name = Name.Left(Comma);
	const float Scale = C->Current.Victim.Contains(TEXT("age ")) ? 0.65f : 1.f;
	Corpse = HUPeople::Lie(this, RootComponent, HUPeople::VictimFace(Name, C->Current.Number), FVector(-112 * Scale, 0, 22), Scale);
	USkeletalMeshComponent* Body = HUPeople::BodyOf(Corpse);
	if (!Body) return;
	const FName Bones[] = { TEXT("head"), TEXT("neck_01"), TEXT("hand_l"), TEXT("hand_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("spine_03"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r") };
	for (int32 Index = 0; Index < BodyZones.Num(); ++Index)
		if (Body->DoesSocketExist(Bones[Index]))
		{
			BodyZones[Index]->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Bones[Index]);
			BodyZones[Index]->SetRelativeScale3D(FVector(1.f));
			BodyZones[Index]->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}


}

void AHUVictim::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Corpse) { Corpse->Destroy(); Corpse = nullptr; }
	Super::EndPlay(Reason);
}

// local +X runs from the feet to the head
void AHUVictim::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Clear(Blocks);
	Spots.Reset();
	const int32 SpotIds[] = { 0, 1, 2, 2, 2, 2, 3, 4, 4, 4, 4 };
	for (int32 Index = 0; Index < BodyZones.Num(); ++Index)
	{
		BodyZones[Index]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Kind == EHUVictimKind::Body) Spots.Add(BodyZones[Index], SpotIds[Index]);
	}
	USceneComponent* R = RootComponent;
	const FLinearColor Blood(0.22f, 0.01f, 0.01f), Tape(0.95f, 0.95f, 0.92f);
	auto Line = [&](FVector A, FVector B) // a strip of white tape on the floor from A to B
	{
		const FVector D = B - A;
		Block(this, R, Blocks, EShape::Cube, (A + B) / 2 + FVector(0, 0, 0.3f), FVector(D.Size2D(), 2.5f, 0.6f), Tape, FRotator(0, D.Rotation().Yaw, 0), false);
	};
	switch (Kind)
	{
	case EHUVictimKind::Body:
	{
		// a pool of blood under the head and shoulders
		Block(this, R, Blocks, EShape::Cylinder, FVector(30, 10, 0.3f), FVector(80, 62, 0.6f), Blood);
		Block(this, R, Blocks, EShape::Cylinder, FVector(62, 30, 0.3f), FVector(26, 18, 0.6f), Blood);


		break;
	}
	case EHUVictimKind::Outline:
	{
		// where they lay until the ambulance came: tape outline and a smear of blood
		Block(this, R, Blocks, EShape::Cylinder, FVector(25, 6, 0.3f), FVector(56, 40, 0.6f), Blood);
		Line({ 32, -18, 0 }, { 32, 18, 0 }); Line({ 32, 18, 0 }, { -32, 20, 0 }); Line({ -32, 20, 0 }, { -32, -20, 0 }); Line({ -32, -20, 0 }, { 32, -18, 0 });
		Line({ 32, -12, 0 }, { 56, -12, 0 }); Line({ 56, -12, 0 }, { 56, 12, 0 }); Line({ 56, 12, 0 }, { 32, 12, 0 });
		Line({ 30, 18, 0 }, { 50, 62, 0 }); Line({ 30, -18, 0 }, { -10, -34, 0 });
		Line({ -32, 14, 0 }, { -110, 12, 0 }); Line({ -32, -14, 0 }, { -100, -36, 0 });
		break;
	}
	case EHUVictimKind::Belongings:
	{
		// a folded coat, a handbag on its side, a hat
		Block(this, R, Blocks, EShape::Cube, FVector(0, 0, 4), FVector(46, 34, 8), Coat);
		Block(this, R, Blocks, EShape::Cube, FVector(40, 20, 10), FVector(30, 12, 20), FLinearColor(0.3f, 0.16f, 0.06f), FRotator(0, 25, 80));
		Block(this, R, Blocks, EShape::Cylinder, FVector(-30, 30, 3), FVector(30, 30, 3), FLinearColor(0.08f, 0.07f, 0.06f));
		Block(this, R, Blocks, EShape::Cylinder, FVector(-30, 30, 8), FVector(18, 18, 8), FLinearColor(0.08f, 0.07f, 0.06f));
		break;
	}
	case EHUVictimKind::Scorch:
	{
		// where the fire started: a burnt patch, a melted bin, charred junk
		Block(this, R, Blocks, EShape::Cylinder, FVector(0, 0, 0.3f), FVector(220, 190, 0.6f), FLinearColor(0.02f, 0.015f, 0.01f));
		Block(this, R, Blocks, EShape::Cylinder, FVector(10, -10, 22), FVector(36, 36, 44), FLinearColor(0.04f, 0.035f, 0.03f));
		for (int32 i = 0; i < 6; i++) Block(this, R, Blocks, EShape::Cube, FVector(-60 + i * 22, 30 - (i % 2) * 50, 3), FVector(26, 8, 6), FLinearColor(0.03f, 0.025f, 0.02f), FRotator(0, i * 37, 0));
		break;
	}
	}
	// a couple of yellow evidence markers around it
	for (const FVector& M : { FVector(70, -40, 0), FVector(-40, 50, 0) })
	{
		Block(this, R, Blocks, EShape::Cube, M + FVector(0, 0, 9), FVector(12, 2, 18), FLinearColor(1.f, 0.75f, 0.05f), FRotator(0, 0, 18), false);
		Block(this, R, Blocks, EShape::Cube, M + FVector(0, 4, 9), FVector(12, 2, 18), FLinearColor(1.f, 0.75f, 0.05f), FRotator(0, 0, -18), false);
	}
}

FString AHUVictim::Notes() const
{
	const UHUCaseSubsystem* S = Cases();
	if (!S || !S->bCaseActive) return TEXT("");
	const FHUCase& C = S->Current;
	switch (Kind)
	{
	case EHUVictimKind::Body:
		return FString::Printf(TEXT("M.E.'S NOTES — %s. Cause of death: %s. Time of death close to %s. Lividity says the body hasn't been moved: it happened right here."), *C.Victim, *C.Weapon, *C.Time);
	case EHUVictimKind::Outline:
		return FString::Printf(TEXT("%s lay here until the ambulance came, just after %s. Attacked with %s. Alive — but not talking yet."), *C.Victim, *C.Time, *C.Weapon);
	case EHUVictimKind::Belongings:
		return FString::Printf(TEXT("%s's coat and bag, left behind around %s. The wallet's still inside — whoever's behind this wasn't after money."), *C.Victim, *C.Time);
	default:
		return FString::Printf(TEXT("The fire started here, around %s: a waste bin and a lot of accelerant. %s was pulled out of the next room."), *C.Time, *C.Victim);
	}
}

FText AHUVictim::GetPrompt_Implementation(APawn* User) const
{
	// looking at a part of the body: examine that part
	if (const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr)
		if (const int32* Spot = Spots.Find(I->FocusPart))
			return FText::FromString(FString::Printf(TEXT("[E] Examine the %s%s  ·  [Q] Search the pockets"), SpotName(*Spot), Examined.Contains(*Spot) ? TEXT(" (checked)") : TEXT("")));
	const TCHAR* Look = Kind == EHUVictimKind::Body ? TEXT("[E] Examine the body") : Kind == EHUVictimKind::Outline ? TEXT("[E] Examine where the victim lay")
		: Kind == EHUVictimKind::Belongings ? TEXT("[E] Examine the victim's things") : TEXT("[E] Examine where the fire started");
	FString P = Look;
	if (!bSearched && Kind != EHUVictimKind::Scorch && Kind != EHUVictimKind::Outline)
		P += Kind == EHUVictimKind::Body ? TEXT("  ·  [Q] Search the pockets") : TEXT("  ·  [Q] Search the bag");
	return FText::FromString(P);
}

void AHUVictim::Interact_Implementation(APawn* User, bool bSecondary)
{
	UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	if (!I) return;
	if (!bSecondary)
	{
		if (const int32* Spot = Spots.Find(I->FocusPart)) { ExamineSpot(*Spot, I); return; }
		I->Say(Notes());
		return;
	}
	if (bSearched || Kind == EHUVictimKind::Scorch || Kind == EHUVictimKind::Outline) return;
	bSearched = true;
	I->PlayReach();
	if (IsValid(Pocket)) // (gone if it was bagged on an earlier visit)
	{
		// lay what you found beside the body
		Pocket->SetActorLocation(GetActorLocation() + GetActorRightVector() * -55.f + GetActorForwardVector() * 10.f);
		Pocket->Reveal();
		I->Say(Kind == EHUVictimKind::Body ? TEXT("In the inside pocket of the coat, something they didn't want found. It's on the floor beside them now — take a look.")
			: TEXT("Tucked in the lining of the bag: something worth a closer look. It's on the floor beside it now."));
		Pocket = nullptr;
	}
	else I->Say(Kind == EHUVictimKind::Body ? TEXT("Keys, some change, a bus transfer. Nothing that helps.") : TEXT("A compact, tissues, a bus transfer. Nothing that helps."));
}

const TCHAR* AHUVictim::SpotName(int32 Spot)
{
	switch (Spot) { case 0: return TEXT("head"); case 1: return TEXT("neck"); case 2: return TEXT("hands and arms"); case 3: return TEXT("torso"); default: return TEXT("legs and shoes"); }
}

// What each part of the body tells you. Fixed per case: the wound matches the weapon; one part carries a sign
// of what kind of offender did this (it counts toward the profile); the rest are the ordinary forensics.
FString AHUVictim::Finding(int32 Spot, EHUProfile& OutSign) const
{
	OutSign = EHUProfile::None;
	const UHUCaseSubsystem* S = Cases();
	if (!S || !S->bCaseActive) return TEXT("");
	const FHUCase& C = S->Current;
	const FString W = C.Weapon.ToLower();

	// the wound, from the weapon
	int32 WoundSpot = 3; FString Wound = TEXT("A single heavy wound to the chest.");
	if (W.Contains(TEXT("knife"))) { WoundSpot = 3; Wound = TEXT("Three deep stab wounds to the chest, close together. Single-edged blade, about six inches."); }
	else if (W.Contains(TEXT("ice pick"))) { WoundSpot = 0; Wound = TEXT("One thin puncture at the base of the skull. Almost no blood outside - whoever did it knew exactly where to put it."); }
	else if (W.Contains(TEXT("tire iron"))) { WoundSpot = 0; Wound = TEXT("A depressed fracture above the left ear. One heavy, narrow blow - something like a tire iron."); }
	else if (W.Contains(TEXT("lamp"))) { WoundSpot = 0; Wound = TEXT("Blunt trauma to the back of the head, two blows. Flecks of brass in the wound."); }
	else if (W.Contains(TEXT("rope"))) { WoundSpot = 1; Wound = TEXT("A ligature mark around the neck, angled upward - strangled from behind. Rough rope fibres in the skin."); }
	else if (W.Contains(TEXT("revolver"))) { WoundSpot = 3; Wound = TEXT("A gunshot wound to the chest, powder burns around it - fired from less than a foot away."); }
	else if (W.Contains(TEXT("poison"))) { WoundSpot = 0; Wound = TEXT("No wounds anywhere. Lips tinged blue, pupils like pinpricks, a faint bitter smell on the breath. Poison."); }

	// the offender's sign, on a different part of the body
	struct FSign { int32 Spot; const TCHAR* Text; };
	FSign Sign = { 2, TEXT("") };
	switch ((EHUProfile)C.Profile)
	{
	case EHUProfile::SerialKiller:   Sign = { 2, TEXT("A small mark cut carefully into the left wrist - after death. Deliberate. A signature.") }; break;
	case EHUProfile::Arsonist:       Sign = { 0, TEXT("Singed eyebrows, soot in the nostrils. They were close to a fire not long before they died.") }; break;
	case EHUProfile::Poisoner:       Sign = { 2, TEXT("A tiny puncture in the crook of the elbow. An injection - or a test dose.") }; break;
	case EHUProfile::ChildPredator:  Sign = { 3, TEXT("Folded in the shirt pocket, a note in the victim's hand: 'Tell the police about them tomorrow. The kids aren't safe.'") }; break;
	case EHUProfile::ContractKiller: Sign = { 2, TEXT("Hands clean, nails unbroken - no struggle at all. One clean job. They never saw it coming.") }; break;
	case EHUProfile::Passion:        Sign = { 3, TEXT("Far more injuries than it took to kill. Someone kept going after it was over. That's rage - and it's personal.") }; break;
	case EHUProfile::Robber:         Sign = { 4, TEXT("Trouser pockets turned out, the wallet gone - and a pale band on the wrist where a watch used to be.") }; break;
	case EHUProfile::ConArtist:      Sign = { 2, TEXT("Ink on the right palm, smudged backwards - someone else's signature, pressed there off a wet document.") }; break;
	case EHUProfile::Enforcer:       Sign = { 2, TEXT("Bruised knuckles, two broken fingers. They were beaten before they died - a lesson first.") }; break;
	case EHUProfile::Stalker:        Sign = { 0, TEXT("A lock of hair cut off, neatly, at the back of the head. Someone wanted a keepsake.") }; break;
	default: break;
	}
	if (Sign.Spot == WoundSpot) Sign.Spot = WoundSpot == 2 ? 3 : 2; // keep them on different parts

	// everything else: the ordinary forensics
	const FString Ordinary[] = {
		FString::Printf(TEXT("Eyes half open. Rigor's setting in at the jaw - dead since around %s."), *C.Time),
		TEXT("Nothing on the neck. No marks, no bruising."),
		TEXT("Under the fingernails: skin and a little blood. They scratched someone. Whoever it was has marks."),
		TEXT("The shirt's torn at the collar. There was a struggle, however short."),
		TEXT("Scuffs on the heels. Lividity says they weren't moved - they fell right here."),
	};

	if (Spot == WoundSpot) return TEXT("CAUSE OF DEATH: ") + Wound;
	if (Spot == Sign.Spot && FCString::Strlen(Sign.Text) > 0) { OutSign = (EHUProfile)C.Profile; return Sign.Text; }
	return Ordinary[FMath::Clamp(Spot, 0, 4)];
}

void AHUVictim::ExamineSpot(int32 Spot, UHUInteractComponent* I)
{
	EHUProfile Sign;
	const FString Text = Finding(Spot, Sign);
	const bool bFirst = !Examined.Contains(Spot);
	Examined.Add(Spot);
	I->PlayReach();
	I->Say(FString::Printf(TEXT("THE %s:  %s"), *FString(SpotName(Spot)).ToUpper(), *Text));
	if (bFirst && Sign != EHUProfile::None)
	{
		if (UHUCaseSubsystem* S = Cases()) if (!S->BodyLeads.Contains(Sign)) S->AddBodyLead(Sign); // (once per case, even over two visits)
		I->Notify(TEXT("PROFILE LEAD"), FString::Printf(TEXT("The body points to a %s"), HUProfiles::Name(Sign)), FLinearColor(0.8f, 0.55f, 0.98f));
	}
	if (bFirst && Examined.Num() == 5) I->Notify(TEXT("BODY EXAMINED"), TEXT("Head to toe. Search the pockets too (Q)."), FLinearColor(0.36f, 0.76f, 1.f));
}

void AHUVictim::HoverParts(UPrimitiveComponent* Part, TArray<UPrimitiveComponent*>& Out) const
{
	// light up just the part you're looking at (all of the hands, all of the head...)
	if (const int32* Spot = Spots.Find(Part)) { for (const TPair<UPrimitiveComponent*, int32>& P : Spots) if (P.Value == *Spot) Out.Add(P.Key); }
}
float AHUVictim::UseTime(APawn* User, bool bSecondary) const
{
	if (bSecondary) return !bSearched && (Kind == EHUVictimKind::Body || Kind == EHUVictimKind::Belongings) ? 2.f : 0.f; // going through the pockets
	const UHUInteractComponent* I = User ? User->FindComponentByClass<UHUInteractComponent>() : nullptr;
	return I && Spots.Contains(I->FocusPart) ? 1.5f : 0.f; // a careful look at one part of the body
}