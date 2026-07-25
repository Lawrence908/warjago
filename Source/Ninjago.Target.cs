// Copyright Chris Lawrence. Personal project, not for distribution.

using UnrealBuildTool;
using System.Collections.Generic;

public class NinjagoTarget : TargetRules
{
	public NinjagoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("Ninjago");
	}
}
