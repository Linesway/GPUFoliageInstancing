#include "SDispatchCS.h"
#include "PixelShaderUtils.h"
#include "Runtime/RenderCore/Public/RenderGraphUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "RHIGPUReadback.h"
#include "NoiseCS.h"
#include "MCCountVertsCS.h"
#include "MCAllocVertsCS.h"
#include "MarchingCS.h"

void FSDispatchCSInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FSDispatchCSParams Params,
	TFunction<void(FSDispatchCSOutput Output)> AsyncCallback)
{
	FNoiseCSDispatchParams NoiseCSDispatchParams = FNoiseCSDispatchParams(Params.WorldSize, Params.Size, Params.Position, Params.LOD, Params.Scale,
		Params.seed);

	// Dispatch the compute shader and wait until it completes
	FNoiseCSInterface::DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), NoiseCSDispatchParams,
		[AsyncCallback, Params](FNoiseCSOutput NoiseCSOutput)
	{
		FMCCountVertsCSDispatchParams MCCountVertsCSDispatchParams = FMCCountVertsCSDispatchParams(Params.Size, 
		Params.isolevel,  NoiseCSOutput.OutVoxels);

		FMCCountVertsCSInterface::DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), MCCountVertsCSDispatchParams,
	[AsyncCallback, Params, NoiseCSOutput](FMCCountVertsCSOutput MCCountVertsCSOutput)
		{
			if(MCCountVertsCSOutput.IndicesCount <= 0)
			{
				AsyncTask(ENamedThreads::GameThread, [AsyncCallback]()
				{
					AsyncCallback(FSDispatchCSOutput());
				});
				return;
			}
		
			FMCAllocVertsCSDispatchParams MCAllocVertsCSDispatchParams = FMCAllocVertsCSDispatchParams(Params.Size, MCCountVertsCSOutput.OutCellMasks);

			FMCAllocVertsCSInterface::DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), MCAllocVertsCSDispatchParams,
[AsyncCallback, Params, NoiseCSOutput, MCCountVertsCSOutput](FMCAllocVertsCSOutput MCAllocVertsCSOutput)
			{
				if(MCAllocVertsCSOutput.NumAllocatedVerts <= 0)
				{
					AsyncTask(ENamedThreads::GameThread, [AsyncCallback]()
					{
						AsyncCallback(FSDispatchCSOutput());
					});
					return;
				}
	
				FMarchingCSDispatchParams MarchingCSDispatchParams = FMarchingCSDispatchParams(Params.WorldSize, Params.Size, Params.isolevel, Params.LOD, Params.Scale,
					Params.Position, Params.seed, NoiseCSOutput.OutVoxels, MCAllocVertsCSOutput.OutCellMasks, MCAllocVertsCSOutput.NumAllocatedVerts,
					MCCountVertsCSOutput.IndicesCount);

				FMarchingCSInterface::DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), MarchingCSDispatchParams,
			[AsyncCallback, MCAllocVertsCSOutput, MCCountVertsCSOutput](FMarchingCSOutput MarchingCSOutput)
				{
					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, MarchingCSOutput, MCAllocVertsCSOutput, MCCountVertsCSOutput]()
					{
						AsyncCallback(FSDispatchCSOutput(MarchingCSOutput.OutputVertices,
							MarchingCSOutput.OutputTris,MarchingCSOutput.OutNormals, MarchingCSOutput.OutColor,
							MCAllocVertsCSOutput.NumAllocatedVerts, MCCountVertsCSOutput.IndicesCount, MarchingCSOutput.Vertices, MarchingCSOutput.Indices));
					});
				});
				
			});
		});
		
	});
}