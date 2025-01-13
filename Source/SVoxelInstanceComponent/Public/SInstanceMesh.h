#pragma once
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderParameterStruct.h"
#include "SInstanceSceneProxy.h"

struct FSDrawInstanceBuffers
{
	/* Culled instance buffer. */
	FBufferRHIRef InstanceBuffer;
	FUnorderedAccessViewRHIRef InstanceBufferUAV;
	FShaderResourceViewRHIRef InstanceBufferSRV;

	/* IndirectArgs buffer for final DrawInstancedIndirect. */
	FBufferRHIRef IndirectArgsBuffer;
	FUnorderedAccessViewRHIRef IndirectArgsBufferUAV;
};

static const int32 IndirectArgsByteOffset_FinalCull = 0;
static const int32 IndirectArgsByteSize = 4 * sizeof(uint32);

struct FSInstanceInfo
{
	uint32 Num = 0;
};
struct FSInstanceRenderInstance
{
	float Position[3];
};
struct FSInstanceMeshItem
{
	float Position[3];
	float Rotation[3];
	float Scale[3];
};

class FInitBuffers_CS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FInitBuffers_CS);
	SHADER_USE_PARAMETER_STRUCT(FInitBuffers_CS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSInstanceInfo>, RWInstanceInfo)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSInstanceMeshItem>, RWMeshBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWFeedbackBuffer)
	SHADER_PARAMETER(uint32, InstanceBufferSize)
	SHADER_PARAMETER(float, Time)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
	{
		return true;
	}
};
class FAddInstances_CS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAddInstances_CS);
	SHADER_USE_PARAMETER_STRUCT(FAddInstances_CS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER(FVector3f, ViewOrigin)
	SHADER_PARAMETER_ARRAY(FVector4f, FrustumPlanes, [5])
	SHADER_PARAMETER(uint32, NumToAdd)
	SHADER_PARAMETER(int32, Seed)
	SHADER_PARAMETER(uint32, InstanceBufferSize)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSInstanceInfo>, RWInstanceInfo)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSInstanceMeshItem>, RWBaseInstanceBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSInstanceMeshItem>, RWMeshBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
	{
		return true;
	}
};
class FInitInstanceBuffer_CS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FInitInstanceBuffer_CS);
	SHADER_USE_PARAMETER_STRUCT(FInitInstanceBuffer_CS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER(int32, NumIndices)
	SHADER_PARAMETER(uint32, InstanceBufferSize)
	SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
	{
		return true;
	}
};
class FCullInstances_CS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCullInstances_CS);
	SHADER_USE_PARAMETER_STRUCT(FCullInstances_CS, FGlobalShader);

	class FReuseCullDim : SHADER_PERMUTATION_BOOL("REUSE_CULL");

	using FPermutationDomain = TShaderPermutationDomain<FReuseCullDim>;

	static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
	{
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER_ARRAY(FVector4f, FrustumPlanes, [5])
	SHADER_PARAMETER(FVector3f, ViewOrigin)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FSInstanceMeshItem>, BaseInstanceBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FSInstanceMeshItem>, MeshBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, IndirectArgsBufferSRV)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSInstanceMeshItem>, RWBaseInstanceBuffer)
	SHADER_PARAMETER_UAV(RWStructuredBuffer<FSInstanceMeshItem>, RWInstanceBuffer)
	SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
	RDG_BUFFER_ACCESS(IndirectArgsBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()
};

struct FViewData
{
	FVector ViewOrigin;
	FMatrix ProjectionMatrix;
	FConvexVolume ViewFrustum;
	bool bViewFrozen;
};

