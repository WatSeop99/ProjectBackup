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

	initMainWidndow();
	initDirect3D();
}

Renderer::~Renderer()
{
	g_pRendrer = nullptr;
	Clear();
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
	m_PostProcessor.Initizlie(m_pResourceManager, config, m_ScreenWidth, m_ScreenHeight, 2);
	m_PostProcessor.SetDescriptorHeap(m_pResourceManager);

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
}

void Renderer::Update(const float DELTA_TIME)
{
	m_Camera.UpdateKeyboard(DELTA_TIME, &m_Keyboard);
	processMouseControl();

	updateGlobalConstants(DELTA_TIME);
	updateLightConstants(DELTA_TIME);
}

void Renderer::Render()
{
	beginRender();

	shadowMapRender();
	objectRender();
	mirrorRender();
	postProcess();

	endRender();

	present();
}

void Renderer::ProcessByThread(UINT threadIndex)
{
	_ASSERT(threadIndex >= 0 && threadIndex < m_RenderThreadCount);

	for (int renderPass = Shadow; renderPass < RenderPassCount; ++renderPass)
	{
		ID3D12CommandQueue* pCommandQueue = m_ppCommandQueue[renderPass];

		switch (renderPass)
		{
			case Shadow:
			{

			}
			break;

			case Object:
			{
			}
			break;

			case Mirror:
			{
			}
			break;

			case Collider:
			{
			}
			break;

			case Post:
			{
			}
			break;

			default:
				__debugbreak();
				break;
		}
	}

	long curActiveThreadCount = _InterlockedDecrement(&m_ActiveThreadCount);
	if (curActiveThreadCount == 0)
	{
		SetEvent(m_hCompletedEvent);
	}
}

