#include "../pch.h"
#include "../Util/Utility.h"
#include "ShadowMap.h"

void ShadowMap::Initialize(ResourceManager* pManager, UINT lightType)
{
	m_LightType = lightType;

	D3D12_RESOURCE_DESC dsvDesc = {};
	dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsvDesc.Alignment = 0;
	dsvDesc.Width = m_ShadowMapWidth;
	dsvDesc.Height = m_ShadowMapHeight;
	dsvDesc.MipLevels = 1;
	dsvDesc.SampleDesc.Count = 1;
	dsvDesc.SampleDesc.Quality = 0;
	dsvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	dsvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
			dsvDesc.DepthOrArraySize = 4;
			dsvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			m_DirectionalLightShadowBuffer.Initialize(pManager, dsvDesc);
			break;

		case LIGHT_POINT:
			dsvDesc.DepthOrArraySize = 6;
			dsvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			m_PointLightShadowBuffer.Initialize(pManager, dsvDesc);
			break;

		case LIGHT_SPOT:
			dsvDesc.DepthOrArraySize = 1;
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			m_SpotLightShadowBuffer.Initialize(pManager, dsvDesc);
			break;

		default:
			break;
	}

	for (int i = 0; i < 6; ++i)
	{
		m_ShadowConstantBuffers[i].Initialize(pManager, sizeof(GlobalConstant));
	}
	m_ShadowConstantsBufferForGS.Initialize(pManager, sizeof(ShadowConstant));
}

void ShadowMap::Update(ResourceManager* pManager, LightProperty& property, Camera& lightCam, Camera& mainCamera)
{
	Matrix lightView;
	Matrix lightProjection = lightCam.GetProjection();

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
		{
			bool bOriginalFPS = mainCamera.bUseFirstPersonView;
			mainCamera.bUseFirstPersonView = true;

			Matrix camView = mainCamera.GetView();
			Matrix camProjection = mainCamera.GetProjection();
			Matrix lightSectionView;
			Matrix lightSectionProjection;
			Vector3 lightSectionPosition;

			for (int i = 0; i < 4; ++i)
			{
				GlobalConstant* pShadowGlobalConstantData = (GlobalConstant*)(m_ShadowConstantBuffers[i].pData);
				ShadowConstant* pShadowConstantData = (ShadowConstant*)(m_ShadowConstantsBufferForGS.pData);

				calculateCascadeLightViewProjection(&lightSectionPosition, &lightSectionView, &lightSectionProjection, camView, camProjection, property.Direction, i);

				pShadowGlobalConstantData->EyeWorld = lightSectionPosition;
				pShadowGlobalConstantData->View = lightSectionView.Transpose();
				pShadowGlobalConstantData->Projection = lightSectionProjection.Transpose();
				pShadowGlobalConstantData->InverseProjection = lightSectionProjection.Invert().Transpose();
				pShadowGlobalConstantData->ViewProjection = (lightSectionView * lightSectionProjection).Transpose();

				pShadowConstantData->ViewProjects[i] = pShadowGlobalConstantData->ViewProjection;

				m_ShadowConstantBuffers[i].Upload();
			}
			m_ShadowConstantsBufferForGS.Upload();

			mainCamera.bUseFirstPersonView = bOriginalFPS;
		}
		break;

		case LIGHT_POINT:
		{
			static const Vector3 s_pVIEW_DIRs[6] = // cubemap view vector.
			{
				Vector3(1.0f, 0.0f, 0.0f),	// right
				Vector3(-1.0f, 0.0f, 0.0f), // left
				Vector3(0.0f, 1.0f, 0.0f),	// up
				Vector3(0.0f, -1.0f, 0.0f), // down
				Vector3(0.0f, 0.0f, 1.0f),	// front
				Vector3(0.0f, 0.0f, -1.0f)	// back
			};
			static const Vector3 s_pUP_DIRs[6] = // ������ ������ view vector�� ���� up vector.
			{
				Vector3(0.0f, 1.0f, 0.0f),
				Vector3(0.0f, 1.0f, 0.0f),
				Vector3(0.0f, 0.0f, -1.0f),
				Vector3(0.0f, 0.0f, 1.0f),
				Vector3(0.0f, 1.0f, 0.0f),
				Vector3(0.0f, 1.0f, 0.0f)
			};

			for (int i = 0; i < 6; ++i)
			{
				GlobalConstant* pShadowGlobalConstantData = (GlobalConstant*)(m_ShadowConstantBuffers[i].pData);
				ShadowConstant* pShadowConstantData = (ShadowConstant*)(m_ShadowConstantsBufferForGS.pData);

				lightView = DirectX::XMMatrixLookAtLH(property.Position, property.Position + s_pVIEW_DIRs[i], s_pUP_DIRs[i]);

				pShadowGlobalConstantData->EyeWorld = property.Position;
				pShadowGlobalConstantData->View = lightView.Transpose();
				pShadowGlobalConstantData->Projection = lightProjection.Transpose();
				pShadowGlobalConstantData->InverseProjection = lightProjection.Invert().Transpose();
				pShadowGlobalConstantData->ViewProjection = (lightView * lightProjection).Transpose();

				pShadowConstantData->ViewProjects[i] = pShadowGlobalConstantData->ViewProjection;

				m_ShadowConstantBuffers[i].Upload();
			}
			m_ShadowConstantsBufferForGS.Upload();
		}
		break;

		case LIGHT_SPOT:
		{
			GlobalConstant* pShadowGlobalConstantData = (GlobalConstant*)(m_ShadowConstantBuffers[0].pData);

			lightView = DirectX::XMMatrixLookAtLH(property.Position, property.Position + property.Direction, lightCam.GetUpDir());

			pShadowGlobalConstantData->EyeWorld = property.Position;
			pShadowGlobalConstantData->View = lightView.Transpose();
			pShadowGlobalConstantData->Projection = lightProjection.Transpose();
			pShadowGlobalConstantData->InverseProjection = lightProjection.Invert().Transpose();
			pShadowGlobalConstantData->ViewProjection = (lightView * lightProjection).Transpose();

			m_ShadowConstantBuffers[0].Upload();
		}
		break;

		default:
			break;
	}
}

