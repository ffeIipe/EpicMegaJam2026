// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EpicMegaJam2026 : ModuleRules
{
	public EpicMegaJam2026(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "AIModule", "NavigationSystem", "GameplayTasks", "FMODStudio" });

		PublicIncludePaths.Add(ModuleDirectory); // allows "Core/..." and "Data/..." includes

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
