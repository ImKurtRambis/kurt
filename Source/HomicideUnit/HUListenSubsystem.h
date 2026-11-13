// Speech to text, on this PC (free, offline): hold V in an interview, say one of the questions, let go.
// The microphone is recorded with Unreal's audio capture, then Moonshine (via sherpa-onnx, both open source)
// turns it into text. The game only uses the text to pick the closest preset question - it never asks
// anything you didn't choose.
//
// Files (not in git; see AI\README.txt):
//   AI\voice\bin\sherpa-onnx-offline.exe      (comes with the voice engine)
//   AI\stt\*.onnx + tokens.txt                 (Moonshine tiny English)
// Turned on/off in Settings ("Speak questions (mic)").
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioCaptureCore.h"
#include "HUListenSubsystem.generated.h"

DECLARE_DELEGATE_OneParam(FHUHeard, const FString& /*Text, empty if nothing understood*/);

UCLASS()
class HOMICIDEUNIT_API UHUListenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Listen") bool IsInstalled() const;
	// installed AND switched on in Settings
	UFUNCTION(BlueprintPure, Category = "Listen") bool IsEnabled() const;
	UFUNCTION(BlueprintPure, Category = "Listen") bool IsListening() const { return bListening; }
	UFUNCTION(BlueprintPure, Category = "Listen") bool IsWorking() const { return bWorking; }
	// how loud the mic is right now, 0..1 (for the HUD meter)
	UFUNCTION(BlueprintPure, Category = "Listen") float Level() const { return MicLevel; }

	bool StartListening();

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
};
