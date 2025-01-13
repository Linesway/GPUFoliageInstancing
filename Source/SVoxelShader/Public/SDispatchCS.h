#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "RenderGraphResources.h"

struct SVOXELSHADER_API FSDispatchCSParams
{
	FIntVector3 WorldSize;
	int Size;
	float isolevel;
	FVector3f Position;
	int LOD;
	int Scale;

	int seed;
};

struct SVOXELSHADER_API FSDispatchCSOutput
{
	TRefCountPtr<FRDGPooledBuffer>  OutputVertices;
	TRefCountPtr<FRDGPooledBuffer>  OutputTris;
	TRefCountPtr<FRDGPooledBuffer>  OutNormals;
	TRefCountPtr<FRDGPooledBuffer>  OutColor;

	int NumVertices;
	int NumIndices;

	TArray<FVector3f> Vertices;
	TArray<FTriIndices> Indices;

	void ReleaseDispatch()
	{
		OutputVertices.SafeRelease();
		OutputTris.SafeRelease();
		OutNormals.SafeRelease();
		OutColor.SafeRelease();
		Vertices.Reset();
		Indices.Reset();
	}
};

// This is a public interface that we define so outside code can invoke our compute shader.
class SVOXELSHADER_API FSDispatchCSInterface {
public:
	
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FSDispatchCSParams Params,
		TFunction<void(FSDispatchCSOutput Output)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FSDispatchCSParams Params,
		TFunction<void(FSDispatchCSOutput Output)> AsyncCallback
	)
	{
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
	}

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FSDispatchCSParams Params,
		TFunction<void(FSDispatchCSOutput Output)> AsyncCallback
	)
	{
		if (IsInRenderingThread())
		{
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}else{
			DispatchGameThread(Params, AsyncCallback);
		}
	}
};


