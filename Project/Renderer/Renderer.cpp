#include "../pch.h"
#include "../Model/GeometryGenerator.h"
#include "../Util/Utility.h"
#include "Renderer.h"

Renderer* g_pRendrer = nullptr;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return g_pRendrer->MsgProc(hWnd, msg, wParam, lParam);
}

Renderer::Renderer()
{
	g_pRendrer = this;
	m_Camera.SetAspectRatio((float)m_ScreenWidth / (float)m_ScreenHeight);
}

Renderer::~Renderer()
{
	g_pRendrer = nullptr;
	Cleanup();
}

void Renderer::Initizlie(InitialData* pIntialData)
{
	m_pRenderObjects = pIntialData->pRenderObjects;
	m_pLights = pIntialData->pLights;
	m_pLightSpheres = pIntialData->pLightSpheres;

	m_pMirror = pIntialData->pMirror;
	m_pMirrorPlane = pIntialData->pMirrorPlane;

	initScene();
	initDescriptorHeap(pIntialData->pEnvTexture, pIntialData->pIrradianceTexture, pIntialData->pSpecularTexture, pIntialData->pBRDFTexture);

	PostProcessor::PostProcessingBuffers config =
	{
		{ m_pRenderTargets[0], m_pRenderTargets[1] },
		m_pFloatBuffer,
		m_pPrevBuffer,
		&m_GlobalConstant,
		m_MainRenderTargetOffset, m_MainRenderTargetOffset + 1, m_FloatBufferSRVOffset, m_PrevBufferSRVOffset
	};
	Renderer* pRenderer = this;
	m_PostProcessor.Initizlie(pRenderer, config, m_ScreenWidth, m_ScreenHeight, 2);
	m_PostProcessor.SetDescriptorHeap(pRenderer);

	m_DynamicDescriptorPool.Initialize(m_pDevice, 1024);

	m_ScreenViewport.TopLeftX = 0;
	m_ScreenViewport.TopLeftY = 0;
	m_ScreenViewport.Width = (float)m_ScreenWidth;
	m_ScreenViewport.Height = (float)m_ScreenHeight;
	m_ScreenViewport.MinDepth = 0.0f;
	m_ScreenViewport.MaxDepth = 1.0f;

	m_ScissorRect.left = 0;
	m_ScissorRect.top = 0;
	m_ScissorRect.right = m_ScreenWidth;
	m_ScissorRect.bottom = m_ScreenHeight;

	m_PhysicsManager.Initialize(2);
}

void Renderer::Update(const float DELTA_TIME)
{
	m_Camera.UpdateKeyboard(DELTA_TIME, &m_Keyboard);
	processMouseControl(DELTA_TIME);

	updateGlobalConstants(DELTA_TIME);
	updateLightConstants(DELTA_TIME);
}

void Renderer::Render()
{
	beginRender();

	renderShadowmap();
	renderObject();
	renderMirror();
	renderObjectBoundingModel();
	postProcess();

	endRender();

	present();
}

void Renderer::ProcessByThread(UINT threadIndex, ResourceManager* pManager, int renderPass)
{
	_ASSERT(threadIndex >= 0 && threadIndex < m_RenderThreadCount);
	_ASSERT(pManager);

	ID3D12CommandQueue* pCommandQueue = m_pCommandQueue;
	CommandListPool* pCommandListPool = m_pppCommandListPool[m_FrameIndex][threadIndex];
	DynamicDescriptorPool* pDescriptorPool = m_pppDescriptorPool[m_FrameIndex][threadIndex];

	switch (renderPass)
	{
		case RenderPass_Shadow:
			// pCommandQueue = m_ppCommandQueue[RenderPass_Shadow];
			m_pppRenderQueue[renderPass][threadIndex]->ProcessLight(threadIndex, pCommandQueue, pCommandListPool, pManager, pDescriptorPool, 100);
			break;

		case RenderPass_Object:
		case RenderPass_Mirror:
		case RenderPass_Collider:
		{
			ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
			CD3DX12_CPU_DESCRIPTOR_HANDLE floatBufferRtvHandle(pManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, m_pResourceManager->m_RTVDescriptorSize);
			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(pManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());
			// pCommandQueue = m_ppCommandQueue[RenderPass_MainRender];

			pCommandList->RSSetViewports(1, &m_ScreenViewport);
			pCommandList->RSSetScissorRects(1, &m_ScissorRect);
			pCommandList->OMSetRenderTargets(1, &floatBufferRtvHandle, FALSE, &dsvHandle);
			m_pppRenderQueue[renderPass][threadIndex]->Process(threadIndex, pCommandQueue, pCommandListPool, pManager, pDescriptorPool, 100);
		}
		break;

		default:
			__debugbreak();
			break;
	}

	long curActiveThreadCount = _InterlockedDecrement(&m_pActiveThreadCounts[renderPass]);
	if (curActiveThreadCount == 0)
	{
		SetEvent(m_phCompletedEvents[renderPass]);
	}
}

void Renderer::Cleanup()
{
#ifdef USE_MULTI_THREAD

	if (m_pThreadDescList)
	{
		for (UINT i = 0; i < m_RenderThreadCount; ++i)
		{
			SetEvent(m_pThreadDescList[i].hEventList[RenderThreadEventType_Desctroy]);

			WaitForSingleObject(m_pThreadDescList[i].hThread, INFINITE);
			CloseHandle(m_pThreadDescList[i].hThread);
			m_pThreadDescList[i].hThread = nullptr;

			for (UINT j = 0; j < RenderThreadEventType_Count; ++j)
			{
				CloseHandle(m_pThreadDescList[i].hEventList[j]);
				m_pThreadDescList[i].hEventList[j] = nullptr;
			}
		}

		delete[] m_pThreadDescList;
		m_pThreadDescList = nullptr;
	}
	for (UINT i = 0; i < RenderPass_RenderPassCount; ++i)
	{
		CloseHandle(m_phCompletedEvents[i]);
		m_phCompletedEvents[i] = nullptr;
	}

#endif

	fence();
	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		waitForFenceValue(m_LastFenceValues[i]);
	}

	if (m_pResourceManager)
	{
		delete m_pResourceManager;
		m_pResourceManager = nullptr;
	}

	if (m_hFenceEvent)
	{
		CloseHandle(m_hFenceEvent);
		m_hFenceEvent = nullptr;
	}
	m_FenceValue = 0;
	SAFE_RELEASE(m_pFence);

	m_GlobalConstant.Cleanup();
	m_LightConstant.Cleanup();
	m_ReflectionGlobalConstant.Cleanup();

	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		for (UINT j = 0; j < MAX_RENDER_THREAD_COUNT; ++j)
		{
			if (m_pppCommandListPool[i][j])
			{
				delete m_pppCommandListPool[i][j];
				m_pppCommandListPool[i][j] = nullptr;
			}
			if (m_pppDescriptorPool[i][j])
			{
				delete m_pppDescriptorPool[i][j];
				m_pppDescriptorPool[i][j] = nullptr;
			}
		}
	}
	for (int i = 0; i < RenderPass_RenderPassCount; ++i)
	{
		SAFE_RELEASE(m_ppCommandQueue[i]);
		for (UINT j = 0; j < MAX_RENDER_THREAD_COUNT; ++j)
		{
			if (m_pppRenderQueue[i][j])
			{
				delete m_pppRenderQueue[i][j];
				m_pppRenderQueue[i][j] = nullptr;
			}
		}
	}

	m_PostProcessor.Cleanup();
	m_DynamicDescriptorPool.Cleanup();

	SAFE_RELEASE(m_pDefaultDepthStencil);

	SAFE_RELEASE(m_pFloatBuffer);
	SAFE_RELEASE(m_pPrevBuffer);
	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		SAFE_RELEASE(m_pRenderTargets[i]);
	}

	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		SAFE_RELEASE(m_ppCommandList[i]);
		SAFE_RELEASE(m_ppCommandAllocator[i]);
	}
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pCommandQueue);

	if (m_pDevice)
	{
		ULONG refCount = m_pDevice->Release();

#ifdef _DEBUG
		// debug layer release.
		if (refCount)
		{
			IDXGIDebug1* pDebug = nullptr;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
			{
				pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
				pDebug->Release();
			}
			__debugbreak();
		}
#endif

		m_pDevice = nullptr;
	}
}

