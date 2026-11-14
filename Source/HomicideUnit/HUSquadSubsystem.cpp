#include "HUSquadSubsystem.h"
#include "HUCaseSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#if HU_STEAM
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

static const FName CodeKey(TEXT("HU_CODE"));
static const FName GameKey(TEXT("HU_GAME"));

void UHUSquadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// a Steam invite, or "Join game" in the friends list (also when that started the game: Steam holds it until now)
	if (IOnlineSessionPtr S = Sessions())
		InviteH = S->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UHUSquadSubsystem::OnInviteAccepted));
}

void UHUSquadSubsystem::Deinitialize()
{
	StopWorldwide();
	if (IOnlineSessionPtr S = Sessions()) S->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteH);
	Super::Deinitialize();
}

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
	Set.bAllowInvites = true; // Steam invites from the overlay (OnInviteAccepted on the friend's side)
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
#if !UE_BUILD_SHIPPING
	// -HUWorldSearch: straight to the worldwide search (to test it on one PC, where Unreal's own search finds the squad)
	if (!IsLan() && FParse::Param(FCommandLine::Get(), TEXT("HUWorldSearch")) && SearchWorldwide()) return;
#endif
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
		JoinResult(R);
		return;
	}
	// Unreal only asked Steam about squads near you: a friend on another continent is found by asking again worldwide
	if (!IsLan() && SearchWorldwide()) return;
	State = EHUSquadState::Failed;
	Status = FString::Printf(TEXT("No squad with code %s answered. Check the code — and that you're on the same %s."), *Code, *NetworkName());
}

void UHUSquadSubsystem::JoinResult(const FOnlineSessionSearchResult& R)
{
	IOnlineSessionPtr S = Sessions();
	if (!S) { State = EHUSquadState::Failed; Status = TEXT("No network available."); return; }
	State = EHUSquadState::Joining;
	Status = FString::Printf(TEXT("Found squad %s — joining..."), *Code);
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit: joining squad %s on the %s"), *Code, *NetworkName());
	if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) C->UseSquadSeed(FCString::Atoi(*Code));
	JoinH = S->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UHUSquadSubsystem::OnJoined));
	if (!S->JoinSession(0, NAME_GameSession, R)) OnJoined(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
}

void UHUSquadSubsystem::OnInviteAccepted(const bool bOk, const int32 User, FUniqueNetIdPtr From, const FOnlineSessionSearchResult& Invite)
{
	FString Got, Game;
	Invite.Session.SessionSettings.Get(CodeKey, Got);
	Invite.Session.SessionSettings.Get(GameKey, Game);
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit: Steam invite accepted (ok %d, squad '%s', game '%s')"), bOk, *Got, *Game);
	if (!bOk || !Invite.IsValid() || Game != TEXT("HomicideUnit"))
	{
		State = EHUSquadState::Failed;
		Status = TEXT("That invite has expired — ask for the squad code instead.");
		return;
	}
	StopWorldwide();
	Code = Got;
	bHost = false;
	IOnlineSessionPtr S = Sessions();
	if (S && S->GetNamedSession(NAME_GameSession))
	{
		// still in a squad (or hosting one): close it first, then join
		PendingJoin = MakeShared<FOnlineSessionSearchResult>(Invite);
		State = EHUSquadState::Joining;
		Status = FString::Printf(TEXT("Leaving your squad to join %s..."), *Code);
		DestroyH = S->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UHUSquadSubsystem::OnDestroyedThenJoin));
		S->DestroySession(NAME_GameSession);
		return;
	}
	JoinResult(Invite);
}

void UHUSquadSubsystem::OnDestroyedThenJoin(FName Session, bool bOk)
{
	if (IOnlineSessionPtr S = Sessions()) S->ClearOnDestroySessionCompleteDelegate_Handle(DestroyH);
	if (!PendingJoin.IsValid()) return;
	const FOnlineSessionSearchResult R = *PendingJoin;
	PendingJoin.Reset();
	JoinResult(R);
}

// ---------- the worldwide search ----------
// Unreal's Steam lobby search hard-codes Steam's "default" distance (your region and the ones beside it), with no
// setting to change it. This asks Steam's lobby list directly, worldwide, for the code, then connects the way Unreal's
// own join does (steam.<host's Steam ID>:<port>, through Steam's relays). It doesn't take a seat in the Steam lobby
// (Unreal's Steam module owns lobby membership), so it checks for a full squad itself.
bool UHUSquadSubsystem::SearchWorldwide()
{
#if HU_STEAM
	ISteamMatchmaking* MM = SteamMatchmaking();
	if (!MM || !SteamUtils()) return false;
	const FString Key = CodeKey.ToString() + TEXT("_s");   // how Unreal names a string setting on a Steam lobby
	MM->AddRequestLobbyListStringFilter(TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Code), k_ELobbyComparisonEqual);
	MM->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterWorldwide);
	MM->AddRequestLobbyListResultCountFilter(20);
	WorldCall = MM->RequestLobbyList();
	if (WorldCall == k_uAPICallInvalid) { WorldCall = 0; return false; }
	WorldStarted = FPlatformTime::Seconds();
	Status = FString::Printf(TEXT("Paging squad %s worldwide on Steam..."), *Code);
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit: squad %s isn't in Steam's nearby list, asking worldwide"), *Code);
	WorldTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UHUSquadSubsystem::PollWorldwide), 0.1f);
	return true;
