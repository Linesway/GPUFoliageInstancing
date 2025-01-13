#include "SInstanceRendererExtension.h"
#include "CommonRenderResources.h"
#include "EngineModule.h"
#include "Engine/Engine.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "RenderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Misc/LowLevelTestAdapter.h"
#include "SInstanceMesh.h"

#define MAX_INSTANCES 1024

void FSInstanceRendererExtension::RegisterExtension()
{
	if (!bInit)
	{
		GEngine->GetPreRenderDelegateEx().AddRaw(this, &FSInstanceRendererExtension::BeginFrame);
		GEngine->GetPostRenderDelegateEx().AddRaw(this, &FSInstanceRendererExtension::EndFrame);
		bInit = true;
	}
}

void FSInstanceRendererExtension::ReleaseRHI()
{
	Buffers.Empty();
}

FSDrawInstanceBuffers &FSInstanceRendererExtension::AddWork(FRHICommandListBase& RHICmdList, FSInstanceSceneProxy const *InProxy, FSceneView const *InMainView, FSceneView const *InCullView)
{
	// If we hit this then BeginFrame()/EndFrame() logic needs fixing in the Scene Renderer.
	if (!ensure(!bInFrame))
	{
		EndFrame();
	}

	// Create workload
	FWorkDesc WorkDesc;
	WorkDesc.ProxyIndex = SceneProxies.AddUnique(InProxy);
	WorkDesc.MainViewIndex = MainViews.AddUnique(InMainView);
	WorkDesc.CullViewIndex = CullViews.AddUnique(InCullView);
	WorkDesc.BufferIndex = -1;

	// Check for an existing duplicate
	for (FWorkDesc &It : WorkDescs)
	{
		if (It.ProxyIndex == WorkDesc.ProxyIndex && It.MainViewIndex == WorkDesc.MainViewIndex && It.CullViewIndex == WorkDesc.CullViewIndex && It.BufferIndex != -1)
		{
			WorkDesc.BufferIndex = It.BufferIndex;
			break;
		}
	}

	// Try to recycle a buffer
	if (WorkDesc.BufferIndex == -1)
	{
		for (int32 BufferIndex = 0; BufferIndex < Buffers.Num(); BufferIndex++)
		{
			if (DiscardIds[BufferIndex] < DiscardId)
			{
				DiscardIds[BufferIndex] = DiscardId;
				WorkDesc.BufferIndex = BufferIndex;
				WorkDescs.Add(WorkDesc);
				break;
			}
		}
	}

	// Allocate new buffer if necessary
	if (WorkDesc.BufferIndex == -1)
	{
		DiscardIds.Add(DiscardId);
		WorkDesc.BufferIndex = Buffers.AddDefaulted();
		WorkDescs.Add(WorkDesc);
		FSInstanceMesh::InitializeInstanceBuffers(RHICmdList, Buffers[WorkDesc.BufferIndex]);
	}

	return Buffers[WorkDesc.BufferIndex];
}

void FSInstanceRendererExtension::BeginFrame(FRDGBuilder &GraphBuilder)
{
	// If we hit this then BeginFrame()/EndFrame() logic needs fixing in the Scene Renderer.
	if (!ensure(!bInFrame))
	{
		EndFrame();
	}
	bInFrame = true;

	if (WorkDescs.Num() > 0)
	{
		SubmitWork(GraphBuilder);
	}
}

void FSInstanceRendererExtension::EndFrame()
{
	ensure(bInFrame);
	bInFrame = false;

	SceneProxies.Reset();
	MainViews.Reset();
	CullViews.Reset();
	WorkDescs.Reset();

	// Clean the buffer pool
	DiscardId++;

	for (int32 Index = 0; Index < DiscardIds.Num();)
	{
		if (DiscardId - DiscardIds[Index] > 4u)
		{
			FSInstanceMesh::ReleaseInstanceBuffers(Buffers[Index]);
			Buffers.RemoveAtSwap(Index);
			DiscardIds.RemoveAtSwap(Index);
		}
		else
		{
			++Index;
		}
	}
}

void FSInstanceRendererExtension::EndFrame(FRDGBuilder &GraphBuilder)
{
	EndFrame();
}


