#pragma once

#include "CoreMinimal.h"
#include "SChunkWorld.h"
#include "HAL/Runnable.h"

struct SVOXELPLUGIN_API FChunkInput
{
	TSet<FIntVector> OldChunks;

	FIntVector3 WorldSize;
	float UndergroundHeight;
	int aboveUpperDistance = 0;
	int aboveDownDistance = 0;
	int underUpperDistance = 0;
	int underDownDistance = 0;


	int Size;
	int Scale;
	FIntVector OriginLocation;;
	
	float Isolevel;
	int32 seed;
};

/**
 *
 */
class SVOXELPLUGIN_API FSChunkWorker : public FRunnable
{
public:
	FSChunkWorker(ASChunkWorld* NewChunkWorld, int NewLOD);
	virtual ~FSChunkWorker() override;
	
	bool Init() override;
	uint32 Run() override;

	virtual void Exit() override;
	void Stop() override;
	
	void StopAndEnsureCompletion();

private:
	FRunnableThread* Thread;
	bool bRunThread;

public:
	bool bInputReady = false;
	bool bDispatched = false;

	FChunkInput ChunkInput;
	TSet<FIntVector> CurrentChunks;

private:
	FThreadSafeCounter NewChunkTasks;
	
	FEvent* DispatchEvent;
	
	TWeakObjectPtr<ASChunkWorld> ChunkWorldPointer;
	
	int LOD;
	int MaxConcurrentTasks = 8;
};