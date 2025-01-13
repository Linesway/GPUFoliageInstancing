#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "RenderGraphResources.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"

class FSVoxelShaderModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
