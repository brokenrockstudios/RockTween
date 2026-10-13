// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

// Automation tests for RockTween. UncookedOnly in the .uplugin: not built for the staged game, so CQTest never leaks into a shipped target.
public class RockTweenTests : ModuleRules
{
	public RockTweenTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"CQTest",
				"RockTween",
			}
		);
	}
}
