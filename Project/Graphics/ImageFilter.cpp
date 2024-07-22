#include "../pch.h"
#include "ConstantDataType.h"
#include "../Renderer/DynamicDescriptorPool.h"
#include "ImageFilter.h"

void ImageFilter::Initialize(Renderer* pRenderer, const int WIDTH, const int HEIGHT)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;

	Cleanup();

	m_ConstantBuffer.Initialize(pRenderer, sizeof(ImageFilterConstant));

	ImageFilterConstant* pConstantData = (ImageFilterConstant*)m_ConstantBuffer.pData;
	pConstantData->DX = 1.0f / WIDTH;
	pConstantData->DY = 1.0f / HEIGHT;
}

void ImageFilter::UpdateConstantBuffers()
{
	m_ConstantBuffer.Upload();
}

void ImageFilter::BeforeRender(Renderer* pRenderer, eRenderPSOType psoSetting, UINT frameIndex)
{
	_ASSERT(pRenderer);
	_ASSERT(m_RTVHandles.size() > 0);
	_ASSERT(m_SRVHandles.size() > 0);

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

	switch (psoSetting)
	{
		case RenderPSOType_Sampling:
		case RenderPSOType_BloomDown:
		case RenderPSOType_BloomUp:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
			BREAK_IF_FAILED(hr);

			const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_RTVHandles[0].pResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t0
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[0].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// b4
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_ConstantBuffer.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->ResourceBarrier(1, &BARRIER);
			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			pCommandList->OMSetRenderTargets(1, &m_RTVHandles[0].CPUHandle, FALSE, nullptr);
		}
		break;

		case RenderPSOType_Combine:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 4);
			BREAK_IF_FAILED(hr);

			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t0
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[0].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t1
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[1].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t2
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[2].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// b4
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_ConstantBuffer.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			pCommandList->OMSetRenderTargets(1, &m_RTVHandles[frameIndex].CPUHandle, FALSE, nullptr);
		}
		break;

		default:
			__debugbreak();
			break;
	}
}

void ImageFilter::BeforeRender(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ResourceManager* pManager, int psoSetting)
{
	_ASSERT(pCommandList);
	_ASSERT(pDescriptorPool);
	_ASSERT(pManager);
	_ASSERT(m_RTVHandles.size() > 0);
	_ASSERT(m_SRVHandles.size() > 0);

	HRESULT hr = S_OK;

	ID3D12Device5* pDevice = pManager->m_pDevice;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

	switch (psoSetting)
	{
		case RenderPSOType_Sampling:
		case RenderPSOType_BloomDown:
		case RenderPSOType_BloomUp:
		{
			hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
			BREAK_IF_FAILED(hr);

			const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_RTVHandles[0].pResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t0
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[0].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// b4
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_ConstantBuffer.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->ResourceBarrier(1, &BARRIER);
			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			pCommandList->OMSetRenderTargets(1, &m_RTVHandles[0].CPUHandle, FALSE, nullptr);
		}
		break;

		case RenderPSOType_Combine:
		{
			hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 4);
			BREAK_IF_FAILED(hr);

			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t0
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[0].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t1
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[1].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t2
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[2].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// b4
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_ConstantBuffer.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			pCommandList->OMSetRenderTargets(1, &m_RTVHandles[*(pManager->m_pFrameIndex)].CPUHandle, FALSE, nullptr);
		}
		break;

		default:
			__debugbreak();
			break;
	}
}

void ImageFilter::AfterRender(Renderer* pRenderer, eRenderPSOType psoSetting, UINT frameIndex)
{
	_ASSERT(pRenderer);
	_ASSERT(m_RTVHandles.size() > 0);
	_ASSERT(m_SRVHandles.size() > 0);

	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();

	switch (psoSetting)
	{
		case RenderPSOType_Sampling:
		case RenderPSOType_BloomDown:
		case RenderPSOType_BloomUp:
		{
			const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_RTVHandles[0].pResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
			pCommandList->ResourceBarrier(1, &BARRIER);
		}
		break;

		case RenderPSOType_Combine:
			break;

		default:
			__debugbreak();
			break;
	}
}

void ImageFilter::AfterRender(ID3D12GraphicsCommandList* pCommandList, int psoSetting)
{
	_ASSERT(pCommandList);
	_ASSERT(m_RTVHandles.size() > 0);
	_ASSERT(m_SRVHandles.size() > 0);

	switch (psoSetting)
	{
		case RenderPSOType_Sampling:
		case RenderPSOType_BloomDown:
		case RenderPSOType_BloomUp:
		{
			const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_RTVHandles[0].pResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
			pCommandList->ResourceBarrier(1, &BARRIER);
		}
		break;

		case RenderPSOType_Combine:
			break;

		default:
			__debugbreak();
			break;
	}
}

void ImageFilter::Cleanup()
{
	m_SRVHandles.clear();
	m_RTVHandles.clear();
	m_ConstantBuffer.Cleanup();
}

void ImageFilter::SetSRVOffsets(Renderer* pRenderer, const std::vector<ImageResource>& SRVs)
{
	_ASSERT(pRenderer);

	ResourceManager* pManager = pRenderer->GetResourceManager();
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(pManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE startSrvHandle(srvHandle);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	m_SRVHandles.clear();
	m_SRVHandles.resize(SRVs.size());
	for (UINT64 i = 0, size = SRVs.size(); i < size; ++i)
	{
		m_SRVHandles[i] = { SRVs[i].pResource, srvHandle.Offset(SRVs[i].SRVOffset, CBV_SRV_DESCRIPTOR_SIZE) };
		srvHandle = startSrvHandle;
	}
}

void ImageFilter::SetRTVOffsets(Renderer* pRenderer, const std::vector<ImageResource>& RTVs)
{
	_ASSERT(pRenderer);

	ResourceManager* pManager = pRenderer->GetResourceManager();
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE startRtvHandle(rtvHandle);
	const UINT RTV_DESCRIPTOR_SIZE = pManager->m_RTVDescriptorSize;

	m_RTVHandles.clear();
	m_RTVHandles.resize(RTVs.size());
	for (UINT64 i = 0, size = RTVs.size(); i < size; ++i)
	{
		m_RTVHandles[i] = { RTVs[i].pResource, rtvHandle.Offset(RTVs[i].RTVOffset, RTV_DESCRIPTOR_SIZE) };
		rtvHandle = startRtvHandle;
	}
}

void ImageFilter::SetDescriptorHeap(Renderer* pRenderer)
{
	_ASSERT(pRenderer);

	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12DescriptorHeap* pCBVSRVHeap = pManager->m_pCBVSRVUAVHeap;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvLastHandle;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = m_ConstantBuffer.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_ConstantBuffer.GetBufferSize();
	
	if (m_ConstantBuffer.GetCBVHandle().ptr == 0xffffffffffffffff)
	{
		cbvSrvLastHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_CBVSRVUAVHeapSize, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		m_ConstantBuffer.SetCBVHandle(cbvSrvLastHandle);
		++(pManager->m_CBVSRVUAVHeapSize);
	}
	else
	{
		cbvSrvLastHandle = m_ConstantBuffer.GetCBVHandle();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	}
}
