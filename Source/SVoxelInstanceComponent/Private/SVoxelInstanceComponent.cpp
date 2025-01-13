#include "SVoxelInstanceComponent.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "RHI.h"
#include "GlobalShader.h"
#include "RHICommandList.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FSVoxelInstanceComponentModule"

void FSVoxelInstanceComponentModule::StartupModule()
{
    // Maps virtual shader source directory to the plugin's actual shaders directory.
    // Virtual Shader Directory name has to be different than in other plugins etc.
    FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("SVoxelPlugin"))->GetBaseDir(), TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/InstanceShaders"), PluginShaderDir);
}

void FSVoxelInstanceComponentModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FSVoxelInstanceComponentModule, SVoxelInstanceComponent)