#include "SInstanceSceneProxy.h"

#include "CommonRenderResources.h"
#include "EngineModule.h"
#include "Engine/Engine.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "RenderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "MeshPassProcessor.h"
#include "PrimitiveSceneInfo.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "SInstanceComponent.h"
#include "SInstanceVertexFactory.h"
#include "StaticMeshSceneProxy.h"
#include "Elements/Framework/TypedElementUtil.h"
#include "Misc/LowLevelTestAdapter.h"
#include "Materials/MaterialRenderProxy.h"
#include "SInstanceMesh.h"
#include "SInstanceRendererExtension.h"

TGlobalResource<FSInstanceRendererExtension> SInstanceRendererExtension;

// This is the max number of instances that can be stored and rendered, attempting to add instances past this will overwrite the oldest instance
FSInstanceSceneProxy::FSInstanceSceneProxy(USInstanceComponent* Component)
	: FPrimitiveSceneProxy(Component, "SIndirectInstancing")
	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, AddInstancesNextFrame(true)
	
{
	bIsMeshValid = true;

	SInstanceRendererExtension.RegisterExtension();

	if (!IsValid(Component->GetStaticMesh()))
	{
		bIsMeshValid = false;
		return;
	}

	LODIndex = Component->LODIndex;

	UMaterialInterface* ComponentMaterial = Component->GetMaterial(0);
	LocalStaticMesh = Component->GetStaticMesh();
	RenderData = LocalStaticMesh->GetRenderData();

	if (!(RenderData && RenderData->LODResources.Num() > 0))
	{
		RenderData = nullptr;
		bIsMeshValid = false;
		return;
	}

	const bool bValidMaterial = ComponentMaterial != nullptr && ComponentMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_VirtualHeightfieldMesh);
	Material = bValidMaterial ? ComponentMaterial->GetRenderProxy() : UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	MaterialRelevance = Material->GetMaterialInterface()->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
}

void FSInstanceSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	if (!bIsMeshValid)
		return;

	VertexFactory = new FSInstanceVertexFactory(GetScene().GetFeatureLevel());
	VertexFactory->InitResource(FRHICommandListImmediate::Get());
	
	const FStaticMeshLODResources &LODModel = RenderData->LODResources[LODIndex];
	LODModel.VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, StaticMeshData);
	LODModel.VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, StaticMeshData);
	LODModel.VertexBuffers.StaticMeshVertexBuffer.BindTexCoordVertexBuffer(VertexFactory, StaticMeshData, MAX_TEXCOORDS);
	LODModel.VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(VertexFactory, StaticMeshData);
	VertexFactory->SetData(RHICmdList, StaticMeshData);
	
	FSInstanceUniformParameters Params;
	const int32 NumTexCoords = StaticMeshData.NumTexCoords;
	const int32 ColorIndexMask = StaticMeshData.ColorIndexMask;
	
	Params.VertexFetch_Parameters = {ColorIndexMask, NumTexCoords, INDEX_NONE, INDEX_NONE};
	Params.VertexFetch_TexCoordBuffer = StaticMeshData.TextureCoordinatesSRV;
	Params.VertexFetch_ColorComponentsBuffer = StaticMeshData.ColorComponentsSRV;
	
	VertexFactory->SetParameters(FSInstanceUniformBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame));
}

void FSInstanceSceneProxy::DestroyRenderThreadResources()
{
	if (RenderData)
	{
		RenderData = nullptr;
	}
	if(VertexFactory)
	{
		if(bIsMeshValid)
		{
			VertexFactory->ReleaseResource();
		}
	}
	BaseInstanceBuffer.SafeRelease();
	InstanceInfoBuffer.SafeRelease();
}

void FSInstanceSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	if (SInstanceRendererExtension.IsInFrame())
    {
    	// Can't add new work while bInFrame.
    	// In UE5 we need to AddWork()/SubmitWork() in two phases: InitViews() and InitViewsAfterPrepass()
    	// The main renderer hooks for that don't exist in UE5.0 and are only added in UE5.1
    	// That means that for UE5.0 we always hit this for shadow drawing and shadows will not be rendered.
    	// Not earlying out here can lead to crashes from buffers being released too soon.
    	return;
    }
	
	if (!bIsMeshValid)
	{
		return;
	}
	
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if(VisibilityMap & (1 << ViewIndex))
		{
			FSDrawInstanceBuffers &Buffers = SInstanceRendererExtension.AddWork(Collector.GetRHICommandList(),this, ViewFamily.Views[0], Views[ViewIndex]);
			
			FMeshBatch &Mesh = Collector.AllocateMesh();
			Mesh.CastShadow = true;
			Mesh.bUseAsOccluder = false;
			Mesh.VertexFactory = VertexFactory;
			Mesh.MaterialRenderProxy = Material;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.Type = PT_TriangleList;
			Mesh.bUseForDepthPass = true;

			Mesh.Elements.SetNumZeroed(1);
			FMeshBatchElement &BatchElement = Mesh.Elements[0];

			BatchElement.IndirectArgsBuffer = Buffers.IndirectArgsBuffer;
			BatchElement.IndirectArgsOffset = 0;

			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = 0;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = 0;
			BatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;

			FSInstanceUserData* UserData = &Collector.AllocateOneFrameResource<FSInstanceUserData>();
			BatchElement.UserData = (void *)UserData;
			UserData->InstanceBufferSRV = Buffers.InstanceBufferSRV;

			BatchElement.IndexBuffer = &RenderData->LODResources[LODIndex].IndexBuffer;

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}

FPrimitiveViewRelevance FSInstanceSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View) && ShouldRenderInMainPass();
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = true;
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = false;
	Result.bVelocityRelevance = false;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}

SIZE_T FSInstanceSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FSInstanceSceneProxy::GetMemoryFootprint() const
{
	return(sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize());
}

bool FSInstanceSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}
	
