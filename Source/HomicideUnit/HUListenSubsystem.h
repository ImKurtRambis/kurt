// Speech to text, on this PC (free, offline): hold V in an interview, talk, let go.
// The microphone is recorded with Unreal's audio capture, then sherpa-onnx (open source) turns it into text with
// the best model installed:
//   AI\stt-accurate\  NVIDIA Parakeet TDT 0.6B v2 (CC-BY-4.0): about half Moonshine's mistakes on accented speech
//                     and in noisy rooms. It takes ~2 s to load, so it runs in an always-on local server
//                     (sherpa-onnx-offline-websocket-server, started when an interview opens) and answers in ~0.3 s.
//   AI\stt\           Moonshine tiny English: the small fallback, one process per question.
//
// Files (not in git; see AI\README.txt):
//   AI\voice\bin\sherpa-onnx-offline*.exe     (comes with the voice engine)
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioCaptureCore.h"
#include "HAL/PlatformProcess.h"
#include "IWebSocket.h"
#include "TimerManager.h"
#include "HUListenSubsystem.generated.h"

DECLARE_DELEGATE_OneParam(FHUHeard, const FString& /*Text, empty if nothing understood*/);

UCLASS()
class HOMICIDEUNIT_API UHUListenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
#if !UE_BUILD_SHIPPING
	virtual void Initialize(FSubsystemCollectionBase& Collection) override; // -HUListenTest=<wav>
#endif
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Listen") bool IsInstalled() const;
	// installed AND switched on in Settings
	UFUNCTION(BlueprintPure, Category = "Listen") bool IsEnabled() const;
	UFUNCTION(BlueprintPure, Category = "Listen") bool IsListening() const { return bListening; }
	UFUNCTION(BlueprintPure, Category = "Listen") bool IsWorking() const { return bWorking; }
	// how loud the mic is right now, 0..1 (for the HUD meter)
	UFUNCTION(BlueprintPure, Category = "Listen") float Level() const { return MicLevel; }

	bool StartListening();
	// load the accurate model now and keep it loaded (an interview is opening); does nothing if it's already up
	void WarmUp();
	// which model hears the player, for the log
	FString ModelName() const;

	// ---- the microphone (Settings): this game is played by talking, so pick the right one ----
	// every input device Windows has (names); refreshed each call
	TArray<FString> MicNames();
	// the picked one's name, or "Windows default" - and step through them (saved)
	FString MicLabel();
	void CycleMic(int32 Dir);
	// hear the mic without recording (the level meter in Settings)
	bool StartMonitor();
	void StopMonitor();
	bool IsMonitoring() const { return bMonitoring; }
	void StopListening(FHUHeard Done);
	// AUTO-DETECT: listen to every microphone for a moment (in the background) and use one that actually picks
	// up sound. bAfterSilence: the current one just recorded nothing, so prefer another. Says what it picked.
	void AutoPickMic(bool bAfterSilence);
	bool LastWasSilent() const { return bLastSilent; }
	void RecheckMic() { bProbedOnce = false; AutoPickMic(false); } // HU.MicCheck

private:
	TUniquePtr<Audio::FAudioCapture> Capture;
	FCriticalSection Lock;
	TArray<float> Samples;
	int32 CaptureRate = 48000;
	bool bListening = false, bWorking = false, bMonitoring = false;
	bool OpenMic(); // opens the picked device (samples kept only while listening)
	int32 PickedDeviceIndex();
	float MicLevel = 0.f;
	bool bLastSilent = false, bProbing = false, bProbedOnce = false;
	void FinishProbe(const TArray<FString>& Names, const TArray<float>& Rms, const FString& DefaultName, bool bAfterSilence);
	void Tell(const FString& Body);
	int32 Counter = 0;
	FString Dir() const;

	// ---- which model, and how it's run ----
	bool HasAccurate() const;  // AI\stt-accurate holds a NeMo transducer (encoder/decoder/joiner + tokens.txt)
	bool HasFast() const;      // AI\stt holds Moonshine
	FString ModelArgs() const; // sherpa-onnx flags for the model in use
	// the always-on server for the accurate model, and our connection to it
	FProcHandle Server;
	TSharedPtr<IWebSocket> Socket;
	bool bSocketOpen = false, bConnecting = false;
	double ServerStartedAt = 0.0;
	static constexpr int32 ServerPort = 8096; // the brain uses 8089, the voices 8005 and 8090
	void Connect();
	void DropSocket();
	void OnReply(const FString& Msg);
	// a question waiting for the server's answer (one at a time)
	FHUHeard Pending;
	FString PendingFile;
	FTimerHandle PendingTimer, RetryTimer;
	// 16 kHz it, keep a copy in Saved/Voice, and hand it to the model (the server if it's up, else the slow path)
	void Hear(const TArray<float>& Mono, int32 Rate, FHUHeard Done);
	// the slow path: a fresh sherpa-onnx process for this one question (the model loads each time)
	void RunExe(const FString& File, FHUHeard Done);
	void Finish(const FString& Text, FHUHeard Done);
};