void FSInstanceRendererExtension::SubmitWork(FRDGBuilder &GraphBuilder)
{
	// Sort work so that we can batch by proxy/view
	WorkDescs.Sort(FWorkDescSort());

	// Add pass to transition all output buffers for writing
	TArray<int32, TInlineAllocator<8>> UsedBufferIndices;
	for (FWorkDesc WorkdDesc : WorkDescs)
	{
		UsedBufferIndices.Add(WorkdDesc.BufferIndex);
	}
	FSInstanceMesh::AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, true);

	// Add passes to initialize the output buffers
	for (FWorkDesc WorkDesc : WorkDescs)
	{
		int NumIndices = SceneProxies[WorkDesc.ProxyIndex]->RenderData->LODResources[0].IndexBuffer.GetNumIndices();
		FSInstanceMesh::AddPass_InitInstanceBuffer(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), Buffers[WorkDesc.BufferIndex], NumIndices);
	}

	// Iterate workloads and submit work
	const int32 NumWorkItems = WorkDescs.Num();
	int32 WorkIndex = 0;
	while (WorkIndex < NumWorkItems)
	{
		// Gather data per proxy
		FSInstanceSceneProxy const *Proxy = SceneProxies[WorkDescs[WorkIndex].ProxyIndex];

		FProxyDesc ProxyDesc;
		ProxyDesc.SceneProxy = Proxy;

		// 1 << CeilLogTwo takes a number and returns the next power of two. so: 53 -> 64, 80 -> 128, etc.
		ProxyDesc.MaxRenderItems = 1 << FMath::CeilLogTwo(MAX_INSTANCES);
		ProxyDesc.NumAddPassWavefronts = 16;

		while (WorkIndex < NumWorkItems && SceneProxies[WorkDescs[WorkIndex].ProxyIndex] == Proxy)
		{
			// Gather data per main view
			FSceneView const *MainView = MainViews[WorkDescs[WorkIndex].MainViewIndex];

			FViewData MainViewData;
			GetViewData(MainView, MainViewData);

			FMainViewDesc MainViewDesc;
			MainViewDesc.ViewDebug = MainView;

			// ViewOrigin and Frustum Planes are all converted to UV space for the shader.
			MainViewDesc.ViewOrigin = MainViewData.ViewOrigin;

			const int32 MainViewNumPlanes = FMath::Min(MainViewData.ViewFrustum.Planes.Num(), 5);
			for (int32 PlaneIndex = 0; PlaneIndex < MainViewNumPlanes; ++PlaneIndex)
			{
				FPlane Plane = MainViewData.ViewFrustum.Planes[PlaneIndex];
				Plane = Plane.TransformBy(Proxy->GetLocalToWorld().Inverse());
				MainViewDesc.Planes[PlaneIndex] = FVector4(-Plane.X, -Plane.Y, -Plane.Z, Plane.W);
			}
			for (int32 PlaneIndex = MainViewNumPlanes; PlaneIndex < 5; ++PlaneIndex)
			{
				MainViewDesc.Planes[PlaneIndex] = FPlane(0, 0, 0, 1); // Null plane won't cull anything
			}

			// Build volatile graph resources
			FVolatileResources VolatileResources;
			FSInstanceMesh::InitializeResources(GraphBuilder, ProxyDesc, MainViewDesc, VolatileResources);
			
			FSInstanceMesh::AddPass_InitBuffers(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), ProxyDesc, VolatileResources);

			while (WorkIndex < NumWorkItems && MainViews[WorkDescs[WorkIndex].MainViewIndex] == MainView)
			{
				// Gather data per child view
				FSceneView const *CullView = CullViews[WorkDescs[WorkIndex].CullViewIndex];
				FConvexVolume const *ShadowFrustum = CullView->GetDynamicMeshElementsShadowCullFrustum();
				FConvexVolume const &Frustum = ShadowFrustum != nullptr && ShadowFrustum->Planes.Num() > 0 ? *ShadowFrustum : CullView->ViewFrustum;
				const FVector PreShadowTranslation = ShadowFrustum != nullptr ? CullView->GetPreShadowTranslation() : FVector::ZeroVector;

				FChildViewDesc ChildViewDesc;
				ChildViewDesc.ViewDebug = MainView;
				ChildViewDesc.bIsMainView = CullView == MainView;

				const int32 ChildViewNumPlanes = FMath::Min(Frustum.Planes.Num(), 5);
				for (int32 PlaneIndex = 0; PlaneIndex < ChildViewNumPlanes; ++PlaneIndex)
				{
					FPlane Plane = Frustum.Planes[PlaneIndex];
					Plane = Plane.TransformBy(Proxy->GetLocalToWorld().Inverse());
					ChildViewDesc.Planes[PlaneIndex] = FVector4(-Plane.X, -Plane.Y, -Plane.Z, Plane.W);
				}
				for (int32 PlaneIndex = ChildViewNumPlanes; PlaneIndex < 5; ++PlaneIndex)
				{
					MainViewDesc.Planes[PlaneIndex] = FPlane(0, 0, 0, 1); // Null plane won't cull anything
				}

				// Build graph
				FSInstanceMesh::AddPass_CullInstances(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), ProxyDesc, VolatileResources, Buffers[WorkDescs[WorkIndex].BufferIndex], ChildViewDesc);

				WorkIndex++;
			}
		}
	}
	FSInstanceMesh::AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, false);
}
