using UnrealBuildTool;
using System.Collections.Generic;

public class HomicideUnitTarget : TargetRules
{
	public HomicideUnitTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HomicideUnit");
		// stamp the build with its version from git (Tools/StampVersion.ps1 -> Source/HomicideUnit/HUBuildInfo.h)
		PreBuildSteps.Add("powershell -NoProfile -ExecutionPolicy Bypass -File \"$(ProjectDir)\\Tools\\StampVersion.ps1\" -Quiet");
	}
}
