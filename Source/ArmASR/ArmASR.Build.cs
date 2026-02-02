//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

using UnrealBuildTool;
using System.IO;
public class ArmASR : ModuleRules
{
	public ArmASR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.Combine(EngineDirectory,"Source/Runtime/Renderer/Private"),
				Path.Combine(PluginDirectory,"Shaders/Private/fsr2"),
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings"
				// ... add other public dependencies that you statically link with here ...
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Renderer",
				"RenderCore",
				"Projects",
				"RHI"
				// ... add private dependencies that you statically link with here ...
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows)
			|| Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "VulkanRHI");
		}
	}
}
