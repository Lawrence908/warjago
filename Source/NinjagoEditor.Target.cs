// Copyright Chris Lawrence. Personal project, not for distribution.

using UnrealBuildTool;
using System.Collections.Generic;

public class NinjagoEditorTarget : TargetRules
{
	public NinjagoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("Ninjago");
	}
}
