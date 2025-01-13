#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "PixelShaderUtils.h"
#include "Runtime/RenderCore/Public/RenderGraphUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "RHIGPUReadback.h"
#include "Kismet/BlueprintAsyncActionBase.h"

struct SVOXELSHADER_API FMCAllocVertsCSDispatchParams
{
	int Size;
	TRefCountPtr<FRDGPooledBuffer> InCellMasks;
};

struct SVOXELSHADER_API FMCAllocVertsCSOutput
{
	TRefCountPtr<FRDGPooledBuffer> OutCellMasks;
	uint32_t NumAllocatedVerts;
};

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class SVOXELSHADER_API FMCAllocVertsCS: public FGlobalShader
{
public:
	
	DECLARE_GLOBAL_SHADER(FMCAllocVertsCS);
	SHADER_USE_PARAMETER_STRUCT(FMCAllocVertsCS, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(int, Size) 
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32_t>, cellMasks)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32_t>, NumAllocatedVerts)

	END_SHADER_PARAMETER_STRUCT()
};

// This is a public interface that we define so outside code can invoke our compute shader.
class SVOXELSHADER_API FMCAllocVertsCSInterface {
public:
	
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FMCAllocVertsCSDispatchParams Params,
		TFunction<void(FMCAllocVertsCSOutput Output)> AsyncCallback
	);
};