LRESULT Renderer::MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_SIZE:
		{
			// ȭ�� �ػ󵵰� �ٲ�� SwapChain�� �ٽ� ����.
			if (m_pSwapChain)
			{
				fence();
				for (int i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
				{
					waitForFenceValue(m_LastFenceValues[i]);
				}

				m_ScreenWidth = (UINT)LOWORD(lParam);
				m_ScreenHeight = (UINT)HIWORD(lParam);

				m_ScreenViewport.Width = (float)m_ScreenWidth;
				m_ScreenViewport.Height = (float)m_ScreenHeight;
				m_ScissorRect.right = m_ScreenWidth;
				m_ScissorRect.bottom = m_ScreenHeight;

				// �����찡 Minimize ��忡���� screenWidth/Height�� 0.
				if (m_ScreenWidth && m_ScreenHeight)
				{
#ifdef _DEBUG
					char debugString[256] = { 0, };
					OutputDebugStringA("Resize SwapChain to ");
					sprintf(debugString, "%d", m_ScreenWidth);
					OutputDebugStringA(debugString);
					OutputDebugStringA(" ");
					sprintf(debugString, "%d", m_ScreenHeight);
					OutputDebugStringA(debugString);
					OutputDebugStringA("\n");
#endif 

					// ���� ���� �ʱ�ȭ.
					for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
					{
						m_pRenderTargets[i]->Release();
						m_pRenderTargets[i] = nullptr;
					}
					m_pFloatBuffer->Release();
					m_pFloatBuffer = nullptr;
					m_pPrevBuffer->Release();
					m_pPrevBuffer = nullptr;
					m_pDefaultDepthStencil->Release();
					m_pDefaultDepthStencil = nullptr;
					m_PostProcessor.Cleanup();
					m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

					// swap chain resize.
					m_pSwapChain->ResizeBuffers(0,					 // ���� ���� ����.
												m_ScreenWidth,		 // �ػ� ����.
												m_ScreenHeight,
												DXGI_FORMAT_UNKNOWN, // ���� ���� ����.
												0);
					// float buffer, prev buffer �����.
					{
						HRESULT hr = S_OK;

						D3D12_RESOURCE_DESC resourceDesc = {};
						resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
						resourceDesc.Alignment = 0;
						resourceDesc.Width = m_ScreenWidth;
						resourceDesc.Height = m_ScreenHeight;
						resourceDesc.DepthOrArraySize = 1;
						resourceDesc.MipLevels = 1;
						resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
						resourceDesc.SampleDesc.Count = 1;
						resourceDesc.SampleDesc.Quality = 0;
						resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
						resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

						D3D12_HEAP_PROPERTIES heapProps = {};
						heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
						heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
						heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
						heapProps.CreationNodeMask = 1;
						heapProps.VisibleNodeMask = 1;

						hr = m_pDevice->CreateCommittedResource(&heapProps,
																D3D12_HEAP_FLAG_NONE,
																&resourceDesc,
																D3D12_RESOURCE_STATE_COMMON,
																nullptr,
																IID_PPV_ARGS(&m_pFloatBuffer));
						BREAK_IF_FAILED(hr);
						m_pFloatBuffer->SetName(L"FloatBuffer");


						resourceDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
						resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
						hr = m_pDevice->CreateCommittedResource(&heapProps,
																D3D12_HEAP_FLAG_NONE,
																&resourceDesc,
																D3D12_RESOURCE_STATE_COMMON,
																nullptr,
																IID_PPV_ARGS(&m_pPrevBuffer));
						BREAK_IF_FAILED(hr);
						m_pPrevBuffer->SetName(L"PrevBuffer");
					}

					// ����� ũ�⿡ ���� rtv, dsv, srv �����.
					{
						const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
						CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset, RTV_DESCRIPTOR_SIZE);
						CD3DX12_CPU_DESCRIPTOR_HANDLE startRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart());
						for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
						{
							m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i]));
							m_pDevice->CreateRenderTargetView(m_pRenderTargets[i], nullptr, rtvHandle);
							rtvHandle.Offset(1, m_pResourceManager->m_RTVDescriptorSize);
						}
						rtvHandle = startRtvHandle;

						D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
						rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
						rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
						rtvDesc.Texture2D.MipSlice = 0;
						rtvDesc.Texture2D.PlaneSlice = 0;
						rtvHandle.Offset(m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
						m_pDevice->CreateRenderTargetView(m_pFloatBuffer, &rtvDesc, rtvHandle);
					}
					{
						HRESULT hr = S_OK;

						D3D12_RESOURCE_DESC dsvDesc;
						dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
						dsvDesc.Alignment = 0;
						dsvDesc.Width = m_ScreenWidth;
						dsvDesc.Height = m_ScreenHeight;
						dsvDesc.DepthOrArraySize = 1;
						dsvDesc.MipLevels = 1;
						dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
						dsvDesc.SampleDesc.Count = 1;
						dsvDesc.SampleDesc.Quality = 0;
						dsvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
						dsvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

						D3D12_HEAP_PROPERTIES heapProps;
						heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
						heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
						heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
						heapProps.CreationNodeMask = 1;
						heapProps.VisibleNodeMask = 1;

						D3D12_CLEAR_VALUE clearValue;
						clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
						clearValue.DepthStencil.Depth = 1.0f;
						clearValue.DepthStencil.Stencil = 0;

						hr = m_pDevice->CreateCommittedResource(&heapProps,
																D3D12_HEAP_FLAG_NONE,
																&dsvDesc,
																D3D12_RESOURCE_STATE_DEPTH_WRITE,
																&clearValue,
																IID_PPV_ARGS(&m_pDefaultDepthStencil));
						BREAK_IF_FAILED(hr);
						m_pDefaultDepthStencil->SetName(L"DefaultDepthStencil");

						D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
						depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
						depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
						depthStencilViewDesc.Texture2D.MipSlice = 0;

						CD3DX12_CPU_DESCRIPTOR_HANDLE depthHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());
						m_pDevice->CreateDepthStencilView(m_pDefaultDepthStencil, &depthStencilViewDesc, depthHandle);
					}
					{
						const UINT SRV_DESCRIPTOR_SIZE = m_pResourceManager->m_CBVSRVUAVDescriptorSize;
						CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_pResourceManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferSRVOffset, SRV_DESCRIPTOR_SIZE);
						CD3DX12_CPU_DESCRIPTOR_HANDLE startSrvHandle(m_pResourceManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart());

						D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						srvDesc.Texture2D.MostDetailedMip = 0;
						srvDesc.Texture2D.MipLevels = 1;
						srvDesc.Texture2D.PlaneSlice = 0;
						srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

						// float buffer srv
						srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
						m_pDevice->CreateShaderResourceView(m_pFloatBuffer, &srvDesc, srvHandle);

						// prev buffer srv
						srvHandle = startSrvHandle;
						srvHandle.Offset(m_PrevBufferSRVOffset, SRV_DESCRIPTOR_SIZE);
						// srvDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
						m_pDevice->CreateShaderResourceView(m_pPrevBuffer, &srvDesc, srvHandle);
					}

					PostProcessor::PostProcessingBuffers config =
					{
						{ m_pRenderTargets[0], m_pRenderTargets[1] },
						m_pFloatBuffer,
						m_pPrevBuffer,
						&m_GlobalConstant,
						m_MainRenderTargetOffset, m_MainRenderTargetOffset + 1, m_FloatBufferSRVOffset, m_PrevBufferSRVOffset
					};
					Renderer* pRenderer = this;
					m_PostProcessor.Initizlie(pRenderer, config, m_ScreenWidth, m_ScreenHeight, 4);
					m_PostProcessor.SetDescriptorHeap(pRenderer);
					m_Camera.SetAspectRatio((float)m_ScreenWidth / (float)m_ScreenHeight);
				}
			}
		}
		break;

		case WM_SYSCOMMAND:
		{
			if ((wParam & 0xfff0) == SC_KEYMENU) // ALTŰ ��Ȱ��ȭ.
			{
				return 0;
			}
		}
		break;

		case WM_MOUSEMOVE:
			onMouseMove(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_LBUTTONDOWN:
		{
			if (!m_Mouse.bMouseLeftButton)
			{
				m_Mouse.bMouseDragStartFlag = true; // �巡�׸� ���� �����ϴ��� Ȯ��.
			}
			m_Mouse.bMouseLeftButton = true;
			onMouseClick(LOWORD(lParam), HIWORD(lParam));
		}
		break;

		case WM_LBUTTONUP:
			m_Mouse.bMouseLeftButton = false;
			break;

		case WM_RBUTTONDOWN:
		{
			if (!m_Mouse.bMouseRightButton)
			{
				m_Mouse.bMouseDragStartFlag = true; // �巡�׸� ���� �����ϴ��� Ȯ��.
			}
			m_Mouse.bMouseRightButton = true;
		}
		break;

		case WM_RBUTTONUP:
			m_Mouse.bMouseRightButton = false;
			break;

		case WM_KEYDOWN:
		{
			m_Keyboard.bPressed[wParam] = true;
			if (wParam == VK_ESCAPE) // ESCŰ ����.
			{
				DestroyWindow(m_hMainWindow);
			}
			if (wParam == VK_SPACE)
			{
				(*m_pLights)[1].bRotated = !(*m_pLights)[1].bRotated;
			}
		}
		break;

		case WM_KEYUP:
		{
			if (wParam == 'F')  // fŰ ����Ī ����.
			{
				m_Camera.bUseFirstPersonView = !m_Camera.bUseFirstPersonView;
			}

			m_Keyboard.bPressed[wParam] = false;
		}
		break;

		case WM_MOUSEWHEEL:
			m_Mouse.WheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		default:
			break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void Renderer::initMainWidndow()
{
	WNDCLASSEX wc =
	{
		sizeof(WNDCLASSEX),			// cbSize
		CS_HREDRAW | CS_VREDRAW,	// style
		WndProc,					// lpfnWndProc
		0,							// cbClsExtra
		0,							// cbWndExtra
		GetModuleHandle(NULL),		// hInstance
		NULL, 						// hIcon
		NULL,						// hCursor
		(HBRUSH)(COLOR_WINDOW + 1),	// hbrBackground
		nullptr,					// lpszMenuName
		L"OWL_",					// lpszClassName
		NULL						// hIconSm
	};

	if (!RegisterClassEx(&wc))
	{
		__debugbreak();
	}

	RECT wr = { 0, 0, (long)m_ScreenWidth, (long)m_ScreenHeight };
	BOOL bResult = AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
	m_hMainWindow = CreateWindow(wc.lpszClassName,
								 L"DX12",
								 WS_OVERLAPPEDWINDOW,
								 100,							// ������ ���� ����� x ��ǥ
								 100,							// ������ ���� ����� y ��ǥ
								 wr.right - wr.left,			// ������ ���� ���� �ػ�
								 wr.bottom - wr.top,			// ������ ���� ���� �ػ�
								 NULL, NULL, wc.hInstance, NULL);

	if (!m_hMainWindow)
	{
		__debugbreak();
	}

	ShowWindow(m_hMainWindow, SW_SHOWDEFAULT);
	UpdateWindow(m_hMainWindow);
}

void Renderer::initDirect3D()
{
	HRESULT hr = S_OK;

	ID3D12Debug* pDebugController = nullptr;
	UINT createDeviceFlags = 0;
	UINT createFactoryFlags = 0;

#ifdef _DEBUG
	ID3D12Debug6* pDebugController6 = nullptr;

	hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController));
	if (SUCCEEDED(hr))
	{
		pDebugController->EnableDebugLayer();

		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

		hr = pDebugController->QueryInterface(IID_PPV_ARGS(&pDebugController6));
		if (SUCCEEDED(hr))
		{
			pDebugController6->SetEnableGPUBasedValidation(TRUE);
			pDebugController6->SetEnableAutoName(TRUE);
			SAFE_RELEASE(pDebugController6);
		}
	}