void Renderer::Clear()
{
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

	m_GlobalConstant.Clear();
	m_LightConstant.Clear();
	m_ReflectionGlobalConstant.Clear();

	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		for (UINT j = 0; j < MAX_RENDER_THREAD_COUNT; ++j)
		{
			if (m_ppCommandListPool[i][j])
			{
				delete m_ppCommandListPool[i][j];
				m_ppCommandListPool[i][j] = nullptr;
			}
			if (m_ppDescriptorPool[i][j])
			{
				delete m_ppDescriptorPool[i][j];
				m_ppDescriptorPool[i][j] = nullptr;
			}
		}
	}
	for (UINT i = 0; i < MAX_RENDER_THREAD_COUNT; ++i)
	{
		if (m_ppRenderQueue[i])
		{
			delete m_ppRenderQueue[i];
			m_ppRenderQueue[i] = nullptr;
		}
	}

	m_PostProcessor.Clear();
	m_DynamicDescriptorPool.Clear();

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
			// 화면 해상도가 바뀌면 SwapChain을 다시 생성.
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

				// 윈도우가 Minimize 모드에서는 screenWidth/Height가 0.
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

					// 기존 버퍼 초기화.
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
					m_PostProcessor.Clear();
					m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

					// swap chain resize.
					m_pSwapChain->ResizeBuffers(0,					 // 현재 개수 유지.
												m_ScreenWidth,		 // 해상도 변경.
												m_ScreenHeight,
												DXGI_FORMAT_UNKNOWN, // 현재 포맷 유지.
												0);
					// float buffer, prev buffer 재생성.
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

					// 변경된 크기에 맞춰 rtv, dsv, srv 재생성.
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
						srvDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
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
					m_PostProcessor.Initizlie(m_pResourceManager, config, m_ScreenWidth, m_ScreenHeight, 4);
					m_PostProcessor.SetDescriptorHeap(m_pResourceManager);
					m_Camera.SetAspectRatio((float)m_ScreenWidth / (float)m_ScreenHeight);
				}
			}
		}
		break;

		case WM_SYSCOMMAND:
		{
			if ((wParam & 0xfff0) == SC_KEYMENU) // ALT키 비활성화.
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
				m_Mouse.bMouseDragStartFlag = true; // 드래그를 새로 시작하는지 확인.
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
				m_Mouse.bMouseDragStartFlag = true; // 드래그를 새로 시작하는지 확인.
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
			if (wParam == VK_ESCAPE) // ESC키 종료.
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
			if (wParam == 'F')  // f키 일인칭 시점.
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
								 100,							// 윈도우 좌측 상단의 x 좌표
								 100,							// 윈도우 좌측 상단의 y 좌표
								 wr.right - wr.left,			// 윈도우 가로 방향 해상도
								 wr.bottom - wr.top,			// 윈도우 세로 방향 해상도
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

	// create command queue
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		hr = m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue));
		BREAK_IF_FAILED(hr);
		m_pCommandQueue->SetName(L"CommandQueue");
	}

	// create swapchain
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = m_ScreenWidth;
		swapChainDesc.Height = m_ScreenHeight;
		swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
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
			std::wstring debugName;

			hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_ppCommandAllocator[i]));
			BREAK_IF_FAILED(hr);
			debugName = std::wstring(L"CommandAllocator") + std::to_wstring(i);
			m_ppCommandAllocator[i]->SetName(debugName.c_str());

			hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_ppCommandAllocator[i], nullptr, IID_PPV_ARGS(&m_ppCommandList[i]));
			BREAK_IF_FAILED(hr);
			debugName = std::wstring(L"CommandList") + std::to_wstring(i);
			m_ppCommandList[i]->SetName(debugName.c_str());

			// Command lists are created in the recording state, but there is nothing
			// to record yet. The main loop expects it to be closed, so close it now.
			m_ppCommandList[i]->Close();
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


	UINT physicalCoreCount = 0;
	UINT logicalCoreCount = 0;
	GetPhysicalCoreCount(&physicalCoreCount, &logicalCoreCount);
	m_RenderThreadCount = physicalCoreCount;
	if (m_RenderThreadCount > MAX_RENDER_THREAD_COUNT)
	{
		m_RenderThreadCount = MAX_RENDER_THREAD_COUNT;
	}


	ResourceManager::InitialData initData = { m_pDevice, m_pCommandQueue, m_ppCommandAllocator, m_ppCommandList, &m_DynamicDescriptorPool, m_hFenceEvent, m_pFence, &m_FrameIndex, &m_FenceValue, m_LastFenceValues };
	m_pResourceManager = new ResourceManager;
	m_pResourceManager->Initialize(&initData);
	m_pResourceManager->InitRTVDescriptorHeap(16);
	m_pResourceManager->InitDSVDescriptorHeap(8);
	m_pResourceManager->InitCBVSRVUAVDescriptorHeap(1024);

	SAFE_RELEASE(pDebugController);
	SAFE_RELEASE(pFactory5);
	SAFE_RELEASE(pAdapter3);
}

