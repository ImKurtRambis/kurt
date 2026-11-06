using UnrealBuildTool;

public class HomicideUnit : ModuleRules
{
	public HomicideUnit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "JsonUtilities", "UMG", "Slate", "SlateCore", "ApplicationCore", "OnlineSubsystem", "OnlineSubsystemUtils", "TextToSpeech", "HTTP", "AudioMixer" });
	}
}