#endif

	const D3D_FEATURE_LEVEL FEATURE_LEVELS[] =
	{
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	UINT numFeatureLevels = _countof(FEATURE_LEVELS);
	IDXGIFactory5* pFactory5 = nullptr;
	IDXGIAdapter3* pAdapter3 = nullptr;

	hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&pFactory5));
	BREAK_IF_FAILED(hr);

	for (UINT featureLevelIndex = 0; featureLevelIndex < numFeatureLevels; ++featureLevelIndex)
	{
		UINT adapterIndex = 0;
		while (pFactory5->EnumAdapters1(adapterIndex, (IDXGIAdapter1**)(&pAdapter3)) != DXGI_ERROR_NOT_FOUND)
		{
			pAdapter3->GetDesc2(&m_AdapterDesc);
			hr = D3D12CreateDevice(pAdapter3, FEATURE_LEVELS[featureLevelIndex], IID_PPV_ARGS(&m_pDevice));
			if (SUCCEEDED(hr))
			{
				goto LB_EXIT;
			}

			SAFE_RELEASE(pAdapter3);
			++adapterIndex;
		}
	}
LB_EXIT:
	BREAK_IF_FAILED(hr);
	m_pDevice->SetName(L"D3DDevice");

	if (pDebugController)
	{
		SetDebugLayerInfo(m_pDevice);
	}

	UINT physicalCoreCount = 0;
	UINT logicalCoreCount = 0;
	GetPhysicalCoreCount(&physicalCoreCount, &logicalCoreCount);
	m_RenderThreadCount = physicalCoreCount;
	if (m_RenderThreadCount > MAX_RENDER_THREAD_COUNT)
	{
		m_RenderThreadCount = MAX_RENDER_THREAD_COUNT;
	}

	// create command queue
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		hr = m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue));
		BREAK_IF_FAILED(hr);
		m_pCommandQueue->SetName(L"CommandQueue");

		for (int i = 0; i < RenderPass_RenderPassCount; ++i)
		{
			hr = m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_ppCommandQueue[i]));
			BREAK_IF_FAILED(hr);

			wchar_t debugName[256];
			swprintf_s(debugName, 256, L"CommandQueue%d", i);
			m_ppCommandQueue[i]->SetName(debugName);
		}
	}

	// create swapchain
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = m_ScreenWidth;
		swapChainDesc.Height = m_ScreenHeight;
		// swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		swapChainDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = SWAP_CHAIN_FRAME_COUNT;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
		/*fsSwapChainDesc.RefreshRate.Numerator = 60;
		fsSwapChainDesc.RefreshRate.Denominator = 1;*/
		fsSwapChainDesc.Windowed = TRUE;

		IDXGISwapChain1* pSwapChain1 = nullptr;
		hr = pFactory5->CreateSwapChainForHwnd(m_pCommandQueue, m_hMainWindow, &swapChainDesc, &fsSwapChainDesc, nullptr, &pSwapChain1);
		BREAK_IF_FAILED(hr);
		pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
		SAFE_RELEASE(pSwapChain1);

		m_BackBufferFormat = swapChainDesc.Format;

		m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
	}

	// create command list
	{
		for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
		{
			WCHAR debugName[256];

			hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_ppCommandAllocator[i]));
			BREAK_IF_FAILED(hr);
			swprintf_s(debugName, 256, L"CommandAllocator%u", i);
			m_ppCommandAllocator[i]->SetName(debugName);

			hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_ppCommandAllocator[i], nullptr, IID_PPV_ARGS(&m_ppCommandList[i]));
			BREAK_IF_FAILED(hr);
			swprintf_s(debugName, 256, L"CommandList%u", i);
			m_ppCommandList[i]->SetName(debugName);

			// Command lists are created in the recording state, but there is nothing
			// to record yet. The main loop expects it to be closed, so close it now.
			m_ppCommandList[i]->Close();
		}
	}

	// create render queue and command list pool, descriptor pool
	{
		for (int i = 0; i < RenderPass_RenderPassCount; ++i)
		{
			for (UINT j = 0; j < MAX_RENDER_THREAD_COUNT; ++j)
			{
				m_pppRenderQueue[i][j] = new RenderQueue;
				m_pppRenderQueue[i][j]->Initialize(8192);
			}
		}

		for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; i++)
		{
			for (UINT j = 0; j < m_RenderThreadCount; j++)
			{
				m_pppCommandListPool[i][j] = new CommandListPool;
				m_pppCommandListPool[i][j]->Initialize(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, 256);

				m_pppDescriptorPool[i][j] = new DynamicDescriptorPool;
				m_pppDescriptorPool[i][j]->Initialize(m_pDevice, 4096 * 9);
			}
		}
	}

	// create fence
	{
		hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
		BREAK_IF_FAILED(hr);

		m_FenceValue = 0;

		m_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}

	// create float buffer and prev buffer
	{
		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = m_ScreenWidth;
		resourceDesc.Height = m_ScreenHeight;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		hr = m_pDevice->CreateCommittedResource(&heapProps,
												D3D12_HEAP_FLAG_NONE,
												&resourceDesc,
												D3D12_RESOURCE_STATE_COMMON,
												nullptr,
												IID_PPV_ARGS(&m_pFloatBuffer));
		BREAK_IF_FAILED(hr);
		m_pFloatBuffer->SetName(L"FloatBuffer");


		// resourceDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		hr = m_pDevice->CreateCommittedResource(&heapProps,
												D3D12_HEAP_FLAG_NONE,
												&resourceDesc,
												D3D12_RESOURCE_STATE_COMMON,
												nullptr,
												IID_PPV_ARGS(&m_pPrevBuffer));
		BREAK_IF_FAILED(hr);
		m_pPrevBuffer->SetName(L"PrevBuffer");
	}

	ResourceManager::InitialData initData = { m_pDevice, m_pCommandQueue, m_ppCommandAllocator, m_ppCommandList, &m_DynamicDescriptorPool, m_hFenceEvent, m_pFence, &m_FrameIndex, &m_FenceValue, m_LastFenceValues };
	m_pResourceManager = new ResourceManager;
	m_pResourceManager->Initialize(&initData);
	m_pResourceManager->InitRTVDescriptorHeap(16);
	m_pResourceManager->InitDSVDescriptorHeap(8);
	m_pResourceManager->InitCBVSRVUAVDescriptorHeap(1024);
	
#ifdef USE_MULTI_THREAD
	// create thread and event
	{
		initRenderThreadPool(m_RenderThreadCount);
		for (int i = 0; i < RenderPass_RenderPassCount; ++i)
		{
			m_phCompletedEvents[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		}
	}
#endif

	SAFE_RELEASE(pDebugController);
	SAFE_RELEASE(pFactory5);
	SAFE_RELEASE(pAdapter3);
}

void Renderer::initScene()
{
	// m_Camera.Reset(Vector3(3.74966f, 5.03645f, -2.54918f), -0.819048f, 0.741502f);
	m_Camera.Reset(Vector3(0.0f, 3.0f, -2.0f), -0.819048f, 0.741502f);

	Renderer* pRenderer = this;
	m_GlobalConstant.Initialize(pRenderer, sizeof(GlobalConstant));
	m_LightConstant.Initialize(pRenderer, sizeof(LightConstant));
	m_ReflectionGlobalConstant.Initialize(pRenderer, sizeof(GlobalConstant));
	m_pResourceManager->SetGlobalConstants(&m_GlobalConstant, &m_LightConstant, &m_ReflectionGlobalConstant);

	// ���� global constant ����.
	{
		GlobalConstant* pGlobalData = (GlobalConstant*)m_GlobalConstant.pData;
		pGlobalData->StrengthIBL = 0.3f;

		LightConstant* pLightData = (LightConstant*)m_LightConstant.pData;
		for (int i = 0; i < MAX_LIGHTS; ++i)
		{
			memcpy(&pLightData->Lights[i], &(*m_pLights)[i].Property, sizeof(LightProperty));
		}
	}
}

