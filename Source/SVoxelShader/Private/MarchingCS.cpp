#include "MarchingCS.h"
#include "PixelShaderUtils.h"
#include "Runtime/RenderCore/Public/RenderGraphUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "RHIGPUReadback.h"


// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FMarchingCS, "/Shaders/Private/MarchingCS.usf", "March", SF_Compute);

void FMarchingCSInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FMarchingCSDispatchParams Params,
	TFunction<void(FMarchingCSOutput Output)> AsyncCallback)
{
	FRDGBuilder GraphBuilder(RHICmdList);
	
	TShaderMapRef<FMarchingCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FMarchingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarchingCS::FParameters>();

	PassParameters->WorldSize = Params.WorldSize;
	PassParameters->Size = Params.Size;
	PassParameters->isolevel = Params.isolevel;
	PassParameters->LOD = Params.LOD;
	PassParameters->Scale = Params.Scale;

	PassParameters->Position = Params.Position;
	PassParameters->seed = Params.seed;

	FRDGBufferRef InVoxelsBuffer = GraphBuilder.RegisterExternalBuffer(Params.InVoxels);
	PassParameters->InVoxels = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InVoxelsBuffer, PF_R32_SINT));

	FRDGBufferRef InCellMasksBuffer = GraphBuilder.RegisterExternalBuffer(Params.InCellMasks);
	PassParameters->cellMasks = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(InCellMasksBuffer, PF_R32_SINT));

	FRDGBufferRef NumEmittedIndicesBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("NumEmittedVertsBuffer"),
		sizeof(uint32_t),
		1,
		TArray<int>({0}).GetData(), //count starts at zero
		sizeof(uint32_t) * 1
		);
	PassParameters->NumEmittedIndices = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(NumEmittedIndicesBuffer, PF_R32_SINT));
	
	TArray<FVector3f> InVertices;
	InVertices.Init(FVector3f(-1,-1,-1), Params.VertexCount);
	
	FRDGBufferRef OutVerticesBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("OutVerticesBuffer"),
		sizeof(FVector3f),
		Params.VertexCount,
		InVertices.GetData(),
		sizeof(FVector3f) * Params.VertexCount
		);
	PassParameters->OutVertices = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutVerticesBuffer, PF_R32_SINT));
	
	TArray<uint32> InTris;
	InTris.Init(0, Params.IndicesCount);
	FRDGBufferRef OutTrisBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(
			sizeof(uint32),
			Params.IndicesCount),
			TEXT("OutTrisBuffer"));
	GraphBuilder.QueueBufferUpload(OutTrisBuffer, InTris.GetData(),
		sizeof(uint32) * Params.IndicesCount, ERDGInitialDataFlags::None);
	PassParameters->OutTris = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutTrisBuffer, PF_R32_SINT));

	TArray<FVector3f> InNormals;
	InNormals.Init(FVector3f(0, 0, 0), Params.VertexCount);
	FRDGBufferRef OutNormalsBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("OutNormalsBuffer"),
		sizeof(FVector3f),
		Params.VertexCount,
		InNormals.GetData(),
		sizeof(FVector3f) * Params.VertexCount
		);
	PassParameters->OutNormals = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutNormalsBuffer, PF_R32_SINT));

	TArray<FVector4f> InColor;
	InColor.Init(FVector4f(0, 0, 0, 0), Params.VertexCount);
	FRDGBufferRef OutColorBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("OutColorBuffer"),
		sizeof(FVector4f),
		Params.VertexCount,
		InColor.GetData(),
		sizeof(FVector4f) * Params.VertexCount
		);
	PassParameters->OutColor = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutColorBuffer, PF_R32_SINT));

	//so the total number of iterations is Size + 1
	auto GroupCount = FComputeShaderUtils::GetGroupCount(
		FIntVector(Params.Size + 1, Params.Size + 1, Params.Size + 1),
		FIntVector(8, 8, 8));
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ExecuteMarchingCS"),
		PassParameters,
		ERDGPassFlags::AsyncCompute,
		[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
	{
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
	});
	
	TRefCountPtr<FRDGPooledBuffer> OutVertices;
	TRefCountPtr<FRDGPooledBuffer> OutTris;
	TRefCountPtr<FRDGPooledBuffer> OutNormals;
	TRefCountPtr<FRDGPooledBuffer> OutColor;
	
	GraphBuilder.QueueBufferExtraction(OutVerticesBuffer, &OutVertices);
	GraphBuilder.QueueBufferExtraction(OutTrisBuffer, &OutTris);
	GraphBuilder.QueueBufferExtraction(OutNormalsBuffer, &OutNormals);
	GraphBuilder.QueueBufferExtraction(OutColorBuffer, &OutColor);

	FRHIGPUBufferReadback* GPUOutVerticesBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMarchingCSOutput"));
	AddEnqueueCopyPass(GraphBuilder, GPUOutVerticesBufferReadback, OutVerticesBuffer, 0u);
	FRHIGPUBufferReadback* GPUOutTrisBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMarchingCSOutput"));
	AddEnqueueCopyPass(GraphBuilder, GPUOutTrisBufferReadback, OutTrisBuffer, 0u);
	
	GraphBuilder.Execute();

	if(Params.LOD == 0)
	{
		auto RunnerFunc = [GPUOutVerticesBufferReadback, GPUOutTrisBufferReadback, AsyncCallback, Params,
			OutVertices, OutTris, OutNormals, OutColor](auto&& RunnerFunc) -> void
		{
			if (GPUOutVerticesBufferReadback->IsReady() && GPUOutTrisBufferReadback->IsReady())
			{
				FVector3f* VerticesData = (FVector3f*)GPUOutVerticesBufferReadback->Lock(1);
				TArray<FVector3f> Vertices = TArray(VerticesData, Params.VertexCount);

				uint32* TrisData = (uint32*)GPUOutTrisBufferReadback->Lock(1);
				TArray<uint32> Tris = TArray(TrisData, Params.IndicesCount);

				TArray<FTriIndices> Indices;
				for (int32 TriIdx = 0; TriIdx < Params.IndicesCount/3; TriIdx++)
				{
					// Need to add base offset for indices
					FTriIndices Triangle;
					Triangle.v0 = Tris[(TriIdx * 3) + 0];
					Triangle.v1 = Tris[(TriIdx * 3) + 1];
					Triangle.v2 = Tris[(TriIdx * 3) + 2];
					Indices.Add(Triangle);
				}
				AsyncCallback(FMarchingCSOutput(OutVertices, OutTris, OutNormals, OutColor, Vertices, Indices));

				GPUOutVerticesBufferReadback->Unlock();
				GPUOutTrisBufferReadback->Unlock();
				delete GPUOutVerticesBufferReadback;
				delete GPUOutTrisBufferReadback;
			}
			else
			{
				AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]()
				{
					RunnerFunc(RunnerFunc);
				});
			}
		};
		AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]()
		{
			RunnerFunc(RunnerFunc);
		});
	}
	else
	{
		AsyncCallback(FMarchingCSOutput(OutVertices, OutTris, OutNormals, OutColor));
	}
}