void Renderer::initScene()
{
	// m_Camera.Reset(Vector3(3.74966f, 5.03645f, -2.54918f), -0.819048f, 0.741502f);
	m_Camera.Reset(Vector3(0.0f, 3.0f, -2.0f), -0.819048f, 0.741502f);

	m_GlobalConstant.Initialize(m_pResourceManager, sizeof(GlobalConstant));
	m_LightConstant.Initialize(m_pResourceManager, sizeof(LightConstant));
	m_ReflectionGlobalConstant.Initialize(m_pResourceManager, sizeof(GlobalConstant));
	m_pResourceManager->SetGlobalConstants(&m_GlobalConstant, &m_LightConstant, &m_ReflectionGlobalConstant);

	// 공용 global constant 설정.
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

	// Model에서 쓰이는 descriptor 저장.
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
		srvDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		m_pDevice->CreateShaderResourceView(m_pPrevBuffer, &srvDesc, cbvSrvHandle);
		m_PrevBufferSRVOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t8, t9, t10
		m_pResourceManager->m_GlobalShaderResourceViewStartOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		(*m_pLights)[1].ShadowMap.SetDescriptorHeap(m_pResourceManager);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t11
		(*m_pLights)[0].ShadowMap.SetDescriptorHeap(m_pResourceManager);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		// t12
		(*m_pLights)[2].ShadowMap.SetDescriptorHeap(m_pResourceManager);
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

		// null. 15번째.
		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, cbvSrvHandle);
		// cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// Model 내 생성된 버퍼들 등록.
		for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
		{
			Model* pModel = (*m_pRenderObjects)[i];

			switch (pModel->ModelType)
			{
				case DefaultModel: case SkyboxModel: case MirrorModel:
					pModel->SetDescriptorHeap(m_pResourceManager);
					break;

				case SkinnedModel:
				{
					SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pModel;
					pCharacter->SetDescriptorHeap(m_pResourceManager);
				}
				break;

				default:
					break;
			}
		}
	}
}

void Renderer::beginRender()
{
	HRESULT hr = S_OK;

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
}

void Renderer::shadowMapRender()
{
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		(*m_pLights)[i].RenderShadowMap(m_pResourceManager, m_pRenderObjects);
	}
}

void Renderer::objectRender()
{
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

	const float COLOR[] = { 0.0f, 0.0f, 1.0f, 1.0f };
	m_ppCommandList[m_FrameIndex]->ClearRenderTargetView(rtvHandle, COLOR, 0, nullptr);
	m_ppCommandList[m_FrameIndex]->ClearRenderTargetView(floatRtvHandle, COLOR, 0, nullptr);
	m_ppCommandList[m_FrameIndex]->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_ppCommandList[m_FrameIndex]->RSSetViewports(1, &m_ScreenViewport);
	m_ppCommandList[m_FrameIndex]->RSSetScissorRects(1, &m_ScissorRect);
	m_ppCommandList[m_FrameIndex]->OMSetRenderTargets(1, &floatRtvHandle, FALSE, &dsvHandle);

	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];
		if (pCurModel->bIsVisible)
		{
			switch (pCurModel->ModelType)
			{
				case DefaultModel:
				{
					m_pResourceManager->SetCommonState(Default);
					pCurModel->Render(m_pResourceManager, Default);
				}
				break;

				case SkinnedModel:
				{
					SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
					m_pResourceManager->SetCommonState(Skinned);
					pCharacter->Render(m_pResourceManager, Skinned);
				}
				break;

				case SkyboxModel:
				{
					m_pResourceManager->SetCommonState(Skybox);
					pCurModel->Render(m_pResourceManager, Skybox);
				}
				break;

				default:
					break;
			}
		}
	}
}

void Renderer::mirrorRender()
{
	if (!m_pMirror)
	{
		return;
	}

	// 0.5의 투명도를 가진다고 가정.
	D3D12_CPU_DESCRIPTOR_HANDLE defaultDSVHandle = m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart();

	// 거울 위치만 StencilBuffer에 1로 표기.
	m_pResourceManager->SetCommonState(StencilMask);
	m_pMirror->Render(m_pResourceManager, StencilMask);

	// 거울 위치에 반사된 물체들을 렌더링.
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];
		if (pCurModel->bIsVisible)
		{
			switch (pCurModel->ModelType)
			{
				case DefaultModel:
				{
					m_pResourceManager->SetCommonState(ReflectionDefault);
					pCurModel->Render(m_pResourceManager, ReflectionDefault);
				}
				break;

				case SkinnedModel:
				{
					SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
					m_pResourceManager->SetCommonState(ReflectionSkinned);
					pCharacter->Render(m_pResourceManager, ReflectionSkinned);
				}
				break;

				case SkyboxModel:
				{
					m_pResourceManager->SetCommonState(ReflectionSkybox);
					pCurModel->Render(m_pResourceManager, ReflectionSkybox);
				}
				break;

				default:
					break;
			}
		}
	}

	// 거울 렌더링.
	m_pResourceManager->SetCommonState(MirrorBlend);
	m_pMirror->Render(m_pResourceManager, MirrorBlend);


	// obb rendering
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];
		if (pCurModel->bIsVisible)
		{
			m_pResourceManager->SetCommonState(Wire);
			switch (pCurModel->ModelType)
			{
				case DefaultModel:
					pCurModel->RenderBoundingSphere(m_pResourceManager, Wire);
					break;

				case SkinnedModel:
				{
					SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
					pCharacter->RenderBoundingSphere(m_pResourceManager, Wire);
					pCharacter->RenderEndEffectorSphere(m_pResourceManager, Wire);
				}
				break;

				default:
					break;
			}
		}
	}
}

