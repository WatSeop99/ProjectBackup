#pragma once

#include "Camera.h"
#include "ConstantDataType.h"
#include "ConstantBuffer.h"
#include "../Model/SkinnedMeshModel.h"
#include "Texture.h"

class ShadowMap
{
public:
	ShadowMap(UINT width = 1280, UINT height = 1280) : m_ShadowMapWidth(width), m_ShadowMapHeight(height) { }
	~ShadowMap() { Cleanup(); }

	void Initialize(Renderer* pRenderer, UINT lightType);

	void Update(Renderer* pRenderer, LightProperty& property, Camera& lightCam, Camera& mainCamera);

	void Render(Renderer* pRenderer, std::vector<Model*>* pRenderObjects);

	void Cleanup();

	inline UINT GetShadowWidth() { return m_ShadowMapWidth; }
	inline UINT GetShadowHeight() { return m_ShadowMapHeight; }

	inline Texture* GetSpotLightShadowBufferPtr() { return &m_SpotLightShadowBuffer; }
	inline Texture* GetPointLightShadowBufferPtr() { return &m_PointLightShadowBuffer; }
	inline Texture* GetDirectionalLightShadowBufferPtr() { return &m_DirectionalLightShadowBuffer; }

	inline ConstantBuffer* GetShadowConstantsBufferPtr() { return m_ShadowConstantBuffers; }
	inline ConstantBuffer* GetShadowConstantBufferForGSPtr() { return &m_ShadowConstantsBufferForGS; }

	void SetShadowWidth(const UINT WIDTH);
	void SetShadowHeight(const UINT HEIGHT);

	void SetDescriptorHeap(Renderer* pRenderer);
	void SetViewportsAndScissorRect(ID3D12GraphicsCommandList* pCommandList);

protected:
	void setShadowViewport(ID3D12GraphicsCommandList* pCommandList);
	void setShadowScissorRect(ID3D12GraphicsCommandList* pCommandList);

	void calculateCascadeLightViewProjection(Vector3* pPosition, Matrix* pView, Matrix* pProjection, const Matrix& VIEW, const Matrix& PROJECTION, const Vector3& DIR, int cascadeIndex);

private:
	UINT m_ShadowMapWidth;
	UINT m_ShadowMapHeight;
	UINT m_LightType = LIGHT_OFF;
	const UINT m_TOTAL_LIGHT_TYPE = (LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT);

	D3D12_VIEWPORT m_pViewPorts[6] = { 0.0f, };
	D3D12_RECT m_pScissorRects[6] = { 0.0f, };

	union
	{
		Texture m_SpotLightShadowBuffer;
		Texture m_PointLightShadowBuffer;
		Texture m_DirectionalLightShadowBuffer;
	};
	ConstantBuffer m_ShadowConstantBuffers[6];	 // spot, point, direc => 0, 6, 4���� ���.
	ConstantBuffer m_ShadowConstantsBufferForGS; // 2�� �̻��� view ����� ����ϴ� ������ ����  geometry�� �������;
};
