// Squads: playing together with a 6-digit code.
//   HostSquad()      - makes a squad with a fresh code and opens the squad room (the pre-lobby) as a listen server.
//   JoinByCode(code) - finds the squad with that code and travels into its squad room.
//   StartShift()     - host only: everyone in the squad room goes to the briefing together.
//   LeaveSquad()     - ends / leaves the squad (going back to the desk does this).
// Uses Unreal's online sessions: over Steam when Steam is running (anyone, anywhere), otherwise on the local
// network (same Wi-Fi / same PC). The code is stored on the session, and joining searches for it.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "HUSquadSubsystem.generated.h"

UENUM(BlueprintType)
enum class EHUSquadState : uint8 { None, Hosting, Searching, Joining, InSquad, Failed };

UCLASS()
class HOMICIDEUNIT_API UHUSquadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad") FName LobbyLevel = TEXT("/Game/Maps/L_Lobby");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad") FName BriefingLevel = TEXT("/Game/Maps/L_Briefing");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad") int32 MaxPlayers = 4;

	UPROPERTY(BlueprintReadOnly, Category = "Squad") FString Code;          // the squad's 6 digits ("" when not in one)
	UPROPERTY(BlueprintReadOnly, Category = "Squad") EHUSquadState State = EHUSquadState::None;
	UPROPERTY(BlueprintReadOnly, Category = "Squad") FString Status;        // what's happening, for the menu
	UPROPERTY(BlueprintReadOnly, Category = "Squad") bool bHost = false;

	UFUNCTION(BlueprintCallable, Category = "Squad") void HostSquad();
	UFUNCTION(BlueprintCallable, Category = "Squad") void JoinByCode(const FString& SixDigits);
	UFUNCTION(BlueprintCallable, Category = "Squad") void StartShift();
	UFUNCTION(BlueprintCallable, Category = "Squad") void LeaveSquad();
	// "Steam" or "local network"
	UFUNCTION(BlueprintPure, Category = "Squad") FString NetworkName() const;

private:
	IOnlineSessionPtr Sessions() const;
	bool IsLan() const;
	FString TravelOptions() const;
	void CreateNow();
	void OnDestroyedThenCreate(FName Session, bool bOk);
	void OnCreated(FName Session, bool bOk);
	void OnFound(bool bOk);
	void OnJoined(FName Session, EOnJoinSessionCompleteResult::Type Result);
	TSharedPtr<class FOnlineSessionSearch> Search;
	FDelegateHandle CreateH, FindH, JoinH, DestroyH;
};
