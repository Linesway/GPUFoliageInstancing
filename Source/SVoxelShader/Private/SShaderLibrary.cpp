#include "SShaderLibrary.h"
#include "CommonRenderResources.h"
#include "BaseComputeShader.h"
#include "MarchingCS.h"
#include "RenderGraph.h"


void USShaderLibrary::CallBaseCS(int Arg1, int Arg2)
{
	FBaseCSDispatchParams Params;
	Params.Input[0] = Arg1;
	Params.Input[1] = Arg2;

	//Initialize output too just in case
	Params.Output = 1;

	// Dispatch the compute shader and wait until it completes
	FBaseCSInterface::Dispatch(Params, [](int OutputVal)
	{
		UE_LOG(LogTemp, Warning, TEXT("%d"), OutputVal);
	});
	
}


