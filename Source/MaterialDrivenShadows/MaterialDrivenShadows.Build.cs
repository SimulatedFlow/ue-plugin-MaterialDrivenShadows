// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class MaterialDrivenShadows : ModuleRules
{
	public MaterialDrivenShadows(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RenderCore",
			"RHI",
			"DeveloperSettings",
			"Projects",
		});
	}
}
