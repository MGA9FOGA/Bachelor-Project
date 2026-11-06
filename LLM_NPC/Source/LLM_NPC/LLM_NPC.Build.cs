// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LLM_NPC : ModuleRules
{
	public LLM_NPC(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule", "NavigationSystem", "PhysicsCore" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"HTTP",
			"Json",
			"JsonUtilities",
			"UMG",
			"Slate",
			"SlateCore"
		});
	}
}