#include "NoiseCS.h"
#include "PixelShaderUtils.h"
#include "Runtime/RenderCore/Public/RenderGraphUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "RHIGPUReadback.h"


// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FNoiseCS, "/Shaders/Private/NoiseCS.usf", "GetNoise", SF_Compute);

void FNoiseCSInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FNoiseCSDispatchParams Params,
	TFunction<void(FNoiseCSOutput Output)> AsyncCallback)
{
	FRDGBuilder GraphBuilder(RHICmdList);
	
	TShaderMapRef<FNoiseCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FNoiseCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNoiseCS::FParameters>();

	PassParameters->WorldSize = Params.WorldSize;
	PassParameters->Size = Params.Size;
	PassParameters->Position = Params.Position;
	PassParameters->LOD = Params.LOD;
	PassParameters->Scale = Params.Scale;

	PassParameters->seed = Params.seed;

	//Max Number of Voxels (Size + 3 as need access to ring around the marching cube)
	int NumVoxels = (Params.Size + 4) * (Params.Size + 4) * (Params.Size + 4);

	TArray<float> InVoxels;
	InVoxels.Init(0, NumVoxels);
			
	FRDGBufferRef OutVoxelsBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("OutVoxelsBuffer"),
		sizeof(float),
		NumVoxels,
		InVoxels.GetData(),
		sizeof(float) * NumVoxels);

	PassParameters->OutVoxels = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutVoxelsBuffer, PF_R32_SINT));

	//The total number of iterations is Size + 5
	auto GroupCount = FComputeShaderUtils::GetGroupCount(
		FIntVector(Params.Size + 4, Params.Size + 4, Params.Size + 4),
		FIntVector(8, 8, 8));
			
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ExecuteNoiseCS"),
		PassParameters,
		ERDGPassFlags::AsyncCompute,
		[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
	{
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
	});

	TRefCountPtr<FRDGPooledBuffer> OutVoxels;
	GraphBuilder.QueueBufferExtraction(OutVoxelsBuffer, &OutVoxels);
	
	GraphBuilder.Execute();

	AsyncCallback(FNoiseCSOutput(OutVoxels));
	
}