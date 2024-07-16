#pragma once

enum eRenderObjectType // same with eModelType
{
	RenderObjectType_DefaultType = 0,
	RenderObjectType_SkinnedType,
	RenderObjectType_SkyboxType,
	RenderObjectType_MirrorType,
	RenderObjectType_TotalObjectType
};
enum eRenderPSOType // same with ePipelineStateSetting
{
	RenderPSOType_Default,
	RenderPSOType_Skinned,
	RenderPSOType_Skybox,
	RenderPSOType_StencilMask,
	RenderPSOType_MirrorBlend,
	RenderPSOType_ReflectionDefault,
	RenderPSOType_ReflectionSkinned,
	RenderPSOType_ReflectionSkybox,
	RenderPSOType_DepthOnlyDefault,
	RenderPSOType_DepthOnlySkinned,
	RenderPSOType_DepthOnlyCubeDefault,
	RenderPSOType_DepthOnlyCubeSkinned,
	RenderPSOType_DepthOnlyCascadeDefault,
	RenderPSOType_DepthOnlyCascadeSkinned,
	RenderPSOType_Sampling,
	RenderPSOType_BloomDown,
	RenderPSOType_BloomUp,
	RenderPSOType_Combine,
	RenderPSOType_Wire,
	RenderPSOType_PipelineStateCount,
};
struct RenderItem
{
	eRenderObjectType ModelType;
	eRenderPSOType PSOType;
	void* pObjectHandle;
	void* pLight; // for shadow pass.
	void* pFilter;
};

class RenderQueue
{
public:
	RenderQueue() = default;
	~RenderQueue() { Clear(); }

	void Initialize(UINT maxItemCount);

	bool Add(const RenderItem* pItem);

	UINT Process(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, DynamicDescriptorPool* pDescriptorPool, int processCountPerCommandList);
	UINT ProcessLight(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, DynamicDescriptorPool* pDescriptorPool, int processCountPerCommandList);
	UINT ProcessPostProcessing(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, DynamicDescriptorPool* pDescriptorPool, int processCountPerCommandList);

	void Reset();

	void Clear();

protected:
	const RenderItem* dispatch();
	
private:
	BYTE* m_pBuffer = nullptr;
	UINT m_MaxBufferSize = 0;
	UINT m_AllocatedSize = 0;
	UINT m_ReadBufferPos = 0;
	UINT m_RenderObjectCount = 0;
};