void Renderer::postProcess()
{
	const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	m_ppCommandList[m_FrameIndex]->ResourceBarrier(1, &BARRIER);
	m_PostProcessor.Render(m_pResourceManager, m_FrameIndex);
}

void Renderer::endRender()
{
	_ASSERT(m_ppCommandList[m_FrameIndex]);
	_ASSERT(m_pCommandQueue);

	const CD3DX12_RESOURCE_BARRIER RTV_AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
	m_ppCommandList[m_FrameIndex]->ResourceBarrier(1, &RTV_AFTER_BARRIER);
	m_ppCommandList[m_FrameIndex]->Close();

	ID3D12CommandList* ppCommandLists[] = { m_ppCommandList[m_FrameIndex] };
	m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

#ifdef MULTI_THREAD
	// shadow pass.
	m_ActiveThreadCount = m_RenderThreadCount;
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent();
	}
	WaitForSingleObject();

	// default render pass.
	m_ActiveThreadCount = m_RenderThreadCount;
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent();
	}
	WaitForSingleObject();

	// mirror pass.
	// stencil process.
	m_ActiveThreadCount = m_RenderThreadCount;
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent();
	}
	WaitForSingleObject();

	// postprocessing pass.
	m_ActiveThreadCount = m_RenderThreadCount;
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent();
	}
	WaitForSingleObject();
#endif
}

void Renderer::present()
{
	fence();

	UINT syncInterval = 1;	  // VSync On
	// UINT syncInterval = 0;  // VSync Off
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
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
	waitForFenceValue(m_LastFenceValues[m_FrameIndex]);

	m_DynamicDescriptorPool.Reset();
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
	LightConstant* pLightConstData = (LightConstant*)m_LightConstant.pData;

	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		(*m_pLights)[i].Update(m_pResourceManager, DELTA_TIME, m_Camera);
		(*m_pLightSpheres)[i]->UpdateWorld(Matrix::CreateScale(Max(0.01f, (*m_pLights)[i].Property.Radius)) * Matrix::CreateTranslation((*m_pLights)[i].Property.Position));
		memcpy(&pLightConstData->Lights[i], &(*m_pLights)[i].Property, sizeof(LightProperty));
	}

	m_LightConstant.Upload();
}

void Renderer::onMouseMove(const int MOUSE_X, const int MOUSE_Y)
{
	m_Mouse.MouseX = MOUSE_X;
	m_Mouse.MouseY = MOUSE_Y;

	// 마우스 커서의 위치를 NDC로 변환.
	// 마우스 커서는 좌측 상단 (0, 0), 우측 하단(width-1, height-1).
	// NDC는 좌측 하단이 (-1, -1), 우측 상단(1, 1).
	m_Mouse.MouseNDCX = (float)MOUSE_X * 2.0f / (float)m_ScreenWidth - 1.0f;
	m_Mouse.MouseNDCY = (float)(-MOUSE_Y) * 2.0f / (float)m_ScreenHeight + 1.0f;

	// 커서가 화면 밖으로 나갔을 경우 범위 조절.
	/*m_Mouse.MouseNDCX = Clamp(m_Mouse.MouseNDCX, -1.0f, 1.0f);
	m_Mouse.MouseNDCY = Clamp(m_Mouse.MouseNDCY, -1.0f, 1.0f);*/

	// 카메라 시점 회전.
	m_Camera.UpdateMouse(m_Mouse.MouseNDCX, m_Mouse.MouseNDCY);
}