#else
	return false;
#endif
}

bool UHUSquadSubsystem::PollWorldwide(float Dt)
{
#if HU_STEAM
	ISteamUtils* SU = SteamUtils();
	ISteamMatchmaking* MM = SteamMatchmaking();
	bool bFailed = false;
	if (SU && MM && !SU->IsAPICallCompleted(WorldCall, &bFailed) && FPlatformTime::Seconds() - WorldStarted < 15.0) return true;
	LobbyMatchList_t List = {};
	const bool bGot = SU && MM && !bFailed && SU->GetAPICallResult(WorldCall, &List, sizeof(List), LobbyMatchList_t::k_iCallback, &bFailed) && !bFailed;
	WorldTick.Reset();   // returning false ends this ticker
	WorldCall = 0;
	if (State != EHUSquadState::Searching) return false;   // left, or an invite came in meanwhile
	UE_LOG(LogTemp, Log, TEXT("HomicideUnit: worldwide search for squad %s: %s, %d lobbies"), *Code, bGot ? TEXT("answered") : TEXT("no answer"), bGot ? (int32)List.m_nLobbiesMatching : 0);
	FString Why = FString::Printf(TEXT("No squad with code %s answered, anywhere on Steam. Check the code — and that the host is still in the squad room."), *Code);
	const FString CodeSteamKey = CodeKey.ToString() + TEXT("_s"), GameSteamKey = GameKey.ToString() + TEXT("_s");
	for (uint32 i = 0; bGot && i < List.m_nLobbiesMatching; ++i)
	{
		const CSteamID L = MM->GetLobbyByIndex(i);
		const FString Got = UTF8_TO_TCHAR(MM->GetLobbyData(L, TCHAR_TO_UTF8(*CodeSteamKey)));
		const FString Game = UTF8_TO_TCHAR(MM->GetLobbyData(L, TCHAR_TO_UTF8(*GameSteamKey)));
		if (Got != Code || Game != TEXT("HomicideUnit")) continue;
		if (FCString::Atoi(UTF8_TO_TCHAR(MM->GetLobbyData(L, "BUILDID"))) != GetBuildUniqueId())
		{
			Why = FString::Printf(TEXT("Squad %s is playing a different version of the game. Both of you need the same build."), *Code);
			continue;
		}
		const int32 Limit = MM->GetLobbyMemberLimit(L);
		if (Limit > 0 && MM->GetNumLobbyMembers(L) >= Limit) { Why = TEXT("That squad is full."); continue; }
		FString Addr = UTF8_TO_TCHAR(MM->GetLobbyData(L, "P2PADDR"));
		Addr.RemoveFromStart(TEXT("steam."));
		uint64 Host = FCString::Strtoui64(*Addr, nullptr, 10);
		if (!Host) Host = MM->GetLobbyOwner(L).ConvertToUint64();
		int32 Port = FCString::Atoi(UTF8_TO_TCHAR(MM->GetLobbyData(L, "P2PPORT")));
		if (Port <= 0) Port = 7777;
		APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
		if (!Host || !PC) continue;
		State = EHUSquadState::InSquad;
		Status = FString::Printf(TEXT("In squad %s."), *Code);
		if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) C->UseSquadSeed(FCString::Atoi(*Code));
		const FString Url = FString::Printf(TEXT("steam.%llu:%d"), Host, Port);
		UE_LOG(LogTemp, Log, TEXT("HomicideUnit: squad %s found worldwide, travelling to %s"), *Code, *Url);
		PC->ClientTravel(Url + TEXT("?") + TravelOptions(), TRAVEL_Absolute);
		return false;
	}
	State = EHUSquadState::Failed;
	Status = Why;
	return false;
#else
	return false;
#endif
}

void UHUSquadSubsystem::StopWorldwide()
{
	if (WorldTick.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(WorldTick);
	WorldTick.Reset();
	WorldCall = 0;
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
	StopWorldwide();
	PendingJoin.Reset();
	if (IOnlineSessionPtr S = Sessions()) if (S->GetNamedSession(NAME_GameSession)) S->DestroySession(NAME_GameSession);
	Code.Reset();
	bHost = false;
	State = EHUSquadState::None;
	Status.Reset();
	if (UHUCaseSubsystem* C = GetGameInstance()->GetSubsystem<UHUCaseSubsystem>()) C->UseSquadSeed(0);
}
