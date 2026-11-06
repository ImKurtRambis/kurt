using UnrealBuildTool;

public class HomicideUnit : ModuleRules
{
	public HomicideUnit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// compile each file on its own: files define helpers with short names (Red, First...) that clash when batched together
		bUseUnity = false;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "JsonUtilities", "UMG", "Slate", "SlateCore", "ApplicationCore", "OnlineSubsystem", "OnlineSubsystemUtils", "TextToSpeech", "HTTP", "AudioMixer", "AudioCaptureCore" });
	}
}
