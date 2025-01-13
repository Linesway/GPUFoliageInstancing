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

struct SVOXELSHADER_API FMCCountVertsCSDispatchParams
{
	int Size;
	float isolevel;
		
	TRefCountPtr<FRDGPooledBuffer> InVoxels;
};

struct SVOXELSHADER_API FMCCountVertsCSOutput
{
	TRefCountPtr<FRDGPooledBuffer> OutCellMasks;
	uint32_t IndicesCount;
};

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class SVOXELSHADER_API FMCCountVertsCS: public FGlobalShader
{
public:
	
	DECLARE_GLOBAL_SHADER(FMCCountVertsCS);
	SHADER_USE_PARAMETER_STRUCT(FMCCountVertsCS, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, Size) 
		SHADER_PARAMETER(float, isolevel) 
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InVoxels)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32_t>, cellMasks)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32_t>, VertexCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32_t>, IndicesCount)
	END_SHADER_PARAMETER_STRUCT()
};

// This is a public interface that we define so outside code can invoke our compute shader.
class SVOXELSHADER_API FMCCountVertsCSInterface {
public:
	
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList, FMCCountVertsCSDispatchParams Params,
		TFunction<void(FMCCountVertsCSOutput Output)> AsyncCallback
	);
};


