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
	}
}
