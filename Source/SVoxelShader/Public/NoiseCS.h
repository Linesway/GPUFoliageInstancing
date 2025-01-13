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

struct SVOXELSHADER_API FNoiseCSDispatchParams
{
	FIntVector3 WorldSize;
	int Size;
	FVector3f Position;
	int LOD;
	int Scale;

	int seed;
};

struct SVOXELSHADER_API FNoiseCSOutput
{
	TRefCountPtr<FRDGPooledBuffer> OutVoxels;
};

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class SVOXELSHADER_API FNoiseCS : public FGlobalShader
{
public:
	
	DECLARE_GLOBAL_SHADER(FNoiseCS);
	SHADER_USE_PARAMETER_STRUCT(FNoiseCS, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector3, WorldSize)
		SHADER_PARAMETER(int, Size)
		SHADER_PARAMETER(FVector3f, Position)
		SHADER_PARAMETER(int, LOD)
		SHADER_PARAMETER(int, Scale)
	
		SHADER_PARAMETER(int, seed)
	
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, OutVoxels)

	END_SHADER_PARAMETER_STRUCT()
};

// This is a public interface that we define so outside code can invoke our compute shader.
class SVOXELSHADER_API FNoiseCSInterface {
public:

	// Executes this shader on the render thread
	static void DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FNoiseCSDispatchParams Params,
		TFunction<void(FNoiseCSOutput Output)> AsyncCallback
	);
};


