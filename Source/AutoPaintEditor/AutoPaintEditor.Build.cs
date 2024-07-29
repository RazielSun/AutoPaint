using UnrealBuildTool;

public class AutoPaintEditor : ModuleRules
{
    public AutoPaintEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "AutoPaint",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "AssetTools",
                "Slate",
                "SlateCore",
                "UnrealEd",
                "AdvancedPreviewScene",
                "RHI",
                "RenderCore"
            }
        );
    }
}