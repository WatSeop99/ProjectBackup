#include "../pch.h"
#include "../Graphics/ImageFilter.h"
#include "../Graphics/Light.h"
#include "../Model/SkinnedMeshModel.h"
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

UINT RenderQueue::Process(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, int processCountPerCommandList)
{
	_ASSERT(threadIndex >= 0 && threadIndex < MAX_RENDER_THREAD_COUNT);
	_ASSERT(pCommandQueue);
	_ASSERT(pCommandListPool);
	_ASSERT(pManager);

	ID3D12GraphicsCommandList* ppCommandLists[64] = { };
	UINT commandListCount = 0;

	ID3D12GraphicsCommandList* pCommandList = nullptr;
	UINT processedCount = 0;
	UINT processedPerCommandList = 0;
	const RenderItem* pRenderItem = nullptr;

	while (pRenderItem = dispatch())
	{
		pCommandList = pCommandListPool->GetCurrentCommandList();
		
		pManager->SetCommonState(pCommandList, pRenderItem->PSOType);
		switch (pRenderItem->ModelType)
		{
			case RenderObjectType_DefaultType:
			case RenderObjectType_SkyboxType:
			case RenderObjectType_MirrorType:
			{
				Model* pModel = (Model*)pRenderItem->pObjectHandle;
				pModel->Render(threadIndex, pCommandList, pManager, pRenderItem->PSOType);
			}
				break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pRenderItem->pObjectHandle;
				pCharacter->Render(threadIndex, pCommandList, pManager, pRenderItem->PSOType);
			}
				break;

			default:
				__debugbreak();
				break;
		}

		++processedCount;
		++processedPerCommandList;

		if (processedPerCommandList > processCountPerCommandList)
		{
			pCommandListPool->Close();
			ppCommandLists[commandListCount] = pCommandList;
			++commandListCount;
			pCommandList = nullptr;
			processedPerCommandList = 0;
		}
	}

	if (processedPerCommandList)
	{
		pCommandListPool->Close();
		ppCommandLists[commandListCount] = pCommandList;
		++commandListCount;
		pCommandList = nullptr;
		processedPerCommandList = 0;
	}
	if (commandListCount)
	{
		pCommandQueue->ExecuteCommandLists(commandListCount, (ID3D12CommandList**)ppCommandLists);
	}
	
	m_RenderObjectCount = 0;
	return commandListCount;
}

UINT RenderQueue::ProcessLight(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, int processCountPerCommandList)
{
	// pDepthStencilResource에 대한 resource barrier 처리 필요.
	_ASSERT(threadIndex >= 0 && threadIndex < MAX_RENDER_THREAD_COUNT);
	_ASSERT(pCommandQueue);
	_ASSERT(pCommandListPool);
	_ASSERT(pManager);

	ID3D12GraphicsCommandList* ppCommandLists[64] = { };
	UINT commandListCount = 0;

	ID3D12GraphicsCommandList* pCommandList = nullptr;
	UINT processedCount = 0;
	UINT processedPerCommandList = 0;
	const RenderItem* pRenderItem = nullptr;

	while (pRenderItem = dispatch())
	{
		pCommandList = pCommandListPool->GetCurrentCommandList();

		Light* pCurLight = (Light*)pRenderItem->pLight;
		Texture* pShadowBuffer = nullptr;
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
		ID3D12Resource* pDepthStencilResource = nullptr;

		pManager->SetCommonState(pCommandList, pRenderItem->PSOType);
		switch (pCurLight->Property.LightType & (LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT))
		{
			case LIGHT_DIRECTIONAL:
				pShadowBuffer = pCurLight->ShadowMap.GetDirectionalLightShadowBufferPtr();
				pCommandList->SetGraphicsRootConstantBufferView(1, pCurLight->ShadowMap.GetShadowConstantBufferForGSPtr()->GetGPUMemAddr());
				break;

			case LIGHT_POINT:
				pShadowBuffer = pCurLight->ShadowMap.GetPointLightShadowBufferPtr();
				pCommandList->SetGraphicsRootConstantBufferView(1, pCurLight->ShadowMap.GetShadowConstantBufferForGSPtr()->GetGPUMemAddr());
				break;

			case LIGHT_SPOT:
				pShadowBuffer = pCurLight->ShadowMap.GetSpotLightShadowBufferPtr();
				break;

			default:
				__debugbreak();
				break;
		}

		dsvHandle = pShadowBuffer->GetDSVHandle();
		pDepthStencilResource = pShadowBuffer->GetResource();
		pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
		switch (pRenderItem->ModelType)
		{
			case RenderObjectType_DefaultType:
			case RenderObjectType_SkyboxType:
			case RenderObjectType_MirrorType:
			{
				Model* pModel = (Model*)pRenderItem->pObjectHandle;
				pModel->Render(threadIndex, pCommandList, pManager, pRenderItem->PSOType);
			}
			break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pRenderItem->pObjectHandle;
				pCharacter->Render(threadIndex, pCommandList, pManager, pRenderItem->PSOType);
			}
			break;

			default:
				__debugbreak();
				break;
		}

		++processedCount;
		++processedPerCommandList;

		if (processedPerCommandList > processCountPerCommandList)
		{
			pCommandListPool->Close();
			ppCommandLists[commandListCount] = pCommandList;
			++commandListCount;
			pCommandList = nullptr;
			processedPerCommandList = 0;
		}
	}

	if (processedPerCommandList)
	{
		pCommandListPool->Close();
		ppCommandLists[commandListCount] = pCommandList;
		++commandListCount;
		pCommandList = nullptr;
		processedPerCommandList = 0;
	}
	if (commandListCount)
	{
		pCommandQueue->ExecuteCommandLists(commandListCount, (ID3D12CommandList**)ppCommandLists);
	}

	m_RenderObjectCount = 0;
	return commandListCount;
}