void ShadowMap::Render(ResourceManager* pManager, std::vector<Model*>* pRenderObjects)
{
	_ASSERT(pManager);

	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();
	const UINT DSV_DESCRIPTOR_SIZE = pManager->m_DSVDescriptorSize;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
	ID3D12Resource* pDepthStencilResource = nullptr;
	CD3DX12_RESOURCE_BARRIER beforeBarrier = {};
	CD3DX12_RESOURCE_BARRIER afterBarrier = {};

	setShadowViewport(pCommandList);
	setShadowScissorRect(pCommandList);

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
		{
			dsvHandle = m_DirectionalLightShadowBuffer.GetDSVHandle();
			pDepthStencilResource = m_DirectionalLightShadowBuffer.GetResource();
			beforeBarrier = CD3DX12_RESOURCE_BARRIER::Transition(pDepthStencilResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			pCommandList->ResourceBarrier(1, &beforeBarrier);

			pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
			
			for (UINT64 i = 0, size = pRenderObjects->size(); i < size; ++i)
			{
				Model* pModel = (*pRenderObjects)[i];
				if (pModel->bIsVisible && pModel->bCastShadow)
				{
					switch (pModel->ModelType)
					{
						case DefaultModel:
						{
							pManager->SetCommonState(DepthOnlyCascadeDefault);
							pCommandList->SetGraphicsRootConstantBufferView(1, m_ShadowConstantsBufferForGS.GetGPUMemAddr());
							pModel->Render(pManager, DepthOnlyCascadeDefault);
						}
						break;

						case SkinnedModel:
						{
							SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pModel;
							pManager->SetCommonState(DepthOnlyCascadeSkinned);
							pCommandList->SetGraphicsRootConstantBufferView(1, m_ShadowConstantsBufferForGS.GetGPUMemAddr());
							pCharacter->Render(pManager, DepthOnlyCascadeSkinned);
						}
						break;

						case MirrorModel:
						{
							pManager->SetCommonState(DepthOnlyCascadeDefault);
							pCommandList->SetGraphicsRootConstantBufferView(1, m_ShadowConstantsBufferForGS.GetGPUMemAddr());
							pModel->Render(pManager, DepthOnlyCascadeDefault);
						}
						break;

						default:
							break;
					}
				}
			}
		}
		break;

		case LIGHT_POINT:
		{
			dsvHandle = m_PointLightShadowBuffer.GetDSVHandle();
			pDepthStencilResource = m_PointLightShadowBuffer.GetResource();
			beforeBarrier = CD3DX12_RESOURCE_BARRIER::Transition(pDepthStencilResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			pCommandList->ResourceBarrier(1, &beforeBarrier);

			pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

			for (UINT64 i = 0, size = pRenderObjects->size(); i < size; ++i)
			{
				Model* pModel = (*pRenderObjects)[i];
				if (pModel->bIsVisible && pModel->bCastShadow)
				{
					switch (pModel->ModelType)
					{
						case DefaultModel:
						{
							pManager->SetCommonState(DepthOnlyCubeDefault);
							pCommandList->SetGraphicsRootConstantBufferView(1, m_ShadowConstantsBufferForGS.GetGPUMemAddr());
							pModel->Render(pManager, DepthOnlyCubeDefault);
						}
						break;

						case SkinnedModel:
						{
							SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pModel;
							pManager->SetCommonState(DepthOnlyCubeSkinned);
							pCommandList->SetGraphicsRootConstantBufferView(1, m_ShadowConstantsBufferForGS.GetGPUMemAddr());
							pCharacter->Render(pManager, DepthOnlyCubeSkinned);
						}
						break;

						case MirrorModel:
						{
							pManager->SetCommonState(DepthOnlyCubeDefault);
							pCommandList->SetGraphicsRootConstantBufferView(1, m_ShadowConstantsBufferForGS.GetGPUMemAddr());
							pModel->Render(pManager, DepthOnlyCubeDefault);
						}
						break;

						default:
							break;
					}
				}
			}
		}
		break;

		case LIGHT_SPOT:
		{
			dsvHandle = m_SpotLightShadowBuffer.GetDSVHandle();
			pDepthStencilResource = m_SpotLightShadowBuffer.GetResource();
			beforeBarrier = CD3DX12_RESOURCE_BARRIER::Transition(pDepthStencilResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			pCommandList->ResourceBarrier(1, &beforeBarrier);

			pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

			for (UINT64 i = 0, size = pRenderObjects->size(); i < size; ++i)
			{
				Model* pModel = (*pRenderObjects)[i];
				if (pModel->bIsVisible && pModel->bCastShadow)
				{
					switch (pModel->ModelType)
					{
						case DefaultModel:
						{
							pManager->SetCommonState(DepthOnlyDefault);
							pCommandList->SetGraphicsRootConstantBufferView(1, m_ShadowConstantsBufferForGS.GetGPUMemAddr());
							pModel->Render(pManager, DepthOnlyDefault);
						}
						break;

						case SkinnedModel:
						{
							SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pModel;
							pManager->SetCommonState(DepthOnlySkinned);
							pCommandList->SetGraphicsRootConstantBufferView(1, m_ShadowConstantsBufferForGS.GetGPUMemAddr());
							pCharacter->Render(pManager, DepthOnlySkinned);
						}
						break;

						case MirrorModel:
						{
							pManager->SetCommonState(DepthOnlyDefault);
							pCommandList->SetGraphicsRootConstantBufferView(1, m_ShadowConstantsBufferForGS.GetGPUMemAddr());
							pModel->Render(pManager, DepthOnlyDefault);
						}
						break;

						default:
							break;
					}
				}
			}
		}
		break;

		default:
			break;
	}

	_ASSERT(pDepthStencilResource);
	afterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(pDepthStencilResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
	pCommandList->ResourceBarrier(1, &afterBarrier);
}

void ShadowMap::Clear()
{
	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
			m_DirectionalLightShadowBuffer.Clear();
			for (int i = 0; i < 4; ++i)
			{
				m_ShadowConstantBuffers[i].Clear();
			}
			m_ShadowConstantsBufferForGS.Clear();
			break;

		case LIGHT_POINT:
			m_PointLightShadowBuffer.Clear();
			for (int i = 0; i < 6; ++i)
			{
				m_ShadowConstantBuffers[i].Clear();
			}
			m_ShadowConstantsBufferForGS.Clear();
			break;

		case LIGHT_SPOT:
			m_SpotLightShadowBuffer.Clear();
			m_ShadowConstantBuffers[0].Clear();
			break;

		default:
			break;
	}
}

void ShadowMap::SetDescriptorHeap(ResourceManager* pManager)
{
	_ASSERT(pManager);

	ID3D12Device5* pDevice = pManager->m_pDevice;
	const UINT DSV_DESCRIPTOR_SIZE = pManager->m_DSVDescriptorSize;
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(pManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_DSVHeapSize, DSV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(pManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_CBVSRVUAVHeapSize, CBV_SRV_DESCRIPTOR_SIZE);
	ID3D12Resource* pResource = nullptr;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	ZeroMemory(&dsvDesc, sizeof(dsvDesc));
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
		{
			pResource = m_DirectionalLightShadowBuffer.GetResource();

			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.ArraySize = 4;
			pDevice->CreateDepthStencilView(pResource, &dsvDesc, dsvHandle);
			m_DirectionalLightShadowBuffer.SetDSVHandle(dsvHandle);

			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.ArraySize = 4;
			pDevice->CreateShaderResourceView(pResource, &srvDesc, cbvSrvHandle);
			m_DirectionalLightShadowBuffer.SetSRVHandle(cbvSrvHandle);
		}
		break;

		case LIGHT_POINT:
		{
			pResource = m_PointLightShadowBuffer.GetResource();

			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.ArraySize = 6;
			pDevice->CreateDepthStencilView(pResource, &dsvDesc, dsvHandle);
			m_PointLightShadowBuffer.SetDSVHandle(dsvHandle);

			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = 1;
			pDevice->CreateShaderResourceView(pResource, &srvDesc, cbvSrvHandle);
			m_PointLightShadowBuffer.SetSRVHandle(cbvSrvHandle);
		}
		break;

		case LIGHT_SPOT:
		{
			pResource = m_SpotLightShadowBuffer.GetResource();

			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;
			pDevice->CreateDepthStencilView(pResource, &dsvDesc, dsvHandle);
			m_SpotLightShadowBuffer.SetDSVHandle(dsvHandle);

			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			pDevice->CreateShaderResourceView(pResource, &srvDesc, cbvSrvHandle);
			m_SpotLightShadowBuffer.SetSRVHandle(cbvSrvHandle);
		}
		break;

		default:
			__debugbreak();
			break;
	}

	++(pManager->m_DSVHeapSize);
	++(pManager->m_CBVSRVUAVHeapSize);
}

void ShadowMap::setShadowViewport(ID3D12GraphicsCommandList* pCommandList)
{
	_ASSERT(pCommandList);

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
		{
			const D3D12_VIEWPORT VIEWPORTS[4] =
			{
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
			};
			pCommandList->RSSetViewports(4, VIEWPORTS);
		}
		break;

		case LIGHT_POINT:
		{
			const D3D12_VIEWPORT VIEWPORTS[6] =
			{
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
				{ 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f },
			};
			pCommandList->RSSetViewports(6, VIEWPORTS);
		}
		break;

		case LIGHT_SPOT:
		{
			D3D12_VIEWPORT shadowViewport = { 0, };
			shadowViewport.TopLeftX = 0;
			shadowViewport.TopLeftY = 0;
			shadowViewport.Width = (float)m_ShadowMapWidth;
			shadowViewport.Height = (float)m_ShadowMapHeight;
			shadowViewport.MinDepth = 0.0f;
			shadowViewport.MaxDepth = 1.0f;
			pCommandList->RSSetViewports(1, &shadowViewport);
		}
		break;

		default:
			__debugbreak();
			break;
	}
}

void ShadowMap::setShadowScissorRect(ID3D12GraphicsCommandList* pCommandList)
{
	_ASSERT(pCommandList);

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
		{
			const D3D12_RECT SCISSOR_RECTS[4] =
			{
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
			};
			pCommandList->RSSetScissorRects(4, SCISSOR_RECTS);
		}
		break;

		case LIGHT_POINT:
		{
			const D3D12_RECT SCISSOR_RECTS[6] =
			{
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
				{ 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight },
			};
			pCommandList->RSSetScissorRects(6, SCISSOR_RECTS);
		}
		break;

		case LIGHT_SPOT:
		{
			D3D12_RECT scissorRect = { 0, };
			scissorRect.left = 0;
			scissorRect.top = 0;
			scissorRect.right = (long)m_ShadowMapWidth;
			scissorRect.bottom = (long)m_ShadowMapHeight;
			pCommandList->RSSetScissorRects(1, &scissorRect);
		}
		break;

		default:
			__debugbreak();
			break;
	}
}

