// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

public class RockTween : ModuleRules
{
	public RockTween(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"RockCore",
				"Engine",
			}
		);
	}
}
