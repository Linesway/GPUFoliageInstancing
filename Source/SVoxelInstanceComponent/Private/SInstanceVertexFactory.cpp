// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "SInstanceVertexFactory.h"

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

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FSInstanceUniformParameters, "SInstanceVF");
class FSInstanceShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FSInstanceShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap &ParameterMap)
	{
        InstanceBufferParameter.Bind(ParameterMap, TEXT("InstanceBuffer"));
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
		FSInstanceVertexFactory* VertexFactory = (FSInstanceVertexFactory*)InVertexFactory;
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FSInstanceUniformParameters>(), VertexFactory->GetUniformBuffer());
		
		FSInstanceUserData* UserData = (FSInstanceUserData*)BatchElement.UserData;
		ShaderBindings.Add(InstanceBufferParameter, UserData->InstanceBufferSRV);
	}
protected:
	LAYOUT_FIELD(FShaderResourceParameter, InstanceBufferParameter);
};
IMPLEMENT_TYPE_LAYOUT(FSInstanceShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSInstanceVertexFactory, SF_Vertex, FSInstanceShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSInstanceVertexFactory, SF_Pixel, FSInstanceShaderParameters);

void FSInstanceVertexFactory::InitRHI(FRHICommandListBase &RHICmdList)
{
	FVertexDeclarationElementList Elements;

	if (Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));
	}
	uint8 TangentBasisAttributes[2] = {1, 2};
	for (int32 AxisIndex = 0; AxisIndex < 2; AxisIndex++)
	{
		if (Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex]));
		}
	}
	
	if (Streams.Num() > 0)
	{
		InitDeclaration(Elements);
		check(IsValidRef(GetDeclaration()));
	}
}

void FSInstanceVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	FVertexFactory::ReleaseRHI();
}

bool FSInstanceVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
    return (Parameters.MaterialParameters.MaterialDomain == MD_Surface)
	|| Parameters.MaterialParameters.bIsSpecialEngineMaterial
	|| Parameters.MaterialParameters.bIsDefaultMaterial;
}

void FSInstanceVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
}

void FSInstanceVertexFactory::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform,
	const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
}

void FSInstanceVertexFactory::SetData(FRHICommandListBase& RHICmdList, const FStaticMeshDataType& InData)
{
	check(IsInRenderingThread());
	Data = InData;
	UpdateRHI(RHICmdList);
}

void FSInstanceVertexFactory::SetParameters(const FSInstanceUniformBufferRef& InMeshInstanceUniformBuffer)
{
	UniformBuffer = InMeshInstanceUniformBuffer;
}

FUniformBufferRHIRef FSInstanceVertexFactory::GetUniformBuffer() const
{
	return UniformBuffer;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FSInstanceVertexFactory,
                              "/InstanceShaders/Private/Instance/InstanceLocalVertexFactory.ush",
                              EVertexFactoryFlags::UsedWithMaterials
                              | EVertexFactoryFlags::SupportsDynamicLighting
                              | EVertexFactoryFlags::SupportsPrecisePrevWorldPos
)




