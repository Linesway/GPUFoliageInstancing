#include "SInstanceMesh.h"
#include "CommonRenderResources.h"
#include "EngineModule.h"
#include "Engine/Engine.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "RenderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SInstanceSceneProxy.h"
#include "Misc/LowLevelTestAdapter.h"

IMPLEMENT_GLOBAL_SHADER(FInitBuffers_CS, "/InstanceShaders/Private/Instance/InstanceCS.usf", "InitBuffersCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAddInstances_CS, "/InstanceShaders/Private/Instance/InstanceCS.usf", "AddInstancesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FInitInstanceBuffer_CS, "/InstanceShaders/Private/Instance/InstanceCS.usf", "InitInstanceBufferCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FCullInstances_CS, "/InstanceShaders/Private/Instance/InstanceCS.usf", "CullInstancesCS", SF_Compute);

/** Initialize the FDrawInstanceBuffers objects. */
void FSInstanceMesh::InitializeInstanceBuffers(FRHICommandListBase& InRHICmdList, FSDrawInstanceBuffers & InBuffers)
{
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FSInstance.InstanceBuffer"));
		const int32 InstanceSize = sizeof(FSInstanceRenderInstance);
		const int32 InstanceBufferSize = 1024 * 4 * InstanceSize;
		InBuffers.InstanceBuffer = InRHICmdList.CreateStructuredBuffer(InstanceSize, InstanceBufferSize, BUF_UnorderedAccess | BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
		InBuffers.InstanceBufferUAV = InRHICmdList.CreateUnorderedAccessView(InBuffers.InstanceBuffer, false, false);
		InBuffers.InstanceBufferSRV = InRHICmdList.CreateShaderResourceView(InBuffers.InstanceBuffer);
	}
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FSInstance.InstanceIndirectArgsBuffer"));
		InBuffers.IndirectArgsBuffer = InRHICmdList.CreateVertexBuffer(5 * sizeof(uint32), BUF_UnorderedAccess | BUF_DrawIndirect, ERHIAccess::IndirectArgs, CreateInfo);
		InBuffers.IndirectArgsBufferUAV = InRHICmdList.CreateUnorderedAccessView(InBuffers.IndirectArgsBuffer, PF_R32_UINT);
	}
}

void FSInstanceMesh::ReleaseInstanceBuffers(FSDrawInstanceBuffers& InBuffers)
{
	InBuffers.InstanceBuffer.SafeRelease();
	InBuffers.InstanceBufferUAV.SafeRelease();
	InBuffers.InstanceBufferSRV.SafeRelease();
	InBuffers.IndirectArgsBuffer.SafeRelease();
	InBuffers.IndirectArgsBufferUAV.SafeRelease();
}

/** Initialise the draw indirect buffer. */
void FSInstanceMesh::AddPass_InitInstanceBuffer(FRDGBuilder & GraphBuilder, FGlobalShaderMap * InGlobalShaderMap, FSDrawInstanceBuffers& InOutputResources, int NumIndices)
{
	TShaderMapRef<FInitInstanceBuffer_CS> ComputeShader(InGlobalShaderMap);

	FInitInstanceBuffer_CS::FParameters *PassParameters = GraphBuilder.AllocParameters<FInitInstanceBuffer_CS::FParameters>();
	PassParameters->NumIndices = NumIndices;
	PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;

	FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitInstanceBuffer"),
			ComputeShader, PassParameters, FIntVector(1, 1, 1));
}

