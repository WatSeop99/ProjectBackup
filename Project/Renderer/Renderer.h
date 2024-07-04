#pragma once

#include "../Graphics/Camera.h"
#include "../Graphics/ConstantDataType.h"
#include "DynamicDescriptorPool.h"
#include "../Util/KnM.h"
#include "../Graphics/Light.h"
#include "../Model/Model.h"
#include "../Model/SkinnedMeshModel.h"
#include "../Graphics/PostProcessor.h"

const UINT SWAP_CHAIN_FRAME_COUNT = 2;
const UINT MAX_PENDING_FRAME_NUM = SWAP_CHAIN_FRAME_COUNT - 1;

class Renderer
{
public:
	struct InitialData
	{
		std::vector<Model*>* pRenderObjects;
		std::vector<Light>* pLights;
		std::vector<Model*>* pLightSpheres;

		Texture* pEnvTexture;
		Texture* pIrradianceTexture;
		Texture* pSpecularTexture;
		Texture* pBRDFTexture;

		Model* pSkybox;
		Model* pGround;
		Model* pMirror;
		Model* pPickedModel;
		Model* pCharacter;
		DirectX::SimpleMath::Plane* pMirrorPlane;
	};

public:
	Renderer();
	virtual ~Renderer();

	void Initizlie(InitialData* pIntialData);

	void Update(const float DELTA_TIME);

	void Render();

	void Clear();

	LRESULT MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	inline ResourceManager* GetResourceManager() { return m_pResourceManager; }
	inline HWND GetWindow() { return m_hMainWindow; }

protected:
	void initMainWidndow();
	void initDirect3D();
	void initScene();
	void initDescriptorHeap(Texture* pEnvTexture, Texture* pIrradianceTexture, Texture* pSpecularTexture, Texture* pBRDFTexture);

	// for single thread.
	void beginRender();
	void shadowMapRender();
	void objectRender();
	void mirrorRender();
	void postProcess();
	void endRender();
	void present();

	void updateGlobalConstants(const float DELTA_TIME);
	void updateLightConstants(const float DELTA_TIME);

	void onMouseMove(const int MOUSE_X, const int MOUSE_Y);
	void onMouseClick(const int MOUSE_X, const int MOUSE_Y);
	void processMouseControl();
	Model* pickClosest(const DirectX::SimpleMath::Ray& PICKING_RAY, float* pMinDist);

	UINT64 fence();
	void waitForFenceValue();

protected:
	HWND m_hMainWindow = nullptr;

	Keyboard m_Keyboard;
	Mouse m_Mouse;

	// external data
	ConstantBuffer m_GlobalConstant;
	ConstantBuffer m_LightConstant;
	ConstantBuffer m_ReflectionGlobalConstant;

	std::vector<Model*>* m_pRenderObjects = nullptr;
	std::vector<Light>* m_pLights = nullptr;
	std::vector<Model*>* m_pLightSpheres = nullptr;

	Model* m_pSkybox = nullptr;
	Model* m_pGround = nullptr;
	Model* m_pMirror = nullptr;
	Model* m_pPickedModel = nullptr;
	Model* m_pCharacter = nullptr;
	DirectX::SimpleMath::Plane* m_pMirrorPlane = nullptr;

private:
	ResourceManager* m_pResourceManager = nullptr;

	D3D_FEATURE_LEVEL m_FeatureLevel;
	DXGI_FORMAT m_BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_VIEWPORT m_ScreenViewport = { 0, };
	D3D12_RECT m_ScissorRect = { 0, };
	DXGI_ADAPTER_DESC2 m_AdapterDesc = { 0, };
	UINT m_ScreenWidth = 1280;
	UINT m_ScreenHeight = 720;

	ID3D12Device5* m_pDevice = nullptr;
	IDXGISwapChain4* m_pSwapChain = nullptr;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;

	HANDLE m_hFenceEvent = nullptr;
	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_FenceValue = 0;

	// main resources.
	DynamicDescriptorPool m_DynamicDescriptorPool;
	ID3D12Resource* m_pRenderTargets[SWAP_CHAIN_FRAME_COUNT] = { nullptr, };
	ID3D12Resource* m_pFloatBuffer = nullptr;
	ID3D12Resource* m_pPrevBuffer = nullptr;
	ID3D12Resource* m_pDefaultDepthStencil = nullptr;
	UINT m_MainRenderTargetOffset = 0xffffffff;
	UINT m_FloatBufferRTVOffset = 0xffffffff;
	UINT m_FloatBufferSRVOffset = 0xffffffff;
	UINT m_PrevBufferSRVOffset = 0xffffffff;
	UINT m_FrameIndex = 0;

	PostProcessor m_PostProcessor;

	// control.
	Camera m_Camera;
};
