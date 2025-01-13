#pragma once
#include "SInstanceRendererExtension.h"
#include "SInstanceSceneProxy.h"
#include "SInstanceMesh.h"

/** Renderer extension to manage the buffer pool and add hooks for GPU culling passes. */
class FSInstanceRendererExtension : public FRenderResource
{
public:
	FSInstanceRendererExtension()
			: bInFrame(false), DiscardId(0)
	{
	}

	virtual ~FSInstanceRendererExtension()
	{
	}

	bool IsInFrame() { return bInFrame; }

	/** Call once to register this extension. */
	void RegisterExtension();

	/** Call once per frame for each mesh/view that has relevance. This allocates the buffers to use for the frame and adds the work to fill the buffers to the queue. */
	FSDrawInstanceBuffers& AddWork(FRHICommandListBase& RHICmdList, FSInstanceSceneProxy const *InProxy, FSceneView const *InMainView, FSceneView const *InCullView);
	/** Submit all the work added by AddWork(). The work fills all of the buffers ready for use by the referencing mesh batches. */
	void SubmitWork(FRDGBuilder & GraphBuilder);

protected:
	//~ Begin FRenderResource Interface
	virtual void ReleaseRHI() override;
	//~ End FRenderResource Interface

private:
	/** Called by renderer at start of render frame. */
	void BeginFrame(FRDGBuilder & GraphBuilder);
	/** Called by renderer at end of render frame. */
	void EndFrame(FRDGBuilder & GraphBuilder);
	void EndFrame();
	
	bool bInit = false;

	/** Flag for frame validation. */
	bool bInFrame;

	/** Buffers to fill. Resources can persist between frames to reduce allocation cost, but contents don't persist. */
	TArray<FSDrawInstanceBuffers> Buffers;
	/** Per buffer frame time stamp of last usage. */
	TArray<uint32> DiscardIds;
	/** Current frame time stamp. */
	uint32 DiscardId;

	/** Arrary of unique scene proxies to render this frame. */
	TArray<FSInstanceSceneProxy const *> SceneProxies;
	/** Arrary of unique main views to render this frame. */
	TArray<FSceneView const *> MainViews;
	/** Arrary of unique culling views to render this frame. */
	TArray<FSceneView const *> CullViews;

	/** Key for each buffer we need to generate. */
	struct FWorkDesc
	{
		int32 ProxyIndex;
		int32 MainViewIndex;
		int32 CullViewIndex;
		int32 BufferIndex;
	};

	/** Keys specifying what to render. */
	TArray<FWorkDesc> WorkDescs;

	/** Sort predicate for FWorkDesc. When rendering we want to batch work by proxy, then by main view. */
	struct FWorkDescSort
	{
		uint32 SortKey(FWorkDesc const &WorkDesc) const
		{
			return (WorkDesc.ProxyIndex << 24) | (WorkDesc.MainViewIndex << 16) | (WorkDesc.CullViewIndex << 8) | WorkDesc.BufferIndex;
		}

		bool operator()(FWorkDesc const &A, FWorkDesc const &B) const
		{
			return SortKey(A) < SortKey(B);
		}
	};
};