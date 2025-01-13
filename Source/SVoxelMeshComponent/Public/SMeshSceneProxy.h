#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "SDispatchCS.h"
#include "SMeshVertexFactory.h"

class USMeshComponent;

class FSMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
	
	FSMeshSceneProxy(USMeshComponent* Component);
	
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual bool CanBeOccluded() const override;
	
	
	
private:
	FSDispatchCSOutput InitDispatchCSOutput;
	
	FSMeshVertexFactory* VertexFactory;
	
	UMaterialInterface* Material;
	FMaterialRelevance MaterialRelevance;
	
	bool bCastShadow;
};