/** Initialize the volatile resources used in the render graph. */
void FSInstanceMesh::InitializeResources(FRDGBuilder & GraphBuilder, FProxyDesc const &InDesc, FMainViewDesc const &InMainViewDesc, FVolatileResources &OutResources)
{
	bool bInitialized;
	if (!InDesc.SceneProxy->BaseInstanceBuffer.IsValid())
	{
		// We need to create the instance buffers.
		OutResources.BaseInstanceBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FSInstanceMeshItem), InDesc.MaxRenderItems),
			TEXT("SInstanceMesh.BaseInstanceBuffer"));

		OutResources.InstanceInfoBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("SInstanceMesh.InstanceInfoBuffer"),
			sizeof(FSInstanceInfo), 1, new FSInstanceInfo{0}, sizeof(FSInstanceInfo) * 1);

		InDesc.SceneProxy->BaseInstanceBuffer = GraphBuilder.ConvertToExternalBuffer(OutResources.BaseInstanceBuffer);
		InDesc.SceneProxy->InstanceInfoBuffer = GraphBuilder.ConvertToExternalBuffer(OutResources.InstanceInfoBuffer);
		bInitialized = false;
	}
	else
	{
		// Buffers already exist, we can use them.
		OutResources.BaseInstanceBuffer = GraphBuilder.RegisterExternalBuffer(InDesc.SceneProxy->BaseInstanceBuffer);
		OutResources.InstanceInfoBuffer = GraphBuilder.RegisterExternalBuffer(InDesc.SceneProxy->InstanceInfoBuffer);
		bInitialized = true;
	}

	OutResources.BaseInstanceBufferUAV = GraphBuilder.CreateUAV(OutResources.BaseInstanceBuffer);
	OutResources.BaseInstanceBufferSRV = GraphBuilder.CreateSRV(OutResources.BaseInstanceBuffer);

	OutResources.InstanceInfoBufferUAV = GraphBuilder.CreateUAV(OutResources.InstanceInfoBuffer);
	OutResources.InstanceInfoBufferSRV = GraphBuilder.CreateSRV(OutResources.InstanceInfoBuffer);

	OutResources.MeshBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FSInstanceMeshItem), InDesc.MaxRenderItems), TEXT("SInstanceMesh.MeshBuffer"));
	OutResources.MeshBufferUAV = GraphBuilder.CreateUAV(OutResources.MeshBuffer);
	OutResources.MeshBufferSRV = GraphBuilder.CreateSRV(OutResources.MeshBuffer);

	OutResources.IndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(IndirectArgsByteSize), TEXT("SInstanceMesh.IndirectArgsBuffer"));
	OutResources.IndirectArgsBufferUAV = GraphBuilder.CreateUAV(OutResources.IndirectArgsBuffer);
	OutResources.IndirectArgsBufferSRV = GraphBuilder.CreateSRV(OutResources.IndirectArgsBuffer);

	if(bInitialized == false)
		FSInstanceMesh::AddPass_AddInstances(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), InDesc, OutResources, InMainViewDesc);
}

/** Collect potentially visible quads and determine their Lods. */
void FSInstanceMesh::AddPass_AddInstances(FRDGBuilder & GraphBuilder, FGlobalShaderMap * InGlobalShaderMap, FProxyDesc const &InDesc, FVolatileResources &InVolatileResources, FMainViewDesc const &InViewDesc)
{
	int NumToAdd = 100;

	TShaderMapRef<FAddInstances_CS> ComputeShader(InGlobalShaderMap);

	FAddInstances_CS::FParameters *PassParameters = GraphBuilder.AllocParameters<FAddInstances_CS::FParameters>();
	PassParameters->ViewOrigin = (FVector3f)FVector(InDesc.SceneProxy->GetLocalToWorld().Inverse().TransformPosition(InViewDesc.ViewOrigin));
	for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
	{
		PassParameters->FrustumPlanes[PlaneIndex] = FVector4f(InViewDesc.Planes[PlaneIndex]); // LWC_TODO: precision loss
	}

	PassParameters->RWInstanceInfo = InVolatileResources.InstanceInfoBufferUAV;
	PassParameters->RWBaseInstanceBuffer = InVolatileResources.BaseInstanceBufferUAV;
	PassParameters->RWIndirectArgsBuffer = InVolatileResources.IndirectArgsBufferUAV;
	PassParameters->NumToAdd = NumToAdd;
	PassParameters->Seed = rand() % 10000000;
	PassParameters->InstanceBufferSize = InDesc.MaxRenderItems;
	
	FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("AddInstances"),
			ComputeShader, PassParameters, FIntVector(FMath::DivideAndRoundUp(NumToAdd, 64), 1, 1));
}