void Renderer::onMouseClick(const int MOUSE_X, const int MOUSE_Y)
{
	m_Mouse.MouseX = MOUSE_X;
	m_Mouse.MouseY = MOUSE_Y;

	m_Mouse.MouseNDCX = (float)MOUSE_X * 2.0f / (float)m_ScreenWidth - 1.0f;
	m_Mouse.MouseNDCY = (float)(-MOUSE_Y) * 2.0f / (float)m_ScreenHeight + 1.0f;
}

void Renderer::processMouseControl()
{
	static Model* s_pActiveModel = nullptr;
	static Mesh* s_pEndEffector = nullptr;
	static float s_PrevRatio = 0.0f;
	static Vector3 s_PrevPos(0.0f);
	static Vector3 s_PrevVector(0.0f);

	// 적용할 회전과 이동 초기화.
	Quaternion dragRotation = Quaternion::CreateFromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), 0.0f);
	Vector3 dragTranslation(0.0f);
	Vector3 pickPoint(0.0f);
	float dist = 0.0f;

	// 사용자가 두 버튼 중 하나만 누른다고 가정.
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

		if (!s_pActiveModel) // 이전 프레임에서 아무 물체도 선택되지 않았을 경우에는 새로 선택.
		{
			Mesh* pEndEffector = nullptr;
			Model* pSelectedModel = pickClosest(PICKING_RAY, &dist, &pEndEffector);
			if (pSelectedModel)
			{
#ifdef _DEBUG
				OutputDebugStringA("Newly selected model: ");
				OutputDebugStringA(pSelectedModel->Name.c_str());
				OutputDebugStringA("\n");
#endif
				s_pActiveModel = pSelectedModel;
				s_pEndEffector = pEndEffector;
				m_pPickedModel = pSelectedModel; // GUI 조작용 포인터.
				pickPoint = PICKING_RAY.position + dist * PICKING_RAY.direction;

				if (s_pEndEffector)
				{
					// 이동만 처리.
					if (m_Mouse.bMouseRightButton)
					{
						m_Mouse.bMouseDragStartFlag = false;
						s_PrevRatio = dist / (WORLD_FAR - WORLD_NEAR).Length();
						s_PrevPos = pickPoint;
					}
				}
				else
				{
					if (m_Mouse.bMouseLeftButton) // 왼쪽 버튼 회전 준비.
					{
						s_PrevVector = pickPoint - s_pActiveModel->BoundingSphere.Center;
						s_PrevVector.Normalize();
					}
					else
					{ // 오른쪽 버튼 이동 준비
						m_Mouse.bMouseDragStartFlag = false;
						s_PrevRatio = dist / (WORLD_FAR - WORLD_NEAR).Length();
						s_PrevPos = pickPoint;
					}
				}
			}
		}
		else // 이미 선택된 물체가 있었던 경우.
		{
			if (s_pEndEffector)
			{
				// 이동만 처리.
				if (m_Mouse.bMouseRightButton)
				{
					Vector3 newPos = WORLD_NEAR + s_PrevRatio * (WORLD_FAR - WORLD_NEAR);
					if ((newPos - s_PrevPos).Length() > 1e-3)
					{
						dragTranslation = newPos - s_PrevPos;
						s_PrevPos = newPos;
					}
					pickPoint = newPos; // Cursor sphere 그려질 위치.
				}
			}
			else
			{
				if (m_Mouse.bMouseLeftButton) // 왼쪽 버튼으로 계속 회전.
				{
					if (PICKING_RAY.Intersects(s_pActiveModel->BoundingSphere, dist))
					{
						pickPoint = PICKING_RAY.position + dist * PICKING_RAY.direction;
					}
					else // 바운딩 스피어에 가장 가까운 점을 찾기.
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
				else // 오른쪽 버튼으로 계속 이동.
				{
					Vector3 newPos = WORLD_NEAR + s_PrevRatio * (WORLD_FAR - WORLD_NEAR);
					if ((newPos - s_PrevPos).Length() > 1e-3)
					{
						dragTranslation = newPos - s_PrevPos;
						s_PrevPos = newPos;
					}
					pickPoint = newPos; // Cursor sphere 그려질 위치.
				}
			}
		}
	}
	else
	{
		// 버튼에서 손을 땠을 경우에는 움직일 모델은 nullptr로 설정.
		s_pActiveModel = nullptr;
		s_pEndEffector = nullptr;
	}

	if (s_pActiveModel)
	{
		Vector3 translation;
		if (s_pEndEffector)
		{
			MeshConstant* pMeshConstant = (MeshConstant*)s_pEndEffector->MeshConstant.pData;
			translation = pMeshConstant->World.Translation();
			// update world.
			pMeshConstant->World = (Matrix::CreateTranslation(dragTranslation + translation)).Transpose();
		}
		else
		{
			translation = s_pActiveModel->World.Translation();
			s_pActiveModel->World.Translation(Vector3(0.0f));
			s_pActiveModel->UpdateWorld(s_pActiveModel->World* Matrix::CreateFromQuaternion(dragRotation)* Matrix::CreateTranslation(dragTranslation + translation));
			s_pActiveModel->BoundingSphere.Center = s_pActiveModel->World.Translation();
		}

		// 충돌 지점에 작은 구 그리기.
		/*m_pCursorSphere->bIsVisible = true;
		m_pCursorSphere->UpdateWorld(Matrix::CreateTranslation(pickPoint));*/
	}
	else
	{
		// m_pCursorSphere->bIsVisible = false;
	}
}

