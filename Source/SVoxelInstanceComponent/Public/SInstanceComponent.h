// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "UObject/ObjectMacros.h"
#include "Interface_CollisionDataProviderCore.h"
#include "Components/MeshComponent.h"
#include "RenderGraphResources.h"
#include "SDispatchCS.h"
#include "PhysicsEngine/ConvexElem.h"
#include "Engine/StaticMesh.h"
#include "SInstanceComponent.generated.h"


UCLASS()
class SVOXELINSTANCECOMPONENT_API USInstanceComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_BODY()
	
public:
	USInstanceComponent();
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual int32 GetNumMaterials() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:

	UPROPERTY(EditAnywhere, Category = Rendering)
	UStaticMesh* StaticMesh = nullptr;

public:
	UPROPERTY(EditAnywhere, Category = Rendering)
	int LODIndex = 0;
	
    UStaticMesh* GetStaticMesh() const { return StaticMesh; }
private:
	UPROPERTY()
	FBoxSphereBounds LocalBounds;
	
	void UpdateLocalBounds();
	
	friend class FSInstanceSceneProxy;
};


