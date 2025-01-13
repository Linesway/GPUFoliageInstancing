// Fill out your copyright notice in the Description page of Project Settings.


#include "SInstanceComponent.h"

#include "BodySetupEnums.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "Engine/Engine.h"
#include "RenderUtils.h"
#include "SceneManagement.h"
#include "PhysicsEngine/BodySetup.h"
#include "DynamicMeshBuilder.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "SceneInterface.h"
#include "StaticMeshResources.h"
#include "RayTracingInstance.h"
#include "MeshMaterialShader.h"
#include "MeshPassProcessor.h"
#include "RenderGraphResources.h"
#include "SInstanceSceneProxy.h"

USInstanceComponent::USInstanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USInstanceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USInstanceComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

FPrimitiveSceneProxy* USInstanceComponent::CreateSceneProxy()
{
	return new FSInstanceSceneProxy(this);
}

int32 USInstanceComponent::GetNumMaterials() const
{
	return 1;
}

FBoxSphereBounds USInstanceComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FBox(FVector(-10000.0f), FVector(10000.f))).TransformBy(LocalToWorld);
}

void USInstanceComponent::UpdateLocalBounds()
{
	LocalBounds = FBox(ForceInit);
	UpdateBounds();
	MarkRenderTransformDirty();
}
