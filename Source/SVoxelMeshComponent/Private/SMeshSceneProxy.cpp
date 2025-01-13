#include "SMeshSceneProxy.h"

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
#include "SMeshComponent.h"
#include "SMeshVertexFactory.h"
#include "StaticMeshSceneProxy.h"
#include "Elements/Framework/TypedElementUtil.h"
#include "Misc/LowLevelTestAdapter.h"
#include "Materials/MaterialRenderProxy.h"


FSMeshSceneProxy::FSMeshSceneProxy(USMeshComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	InitDispatchCSOutput = Component->InitDispatchCSOutput;
	
	Material = Component->GetMaterial(0);
    if (Material == NULL)
    {
    	Material = UMaterial::GetDefaultMaterial(MD_Surface);
    }
    
    bCastShadow = Component->CastShadow;
}


void FSMeshSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	FSIndirectInstancingParameters UniformParams;
	
	VertexFactory = new FSMeshVertexFactory(GetScene().GetFeatureLevel(), UniformParams);
	VertexFactory->InitDispatchCSOutput = InitDispatchCSOutput;
	VertexFactory->VertexBufferSRV = RHICmdList.CreateShaderResourceView(InitDispatchCSOutput.OutputVertices->GetRHI());
	VertexFactory->NormalBufferSRV = RHICmdList.CreateShaderResourceView(InitDispatchCSOutput.OutNormals->GetRHI());
	VertexFactory->ColorBufferSRV = RHICmdList.CreateShaderResourceView(InitDispatchCSOutput.OutColor->GetRHI());
	
	VertexFactory->InitResource(FRHICommandListImmediate::Get());
}

void FSMeshSceneProxy::DestroyRenderThreadResources()
{
	if (VertexFactory)
	{
		VertexFactory->ReleaseResource();
		delete VertexFactory;
		VertexFactory = nullptr;
	}
	InitDispatchCSOutput.ReleaseDispatch();
}

void FSMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
	FColoredMaterialRenderProxy* WireframeMaterialInstance = NULL;
	if (bWireframe)
	{
		WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
			FLinearColor(0, 0.5f, 1.f)
			);
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	}
	FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Material->GetRenderProxy();
	
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			
			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];

			BatchElement.IndexBuffer = VertexFactory->IndexBuffer;
			Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
			Mesh.VertexFactory = VertexFactory;
			Mesh.MaterialRenderProxy = MaterialProxy;
			Mesh.CastShadow = bCastShadow;
			Mesh.CastRayTracedShadow = false;
			
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
		
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = InitDispatchCSOutput.NumIndices/3;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = InitDispatchCSOutput.NumVertices-1;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = false;
		
			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}

void FSMeshSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	PDI->ReserveMemoryForMeshes(1);
	
    FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();
	
	FMeshBatch Mesh;
	FMeshBatchElement& BatchElement = Mesh.Elements[0];

	BatchElement.IndexBuffer = VertexFactory->IndexBuffer;
	Mesh.bWireframe = false;
	Mesh.VertexFactory = VertexFactory;
	Mesh.MaterialRenderProxy = MaterialProxy;
	Mesh.CastShadow = bCastShadow;
	Mesh.CastRayTracedShadow = false;
	
	BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = InitDispatchCSOutput.NumIndices/3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = InitDispatchCSOutput.NumVertices-1;
	Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Mesh.Type = PT_TriangleList;
	Mesh.DepthPriorityGroup = SDPG_World;
	Mesh.bCanApplyViewModeOverrides = false;

	Mesh.MeshIdInPrimitive = 0;
	Mesh.LODIndex = 0;
	Mesh.SegmentIndex = 0;
	
	PDI->DrawMesh(Mesh, FLT_MAX);
}

FPrimitiveViewRelevance FSMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View) && bCastShadow;
	Result.bDynamicRelevance = false;
	Result.bStaticRelevance = true;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = false;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = IsMovable() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

SIZE_T FSMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FSMeshSceneProxy::GetMemoryFootprint() const
{
	return(sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize());
}

bool FSMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}
	