void ShadowMap::calculateCascadeLightViewProjection(Vector3* pPosition, Matrix* pView, Matrix* pProjection, const Matrix& VIEW, const Matrix& PROJECTION, const Vector3& DIR, int cascadeIndex)
{
	_ASSERT(pPosition);
	_ASSERT(pView);
	_ASSERT(pProjection);

	const float FRUSTUM_Zs[5] = { 0.01f, 10.0f, 40.0f, 80.0f, 500.0f }; // ���� ����� �켱 ����.
	Matrix inverseView = VIEW.Invert();
	Vector3 frustumCenter(0.0f);
	float boundingSphereRadius = 0.0f;

	float fov = 45.0f;
	float aspectRatio = 1270.0f / 720.0f;
	float nearZ = FRUSTUM_Zs[0];
	float farZ = FRUSTUM_Zs[4];
	float tanHalfVFov = tanf(DirectX::XMConvertToRadians(fov * 0.5f)); // ���� �þ߰�.
	float tanHalfHFov = tanHalfVFov * aspectRatio; // ���� �þ߰�.

	float xn = FRUSTUM_Zs[cascadeIndex] * tanHalfHFov;
	float xf = FRUSTUM_Zs[cascadeIndex + 1] * tanHalfHFov;
	float yn = FRUSTUM_Zs[cascadeIndex] * tanHalfVFov;
	float yf = FRUSTUM_Zs[cascadeIndex + 1] * tanHalfVFov;

	Vector3 frustumCorners[8] =
	{
		Vector3(xn, yn, FRUSTUM_Zs[cascadeIndex]),
		Vector3(-xn, yn, FRUSTUM_Zs[cascadeIndex]),
		Vector3(xn, -yn, FRUSTUM_Zs[cascadeIndex]),
		Vector3(-xn, -yn, FRUSTUM_Zs[cascadeIndex]),
		Vector3(xf, yf, FRUSTUM_Zs[cascadeIndex + 1]),
		Vector3(-xf, yf, FRUSTUM_Zs[cascadeIndex + 1]),
		Vector3(xf, -yf, FRUSTUM_Zs[cascadeIndex + 1]),
		Vector3(-xf, -yf, FRUSTUM_Zs[cascadeIndex + 1]),
	};

	for (int i = 0; i < 8; ++i)
	{
		frustumCorners[i] = Vector3::Transform(frustumCorners[i], inverseView);
		frustumCenter += frustumCorners[i];
	}
	frustumCenter /= 8.0f;

	for (int i = 0; i < 8; ++i)
	{
		float dist = (frustumCorners[i] - frustumCenter).Length();
		boundingSphereRadius = Max(boundingSphereRadius, dist);
	}
	boundingSphereRadius = ceil(boundingSphereRadius * 16.0f) / 16.0f;

	Vector3 frustumMax(boundingSphereRadius);
	Vector3 frustumMin = -frustumMax;
	Vector3 cascadeExtents = frustumMax - frustumMin;

	*pPosition = frustumCenter - DIR * fabs(frustumMin.z);
	*pView = DirectX::XMMatrixLookAtLH(*pPosition, frustumCenter, Vector3(0.0f, 1.0f, 0.0f));
	*pProjection = DirectX::XMMatrixOrthographicOffCenterLH(frustumMin.x, frustumMax.x, frustumMin.y, frustumMax.y, 0.001f, cascadeExtents.z);
}
