#include "MCCountVertsCS.h"
#include "PixelShaderUtils.h"
#include "Runtime/RenderCore/Public/RenderGraphUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "RHIGPUReadback.h"


// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FMCCountVertsCS, "/Shaders/Private/MCCountVertsCS.usf", "March", SF_Compute);

void FMCCountVertsCSInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FMCCountVertsCSDispatchParams Params,
	TFunction<void(FMCCountVertsCSOutput Output)> AsyncCallback)
{
	FRDGBuilder GraphBuilder(RHICmdList);
	
	TShaderMapRef<FMCCountVertsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FMCCountVertsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMCCountVertsCS::FParameters>();

	PassParameters->Size = Params.Size;
	PassParameters->isolevel = Params.isolevel;
	
	FRDGBufferRef InVoxelsBuffer = GraphBuilder.RegisterExternalBuffer(Params.InVoxels);
	PassParameters->InVoxels = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InVoxelsBuffer, PF_R32_SINT));

	TArray<uint32_t> InCellMasks;
	InCellMasks.Init(0, (Params.Size+1)*(Params.Size+1)*(Params.Size+1));
	FRDGBufferRef CellMasksBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("CellMasksBuffer"),
		sizeof(uint32_t),
		InCellMasks.Num(),
		InCellMasks.GetData(),
		sizeof(uint32_t) * InCellMasks.Num()
		);
	
	PassParameters->cellMasks = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(CellMasksBuffer, PF_R32_SINT));
	FRDGBufferRef VertexCountBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("VertexCountBuffer"),
		sizeof(uint32_t),
		1,
		TArray<int>({0}).GetData(), //count starts at zero
		sizeof(uint32_t) * 1
		);
	
	PassParameters->VertexCount = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(VertexCountBuffer, PF_R32_SINT));
	FRDGBufferRef IndicesCountBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("IndicesCountBuffer"),
		sizeof(uint32_t),
		1,
		TArray<int>({0}).GetData(), //count starts at zero
		sizeof(uint32_t) * 1
		);
	
	PassParameters->IndicesCount = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(IndicesCountBuffer, PF_R32_SINT));
	
	//so the total number of iterations is Size + 1
	auto GroupCount = FComputeShaderUtils::GetGroupCount(
		FIntVector(Params.Size+1, Params.Size+1, Params.Size+1),
		FIntVector(8, 8, 8));
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ExecuteMCCountVertsCS"),
		PassParameters,
		ERDGPassFlags::AsyncCompute,
		[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
	{
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
	});

	FRHIGPUBufferReadback* GPUIndicesCountBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMCCountVertsCSOutput"));
	AddEnqueueCopyPass(GraphBuilder, GPUIndicesCountBufferReadback, IndicesCountBuffer, 0u);
	
	TRefCountPtr<FRDGPooledBuffer> OutCellMasks;
	GraphBuilder.QueueBufferExtraction(CellMasksBuffer, &OutCellMasks);
	
	GraphBuilder.Execute();
	
    auto RunnerFunc = [GPUIndicesCountBufferReadback, OutCellMasks, AsyncCallback](auto&& RunnerFunc) -> void
    {
    	if (GPUIndicesCountBufferReadback->IsReady())
    	{
    		uint32_t* IndicesCountBufferReadback = (uint32_t*)GPUIndicesCountBufferReadback->Lock(1);
    		uint32_t IndicesCount = IndicesCountBufferReadback[0];

    		AsyncCallback(FMCCountVertsCSOutput(OutCellMasks, IndicesCount));

    		GPUIndicesCountBufferReadback->Unlock();
    		delete GPUIndicesCountBufferReadback;
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