void Renderer::initDescriptorHeap(Texture* pEnvTexture, Texture* pIrradianceTexture, Texture* pSpecularTexture, Texture* pBRDFTexture)
{
	_ASSERT(pEnvTexture);
	_ASSERT(pIrradianceTexture);
	_ASSERT(pSpecularTexture);
	_ASSERT(pBRDFTexture);

	HRESULT hr = S_OK;
	Renderer* pRenderer = this;
	const UINT RTV_DESCRITOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = m_pResourceManager->m_CBVSRVUAVDescriptorSize;

	// render target.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_pResourceManager->m_RTVHeapSize, RTV_DESCRITOR_SIZE);

		m_MainRenderTargetOffset = m_pResourceManager->m_RTVHeapSize;

		for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
		{
			m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i]));
			m_pRenderTargets[i]->SetName(L"RenderTarget");
			m_pDevice->CreateRenderTargetView(m_pRenderTargets[i], nullptr, rtvHandle);
			rtvHandle.Offset(1, RTV_DESCRITOR_SIZE);
		}

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;
		m_pDevice->CreateRenderTargetView(m_pFloatBuffer, &rtvDesc, rtvHandle);
		m_FloatBufferRTVOffset = 2;

		// 2 backbuffer + 1 floatbuffer.
		m_pResourceManager->m_RTVHeapSize += SWAP_CHAIN_FRAME_COUNT + 1;
	}

	// depth stencil.
	{
		D3D12_RESOURCE_DESC dsvDesc;
		dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dsvDesc.Alignment = 0;
		dsvDesc.Width = m_ScreenWidth;
		dsvDesc.Height = m_ScreenHeight;
		dsvDesc.DepthOrArraySize = 1;
		dsvDesc.MipLevels = 1;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.SampleDesc.Count = 1;
		dsvDesc.SampleDesc.Quality = 0;
		dsvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		dsvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_HEAP_PROPERTIES heapProps;
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_CLEAR_VALUE clearValue;
		clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		hr = m_pDevice->CreateCommittedResource(&heapProps,
												D3D12_HEAP_FLAG_NONE,
												&dsvDesc,
												D3D12_RESOURCE_STATE_DEPTH_WRITE,
												&clearValue,
												IID_PPV_ARGS(&m_pDefaultDepthStencil));
		BREAK_IF_FAILED(hr);
		m_pDefaultDepthStencil->SetName(L"DefaultDepthStencil");

		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE depthHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());
		m_pDevice->CreateDepthStencilView(m_pDefaultDepthStencil, &depthStencilViewDesc, depthHandle);

		++(m_pResourceManager->m_DSVHeapSize);
	}

	// Model���� ���̴� descriptor ����.
	{
		// UINT64 totalObject = m_RenderObjects.size();
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(m_pResourceManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart());

		// b0, b1
		m_pResourceManager->m_GlobalConstantViewStartOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = m_GlobalConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)m_GlobalConstant.GetBufferSize();
		m_pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);
		m_GlobalConstant.SetCBVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = m_ReflectionGlobalConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)m_ReflectionGlobalConstant.GetBufferSize();
		m_pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);
		m_ReflectionGlobalConstant.SetCBVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = m_LightConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)m_LightConstant.GetBufferSize();
		m_pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);
		m_LightConstant.SetCBVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);


		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		// float buffer srv
		srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		m_pDevice->CreateShaderResourceView(m_pFloatBuffer, &srvDesc, cbvSrvHandle);
		m_FloatBufferSRVOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// prev buffer srv
		// srvDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		m_pDevice->CreateShaderResourceView(m_pPrevBuffer, &srvDesc, cbvSrvHandle);
		m_PrevBufferSRVOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t8, t9, t10
		m_pResourceManager->m_GlobalShaderResourceViewStartOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		(*m_pLights)[1].LightShadowMap.SetDescriptorHeap(this);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t11
		(*m_pLights)[0].LightShadowMap.SetDescriptorHeap(this);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		// t12
		(*m_pLights)[2].LightShadowMap.SetDescriptorHeap(this);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		// t13
		D3D12_RESOURCE_DESC descInfo;
		descInfo = pEnvTexture->GetResource()->GetDesc();
		srvDesc.Format = descInfo.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		m_pDevice->CreateShaderResourceView(pEnvTexture->GetResource(), &srvDesc, cbvSrvHandle);
		pEnvTexture->SetSRVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t14
		descInfo = pIrradianceTexture->GetResource()->GetDesc();
		srvDesc.Format = descInfo.Format;
		m_pDevice->CreateShaderResourceView(pIrradianceTexture->GetResource(), &srvDesc, cbvSrvHandle);
		pIrradianceTexture->SetSRVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t15
		descInfo = pSpecularTexture->GetResource()->GetDesc();
		srvDesc.Format = descInfo.Format;
		m_pDevice->CreateShaderResourceView(pSpecularTexture->GetResource(), &srvDesc, cbvSrvHandle);
		pSpecularTexture->SetSRVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t16
		descInfo = pBRDFTexture->GetResource()->GetDesc();
		srvDesc.Format = descInfo.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		m_pDevice->CreateShaderResourceView(pBRDFTexture->GetResource(), &srvDesc, cbvSrvHandle);
		pBRDFTexture->SetSRVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// null. 15��°.
		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, cbvSrvHandle);
		// cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// Model �� ������ ���۵� ���.
		for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
		{
			Model* pModel = (*m_pRenderObjects)[i];

			switch (pModel->ModelType)
			{
				case RenderObjectType_DefaultType: 
				case RenderObjectType_SkyboxType:
				case RenderObjectType_MirrorType:
					pModel->SetDescriptorHeap(pRenderer);
					break;

				case RenderObjectType_SkinnedType:
				{
					SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pModel;
					pCharacter->SetDescriptorHeap(pRenderer);
				}
				break;

				default:
					break;
			}
		}
	}
}

void Renderer::initRenderThreadPool(UINT renderThreadCount)
{
	m_pThreadDescList = new RenderThreadDesc[renderThreadCount];
	memset(m_pThreadDescList, 0, sizeof(RenderThreadDesc) * renderThreadCount);

	for (UINT i = 0; i < renderThreadCount; ++i)
	{
		for (int j = 0; j < RenderThreadEventType_Count; ++j)
		{
			m_pThreadDescList[i].hEventList[j] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		}

		m_pThreadDescList[i].pRenderer = this;
		m_pThreadDescList[i].pResourceManager = m_pResourceManager;
		m_pThreadDescList[i].ThreadIndex = i;
		UINT threadID = 0;
		m_pThreadDescList[i].hThread = (HANDLE)_beginthreadex(nullptr, 0, RenderThread, m_pThreadDescList + i, 0, &threadID);
	}
}

