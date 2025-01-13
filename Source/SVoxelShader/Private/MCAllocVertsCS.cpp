#include "MCAllocVertsCS.h"
#include "PixelShaderUtils.h"
#include "Runtime/RenderCore/Public/RenderGraphUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "RHIGPUReadback.h"


// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FMCAllocVertsCS, "/Shaders/Private/MCAllocVertsCS.usf", "March", SF_Compute);

void FMCAllocVertsCSInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FMCAllocVertsCSDispatchParams Params,
	TFunction<void(FMCAllocVertsCSOutput Output)> AsyncCallback)
{
	FRDGBuilder GraphBuilder(RHICmdList);
	
	TShaderMapRef<FMCAllocVertsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FMCAllocVertsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMCAllocVertsCS::FParameters>();

	PassParameters->Size = Params.Size;
	
	FRDGBufferRef CellMasksBuffer = GraphBuilder.RegisterExternalBuffer(Params.InCellMasks);
	PassParameters->cellMasks = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(CellMasksBuffer, PF_R32_SINT));
	
	FRDGBufferRef NumAllocatedVertsBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("NumAllocatedVertsBuffer"),
		sizeof(uint32_t),
		1,
		TArray<uint32_t>({0}).GetData(), //count starts at zero 
		sizeof(uint32_t) * 1
		);
	
	PassParameters->NumAllocatedVerts = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(NumAllocatedVertsBuffer, PF_R32_SINT));
	
	//so the total number of iterations is Size + 1
	auto GroupCount = FComputeShaderUtils::GetGroupCount(
		FIntVector(Params.Size+1, Params.Size+1, Params.Size+1),
		FIntVector(8, 8, 8));
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ExecuteMCAllocVertsCS"),
		PassParameters,
		ERDGPassFlags::AsyncCompute,
		[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
	{
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
	});

	FRHIGPUBufferReadback* GPUNumAllocatedVertsBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMCAllocVertsCSOutput"));
	AddEnqueueCopyPass(GraphBuilder, GPUNumAllocatedVertsBufferReadback, NumAllocatedVertsBuffer, 0u);
	
	TRefCountPtr<FRDGPooledBuffer> OutCellMasks;
	GraphBuilder.QueueBufferExtraction(CellMasksBuffer, &OutCellMasks);
	
	GraphBuilder.Execute();

	auto RunnerFunc = [GPUNumAllocatedVertsBufferReadback, OutCellMasks, AsyncCallback](auto&& RunnerFunc) -> void
	{
		if (GPUNumAllocatedVertsBufferReadback->IsReady())
		{
			uint32_t* NumAllocatedVertsBuffer = (uint32_t*)GPUNumAllocatedVertsBufferReadback->Lock(1);
			uint32_t NumAllocatedVerts = NumAllocatedVertsBuffer[0];
			
			AsyncCallback(FMCAllocVertsCSOutput(OutCellMasks, NumAllocatedVerts));

			GPUNumAllocatedVertsBufferReadback->Unlock();
			delete GPUNumAllocatedVertsBufferReadback;
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