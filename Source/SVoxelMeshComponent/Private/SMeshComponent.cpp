// Fill out your copyright notice in the Description page of Project Settings.


#include "SMeshComponent.h"

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
#include "SMeshSceneProxy.h"

USMeshComponent::USMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USMeshComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	InitDispatchCSOutput.ReleaseDispatch();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

FPrimitiveSceneProxy* USMeshComponent::CreateSceneProxy()
{
	if(InitDispatchCSOutput.OutputTris && InitDispatchCSOutput.OutputVertices)
		return new FSMeshSceneProxy(this);
	
	return nullptr;
}

UBodySetup* USMeshComponent::GetBodySetup()
{
	CreateProcMeshBodySetup();
    return ProcMeshBodySetup;
}

int32 USMeshComponent::GetNumMaterials() const
{
	return 1;
}

FBoxSphereBounds USMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds Ret(LocalBounds.TransformBy(LocalToWorld));

	Ret.BoxExtent *= BoundsScale;
	Ret.SphereRadius *= BoundsScale;

	return Ret;
}

void USMeshComponent::CreateMeshSection(
	FSDispatchCSOutput InInitDispatchOutput,
	UMaterialInterface* InMaterial,
	float Size, int LOD, int Scale,
	bool bCollisionEnabled, FName CollisionProfileName)
{
	InitDispatchCSOutput = InInitDispatchOutput;
	
	SetMaterial(0, InMaterial);

	FVector Min = FVector(0.0f);
	FVector Max = FVector(Size * 100 * (1 << LOD) * Scale);

	UpdateLocalBounds(FBoxSphereBounds(FBox(Min, Max)));

	if(bCollisionEnabled && LOD == 0)
	{
		SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SetCollisionProfileName(CollisionProfileName);
		UpdateCollision();
	}
	
	MarkRenderStateDirty(); // New section requires recreating scene proxy
}

void USMeshComponent::UpdateLocalBounds(FBoxSphereBounds NewBounds)
{
	LocalBounds = NewBounds;
	
	UpdateBounds();
	MarkRenderTransformDirty();
}

void USMeshComponent::CreateProcMeshBodySetup()
{
	if (ProcMeshBodySetup == nullptr)
	{
		ProcMeshBodySetup = CreateBodySetupHelper();
	}
}

void USMeshComponent::UpdateCollision()
{
	// Abort all previous ones still standing
	for (UBodySetup* OldBody : AsyncBodySetupQueue)
	{
		OldBody->AbortPhysicsMeshAsyncCreation();
	}
	AsyncBodySetupQueue.Add(CreateBodySetupHelper());

	UBodySetup* UseBodySetup = AsyncBodySetupQueue.Last();
	UseBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
	UseBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &USMeshComponent::FinishPhysicsAsyncCook, UseBodySetup));
}

void USMeshComponent::FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup)
{
	TArray<UBodySetup*> NewQueue;
	NewQueue.Reserve(AsyncBodySetupQueue.Num());

	int32 FoundIdx;
	if(AsyncBodySetupQueue.Find(FinishedBodySetup, FoundIdx))
	{
		if (bSuccess)
		{
			//The new body was found in the array meaning it's newer so use it
			ProcMeshBodySetup = FinishedBodySetup;
			RecreatePhysicsState();

			//remove any async body setups that were requested before this one
			for (int32 AsyncIdx = FoundIdx + 1; AsyncIdx < AsyncBodySetupQueue.Num(); ++AsyncIdx)
			{
				NewQueue.Add(AsyncBodySetupQueue[AsyncIdx]);
			}

			AsyncBodySetupQueue = NewQueue;
		}
		else
		{
			AsyncBodySetupQueue.RemoveAt(FoundIdx);
		}
	}
}

UBodySetup* USMeshComponent::CreateBodySetupHelper()
{
	// The body setup in a template needs to be public since the property is Tnstanced and thus is the archetype of the instance meaning there is a direct reference
    UBodySetup* NewBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public | RF_ArchetypeObject : RF_NoFlags));
    NewBodySetup->BodySetupGuid = FGuid::NewGuid();

    NewBodySetup->bGenerateMirroredCollision = false;
    NewBodySetup->bDoubleSidedGeometry = true;
    NewBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

    return NewBodySetup;
}

bool USMeshComponent::GetTriMeshSizeEstimates(FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
{
	OutTriMeshEstimates.VerticeCount = InitDispatchCSOutput.Vertices.Num();
	return true;
}

bool USMeshComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	CollisionData->Vertices = InitDispatchCSOutput.Vertices;
	CollisionData->Indices = InitDispatchCSOutput.Indices;
	
	CollisionData->bFlipNormals = true;
	CollisionData->bDeformableMesh = false;
	CollisionData->bFastCook = false;

	return true;
}

bool USMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	if(InitDispatchCSOutput.Indices.Num() >= 3)
		return true;
	
	return false;
}