void Renderer::beginRender()
{
	HRESULT hr = S_OK;

#ifdef USE_MULTI_THREAD
	
	/*CommandListPool* pCommandListPool = m_pppCommandListPool[m_FrameIndex][0];
	ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();

	const CD3DX12_RESOURCE_BARRIER RTV_BEFORE_BARRIERS[2] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
	};
	pCommandList->ResourceBarrier(2, RTV_BEFORE_BARRIERS);

	const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset + m_FrameIndex, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE floatRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

	const float COLOR[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	pCommandList->ClearRenderTargetView(rtvHandle, COLOR, 0, nullptr);
	pCommandList->ClearRenderTargetView(floatRtvHandle, COLOR, 0, nullptr);
	pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	pCommandListPool->ClosedAndExecute(m_ppCommandQueue[RenderPass_MainRender]);*/

	////////////////////////////////////
	CommandListPool* pCommandListPool1 = m_pppCommandListPool[m_FrameIndex][0];
	CommandListPool* pCommandListPool2 = m_pppCommandListPool[m_FrameIndex][1];
	ID3D12GraphicsCommandList* pCommandList1 = pCommandListPool1->GetCurrentCommandList();
	ID3D12GraphicsCommandList* pCommandList2 = pCommandListPool2->GetCurrentCommandList();

	const CD3DX12_RESOURCE_BARRIER RENDER_TARGET_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	const CD3DX12_RESOURCE_BARRIER FLOAT_BUFFER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset + m_FrameIndex, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE floatRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

	pCommandList1->ResourceBarrier(1, &FLOAT_BUFFER_BARRIER);
	pCommandList2->ResourceBarrier(1, &RENDER_TARGET_BARRIER);

	const float COLOR[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	pCommandList2->ClearRenderTargetView(rtvHandle, COLOR, 0, nullptr);
	pCommandList1->ClearRenderTargetView(floatRtvHandle, COLOR, 0, nullptr);
	pCommandList1->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// pCommandListPool1->ClosedAndExecute(m_ppCommandQueue[RenderPass_MainRender]);
	pCommandListPool1->ClosedAndExecute(m_pCommandQueue);
	pCommandListPool2->ClosedAndExecute(m_pCommandQueue);

#else

	hr = m_ppCommandAllocator[m_FrameIndex]->Reset();
	BREAK_IF_FAILED(hr);

	hr = m_ppCommandList[m_FrameIndex]->Reset(m_ppCommandAllocator[m_FrameIndex], nullptr);
	BREAK_IF_FAILED(hr);

	ID3D12DescriptorHeap* ppDescriptorHeaps[] =
	{
		m_DynamicDescriptorPool.GetDescriptorHeap(),
		m_pResourceManager->m_pSamplerHeap
	};
	m_ppCommandList[m_FrameIndex]->SetDescriptorHeaps(2, ppDescriptorHeaps);

#endif
}

void Renderer::renderShadowmap()
{
#ifdef USE_MULTI_THREAD

	CommandListPool* pCommandListPool = m_pppCommandListPool[m_FrameIndex][0];
	const int TOTAL_LIGHT_TYPE = LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT;

	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		CD3DX12_RESOURCE_BARRIER barrier;
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();

		Light* pCurLight = &(*m_pLights)[i];
		eRenderPSOType renderPSO;
		
		// set shadow map writing state for depth buffer.
		switch (pCurLight->Property.LightType & TOTAL_LIGHT_TYPE)
		{
			case LIGHT_DIRECTIONAL:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetDirectionalLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				renderPSO = RenderPSOType_DepthOnlyCascadeDefault;
				break;

			case LIGHT_POINT:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetPointLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				renderPSO = RenderPSOType_DepthOnlyCubeDefault;
				break;

			case LIGHT_SPOT:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetSpotLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				renderPSO = RenderPSOType_DepthOnlyDefault;
				break;

			default:
				__debugbreak();
				break;
		}
		pCommandList->ResourceBarrier(1, &barrier);

		// register object to render queue.
		m_CurThreadIndex = 0;
		for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
		{
			RenderQueue* pRenderQue = m_pppRenderQueue[RenderPass_Shadow][m_CurThreadIndex];
			Model* pModel = (*m_pRenderObjects)[i];

			if (!pModel->bIsVisible || !pModel->bCastShadow)
			{
				continue;
			}

			RenderItem item;
			item.ModelType = (eRenderObjectType)pModel->ModelType;
			item.pObjectHandle = (void*)pModel;
			item.pLight = (void*)pCurLight;
			item.pFilter = nullptr;
			item.PSOType = renderPSO;

			if (pModel->ModelType == SkinnedModel)
			{
				item.PSOType = (eRenderPSOType)(renderPSO + 1);			
			}

			if (!pRenderQue->Add(&item))
			{
				__debugbreak();
			}
			++m_CurThreadIndex;
		}
	}

	// pCommandListPool->ClosedAndExecute(m_ppCommandQueue[RenderPass_Shadow]);
	pCommandListPool->ClosedAndExecute(m_pCommandQueue);

#else

	Renderer* pRenderer = this;
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		(*m_pLights)[i].RenderShadowMap(pRenderer, m_pRenderObjects);
	}

#endif
}

void Renderer::renderObject()
{
#ifdef USE_MULTI_THREAD

	// register obejct to render queue.
	m_CurThreadIndex = 0;
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		RenderQueue* pRenderQue = m_pppRenderQueue[RenderPass_Object][m_CurThreadIndex];
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		RenderItem item;
		item.ModelType = (eRenderObjectType)pCurModel->ModelType;
		item.pObjectHandle = (void*)pCurModel;
		item.pLight = nullptr;
		item.pFilter = nullptr;

		switch (pCurModel->ModelType)
		{
			case DefaultModel:
				item.PSOType = RenderPSOType_Default;
				break;

			case SkinnedModel:
				item.PSOType = RenderPSOType_Skinned;
				break;

			case SkyboxModel:
				item.PSOType = RenderPSOType_Skybox;
				break;

			default:
				break;
		}

		if (!pRenderQue->Add(&item))
		{
			__debugbreak();
		}
		++m_CurThreadIndex;
	}

#else

	Renderer* pRenderer = this;

	const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset + m_FrameIndex, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE floatRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

	const CD3DX12_RESOURCE_BARRIER RTV_BEFORE_BARRIERS[2] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
	};
	m_ppCommandList[m_FrameIndex]->ResourceBarrier(2, RTV_BEFORE_BARRIERS);

	const float COLOR[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	m_ppCommandList[m_FrameIndex]->ClearRenderTargetView(rtvHandle, COLOR, 0, nullptr);
	m_ppCommandList[m_FrameIndex]->ClearRenderTargetView(floatRtvHandle, COLOR, 0, nullptr);
	m_ppCommandList[m_FrameIndex]->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_ppCommandList[m_FrameIndex]->RSSetViewports(1, &m_ScreenViewport);
	m_ppCommandList[m_FrameIndex]->RSSetScissorRects(1, &m_ScissorRect);
	m_ppCommandList[m_FrameIndex]->OMSetRenderTargets(1, &floatRtvHandle, FALSE, &dsvHandle);

	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
			{
				m_pResourceManager->SetCommonState(RenderPSOType_Default);
				pCurModel->Render(pRenderer, RenderPSOType_Default);
			}
			break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
				m_pResourceManager->SetCommonState(RenderPSOType_Skinned);
				pCharacter->Render(pRenderer, RenderPSOType_Skinned);
			}
			break;

			case RenderObjectType_SkyboxType:
			{
				m_pResourceManager->SetCommonState(RenderPSOType_Skybox);
				pCurModel->Render(pRenderer, RenderPSOType_Skybox);
			}
			break;

			default:
				break;
		}
	}

#endif
}

void Renderer::renderMirror()
{
	if (!m_pMirror)
	{
		return;
	}

#ifdef USE_MULTI_THREAD

	// register object to render queue.
	m_CurThreadIndex = 0;
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		RenderQueue* pRenderQue = m_pppRenderQueue[RenderPass_Mirror][m_CurThreadIndex];
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		RenderItem item;
		item.ModelType = (eRenderObjectType)pCurModel->ModelType;
		item.pObjectHandle = (void*)pCurModel;
		item.pLight = nullptr;
		item.pFilter = nullptr;

		switch (pCurModel->ModelType)
		{
			case DefaultModel:
				item.PSOType = RenderPSOType_ReflectionDefault;
				break;

			case SkinnedModel:
				item.PSOType = RenderPSOType_ReflectionSkinned;
				break;

			case SkyboxModel:
				item.PSOType = RenderPSOType_ReflectionSkybox;
				break;

			default:
				break;
		}

		if (!pRenderQue->Add(&item))
		{
			__debugbreak();
		}
		++m_CurThreadIndex;
	}

#else

	Renderer* pRenderer = this;

	// 0.5�� ������ �����ٰ� ����.
	// �ſ� ��ġ�� StencilBuffer�� 1�� ǥ��.
	m_pResourceManager->SetCommonState(RenderPSOType_StencilMask);
	m_pMirror->Render(pRenderer, RenderPSOType_StencilMask);

	// �ſ� ��ġ�� �ݻ�� ��ü���� ������.
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
			{
				m_pResourceManager->SetCommonState(RenderPSOType_ReflectionDefault);
				pCurModel->Render(pRenderer, RenderPSOType_ReflectionDefault);
			}
			break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
				m_pResourceManager->SetCommonState(RenderPSOType_ReflectionSkinned);
				pCharacter->Render(pRenderer, RenderPSOType_ReflectionSkinned);
			}
			break;

			case RenderObjectType_SkyboxType:
			{
				m_pResourceManager->SetCommonState(RenderPSOType_ReflectionSkybox);
				pCurModel->Render(pRenderer, RenderPSOType_ReflectionSkybox);
			}
			break;

			default:
				break;
		}
	}

	// �ſ� ������.
	m_pResourceManager->SetCommonState(RenderPSOType_MirrorBlend);
	m_pMirror->Render(pRenderer, RenderPSOType_MirrorBlend);

#endif
}

void Renderer::renderObjectBoundingModel()
{
	Renderer* pRenderer = this;

	// obb rendering
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		m_pResourceManager->SetCommonState(RenderPSOType_Wire);
		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
				// pCurModel->RenderBoundingSphere(pRenderer, RenderPSOType_Wire);
				break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
				pCharacter->RenderBoundingCapsule(pRenderer, RenderPSOType_Wire);
				pCharacter->RenderJointSphere(pRenderer, RenderPSOType_Wire);
			}
			break;

			default:
				break;
		}
	}
}