UINT RenderQueue::ProcessPostProcessing(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, int processCountPerCommandList)
{
	_ASSERT(threadIndex >= 0 && threadIndex < MAX_RENDER_THREAD_COUNT);
	_ASSERT(pCommandQueue);
	_ASSERT(pCommandListPool);
	_ASSERT(pManager);

	ID3D12GraphicsCommandList* ppCommandLists[64] = { };
	UINT commandListCount = 0;

	ID3D12GraphicsCommandList* pCommandList = nullptr;
	UINT processedCount = 0;
	UINT processedPerCommandList = 0;
	const RenderItem* pRenderItem = nullptr;

	while (pRenderItem = dispatch())
	{
		pCommandList = pCommandListPool->GetCurrentCommandList();

		ImageFilter* pImageFilter = (ImageFilter*)pRenderItem->pFilter;
		Mesh* pScreenMesh = (Mesh*)pRenderItem->pObjectHandle;

		pManager->SetCommonState(pCommandList, pRenderItem->PSOType);
		pImageFilter->BeforeRender(threadIndex, pCommandList, pManager, pRenderItem->PSOType);
		pCommandList->IASetVertexBuffers(0, 1, &pScreenMesh->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&pScreenMesh->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(pScreenMesh->Index.Count, 1, 0, 0, 0);
		pImageFilter->AfterRender(pCommandList, pRenderItem->PSOType);

		++processedCount;
		++processedPerCommandList;

		if (processedPerCommandList > processCountPerCommandList)
		{
			pCommandListPool->Close();
			ppCommandLists[commandListCount] = pCommandList;
			++commandListCount;
			pCommandList = nullptr;
			processedPerCommandList = 0;
		}
	}

	if (processedPerCommandList)
	{
		pCommandListPool->Close();
		ppCommandLists[commandListCount] = pCommandList;
		++commandListCount;
		pCommandList = nullptr;
		processedPerCommandList = 0;
	}
	if (commandListCount)
	{
		pCommandQueue->ExecuteCommandLists(commandListCount, (ID3D12CommandList**)ppCommandLists);
	}

	m_RenderObjectCount = 0;
	return commandListCount;
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

void RenderQueue::drawItem(const RenderItem* pRenderItem, ID3D12GraphicsCommandList* pCommandList)
{
	_ASSERT(pRenderItem);
	_ASSERT(pCommandList);

	switch (pRenderItem->ModelType)
	{

	}
}
