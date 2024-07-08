#include "../pch.h"
#include "RenderQueue.h"

void RenderQueue::Initialize(UINT maxItemCount)
{
	_ASSERT(maxItemCount > 0);

	m_MaxBufferSize = sizeof(RenderItem) * maxItemCount;
	m_pBuffer = (BYTE*)malloc(m_MaxBufferSize);
#ifdef _DEBUG
	if (!m_pBuffer)
	{
		__debugbreak();
	}
#endif
	ZeroMemory(m_pBuffer, m_MaxBufferSize);
}

bool RenderQueue::Add(const RenderItem* pItem)
{
	_ASSERT(pItem);

	bool bRet = false;
	if (m_AllocatedSize + sizeof(RenderItem) > m_MaxBufferSize)
	{
		goto LB_RETURN;
	}

	// allocated and return.
	{
		BYTE* pDest = m_pBuffer + m_AllocatedSize;
		memcpy(pDest, pItem, sizeof(RenderItem));
		m_AllocatedSize += sizeof(RenderItem);
		++m_RenderObjectCount;
		bRet = true;
	}

LB_RETURN:
	return bRet;
}

UINT RenderQueue::Process(ResourceManager* pManager, UINT threadIndex)
{
	_ASSERT(pManager);
	_ASSERT(threadIndex >= 0 && threadIndex < MAX_RENDER_THREAD_COUNT);

	return 0;
}

void RenderQueue::Reset()
{
	m_AllocatedSize = 0;
	m_ReadBufferPos = 0;
}

void RenderQueue::Clear()
{
	if (m_pBuffer)
	{
		free(m_pBuffer);
		m_pBuffer = nullptr;
	}
	m_MaxBufferSize = 0;
	m_AllocatedSize = 0;
	m_ReadBufferPos = 0;
	m_RenderObjectCount = 0;
}

const RenderItem* RenderQueue::dispatch()
{
	const RenderItem* pItem = nullptr;
	if (m_ReadBufferPos + sizeof(RenderItem) > m_AllocatedSize)
	{
		goto LB_RETURN;
	}

	pItem = (const RenderItem*)(m_pBuffer + m_ReadBufferPos);
	m_ReadBufferPos += sizeof(RenderItem);

LB_RETURN:
	return pItem;
}
