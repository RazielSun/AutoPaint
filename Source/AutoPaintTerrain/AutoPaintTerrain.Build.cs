using UnrealBuildTool;

public class AutoPaintTerrain : ModuleRules
{
    public AutoPaintTerrain(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "LandscapePatch",
                "AutoPaint",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "AutoPaintShaders",
                "Landscape"
            }
        );
    }
}