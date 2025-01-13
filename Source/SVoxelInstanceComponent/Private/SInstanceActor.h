// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SInstanceActor.generated.h"

class USInstanceComponent;

UCLASS(hidecategories = (Cooking, Input, LOD), MinimalAPI)
class ASInstanceActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USInstanceComponent* SInstanceComponent;

protected:
};
