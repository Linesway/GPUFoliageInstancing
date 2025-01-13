// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "SMeshVertexFactory.h"

#include <ThirdParty/ShaderConductor/ShaderConductor/External/SPIRV-Headers/include/spirv/unified1/spirv.h>

#include "MeshBatch.h"
#include "MeshDrawShaderBindings.h"
#include "SkeletalRenderPublic.h"
#include "SpeedTreeWind.h"
#include "Misc/DelayedAutoRegister.h"
#include "Rendering/ColorVertexBuffer.h"
#include "MaterialDomain.h"
#include "MeshMaterialShader.h"
#include "PrimitiveUniformShaderParameters.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "GPUSkinCache.h"
#include "GPUSkinVertexFactory.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderUtils.h"
#include "SceneInterface.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FSIndirectInstancingParameters, "FSIndirectInstancingParams");

void FSIndexBuffer::InitRHI(FRHICommandListBase &RHICmdList)
{
	IndexBufferRHI = SIndexBufferRHI;
}

class FSIndirectInstancingShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FSIndirectInstancingShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap &ParameterMap)
	{
		VertexBufferParameter.Bind(ParameterMap, TEXT("InVertexBuffer"), SPF_Optional);
		NormalBufferParameter.Bind(ParameterMap, TEXT("InNormalBuffer"), SPF_Optional);
		ColorBufferParameter.Bind(ParameterMap, TEXT("InColorBuffer"), SPF_Optional);
	}

	void GetElementShaderBindings(
			const class FSceneInterface *Scene,
			const class FSceneView *View,
			const class FMeshMaterialShader *Shader,
			const EVertexInputStreamType InputStreamType,
			ERHIFeatureLevel::Type FeatureLevel,
			const class FVertexFactory *InVertexFactory,
			const struct FMeshBatchElement &BatchElement,
			class FMeshDrawSingleShaderBindings &ShaderBindings,
			FVertexInputStreamArray &VertexStreams) const
	{
		FSMeshVertexFactory* VertexFactory = (FSMeshVertexFactory*)InVertexFactory;
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FSIndirectInstancingParameters>(), VertexFactory->UniformBuffer);
		ShaderBindings.Add(VertexBufferParameter, VertexFactory->VertexBufferSRV);
		ShaderBindings.Add(NormalBufferParameter, VertexFactory->NormalBufferSRV);
		ShaderBindings.Add(ColorBufferParameter, VertexFactory->ColorBufferSRV);
	}
protected:
	LAYOUT_FIELD(FShaderResourceParameter, VertexBufferParameter);
	LAYOUT_FIELD(FShaderResourceParameter, NormalBufferParameter);
	LAYOUT_FIELD(FShaderResourceParameter, ColorBufferParameter);
};
IMPLEMENT_TYPE_LAYOUT(FSIndirectInstancingShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSMeshVertexFactory, SF_Vertex, FSIndirectInstancingShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSMeshVertexFactory, SF_Pixel, FSIndirectInstancingShaderParameters);

FSMeshVertexFactory::FSMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const FSIndirectInstancingParameters& InParams)
	: FVertexFactory(InFeatureLevel),
	Params(InParams)
{
	IndexBuffer = new FSIndexBuffer();
}

FSMeshVertexFactory::~FSMeshVertexFactory()
{
	delete IndexBuffer;
	InitDispatchCSOutput.ReleaseDispatch();
};

void FSMeshVertexFactory::InitRHI(FRHICommandListBase& RHICmdList) 
{
	UniformBuffer = FSIndirectInstancingBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);
	
	IndexBuffer->SIndexBufferRHI = InitDispatchCSOutput.OutputTris->GetRHI();
	IndexBuffer->InitResource(RHICmdList);
	
	FVertexStream NullVertexStream;
	NullVertexStream.VertexBuffer = nullptr;
	NullVertexStream.Stride = 0;
	NullVertexStream.Offset = 0;
	NullVertexStream.VertexStreamUsage = EVertexStreamUsage::ManualFetch;

	check(Streams.Num() == 0);
	Streams.Add(NullVertexStream);
	
	FVertexDeclarationElementList Elements;
	FVertexDeclarationElementList PosOnlyElements;
	FVertexDeclarationElementList PosAndNormalOnlyElements;
	
	InitDeclaration(Elements);
	InitDeclaration(PosOnlyElements, EVertexInputStreamType::PositionOnly);
	InitDeclaration(PosAndNormalOnlyElements, EVertexInputStreamType::PositionAndNormalOnly);
}

void FSMeshVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	VertexBufferSRV.SafeRelease();
	NormalBufferSRV.SafeRelease();
	ColorBufferSRV.SafeRelease();
	if (IndexBuffer)
	{
		IndexBuffer->ReleaseResource();
	}
	FVertexFactory::ReleaseRHI();
}

bool FSMeshVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
    if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
    {
    	return false;
    }
    return (Parameters.MaterialParameters.MaterialDomain == MD_Surface && Parameters.MaterialParameters.bIsUsedWithVirtualHeightfieldMesh)
	|| Parameters.MaterialParameters.bIsSpecialEngineMaterial
	|| Parameters.MaterialParameters.bIsDefaultMaterial;
}

void FSMeshVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
}

void FSMeshVertexFactory::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform,
	const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FSMeshVertexFactory, "/MyShaders/Private/LocalVertexFactory.ush",
                              EVertexFactoryFlags::UsedWithMaterials 
                              | EVertexFactoryFlags::SupportsDynamicLighting
                              | EVertexFactoryFlags::SupportsCachingMeshDrawCommands)




