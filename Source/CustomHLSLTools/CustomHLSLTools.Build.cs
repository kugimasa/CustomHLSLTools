using UnrealBuildTool;

public class CustomHLSLTools : ModuleRules
{
	public CustomHLSLTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DeveloperSettings",
				"DirectoryWatcher",
				"InputCore",
				"MaterialEditor",
				"Projects",
				"PropertyEditor",
				"RenderCore",
				"Slate",
				"SlateCore",
				"SourceCodeAccess",
				"UnrealEd",
			}
		);
	}
}
