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
#include "SMeshComponent.generated.h"


UCLASS()
class SVOXELMESHCOMPONENT_API USMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_BODY()
	
public:
	USMeshComponent();
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual class UBodySetup* GetBodySetup() override;
	virtual int32 GetNumMaterials() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	void CreateMeshSection(FSDispatchCSOutput InInitDispatchOutput,
		UMaterialInterface* InMaterial,
		float Size, int LOD, int Scale,
		bool bCollisionEnabled, FName CollisionProfileName);

private:
	FSDispatchCSOutput InitDispatchCSOutput;
	
	/** Local space bounds of mesh */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;
	
	UPROPERTY(transient)
	TArray<TObjectPtr<UBodySetup>> AsyncBodySetupQueue;
	
	UPROPERTY(Instanced)
	TObjectPtr<class UBodySetup> ProcMeshBodySetup;
	
	void UpdateLocalBounds(FBoxSphereBounds NewBounds);
	void CreateProcMeshBodySetup();
	void UpdateCollision();
	void FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup);

	friend class FSMeshSceneProxy;

public:
	UBodySetup* CreateBodySetupHelper();
	
	virtual bool GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const override;
    virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
    virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
    virtual bool WantsNegXTriMesh() override{ return false; }
};


