// Copyright Chris Lawrence. Personal project, not for distribution.

using UnrealBuildTool;

public class Ninjago : ModuleRules
{
	public Ninjago(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Core runtime dependencies. EnhancedInput and DeveloperSettings are declared
		// up front (used from M2/M5) so the module list stays stable across milestones.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