FORCEINLINE void GetViewData(FSceneView const *InSceneView, FViewData &OutViewData)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const FViewMatrices *FrozenViewMatrices = InSceneView->State != nullptr ? InSceneView->State->GetFrozenViewMatrices() : nullptr;
	if (FrozenViewMatrices != nullptr)
	{
		OutViewData.ViewOrigin = FrozenViewMatrices->GetViewOrigin();
		OutViewData.ProjectionMatrix = FrozenViewMatrices->GetProjectionMatrix();
		GetViewFrustumBounds(OutViewData.ViewFrustum, FrozenViewMatrices->GetViewProjectionMatrix(), true);
		OutViewData.bViewFrozen = true;
	}
	else
#endif
	{
		OutViewData.ViewOrigin = InSceneView->ViewMatrices.GetViewOrigin();
		OutViewData.ProjectionMatrix = InSceneView->ViewMatrices.GetProjectionMatrix();
		OutViewData.ViewFrustum = InSceneView->ViewFrustum;
		OutViewData.bViewFrozen = false;
	}
}

/** Structure describing GPU culling setup for a single Proxy. */
struct FProxyDesc
{
	FSInstanceSceneProxy const *SceneProxy;
	int32 MaxPersistentQueueItems;
	int32 MaxRenderItems;
	int32 NumAddPassWavefronts;
};
/** View description used for LOD calculation in the main view. */
struct FMainViewDesc
{
	FSceneView const *ViewDebug;
	FVector ViewOrigin;
	FVector4 LodDistances;
	float LodBiasScale;
	FVector4 Planes[5];
	FTextureRHIRef OcclusionTexture;
	int32 OcclusionLevelOffset;
};
/** View description used for culling in the child view. */
struct FChildViewDesc
{
	FSceneView const *ViewDebug;
	bool bIsMainView;
	FVector4 Planes[5];
};
/** Structure to carry RDG resources. */
struct FVolatileResources
{
	FRDGBufferRef BaseInstanceBuffer;
	FRDGBufferUAVRef BaseInstanceBufferUAV;
	FRDGBufferSRVRef BaseInstanceBufferSRV;

	FRDGBufferRef InstanceInfoBuffer;
	FRDGBufferUAVRef InstanceInfoBufferUAV;
	FRDGBufferSRVRef InstanceInfoBufferSRV;

	FRDGBufferRef MeshBuffer;
	FRDGBufferUAVRef MeshBufferUAV;
	FRDGBufferSRVRef MeshBufferSRV;

	FRDGBufferRef IndirectArgsBuffer;
	FRDGBufferUAVRef IndirectArgsBufferUAV;
	FRDGBufferSRVRef IndirectArgsBufferSRV;
};

class FSInstanceMesh
{
public:
	static void InitializeInstanceBuffers(FRHICommandListBase& InRHICmdList, FSDrawInstanceBuffers& InBuffers);
	static void ReleaseInstanceBuffers(FSDrawInstanceBuffers& InBuffers);
	static void InitializeResources(FRDGBuilder& GraphBuilder, FProxyDesc const& InDesc, FMainViewDesc const& InMainViewDesc,
	                                FVolatileResources& OutResources);
	static void AddPass_TransitionAllDrawBuffers(FRDGBuilder & GraphBuilder, TArray<FSDrawInstanceBuffers> const &Buffers, TArrayView<int32> const &BufferIndices, bool bToWrite);
	static void AddPass_InitBuffers(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InGlobalShaderMap, FProxyDesc const& InDesc,
	                         FVolatileResources& InVolatileResources);
	static void AddPass_AddInstances(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InGlobalShaderMap, FProxyDesc const& InDesc,
	                          FVolatileResources& InVolatileResources, FMainViewDesc const& InViewDesc);
	static void AddPass_InitInstanceBuffer(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InGlobalShaderMap,
	                                FSDrawInstanceBuffers& InOutputResources, int NumIndices);
	static void AddPass_CullInstances(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InGlobalShaderMap, FProxyDesc const& InDesc,
	                           FVolatileResources& InVolatileResources, FSDrawInstanceBuffers& InOutputResources,
	                           FChildViewDesc const& InViewDesc);
};