/** Initialize the buffers before collecting visible meshes. */
void FSInstanceMesh::AddPass_InitBuffers(FRDGBuilder & GraphBuilder, FGlobalShaderMap * InGlobalShaderMap, FProxyDesc const &InDesc, FVolatileResources &InVolatileResources)
{
	TShaderMapRef<FInitBuffers_CS> ComputeShader(InGlobalShaderMap);

	FInitBuffers_CS::FParameters *PassParameters = GraphBuilder.AllocParameters<FInitBuffers_CS::FParameters>();
	PassParameters->Time = (float)(rand() % 1000) / 50.f;
	PassParameters->RWInstanceInfo = InVolatileResources.InstanceInfoBufferUAV;
	PassParameters->RWMeshBuffer = InVolatileResources.MeshBufferUAV;
	PassParameters->RWIndirectArgsBuffer = InVolatileResources.IndirectArgsBufferUAV;
	PassParameters->InstanceBufferSize = InDesc.MaxRenderItems;

	GraphBuilder.AddPass(
			RDG_EVENT_NAME("InitBuffers"),
			PassParameters,
			ERDGPassFlags::Compute,
			[PassParameters, ComputeShader](FRHICommandList &RHICmdList)
			{
				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, FIntVector(1, 1, 1));
			});
}

/** Cull instances and write to the final output buffer. */
void FSInstanceMesh::AddPass_CullInstances(FRDGBuilder & GraphBuilder, FGlobalShaderMap * InGlobalShaderMap, FProxyDesc const &InDesc, FVolatileResources &InVolatileResources, FSDrawInstanceBuffers &InOutputResources, FChildViewDesc const &InViewDesc)
{
	FCullInstances_CS::FParameters *PassParameters = GraphBuilder.AllocParameters<FCullInstances_CS::FParameters>();
	PassParameters->MeshBuffer = InVolatileResources.MeshBufferSRV;
	PassParameters->IndirectArgsBuffer = InVolatileResources.IndirectArgsBuffer;
	PassParameters->IndirectArgsBufferSRV = InVolatileResources.IndirectArgsBufferSRV;
	PassParameters->BaseInstanceBuffer = InVolatileResources.BaseInstanceBufferSRV;
	PassParameters->RWInstanceBuffer = InOutputResources.InstanceBufferUAV;
	PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;

	for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
	{
		PassParameters->FrustumPlanes[PlaneIndex] = FVector4f(InViewDesc.Planes[PlaneIndex]); // LWC_TODO: precision loss
	}

	int32 IndirectArgOffset = IndirectArgsByteOffset_FinalCull;

	FCullInstances_CS::FPermutationDomain PermutationVector;

	TShaderMapRef<FCullInstances_CS> ComputeShader(InGlobalShaderMap, PermutationVector);
	FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CullInstances"),
			ComputeShader, PassParameters,
			InVolatileResources.IndirectArgsBuffer,
			IndirectArgOffset);
}

/** Transition our output draw buffers for use. Read or write access is set according to the bToWrite parameter. */
void FSInstanceMesh::AddPass_TransitionAllDrawBuffers(FRDGBuilder & GraphBuilder, TArray<FSDrawInstanceBuffers> const &Buffers, TArrayView<int32> const &BufferIndices, bool bToWrite)
{
	TArray<FRHIUnorderedAccessView *> OverlapUAVs;
	OverlapUAVs.Reserve(BufferIndices.Num());

	TArray<FRHITransitionInfo> TransitionInfos;
	TransitionInfos.Reserve(BufferIndices.Num() * 2);

	for (int32 BufferIndex : BufferIndices)
	{
		FRHIUnorderedAccessView *IndirectArgsBufferUAV = Buffers[BufferIndex].IndirectArgsBufferUAV;
		FRHIUnorderedAccessView *InstanceBufferUAV = Buffers[BufferIndex].InstanceBufferUAV;

		OverlapUAVs.Add(IndirectArgsBufferUAV);

		TransitionInfos.Add(FRHITransitionInfo(IndirectArgsBufferUAV, bToWrite ? ERHIAccess::IndirectArgs : ERHIAccess::UAVMask, bToWrite ? ERHIAccess::UAVMask : ERHIAccess::IndirectArgs));
		TransitionInfos.Add(FRHITransitionInfo(InstanceBufferUAV, bToWrite ? ERHIAccess::SRVMask : ERHIAccess::UAVMask, bToWrite ? ERHIAccess::UAVMask : ERHIAccess::SRVMask));
	}

	AddPass(GraphBuilder, RDG_EVENT_NAME("TransitionAllDrawBuffers"), [bToWrite, OverlapUAVs, TransitionInfos](FRHICommandList &InRHICmdList)
					{
		if (!bToWrite)
		{
			InRHICmdList.EndUAVOverlap(OverlapUAVs);
		}

		InRHICmdList.Transition(TransitionInfos);
		
		if (bToWrite)
		{
			InRHICmdList.BeginUAVOverlap(OverlapUAVs);
		} });
}