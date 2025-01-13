// Fill out your copyright notice in the Description page of Project Settings.


#include "SChunkWorld.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "SMeshComponent.h"
#include "MCCountVertsCS.h"
#include "MCAllocVertsCS.h"
#include "MarchingCS.h"
#include "NoiseCS.h"
#include "SDispatchCS.h"
#include "SChunkWorker.h"

// Sets default values
ASChunkWorld::ASChunkWorld()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	SetRootComponent(SceneComponent);

	MaxLOD = 1;
	Scale = 1;

	BoundsScale = 1.0f;
}

// Called when the game starts or when spawned
void ASChunkWorld::BeginPlay()
{
	Super::BeginPlay();

	ChunkWorkers.SetNum(MaxLOD + 1);
	for(int LOD = 0; LOD <= MaxLOD; LOD++)
	{
		ChunkWorkers[LOD] = new FSChunkWorker(this, LOD);
	}
	
	ChunkLODs.SetNum(MaxLOD + 1);
}

void ASChunkWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	for(int LOD = 0; LOD <= MaxLOD; LOD++)
	{
		if(FSChunkWorker* ChunkWorker = ChunkWorkers[LOD])
		{
			ChunkWorker->StopAndEnsureCompletion();
			delete ChunkWorker;
			ChunkWorker = nullptr;
		}
	}
}

void ASChunkWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	UpdateChunks();
}

void ASChunkWorld::UpdateChunks()
{
	auto viewLocations = GetWorld()->ViewLocationsRenderedLastFrame;
	if(viewLocations.Num() == 0)
		return;
	FVector camLocation = viewLocations[0];

	int SmallestChunkSize = Size * 100 * Scale;
	FIntVector OriginLocation = FIntVector(camLocation/SmallestChunkSize) * SmallestChunkSize;
	
	for(int LOD = 0; LOD <= MaxLOD; LOD++)
	{
		FSChunkWorker* ChunkWorker = ChunkWorkers[LOD];
		if(ChunkWorker)
		{
			if(ChunkWorker->bInputReady == false)
			{
				ChunkLODs[LOD].CurrentChunkKeys = ChunkWorker->CurrentChunks;
		
				ChunkWorker->ChunkInput = FChunkInput(ChunkLODs[LOD].CurrentChunkKeys,
					FIntVector3(WorldSize), UndergroundHeight, aboveUpperDistance, aboveDownDistance, underUpperDistance, underDownDistance,
					Size, Scale, OriginLocation, Isolevel, seed);
		
				ChunkWorker->bInputReady = true;
			}
		}
	}
}

void ASChunkWorld::SpawnChunkMesh(FIntVector ChunkKey, int LOD, FSDispatchCSOutput DispatchCSOutput)
{
	if(DispatchCSOutput.OutputVertices && DispatchCSOutput.OutputTris)
	{
		//If we got to this point then that means vertex and index count is > 0
		USMeshComponent* Chunk = NewObject<USMeshComponent>(this, NAME_None);
		if(Chunk)
		{
			Chunk->RegisterComponent();
			Chunk->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			Chunk->SetWorldLocation(FVector(ChunkKey));
			Chunk->SetBoundsScale(BoundsScale);
                    
			Chunk->CreateMeshSection(DispatchCSOutput, Material, Size, LOD, Scale, bCollisionEnabled, CollisionProfileName);
				
			ChunkLODs[LOD].Chunks.Add(ChunkKey, FChunk(Chunk));
		}
	}
}

void ASChunkWorld::DeleteChunkMesh(FIntVector ChunkKey, int LOD)
{
	if(FChunk* DeleteChunkPointer = ChunkLODs[LOD].Chunks.Find(ChunkKey))
	{
		FChunk DeleteChunk = *DeleteChunkPointer;
		if(USMeshComponent* DeleteChunkMesh = DeleteChunk.Mesh)
		{
			DeleteChunkMesh->UnregisterComponent();
			DeleteChunkMesh->DestroyComponent();
		}
	}
	ChunkLODs[LOD].Chunks.Remove(ChunkKey);
}
