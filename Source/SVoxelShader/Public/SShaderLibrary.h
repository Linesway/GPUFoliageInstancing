#pragma once

#include "CoreMinimal.h"
#include "SShaderLibrary.generated.h"

UCLASS()
class SVOXELSHADER_API USShaderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(
		BlueprintCallable,
		Category = "SShaderLibrary",
		meta = (WorldContext = "WorldContextObject"),
		DisplayName = "CallBaseCS"
	)
	static void CallBaseCS(
		int Arg1,
		int Arg2
	);
	
};
	