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

struct SVOXELSHADER_API FMarchingCSDispatchParams
{
	FIntVector3 WorldSize;
	int Size;
	float isolevel;
	int LOD;
	int Scale;

	FVector3f Position;
	int seed;
	
	TRefCountPtr<FRDGPooledBuffer>  InVoxels;
	TRefCountPtr<FRDGPooledBuffer> InCellMasks;
	int VertexCount;
	int IndicesCount;
};

struct SVOXELSHADER_API FMarchingCSOutput
{
	TRefCountPtr<FRDGPooledBuffer>  OutputVertices;
	TRefCountPtr<FRDGPooledBuffer>  OutputTris;
	TRefCountPtr<FRDGPooledBuffer>  OutNormals;
	TRefCountPtr<FRDGPooledBuffer>  OutColor;

	//For Collision
	TArray<FVector3f> Vertices;
	TArray<FTriIndices> Indices;
};

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class SVOXELSHADER_API FMarchingCS : public FGlobalShader
{
public:
	
	DECLARE_GLOBAL_SHADER(FMarchingCS);
	SHADER_USE_PARAMETER_STRUCT(FMarchingCS, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(FIntVector3, WorldSize) 
		SHADER_PARAMETER(int, Size) 
	    SHADER_PARAMETER(float, isolevel) 
		SHADER_PARAMETER(int, LOD)
		SHADER_PARAMETER(int, Scale)
		SHADER_PARAMETER(FVector3f, Position)
		SHADER_PARAMETER(int, seed)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InVoxels)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32_t>, cellMasks)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32_t>, NumEmittedIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, OutVertices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, OutTris)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, OutNormals)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector4f>, OutColor)

	END_SHADER_PARAMETER_STRUCT()
};


// This is a public interface that we define so outside code can invoke our compute shader.
class SVOXELSHADER_API FMarchingCSInterface {
public:
	
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FMarchingCSDispatchParams Params,
		TFunction<void(FMarchingCSOutput)> AsyncCallback
	);
};


