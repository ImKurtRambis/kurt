using UnrealBuildTool;

public class HomicideUnit : ModuleRules
{
	public HomicideUnit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// compile each file on its own: files define helpers with short names (Red, First...) that clash when batched together
		bUseUnity = false;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "JsonUtilities", "UMG", "Slate", "SlateCore", "ApplicationCore", "OnlineSubsystem", "OnlineSubsystemUtils", "TextToSpeech", "HTTP", "AudioMixer", "AudioCaptureCore", "WebSockets" });
		// the squad code is also asked of Steam directly, worldwide (Unreal's Steam search stops at your region):
		// HUSquadSubsystem. The DLL is delay-loaded and only touched when Unreal's Steam module is running.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
			PrivateDefinitions.Add("HU_STEAM=1");
		}
		else PrivateDefinitions.Add("HU_STEAM=0");
	}
}
