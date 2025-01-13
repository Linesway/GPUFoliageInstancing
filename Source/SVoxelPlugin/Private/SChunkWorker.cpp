// Copyright notice

#include "SChunkWorker.h" // Change this to reference the header file above
#include "SDispatchCS.h"

FSChunkWorker::FSChunkWorker(ASChunkWorld* NewChunkWorld, int NewLOD)
{
	ChunkWorldPointer = NewChunkWorld;
	LOD = NewLOD;
	MaxConcurrentTasks = NewChunkWorld->MaxConcurrentTasks;

	Thread = FRunnableThread::Create(this, TEXT("SWorkerThread"));
}

FSChunkWorker::~FSChunkWorker()
{
	if (Thread)
	{
		Thread->Kill();
		delete Thread;
		Thread = nullptr;
	}
}

void FSChunkWorker::StopAndEnsureCompletion()
{
	Stop();
	if(Thread)
	{
		Thread->WaitForCompletion();
	}
}

bool FSChunkWorker::Init()
{
	return true;
}

uint32 FSChunkWorker::Run()
{
	bRunThread = true;
	
	while (bRunThread)
	{
		if (bInputReady)
		{
			if(bDispatched)
			{
				if(NewChunkTasks.GetValue() <= 0)
				{
					bInputReady = false;
					bDispatched = false;
				}
			}
			else
			{
				TSet<FIntVector> CurrentChunkKeys;

				int drawDistance = (LOD + 1) * 2;
				int ChunkSize =  ChunkInput.Size * 100 * (1 << LOD) * ChunkInput.Scale;

				bool bOriginUnderground = ChunkInput.OriginLocation.Z < ChunkInput.UndergroundHeight;
				
				for(int N = 0; N <= (drawDistance-1) * 3; N++)
				{
					for(int X = 0; X <= FMath::Min(N, drawDistance-1); X++)
					{
						for(int Y = 0; Y <= FMath::Min(N - X, drawDistance-1); Y++)
						{
							int Z = N - X - Y;
							if(Z <= drawDistance-1)
							{
								if(X < LOD && Y < LOD && Z < LOD)
									continue;

								int x0 = X; 
								for (int i = 0; i >= -1; i--, x0 *= -1)
								{
									int y0 = Y;
									for (int j = 0; j >= -1; j--, y0 *= -1)
									{
										int z0 = Z;
										for (int k = 0; k >= -1; k--, z0 *= -1)
										{
											int x = x0 + i; int y = y0 + j; int z = z0 + k;

											FIntVector ChunkPosition = ChunkInput.OriginLocation + FIntVector(x,y,z) * ChunkSize;
											bool bChunkUnderground = ChunkPosition.Z+ChunkSize < ChunkInput.UndergroundHeight;
											
											int distanceThreshold = (bOriginUnderground)
											? (bChunkUnderground ? ChunkInput.underDownDistance : ChunkInput.underUpperDistance)
											: (bChunkUnderground ? ChunkInput.aboveDownDistance : ChunkInput.aboveUpperDistance);

											int LODMultiplier = (1 << LOD);
											if (X * LODMultiplier <= distanceThreshold &&
												Y * LODMultiplier <= distanceThreshold &&
												Z * LODMultiplier <= distanceThreshold)
											{
												CurrentChunkKeys.Add(ChunkPosition);
											}
										}
									}
								}
							}
						}
					}
				}
				TSet<FIntVector> DeleteChunkKeys = ChunkInput.OldChunks.Difference(CurrentChunkKeys);
				TSet<FIntVector> NewChunkKeys = CurrentChunkKeys.Difference( ChunkInput.OldChunks);
				TSet<FIntVector> AllChunkKeys = CurrentChunkKeys;

				NewChunkTasks.Reset();
				for (FIntVector& SpawnChunkKey : NewChunkKeys)
				{
					if(!bRunThread)
						return 0;
					while (NewChunkTasks.GetValue() >= MaxConcurrentTasks)
					{
						if(!bRunThread)
							return 0;
						FPlatformProcess::Sleep(0.01f);
					}
					NewChunkTasks.Increment();
					
					FVector3f VoxelOffset = FVector3f(SpawnChunkKey) / 100;
            
					FSDispatchCSParams SDispatchCSParams = FSDispatchCSParams(ChunkInput.WorldSize, ChunkInput.Size, ChunkInput.Isolevel,VoxelOffset,
						LOD, ChunkInput.Scale, ChunkInput.seed);

					FSDispatchCSInterface::Dispatch(SDispatchCSParams, [this, SpawnChunkKey]
						(FSDispatchCSOutput SDispatchCSOutput)
					{
						if(ChunkWorldPointer.IsValid())
						{
							ASChunkWorld* ChunkWorldRef = ChunkWorldPointer.Get();
							ChunkWorldRef->SpawnChunkMesh(SpawnChunkKey, LOD, SDispatchCSOutput);
						}
						NewChunkTasks.Decrement();
					});
				}
				for(FIntVector& DeleteChunkKey : DeleteChunkKeys)
                {
					if(!bRunThread)
						return 0;

					while (NewChunkTasks.GetValue() >= MaxConcurrentTasks)
					{
						if(!bRunThread)
							return 0;
						FPlatformProcess::Sleep(0.01f);
					}
					NewChunkTasks.Increment();
					
                    AsyncTask(ENamedThreads::GameThread, [this, DeleteChunkKey]()
                    {
                    	if(ChunkWorldPointer.IsValid())
                    	{
                    		ASChunkWorld* ChunkWorldRef = ChunkWorldPointer.Get();
                    		ChunkWorldRef->DeleteChunkMesh(DeleteChunkKey, LOD);
                    	}
                    	NewChunkTasks.Decrement();
                     });
                }
				CurrentChunks = CurrentChunkKeys;
				bDispatched = true;
			}
		}
	}
	return 0;
}

void FSChunkWorker::Exit()
{
}


void FSChunkWorker::Stop()
{
	ChunkWorldPointer.Reset();
	
	bRunThread = false;
}