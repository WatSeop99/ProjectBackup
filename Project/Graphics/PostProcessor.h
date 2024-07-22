#pragma once

#include "ConstantBuffer.h"
#include "ImageFilter.h"
#include "../Model/Mesh.h"

class PostProcessor
{
public:
	struct PostProcessingBuffers
	{
		ID3D12Resource* ppBackBuffers[2] = { nullptr, };
		ID3D12Resource* pFloatBuffer = nullptr;
		ID3D12Resource* pPrevBuffer = nullptr;
		ConstantBuffer* pGlobalConstant = nullptr;
		UINT BackBufferRTV1Offset = 0xffffffff;
		UINT BackBufferRTV2Offset = 0xffffffff;
		UINT FloatBufferSRVOffset = 0xffffffff;
		UINT PrevBufferSRVOffset = 0xffffffff;
	};

public:
	PostProcessor() = default;
	~PostProcessor() { Cleanup(); }

	void Initizlie(Renderer* pRenderer, const PostProcessingBuffers& CONFIG, const int WIDTH, const int HEIGHT, const int BLOOMLEVELS);

	void Update(Renderer* pRenderer);

	void Render(Renderer* pRenderer, UINT frameIndex);
	void Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ResourceManager* pManager, UINT frameIndex);

	void Cleanup();

	inline Mesh* GetScreenMeshPtr() { return m_pScreenMesh; }
	inline ImageFilter* GetSamplingFilterPtr() { return &m_BasicSamplingFilter; }
	inline std::vector<ImageFilter>* GetBloomDownFiltersPtr() { return &m_BloomDownFilters; }
	inline std::vector<ImageFilter>* GetBloomUpFiltersPtr() { return &m_BloomUpFilters; }
	inline ImageFilter* GetCombineFilterPtr() { return &m_CombineFilter; }

	void SetDescriptorHeap(Renderer* pRenderer);
	void SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pCommandList);

protected:
	void createPostBackBuffers(Renderer* pRenderer);
	void createImageResources(Renderer* pRenderer, const int WIDTH, const int HEIGHT, ImageFilter::ImageResource* pImageResource);

	void renderPostProcessing(Renderer* pRenderer, UINT frameIndex);
	void renderPostProcessing(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ResourceManager* pManager);
	void renderImageFilter(Renderer* pRenderer, ImageFilter& imageFilter, eRenderPSOType psoSetting, UINT frameIndex);
	void renderImageFilter(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ResourceManager* pManager, ImageFilter& imageFilter, int psoSetting);

	void setRenderConfig(const PostProcessingBuffers& CONFIG);

private:
	Mesh* m_pScreenMesh = nullptr;
	D3D12_VIEWPORT m_Viewport = { 0.0f, };
	D3D12_RECT m_ScissorRect = { 0.0f, };
	UINT m_ScreenWidth = 0;
	UINT m_ScreenHeight = 0;

	ImageFilter m_BasicSamplingFilter;
	ImageFilter m_CombineFilter;
	std::vector<ImageFilter> m_BloomDownFilters;
	std::vector<ImageFilter> m_BloomUpFilters;
	std::vector<ImageFilter::ImageResource> m_BloomResources;

	ID3D12Resource* m_pResolvedBuffer = nullptr;
	UINT m_ResolvedRTVOffset = 0xffffffff;
	UINT m_ResolvedSRVOffset = 0xffffffff;

	// do not release.
	ID3D12Resource* m_ppBackBuffers[2] = { nullptr, };
	ID3D12Resource* m_pFloatBuffer = nullptr;
	ID3D12Resource* m_pPrevBuffer = nullptr;
	ConstantBuffer* m_pGlobalConstant = nullptr;
	UINT m_BackBufferRTV1Offset = 0xffffffff;
	UINT m_BackBufferRTV2Offset = 0xffffffff;
	UINT m_FloatBufferSRVOffset = 0xffffffff;
	UINT m_PrevBufferSRVOffset = 0xffffffff;
};
