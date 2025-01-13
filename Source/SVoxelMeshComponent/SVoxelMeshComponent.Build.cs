using UnrealBuildTool;

public class SVoxelMeshComponent : ModuleRules
{
    public SVoxelMeshComponent(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("TargetPlatform");
        }
        
        PublicDependencyModuleNames.Add("Core");
        PublicDependencyModuleNames.Add("Engine");
        PublicDependencyModuleNames.Add("RHI");
        PublicDependencyModuleNames.Add("SVoxelShader");

        PrivateDependencyModuleNames.AddRange(new string[]
            {
                "Projects",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
            }
        );
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "UnrealEd",
                "MaterialUtilities",
            }
        );
        PrivateDependencyModuleNames.AddRange(new string[]
            { 
                "RHI",
                "InteractiveToolsFramework",
                "MeshDescription",
                "RenderCore",
                "StaticMeshDescription"
            }
        );
    }
}