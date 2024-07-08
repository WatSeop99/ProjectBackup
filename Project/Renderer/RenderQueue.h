#pragma once

enum eRenderObjectType // same with eModelType
{
	DefaultType = 0,
	SkinnedType,
	SkyboxType,
	MirrorType,
	TotalObjectType
};
struct RenderItem
{
	eRenderObjectType ModelType;
	void* pObjectHandle;
};

class RenderQueue
{
public:
	RenderQueue() = default;
	~RenderQueue() { Clear(); }

	void Initialize(UINT maxItemCount);

	bool Add(const RenderItem* pItem);

	UINT Process(ResourceManager* pManager, UINT threadIndex);

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