void Renderer::postProcess()
{
#ifdef USE_MULTI_THREAD

	//m_CurThreadIndex = 1;

	//RenderQueue* pRenderQueue = m_pppRenderQueue[RenderPass_Post][m_CurThreadIndex];

	//RenderItem item;
	//item.ModelType = RenderObjectType_DefaultType;
	//item.pObjectHandle = (void*)m_PostProcessor.GetScreenMeshPtr();
	//item.pLight = nullptr;

	//// sampling.
	//item.PSOType = RenderPSOType_Sampling;
	//item.pFilter = (void*)m_PostProcessor.GetSamplingFilterPtr();
	//if (!pRenderQueue->Add(&item))
	//{
	//	__debugbreak();
	//}
	//// +m_CurThreadIndex;

	//// bloom.
	//std::vector<ImageFilter>* pBloomDownFilters = m_PostProcessor.GetBloomDownFiltersPtr();
	//std::vector<ImageFilter>* pBloomUpFilters = m_PostProcessor.GetBloomUpFiltersPtr();

	//item.PSOType = RenderPSOType_BloomDown;
	//for (UINT64 i = 0, size = pBloomDownFilters->size(); i < size; ++i)
	//{
	//	// pRenderQueue = m_ppRenderQueue[RenderPass_Post][m_CurThreadIndex];
	//	item.pFilter = (void*)&(*pBloomDownFilters)[i];

	//	if (!pRenderQueue->Add(&item))
	//	{
	//		__debugbreak();
	//	}
	//	// ++m_CurThreadIndex;
	//}
	//item.PSOType = RenderPSOType_BloomUp;
	//for (UINT64 i = 0, size = pBloomUpFilters->size(); i < size; ++i)
	//{
	//	// pRenderQueue = m_ppRenderQueue[RenderPass_Post][m_CurThreadIndex];
	//	item.pFilter = (void*)&(*pBloomUpFilters)[i];

	//	if (!pRenderQueue->Add(&item))
	//	{
	//		__debugbreak();
	//	}
	//	// ++m_CurThreadIndex;
	//}

	//// combine.
	//item.PSOType = RenderPSOType_Combine;
	//item.pFilter = (void*)m_PostProcessor.GetCombineFilterPtr();
	//if (!pRenderQueue->Add(&item))
	//{
	//	__debugbreak();
	//}

#else

	Renderer* pRenderer = this;

	const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	m_ppCommandList[m_FrameIndex]->ResourceBarrier(1, &BARRIER);
	m_PostProcessor.Render(pRenderer, m_FrameIndex);

#endif
}

void Renderer::endRender()
{
#ifdef USE_MULTI_THREAD

	CommandListPool* pCommandListPool = m_pppCommandListPool[m_FrameIndex][0];

	for (int i = 0; i < RenderPass_RenderPassCount; ++i)
	{
		m_pActiveThreadCounts[i] = m_RenderThreadCount;
	}

	// shadow pass.
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent(m_pThreadDescList[i].hEventList[RenderThreadEventType_Shadow]);
	}
	WaitForSingleObject(m_phCompletedEvents[RenderPass_Shadow], INFINITE);
	
	const int TOTAL_LIGHT_TYPE = LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT;
	// resource barrier.
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		CD3DX12_RESOURCE_BARRIER barrier;
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
		Light* pCurLight = &(*m_pLights)[i];

		switch (pCurLight->Property.LightType & TOTAL_LIGHT_TYPE)
		{
			case LIGHT_DIRECTIONAL:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetDirectionalLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
				break;

			case LIGHT_POINT:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetPointLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
				break;

			case LIGHT_SPOT:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetSpotLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
				break;

			default:
				__debugbreak();
				break;
		}

		pCommandList->ResourceBarrier(1, &barrier);
	}
	// pCommandListPool->ClosedAndExecute(m_ppCommandQueue[RenderPass_Shadow]);
	pCommandListPool->ClosedAndExecute(m_pCommandQueue);
	fence();

	// default render pass.
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent(m_pThreadDescList[i].hEventList[RenderThreadEventType_Object]);
	}
	WaitForSingleObject(m_phCompletedEvents[RenderPass_Object], INFINITE);

	// mirror pass.
	// stencil process.
	{
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
		DynamicDescriptorPool* pDescriptorPool = m_pppDescriptorPool[m_FrameIndex][0];
		ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
		{
			pDescriptorPool->GetDescriptorHeap(),
			m_pResourceManager->m_pSamplerHeap,
		};
		CD3DX12_CPU_DESCRIPTOR_HANDLE floatBufferRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, m_pResourceManager->m_RTVDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());
		
		m_PostProcessor.SetViewportsAndScissorRects(pCommandList);
		pCommandList->OMSetRenderTargets(1, &floatBufferRtvHandle, FALSE, &dsvHandle);
		pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
		m_pResourceManager->SetCommonState(0, pCommandList, m_pppDescriptorPool[m_FrameIndex][0], RenderPSOType_StencilMask);
		m_pMirror->Render(0, pCommandList, pDescriptorPool, m_pResourceManager, RenderPSOType_StencilMask);
		// pCommandListPool->ClosedAndExecute(m_ppCommandQueue[RenderPass_MainRender]);
		pCommandListPool->ClosedAndExecute(m_pCommandQueue);
	}
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent(m_pThreadDescList[i].hEventList[RenderThreadEventType_Mirror]);
	}
	WaitForSingleObject(m_phCompletedEvents[RenderPass_Mirror], INFINITE);
	// mirror blend process.
	{
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
		DynamicDescriptorPool* pDescriptorPool = m_pppDescriptorPool[m_FrameIndex][0];
		ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
		{
			pDescriptorPool->GetDescriptorHeap(),
			m_pResourceManager->m_pSamplerHeap,
		};
		CD3DX12_CPU_DESCRIPTOR_HANDLE floatBufferRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, m_pResourceManager->m_RTVDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

		m_PostProcessor.SetViewportsAndScissorRects(pCommandList);
		pCommandList->OMSetRenderTargets(1, &floatBufferRtvHandle, FALSE, &dsvHandle);
		pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
		m_pResourceManager->SetCommonState(0, pCommandList, m_pppDescriptorPool[m_FrameIndex][0], RenderPSOType_MirrorBlend);
		m_pMirror->Render(0, pCommandList, pDescriptorPool, m_pResourceManager, RenderPSOType_MirrorBlend);
		
		const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
		pCommandList->ResourceBarrier(1, &BARRIER);

		// pCommandListPool->ClosedAndExecute(m_ppCommandQueue[RenderPass_MainRender]);
		pCommandListPool->ClosedAndExecute(m_pCommandQueue);
	}
	fence();

	// postprocessing pass.
	{
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
		DynamicDescriptorPool* pDescriptorPool = m_pppDescriptorPool[m_FrameIndex][0];
		// const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
		ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
		{
			pDescriptorPool->GetDescriptorHeap(),
			m_pResourceManager->m_pSamplerHeap,
		};
		
		// pCommandList->ResourceBarrier(1, &BARRIER);
		pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
		m_PostProcessor.Render(0, pCommandList, pDescriptorPool, m_pResourceManager, m_FrameIndex);
	
		const CD3DX12_RESOURCE_BARRIER RTV_AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
		pCommandList->ResourceBarrier(1, &RTV_AFTER_BARRIER);
		pCommandListPool->ClosedAndExecute(m_pCommandQueue);
	}
	fence();

	for (int i = 0; i < RenderPass_RenderPassCount; ++i)
	{
		for (UINT j = 0; j < m_RenderThreadCount; ++j)
		{
			m_pppRenderQueue[i][j]->Reset();
		}
	}
	
#else

	_ASSERT(m_ppCommandList[m_FrameIndex]);
	_ASSERT(m_pCommandQueue);

	const CD3DX12_RESOURCE_BARRIER RTV_AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
	m_ppCommandList[m_FrameIndex]->ResourceBarrier(1, &RTV_AFTER_BARRIER);
	m_ppCommandList[m_FrameIndex]->Close();

	ID3D12CommandList* ppCommandLists[] = { m_ppCommandList[m_FrameIndex] };
	m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

#endif
}

void Renderer::present()
{
	fence();

	// UINT syncInterval = 1;	  // VSync On
	UINT syncInterval = 0;  // VSync Off
	UINT presentFlags = 0;

	if (!syncInterval)
	{
		presentFlags = DXGI_PRESENT_ALLOW_TEARING;
	}

	HRESULT hr = m_pSwapChain->Present(syncInterval, presentFlags);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		__debugbreak();
	}

	// for next frame
	UINT nextFrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
	waitForFenceValue(m_LastFenceValues[m_FrameIndex]);

#ifdef USE_MULTI_THREAD

	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		m_pppCommandListPool[nextFrameIndex][i]->Reset();
		m_pppDescriptorPool[nextFrameIndex][i]->Reset();
	}

#else

	m_DynamicDescriptorPool.Reset();

#endif

	m_FrameIndex = nextFrameIndex;
}

void Renderer::updateGlobalConstants(const float DELTA_TIME)
{
	const Vector3 EYE_WORLD = m_Camera.GetEyePos();
	const Matrix REFLECTION = Matrix::CreateReflection(*m_pMirrorPlane);
	const Matrix VIEW = m_Camera.GetView();
	const Matrix PROJECTION = m_Camera.GetProjection();

	GlobalConstant* pGlobalData = (GlobalConstant*)m_GlobalConstant.pData;
	GlobalConstant* pReflectGlobalData = (GlobalConstant*)m_ReflectionGlobalConstant.pData;

	pGlobalData->GlobalTime += DELTA_TIME;
	pGlobalData->EyeWorld = EYE_WORLD;
	pGlobalData->View = VIEW.Transpose();
	pGlobalData->Projection = PROJECTION.Transpose();
	pGlobalData->InverseProjection = PROJECTION.Invert().Transpose();
	pGlobalData->ViewProjection = (VIEW * PROJECTION).Transpose();
	pGlobalData->InverseView = VIEW.Invert().Transpose();
	pGlobalData->InverseViewProjection = pGlobalData->ViewProjection.Invert();

	memcpy(pReflectGlobalData, pGlobalData, sizeof(GlobalConstant));
	pReflectGlobalData->View = (REFLECTION * VIEW).Transpose();
	pReflectGlobalData->ViewProjection = (REFLECTION * VIEW * PROJECTION).Transpose();
	pReflectGlobalData->InverseViewProjection = pReflectGlobalData->ViewProjection.Invert();

	m_GlobalConstant.Upload();
	m_ReflectionGlobalConstant.Upload();
}

