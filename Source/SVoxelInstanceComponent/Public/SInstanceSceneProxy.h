#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "SDispatchCS.h"
#include "SInstanceVertexFactory.h"

class USInstanceComponent;

class FSInstanceSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FSInstanceSceneProxy(USInstanceComponent* Component);

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
    virtual void DestroyRenderThreadResources() override;
	
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual bool CanBeOccluded() const override;
	
public:
	bool bIsMeshValid;
	
	UStaticMesh* LocalStaticMesh;
	FStaticMeshRenderData* RenderData;
	mutable FStaticMeshDataType StaticMeshData;
	
	FMaterialRenderProxy* Material;
	FMaterialRelevance MaterialRelevance;

	int InstanceCount;
	int LODIndex;

	FShaderResourceViewRHIRef OriginSRV;
	FShaderResourceViewRHIRef TransformSRV;

	FSInstanceVertexFactory* VertexFactory;

public:
	mutable std::atomic<bool> AddInstancesNextFrame;
	
	// Multi-frame buffers used to store the instance data.
    // This is initialized in RenderExtension::InitializeResources()
    mutable TRefCountPtr<FRDGPooledBuffer> BaseInstanceBuffer;
    mutable TRefCountPtr<FRDGPooledBuffer> InstanceInfoBuffer;
};