Model* Renderer::pickClosest(const DirectX::SimpleMath::Ray& PICKING_RAY, float* pMinDist, Mesh** ppEndEffector)
{
	*pMinDist = 1e5f;
	Model* pMinModel = nullptr;

	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];
		float dist = 0.0f;

		switch (pCurModel->ModelType)
		{
			case DefaultModel:
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

			case SkinnedModel:
			{
				if (pCurModel->bIsPickable &&
					PICKING_RAY.Intersects(pCurModel->BoundingSphere, dist) &&
					dist < *pMinDist)
				{
					pMinModel = pCurModel;
					*pMinDist = dist;
					dist = FLT_MAX;

					// 4개 end-effector 중 어디에 해당되는 지 확인.
					SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
					if (PICKING_RAY.Intersects(pCharacter->RightHandMiddle, dist) &&
						dist < *pMinDist)
					{
						*ppEndEffector = *(pCharacter->GetRightArmsMesh() + 3);
						*pMinDist = dist;
					}
					if (PICKING_RAY.Intersects(pCharacter->LeftHandMiddle, dist) &&
						dist < *pMinDist)
					{
						*ppEndEffector = *(pCharacter->GetLeftArmsMesh() + 3);
						*pMinDist = dist;
					}
					if (PICKING_RAY.Intersects(pCharacter->RightToe, dist) &&
						dist < *pMinDist)
					{
						*ppEndEffector = *(pCharacter->GetRightLegsMesh() + 3);
						*pMinDist = dist;
					}
					if (PICKING_RAY.Intersects(pCharacter->LeftToe, dist) &&
						dist < *pMinDist)
					{
						*ppEndEffector = *(pCharacter->GetLeftLegsMesh() + 3);
						*pMinDist = dist;
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
