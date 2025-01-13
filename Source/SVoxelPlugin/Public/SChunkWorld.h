// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SDispatchCS.h"
#include "GameFramework/Actor.h"
#include "SChunkWorld.generated.h"

class ASChunk;
class UProceduralMeshComponent;
class USMeshComponent;
struct FMeshData;
struct FSDispatchCSOutput;
class FSChunkWorker;
class FSVertexWorker;

USTRUCT()
struct SVOXELPLUGIN_API FChunk
{
	GENERATED_BODY()
public:
	UPROPERTY()
	USMeshComponent* Mesh;
};

struct SVOXELPLUGIN_API FChunkLOD
{
	TSet<FIntVector> CurrentChunkKeys;
    TMap<FIntVector, FChunk> Chunks;
};

UCLASS()
class SVOXELPLUGIN_API ASChunkWorld : public AActor
{
	GENERATED_BODY()

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
public:	
	// Sets default values for this actor's properties
	ASChunkWorld();

public:
	virtual void Tick( float DeltaSeconds ) override;

private:
	TArray<FSChunkWorker*> ChunkWorkers;
	TArray<FChunkLOD> ChunkLODs;
	
public:

//Defaults
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkWorld")
	int MaxLOD = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkWorld")
    FIntVector WorldSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkWorld")
	float UndergroundHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkWorld")
	int aboveUpperDistance = 16;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkWorld")
	int aboveDownDistance = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkWorld")
	int underUpperDistance = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkWorld")
	int underDownDistance = 6;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkWorker")
	int MaxConcurrentTasks = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int Size = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int Scale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	float BoundsScale;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	int WorldVertexCount = 0;

	/* Chunk Settings Set by the World */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	float Isolevel = 0.0f;

	/* Noise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int32 seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bCollisionEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	FName CollisionProfileName = "BlockAll";

protected:
//ChunkWorld
	void UpdateChunks();
public:
	void SpawnChunkMesh(FIntVector ChunkKey, int LOD, FSDispatchCSOutput DispatchCSOutput);
	void DeleteChunkMesh(FIntVector ChunkKey, int LOD);
};