void Renderer::updateLightConstants(const float DELTA_TIME)
{
	Renderer* pRenderer = this;
	LightConstant* pLightConstData = (LightConstant*)m_LightConstant.pData;

	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		Light* pLight = &(*m_pLights)[i];

		pLight->Update(pRenderer, DELTA_TIME, m_Camera);
		(*m_pLightSpheres)[i]->UpdateWorld(Matrix::CreateScale(Max(0.01f, pLight->Property.Radius)) * Matrix::CreateTranslation(pLight->Property.Position));
		memcpy(&pLightConstData->Lights[i], &pLight->Property, sizeof(LightProperty));
	}

	m_LightConstant.Upload();
}

void Renderer::onMouseMove(const int MOUSE_X, const int MOUSE_Y)
{
	m_Mouse.MouseX = MOUSE_X;
	m_Mouse.MouseY = MOUSE_Y;

	// ���콺 Ŀ���� ��ġ�� NDC�� ��ȯ.
	// ���콺 Ŀ���� ���� ��� (0, 0), ���� �ϴ�(width-1, height-1).
	// NDC�� ���� �ϴ��� (-1, -1), ���� ���(1, 1).
	m_Mouse.MouseNDCX = (float)MOUSE_X * 2.0f / (float)m_ScreenWidth - 1.0f;
	m_Mouse.MouseNDCY = (float)(-MOUSE_Y) * 2.0f / (float)m_ScreenHeight + 1.0f;

	// Ŀ���� ȭ�� ������ ������ ��� ���� ����.
	/*m_Mouse.MouseNDCX = Clamp(m_Mouse.MouseNDCX, -1.0f, 1.0f);
	m_Mouse.MouseNDCY = Clamp(m_Mouse.MouseNDCY, -1.0f, 1.0f);*/

	// ī�޶� ���� ȸ��.
	m_Camera.UpdateMouse(m_Mouse.MouseNDCX, m_Mouse.MouseNDCY);
}

void Renderer::onMouseClick(const int MOUSE_X, const int MOUSE_Y)
{
	m_Mouse.MouseX = MOUSE_X;
	m_Mouse.MouseY = MOUSE_Y;

	m_Mouse.MouseNDCX = (float)MOUSE_X * 2.0f / (float)m_ScreenWidth - 1.0f;
	m_Mouse.MouseNDCY = (float)(-MOUSE_Y) * 2.0f / (float)m_ScreenHeight + 1.0f;
}

void Renderer::processMouseControl(const float DELTA_TIME)
{
	static Model* s_pActiveModel = nullptr;
	static Mesh* s_pEndEffector = nullptr;
	static int s_EndEffectorType = -1;
	static float s_PrevRatio = 0.0f;
	static Vector3 s_PrevPos(0.0f);
	static Vector3 s_PrevVector(0.0f);

	// ������ ȸ���� �̵� �ʱ�ȭ.
	Quaternion dragRotation = Quaternion::CreateFromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), 0.0f);
	Vector3 dragTranslation(0.0f);
	Vector3 pickPoint(0.0f);
	float dist = 0.0f;

	// ����ڰ� �� ��ư �� �ϳ��� �����ٰ� ����.
	if (m_Mouse.bMouseLeftButton || m_Mouse.bMouseRightButton)
	{
		const Matrix VIEW = m_Camera.GetView();
		const Matrix PROJECTION = m_Camera.GetProjection();
		const Vector3 NDC_NEAR = Vector3(m_Mouse.MouseNDCX, m_Mouse.MouseNDCY, 0.0f);
		const Vector3 NDC_FAR = Vector3(m_Mouse.MouseNDCX, m_Mouse.MouseNDCY, 1.0f);
		const Matrix INV_PROJECTION_VIEW = (VIEW * PROJECTION).Invert();
		const Vector3 WORLD_NEAR = Vector3::Transform(NDC_NEAR, INV_PROJECTION_VIEW);
		const Vector3 WORLD_FAR = Vector3::Transform(NDC_FAR, INV_PROJECTION_VIEW);
		Vector3 dir = WORLD_FAR - WORLD_NEAR;
		dir.Normalize();

		const DirectX::SimpleMath::Ray PICKING_RAY(WORLD_NEAR, dir);

		if (!s_pActiveModel) // ���� �����ӿ��� �ƹ� ��ü�� ���õ��� �ʾ��� ��쿡�� ���� ����.
		{
			Mesh* pEndEffector = nullptr;
			Model* pSelectedModel = pickClosest(PICKING_RAY, &dist, &pEndEffector, &s_EndEffectorType);
			if (pSelectedModel)
			{
#ifdef _DEBUG
				OutputDebugStringA("Newly selected model: ");
				OutputDebugStringA(pSelectedModel->Name.c_str());
				OutputDebugStringA("\n");
#endif
				s_pActiveModel = pSelectedModel;
				s_pEndEffector = pEndEffector;
				m_pPickedModel = s_pActiveModel; // GUI ���ۿ� ������.
				m_pPickedEndEffector = s_pEndEffector;
				m_PickedEndEffectorType = s_EndEffectorType;
				pickPoint = PICKING_RAY.position + dist * PICKING_RAY.direction;

				if (s_pEndEffector)
				{
					// �̵��� ó��.
					if (m_Mouse.bMouseRightButton)
					{
						m_Mouse.bMouseDragStartFlag = false;
						s_PrevRatio = dist / (WORLD_FAR - WORLD_NEAR).Length();
						s_PrevPos = pickPoint;
					}
				}
				else
				{
					if (m_Mouse.bMouseLeftButton) // ���� ��ư ȸ�� �غ�.
					{
						s_PrevVector = pickPoint - s_pActiveModel->BoundingSphere.Center;
						s_PrevVector.Normalize();
					}
					else if (m_Mouse.bMouseRightButton) // ������ ��ư �̵� �غ�.
					{ 
						m_Mouse.bMouseDragStartFlag = false;
						s_PrevRatio = dist / (WORLD_FAR - WORLD_NEAR).Length();
						s_PrevPos = pickPoint;
					}
				}
			}
		}
		else // �̹� ���õ� ��ü�� �־��� ���.
		{
			if (s_pEndEffector)
			{
				// �̵��� ó��.
				if (m_Mouse.bMouseRightButton)
				{
					Vector3 newPos = WORLD_NEAR + s_PrevRatio * (WORLD_FAR - WORLD_NEAR);
					if ((newPos - s_PrevPos).Length() > 1e-3)
					{
						dragTranslation = newPos - s_PrevPos;
						m_PickedTranslation = dragTranslation;
						s_PrevPos = newPos;
					}
					pickPoint = newPos; // Cursor sphere �׷��� ��ġ.
				}
			}
			else
			{
				if (m_Mouse.bMouseLeftButton) // ���� ��ư���� ��� ȸ��.
				{
					if (PICKING_RAY.Intersects(s_pActiveModel->BoundingSphere, dist))
					{
						pickPoint = PICKING_RAY.position + dist * PICKING_RAY.direction;
					}
					else // �ٿ�� ���Ǿ ���� ����� ���� ã��.
					{
						Vector3 c = s_pActiveModel->BoundingSphere.Center - WORLD_NEAR;
						Vector3 centerToRay = dir.Dot(c) * dir - c;
						pickPoint = c + centerToRay * Clamp(s_pActiveModel->BoundingSphere.Radius / centerToRay.Length(), 0.0f, 1.0f);
						pickPoint += WORLD_NEAR;
					}

					Vector3 currentVector = pickPoint - s_pActiveModel->BoundingSphere.Center;
					currentVector.Normalize();
					float theta = acos(s_PrevVector.Dot(currentVector));
					if (theta > DirectX::XM_PI / 180.0f * 3.0f)
					{
						Vector3 axis = s_PrevVector.Cross(currentVector);
						axis.Normalize();
						dragRotation = DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(axis, theta);
						s_PrevVector = currentVector;
					}

				}
				else if (m_Mouse.bMouseRightButton) // ������ ��ư���� ��� �̵�.
				{
					Vector3 newPos = WORLD_NEAR + s_PrevRatio * (WORLD_FAR - WORLD_NEAR);
					if ((newPos - s_PrevPos).Length() > 1e-3)
					{
						dragTranslation = newPos - s_PrevPos;
						m_PickedTranslation = dragTranslation;
						s_PrevPos = newPos;
					}
					pickPoint = newPos; // Cursor sphere �׷��� ��ġ.
				}
			}
		}
	}
	else
	{
		// ��ư���� ���� ���� ��쿡�� ������ ���� nullptr�� ����.
		s_pActiveModel = nullptr;
		s_pEndEffector = nullptr;
		m_pPickedModel = nullptr;
		m_pPickedEndEffector = nullptr;
		m_PickedEndEffectorType = -1;
	}

	if (s_pActiveModel)
	{
		Vector3 translation;
		if (s_pEndEffector)
		{
			MeshConstant* pMeshConstant = (MeshConstant*)s_pEndEffector->MeshConstant.pData;
			translation = pMeshConstant->World.Transpose().Translation() + dragTranslation;
			m_PickedTranslation = translation;

			/*{
				std::string debugString = std::string("dragTranslation: ") + std::to_string(dragTranslation.x) + std::string(", ") + std::to_string(dragTranslation.y) + std::string(", ") + std::to_string(dragTranslation.z) + std::string("\n");
				OutputDebugStringA(debugString.c_str());

				debugString = std::string("translation: ") + std::to_string(translation.x) + std::string(", ") + std::to_string(translation.y) + std::string(", ") + std::to_string(translation.z) + std::string("\n");
				OutputDebugStringA(debugString.c_str());
			}*/
			
			SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)s_pActiveModel;
			switch (s_EndEffectorType)
			{
				case 0:
					OutputDebugStringA("RightArm Control.\n");
					break;

				case 1:
					OutputDebugStringA("LeftArm Control.\n");
					break;

				case 2:
					OutputDebugStringA("RightLeg Control.\n");
					break;
					
				case 3:
					OutputDebugStringA("LeftLeg Control.\n");
					break;

				default:
					__debugbreak();
					break;
			}
		}
		else
		{
			translation = s_pActiveModel->World.Translation();
			s_pActiveModel->World.Translation(Vector3(0.0f));
			s_pActiveModel->UpdateWorld(s_pActiveModel->World* Matrix::CreateFromQuaternion(dragRotation)* Matrix::CreateTranslation(dragTranslation + translation));
			s_pActiveModel->BoundingSphere.Center = s_pActiveModel->World.Translation();
		}

		// �浹 ������ ���� �� �׸���.
		/*m_pCursorSphere->bIsVisible = true;
		m_pCursorSphere->UpdateWorld(Matrix::CreateTranslation(pickPoint));*/
	}
	else
	{
		// m_pCursorSphere->bIsVisible = false;
	}
}

