// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#pragma once

#include "CoreMinimal.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RenderResource.h"
#include "RHI.h"
#include "SceneManagement.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "GlobalRenderResources.h"
#include "Components.h"
#include "SDispatchCS.h"
#include "ShaderParameters.h"

class FSInstanceSceneProxy;

struct FSInstanceUserData : public FOneFrameResource
{
	FRHIShaderResourceView* InstanceBufferSRV;
};

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSInstanceUniformParameters, )
	SHADER_PARAMETER_SRV(Buffer<float2>, VertexFetch_TexCoordBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_ColorComponentsBuffer)
	SHADER_PARAMETER(FIntVector4, VertexFetch_Parameters)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
typedef TUniformBufferRef<FSInstanceUniformParameters> FSInstanceUniformBufferRef;

struct FSInstanceVertexFactory : FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSInstanceVertexFactory);
public:
	FSInstanceVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FVertexFactory(InFeatureLevel)
	{}
	FSInstanceVertexFactory()
		: FVertexFactory(ERHIFeatureLevel::Num)
	{}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static void ValidateCompiledResult(const FVertexFactoryType *Type, EShaderPlatform Platform, const FShaderParameterMap &ParameterMap, TArray<FString> &OutErrors);

	void SetData(FRHICommandListBase& RHICmdList, const FStaticMeshDataType &InData);

protected:
	FStaticMeshDataType Data;

	FUniformBufferRHIRef UniformBuffer;
public:
	void SetParameters(const FSInstanceUniformBufferRef& InMeshInstanceUniformBuffer);
	FUniformBufferRHIRef GetUniformBuffer() const;
};
