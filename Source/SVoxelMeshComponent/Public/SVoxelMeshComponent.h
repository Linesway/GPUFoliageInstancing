#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "RenderGraphResources.h"

class FSVoxelMeshComponentModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
