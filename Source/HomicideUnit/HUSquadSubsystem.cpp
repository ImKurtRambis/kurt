#include "HUSquadSubsystem.h"
#include "HUCaseSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

static const FName CodeKey(TEXT("HU_CODE"));
static const FName GameKey(TEXT("HU_GAME"));

IOnlineSessionPtr UHUSquadSubsystem::Sessions() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS) OSS = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

bool UHUSquadSubsystem::IsLan() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	return !OSS || OSS->GetSubsystemName() == NULL_SUBSYSTEM;
}

FString UHUSquadSubsystem::NetworkName() const { return IsLan() ? TEXT("local network") : TEXT("Steam"); }

FString UHUSquadSubsystem::TravelOptions() const
{
	const UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>();
	FString Name = C && !C->DetectiveName.IsEmpty() ? C->DetectiveName : TEXT("Detective");
	Name.ReplaceInline(TEXT("?"), TEXT("")); Name.ReplaceInline(TEXT("="), TEXT("")); Name.ReplaceInline(TEXT(" "), TEXT("_"));
	return TEXT("Name=") + Name;
}

// ---------- hosting ----------
void UHUSquadSubsystem::HostSquad()
{
	IOnlineSessionPtr S = Sessions();
	Code = FString::Printf(TEXT("%06d"), FMath::RandRange(100000, 999999));
	bHost = true;
	State = EHUSquadState::Hosting;
	Status = TEXT("Opening the squad room...");
	if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) C->UseSquadSeed(FCString::Atoi(*Code));
	if (!S)
	{
		// no online services at all: play on your own
		Status = TEXT("No network available — opening the squad room offline.");
		UGameplayStatics::OpenLevel(GetGameInstance(), LobbyLevel, true, TEXT("listen?") + TravelOptions());
		State = EHUSquadState::InSquad;
		return;
	}
	if (S->GetNamedSession(NAME_GameSession))
	{
		DestroyH = S->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UHUSquadSubsystem::OnDestroyedThenCreate));
		S->DestroySession(NAME_GameSession);
		return;
	}
	CreateNow();
}

void UHUSquadSubsystem::OnDestroyedThenCreate(FName Session, bool bOk)
{
	if (IOnlineSessionPtr S = Sessions()) S->ClearOnDestroySessionCompleteDelegate_Handle(DestroyH);
	CreateNow();
}

void UHUSquadSubsystem::CreateNow()
{
	IOnlineSessionPtr S = Sessions();
	FOnlineSessionSettings Set;
	Set.NumPublicConnections = MaxPlayers;
	Set.bShouldAdvertise = true;
	Set.bAllowJoinInProgress = true;
	Set.bIsLANMatch = IsLan();
	Set.bUsesPresence = true;
	Set.bUseLobbiesIfAvailable = true;
	Set.bAllowJoinViaPresence = true;
	Set.Set(CodeKey, Code, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Set.Set(GameKey, FString(TEXT("HomicideUnit")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	CreateH = S->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UHUSquadSubsystem::OnCreated));
	if (!S->CreateSession(0, NAME_GameSession, Set)) OnCreated(NAME_GameSession, false);
}

void UHUSquadSubsystem::OnCreated(FName Session, bool bOk)
{
	if (IOnlineSessionPtr S = Sessions()) S->ClearOnCreateSessionCompleteDelegate_Handle(CreateH);
	State = EHUSquadState::InSquad;
	Status = bOk ? FString::Printf(TEXT("Squad %s is open on the %s."), *Code, *NetworkName()) : TEXT("Couldn't open the squad online — playing offline.");
	UGameplayStatics::OpenLevel(GetGameInstance(), LobbyLevel, true, TEXT("listen?") + TravelOptions());
}

// ---------- joining ----------
void UHUSquadSubsystem::JoinByCode(const FString& SixDigits)
{
	IOnlineSessionPtr S = Sessions();
	Code = SixDigits.TrimStartAndEnd();
	bHost = false;
	if (Code.Len() != 6 || !Code.IsNumeric()) { State = EHUSquadState::Failed; Status = TEXT("A squad code is 6 numbers."); return; }
	if (!S) { State = EHUSquadState::Failed; Status = TEXT("No network available."); return; }
	State = EHUSquadState::Searching;
	Status = FString::Printf(TEXT("Paging squad %s on the %s..."), *Code, *NetworkName());
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = IsLan();
	Search->MaxSearchResults = 500;
	Search->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(FName(TEXT("LOBBYSEARCH")), true, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(CodeKey, Code, EOnlineComparisonOp::Equals);
	FindH = S->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UHUSquadSubsystem::OnFound));
	if (!S->FindSessions(0, Search.ToSharedRef())) OnFound(false);
}

void UHUSquadSubsystem::OnFound(bool bOk)
{
	IOnlineSessionPtr S = Sessions();
	if (S) S->ClearOnFindSessionsCompleteDelegate_Handle(FindH);
	if (!S || !Search.IsValid()) { State = EHUSquadState::Failed; Status = TEXT("Couldn't search for squads."); return; }
	for (const FOnlineSessionSearchResult& R : Search->SearchResults)
	{
		FString Got, Game;
		R.Session.SessionSettings.Get(CodeKey, Got);
		R.Session.SessionSettings.Get(GameKey, Game);
		if (Got != Code || Game != TEXT("HomicideUnit")) continue;
		State = EHUSquadState::Joining;
		Status = FString::Printf(TEXT("Found squad %s — joining..."), *Code);
		if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) C->UseSquadSeed(FCString::Atoi(*Code));
		JoinH = S->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UHUSquadSubsystem::OnJoined));
		S->JoinSession(0, NAME_GameSession, R);
		return;
	}
	State = EHUSquadState::Failed;
	Status = FString::Printf(TEXT("No squad with code %s answered. Check the code — and that you're on the same %s."), *Code, *NetworkName());
}

void UHUSquadSubsystem::OnJoined(FName Session, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr S = Sessions();
	if (S) S->ClearOnJoinSessionCompleteDelegate_Handle(JoinH);
	FString Address;
	if (!S || Result != EOnJoinSessionCompleteResult::Success || !S->GetResolvedConnectString(NAME_GameSession, Address))
	{
		State = EHUSquadState::Failed;
		Status = Result == EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("That squad is full.") : TEXT("Couldn't get into that squad room.");
		return;
	}
	State = EHUSquadState::InSquad;
	Status = FString::Printf(TEXT("In squad %s."), *Code);
	if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
		PC->ClientTravel(Address + TEXT("?") + TravelOptions(), TRAVEL_Absolute);
}

// ---------- starting and leaving ----------
void UHUSquadSubsystem::StartShift()
{
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client) return;
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit: starting the shift (net mode %d)"), (int32)W->GetNetMode());
	// not actually hosting (no network): just go
	if (W->GetNetMode() == NM_Standalone) { UGameplayStatics::OpenLevel(GetGameInstance(), BriefingLevel, true, TEXT("listen?") + TravelOptions()); return; }
	if (IOnlineSessionPtr S = Sessions()) S->StartSession(NAME_GameSession);
	W->ServerTravel(BriefingLevel.ToString()); // already listening: the connection carries over
}

void UHUSquadSubsystem::LeaveSquad()
{
	if (IOnlineSessionPtr S = Sessions()) if (S->GetNamedSession(NAME_GameSession)) S->DestroySession(NAME_GameSession);
	Code.Reset();
	bHost = false;
	State = EHUSquadState::None;
	Status.Reset();
	if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) C->UseSquadSeed(0);
}
