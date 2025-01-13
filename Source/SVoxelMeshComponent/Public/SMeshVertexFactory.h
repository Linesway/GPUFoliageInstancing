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

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSIndirectInstancingParameters, )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FSIndirectInstancingParameters> FSIndirectInstancingBufferRef;

class FSMeshSceneProxy;

class FSIndexBuffer : public FIndexBuffer
{
public:
	FSIndexBuffer()
	{}
	virtual void InitRHI(FRHICommandListBase & RHICmdList) override;

	FRHIBuffer* SIndexBufferRHI;
};

struct FSMeshVertexFactory : FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSMeshVertexFactory);
public:
	FSMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const FSIndirectInstancingParameters &InParams);
	~FSMeshVertexFactory();

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static void ValidateCompiledResult(const FVertexFactoryType *Type, EShaderPlatform Platform, const FShaderParameterMap &ParameterMap, TArray<FString> &OutErrors);

	FSIndirectInstancingParameters Params;

	FSIndirectInstancingBufferRef UniformBuffer;
	
	FShaderResourceViewRHIRef VertexBufferSRV;
	FShaderResourceViewRHIRef NormalBufferSRV;
	FShaderResourceViewRHIRef ColorBufferSRV;
	FSIndexBuffer* IndexBuffer;
	
	FSDispatchCSOutput InitDispatchCSOutput;


};