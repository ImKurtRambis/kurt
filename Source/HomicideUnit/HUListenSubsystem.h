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
	void StopListening(FHUHeard Done);

private:
	TUniquePtr<Audio::FAudioCapture> Capture;
	FCriticalSection Lock;
	TArray<float> Samples;
	int32 CaptureRate = 48000;
	bool bListening = false, bWorking = false;
	float MicLevel = 0.f;
	int32 Counter = 0;
	FString Dir() const;
};
