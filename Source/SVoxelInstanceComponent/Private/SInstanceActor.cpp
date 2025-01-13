// Copyright Epic Games, Inc. All Rights Reserved.

#include "SInstanceActor.h"
#include "SInstanceComponent.h"

ASInstanceActor::ASInstanceActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = SInstanceComponent = CreateDefaultSubobject<USInstanceComponent>(TEXT("SInstanceComponent"));
}
