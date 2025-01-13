using UnrealBuildTool;

public class SVoxelShader : ModuleRules
{
    public SVoxelShader(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("TargetPlatform");
        }
        
        PublicDependencyModuleNames.Add("Core");
        PublicDependencyModuleNames.Add("Engine");

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Renderer",
            "RenderCore",
            "RHI",
            "Projects"
        });
        
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "UnrealEd",
                "MaterialUtilities",
                "SlateCore",
                "Slate"
            }
        );
    }
}