Model* Renderer::pickClosest(const DirectX::SimpleMath::Ray& PICKING_RAY, float* pMinDist, Mesh** ppEndEffector, int* pEndEffectorType)
{
	*pMinDist = 1e5f;
	Model* pMinModel = nullptr;

	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];
		float dist = 0.0f;

		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
			{
				if (pCurModel->bIsPickable &&
					PICKING_RAY.Intersects(pCurModel->BoundingSphere, dist) &&
					dist < *pMinDist)
				{
					pMinModel = pCurModel;
					*pMinDist = dist;
				}
			}
			break;

			case RenderObjectType_SkinnedType:
			{
				if (pCurModel->bIsPickable &&
					PICKING_RAY.Intersects(pCurModel->BoundingSphere, dist) &&
					dist < *pMinDist)
				{
					pMinModel = pCurModel;
					*pMinDist = dist;
					dist = FLT_MAX;

					// 4�� end-effector �� ��� �ش�Ǵ� �� Ȯ��.
					SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
					/*{
						std::string debugString;

						Vector3 rayOriginToSphereCencter(Vector3(pCharacter->BoundingSphere.Center) - PICKING_RAY.position);
						float distAtRayAndSphereCenter = rayOriginToSphereCencter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("rayOriginToSphereCenter: ") + std::to_string(distAtRayAndSphereCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						Vector3 rayOriginToRightToeCenter(Vector3(pCharacter->RightToe.Center) - PICKING_RAY.position);
						float distAtRayAndRightToeCenter = rayOriginToRightToeCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("distAtRayAndRightToeCenter: ") + std::to_string(distAtRayAndRightToeCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						Vector3 rayOriginToLeftToeCenter(Vector3(pCharacter->LeftToe.Center) - PICKING_RAY.position);
						float distAtRayAndLeftToeCenter = rayOriginToRightToeCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("distAtRayAndLeftToeCenter: ") + std::to_string(distAtRayAndLeftToeCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						Vector3 rayOriginToRightHandCenter(Vector3(pCharacter->RightHandMiddle.Center) - PICKING_RAY.position);
						float distAtRayAndRightHandCenter = rayOriginToRightHandCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("distAtRayAndRightHand: ") + std::to_string(distAtRayAndRightHandCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						Vector3 rayOriginToLeftHandCenter(Vector3(pCharacter->LeftHandMiddle.Center) - PICKING_RAY.position);
						float distAtRayAndLeftHandCenter = rayOriginToLeftHandCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("distAtRayAndLeftHand: ") + std::to_string(distAtRayAndLeftHandCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						// Vector3 rightToeToCenter(Vector3(pCharacter->BoundingSphere.Center) - Vector3(pCharacter->RightToe.Center));
						// Vector3 leftToeToCenter(Vector3(pCharacter->BoundingSphere.Center) - Vector3(pCharacter->LeftToe.Center));
						// Vector3 rightHandToCenter(Vector3(pCharacter->BoundingSphere.Center) - Vector3(pCharacter->RightHandMiddle.Center));
						// Vector3 leftHandToCenter(Vector3(pCharacter->BoundingSphere.Center) - Vector3(pCharacter->LeftHandMiddle.Center));
						//
						// debugString = std::string("rightToeToCenter: ") + std::to_string(rightToeToCenter.Length()) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());
						// debugString = std::string("leftToeToCenter: ") + std::to_string(leftToeToCenter.Length()) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());
						// debugString = std::string("rightHandToCenter: ") + std::to_string(rightHandToCenter.Length()) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());
						// debugString = std::string("leftHandToCenter: ") + std::to_string(leftHandToCenter.Length()) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());
						// debugString = std::string("sphere radius: ") + std::to_string(pCharacter->BoundingSphere.Radius) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());

						OutputDebugStringA("\n\n");
					}*/

					//if (PICKING_RAY.Intersects(pCharacter->RightHandMiddle, dist) &&
					//	dist < *pMinDist)
					//{
					//	*ppEndEffector = *(pCharacter->GetRightArmsMesh() + 3);
					//	*pMinDist = dist;
					//}
					//if (PICKING_RAY.Intersects(pCharacter->LeftHandMiddle, dist) &&
					//	dist < *pMinDist)
					//{
					//	*ppEndEffector = *(pCharacter->GetLeftArmsMesh() + 3);
					//	*pMinDist = dist;
					//}
					//if (PICKING_RAY.Intersects(pCharacter->RightToe, dist) &&
					//	dist < *pMinDist)
					//{
					//	*ppEndEffector = *(pCharacter->GetRightLegsMesh() + 3);
					//	*pMinDist = dist;
					//}
					//if (PICKING_RAY.Intersects(pCharacter->LeftToe, dist) &&
					//	dist < *pMinDist)
					//{
					//	*ppEndEffector = *(pCharacter->GetLeftLegsMesh() + 3);
					//	*pMinDist = dist;
					//}

					// ���� picking ����� ����ϸ�, �� joint�� sphere�� �ʹ� �۾Ƽ����� ������ ����� �� ����.
					// �׷��� ������ �ϳ� ��. �� picking�ϸ� end-effector�� ��´ٰ� ����.
					// picking ray�� �� end-effector �߽� �Ÿ��� ���� ���� ���� ����.
					Vector3 rayToEndEffectorCenter;
					float rayToEndEffectorDist;
					const float PICKING_RAY_DIR_LENGTH = PICKING_RAY.direction.Length();

					// right hand middle.
					rayToEndEffectorCenter = pCharacter->RightHandMiddle.Center - PICKING_RAY.position;
					rayToEndEffectorDist = rayToEndEffectorCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY_DIR_LENGTH;
					if (rayToEndEffectorDist < dist)
					{
						*ppEndEffector = *(pCharacter->GetRightArmsMesh() + 3);
						*pEndEffectorType = 0;
						dist = rayToEndEffectorDist;
					}

					// left hand middle.
					rayToEndEffectorCenter = pCharacter->LeftHandMiddle.Center - PICKING_RAY.position;
					rayToEndEffectorDist = rayToEndEffectorCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY_DIR_LENGTH;
					if (rayToEndEffectorDist < dist)
					{
						*ppEndEffector = *(pCharacter->GetLeftArmsMesh() + 3);
						*pEndEffectorType = 1;
						dist = rayToEndEffectorDist;
					}

					// right toe.
					rayToEndEffectorCenter = pCharacter->RightToe.Center - PICKING_RAY.position;
					rayToEndEffectorDist = rayToEndEffectorCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY_DIR_LENGTH;
					if (rayToEndEffectorDist < dist)
					{
						*ppEndEffector = *(pCharacter->GetRightLegsMesh() + 3);
						*pEndEffectorType = 2;
						dist = rayToEndEffectorDist;
					}

					// left toe.
					rayToEndEffectorCenter = pCharacter->LeftToe.Center - PICKING_RAY.position;
					rayToEndEffectorDist = rayToEndEffectorCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY_DIR_LENGTH;
					if (rayToEndEffectorDist < dist)
					{
						*ppEndEffector = *(pCharacter->GetLeftLegsMesh() + 3);
						*pEndEffectorType = 3;
						dist = rayToEndEffectorDist;
					}
				}
			}
			break;

			default:
				break;
		}
	}

	return pMinModel;
}

UINT64 Renderer::fence()
{
	++m_FenceValue;
	m_pCommandQueue->Signal(m_pFence, m_FenceValue);
	m_LastFenceValues[m_FrameIndex] = m_FenceValue;
	return m_FenceValue;
}

void Renderer::waitForFenceValue(UINT64 expectedFenceValue)
{
	// Wait until the previous frame is finished.
	if (m_pFence->GetCompletedValue() < expectedFenceValue)
	{
		m_pFence->SetEventOnCompletion(expectedFenceValue, m_hFenceEvent);
		WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}
