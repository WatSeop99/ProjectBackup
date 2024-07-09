#include "../pch.h"
#include "../Graphics/ConstantDataType.h"
#include "../Model/GeometryGenerator.h"
#include "../Graphics/GraphicsUtil.h"
#include "SkinnedMeshModel.h"

SkinnedMeshModel::SkinnedMeshModel(ResourceManager* pManager, const std::vector<MeshInfo>& MESHES, const AnimationData& ANIM_DATA)
{
	ModelType = SkinnedModel;
	Initialize(pManager, MESHES, ANIM_DATA);
}

void SkinnedMeshModel::Initialize(ResourceManager* pManager, const std::vector<MeshInfo>& MESH_INFOS, const AnimationData& ANIM_DATA)
{
	Model::Initialize(pManager, MESH_INFOS);
	InitAnimationData(pManager, ANIM_DATA);

	MeshInfo meshData;
	MeshConstant* pMeshConst = nullptr;
	MaterialConstant* pMaterialConst = nullptr;
	
	// right hand bounding sphere
	{
		RightHand = DirectX::BoundingSphere(BoundingSphere.Center, 0.08f);
		meshData = INIT_MESH_INFO;

		MakeWireSphere(&meshData, RightHand.Center, RightHand.Radius);
		m_pRightHand = new Mesh;
		m_pRightHand->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
		m_pRightHand->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
		pMeshConst = (MeshConstant*)m_pRightHand->MeshConstant.pData;
		pMeshConst->World = Matrix();

		InitMeshBuffers(pManager, meshData, m_pRightHand);
	}
	// left hand bounding sphere
	{
		LeftHand = DirectX::BoundingSphere(BoundingSphere.Center, 0.08f);
		meshData = INIT_MESH_INFO;

		MakeWireSphere(&meshData, LeftHand.Center, LeftHand.Radius);
		m_pLeftHand = new Mesh;
		m_pLeftHand->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
		m_pLeftHand->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
		pMeshConst = (MeshConstant*)m_pLeftHand->MeshConstant.pData;
		pMeshConst->World = Matrix();

		InitMeshBuffers(pManager, meshData, m_pLeftHand);
	}
	// right toe bounding sphere
	{
		RightToe = DirectX::BoundingSphere(BoundingSphere.Center, 0.08f);
		meshData = INIT_MESH_INFO;

		MakeWireSphere(&meshData, RightToe.Center, RightToe.Radius);
		m_pRightToe = new Mesh;
		m_pRightToe->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
		m_pRightToe->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
		pMeshConst = (MeshConstant*)m_pRightToe->MeshConstant.pData;
		pMeshConst->World = Matrix();

		InitMeshBuffers(pManager, meshData, m_pRightToe);
	}
	// left toe bounding sphere
	{
		LeftToe = DirectX::BoundingSphere(BoundingSphere.Center, 0.08f);
		meshData = INIT_MESH_INFO;

		MakeWireSphere(&meshData, LeftHand.Center, LeftHand.Radius);
		m_pLeftToe = new Mesh;
		m_pLeftToe->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
		m_pLeftToe->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
		pMeshConst = (MeshConstant*)m_pLeftToe->MeshConstant.pData;
		pMeshConst->World = Matrix();

		InitMeshBuffers(pManager, meshData, m_pLeftToe);
	}

	// for chain debugging.
	{
		meshData = INIT_MESH_INFO;

		for (int i = 0; i < 3; ++i)
		{
			Mesh** ppRightArmPart = &m_ppRightArm[i];
			Mesh** ppLeftArmPart = &m_ppLeftArm[i];
			Mesh** ppRightLegPart = &m_ppRightLeg[i];
			Mesh** ppLeftLegPart = &m_ppLeftLeg[i];

			MakeWireSphere(&meshData, BoundingSphere.Center, 0.08f);

			*ppRightArmPart = new Mesh;
			*ppLeftArmPart = new Mesh;
			*ppRightLegPart = new Mesh;
			*ppLeftLegPart = new Mesh;

			(*ppRightArmPart)->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
			(*ppLeftArmPart)->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
			(*ppRightLegPart)->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
			(*ppLeftLegPart)->MeshConstant.Initialize(pManager, sizeof(MeshConstant));

			(*ppRightArmPart)->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
			(*ppLeftArmPart)->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
			(*ppRightLegPart)->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
			(*ppLeftLegPart)->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
			
			pMeshConst = (MeshConstant*)(*ppRightArmPart)->MeshConstant.pData;
			pMeshConst->World = Matrix();
			pMeshConst = (MeshConstant*)(*ppLeftArmPart)->MeshConstant.pData;
			pMeshConst->World = Matrix();
			pMeshConst = (MeshConstant*)(*ppRightLegPart)->MeshConstant.pData;
			pMeshConst->World = Matrix();
			pMeshConst = (MeshConstant*)(*ppLeftLegPart)->MeshConstant.pData;
			pMeshConst->World = Matrix();

			// need to be for pp ver.
			InitMeshBuffers(pManager, meshData, ppRightArmPart);
			InitMeshBuffers(pManager, meshData, ppLeftArmPart);
			InitMeshBuffers(pManager, meshData, ppRightLegPart);
			InitMeshBuffers(pManager, meshData, ppLeftLegPart);
		}
	}
}

void SkinnedMeshModel::InitMeshBuffers(ResourceManager* pManager, const MeshInfo& MESH_INFO, Mesh* pNewMesh)
{
	_ASSERT(pManager);

	HRESULT hr = S_OK;

	// vertex buffer.
	{
		if (MESH_INFO.SkinnedVertices.size() > 0)
		{
			hr = pManager->CreateVertexBuffer(sizeof(SkinnedVertex),
											  (UINT)MESH_INFO.SkinnedVertices.size(),
											  &pNewMesh->Vertex.VertexBufferView,
											  &pNewMesh->Vertex.pBuffer,
											  (void*)MESH_INFO.SkinnedVertices.data());
			BREAK_IF_FAILED(hr);
			pNewMesh->Vertex.Count = (UINT)MESH_INFO.SkinnedVertices.size();
			pNewMesh->bSkinnedMesh = true;
		}
		else
		{
			hr = pManager->CreateVertexBuffer(sizeof(Vertex),
											  (UINT)MESH_INFO.Vertices.size(),
											  &pNewMesh->Vertex.VertexBufferView,
											  &pNewMesh->Vertex.pBuffer,
											  (void*)MESH_INFO.Vertices.data());
			BREAK_IF_FAILED(hr);
			pNewMesh->Vertex.Count = (UINT)MESH_INFO.Vertices.size();
		}
	}

	// index buffer.
	{
		hr = pManager->CreateIndexBuffer(sizeof(UINT),
										 (UINT)MESH_INFO.Indices.size(),
										 &pNewMesh->Index.IndexBufferView,
										 &pNewMesh->Index.pBuffer,
										 (void*)MESH_INFO.Indices.data());
		BREAK_IF_FAILED(hr);
		pNewMesh->Index.Count = (UINT)MESH_INFO.Indices.size();
	}
}

void SkinnedMeshModel::InitMeshBuffers(ResourceManager* pManager, const MeshInfo& MESH_INFO, Mesh** ppNewMesh)
{
	_ASSERT(pManager);

	HRESULT hr = S_OK;

	// vertex buffer.
	{
		if (MESH_INFO.SkinnedVertices.size() > 0)
		{
			hr = pManager->CreateVertexBuffer(sizeof(SkinnedVertex),
											  (UINT)MESH_INFO.SkinnedVertices.size(),
											  &(*ppNewMesh)->Vertex.VertexBufferView,
											  &(*ppNewMesh)->Vertex.pBuffer,
											  (void*)MESH_INFO.SkinnedVertices.data());
			BREAK_IF_FAILED(hr);
			(*ppNewMesh)->Vertex.Count = (UINT)MESH_INFO.SkinnedVertices.size();
			(*ppNewMesh)->bSkinnedMesh = true;
		}
		else
		{
			hr = pManager->CreateVertexBuffer(sizeof(Vertex),
											  (UINT)MESH_INFO.Vertices.size(),
											  &(*ppNewMesh)->Vertex.VertexBufferView,
											  &(*ppNewMesh)->Vertex.pBuffer,
											  (void*)MESH_INFO.Vertices.data());
			BREAK_IF_FAILED(hr);
			(*ppNewMesh)->Vertex.Count = (UINT)MESH_INFO.Vertices.size();
		}
	}

	// index buffer.
	{
		hr = pManager->CreateIndexBuffer(sizeof(UINT),
										 (UINT)MESH_INFO.Indices.size(),
										 &(*ppNewMesh)->Index.IndexBufferView,
										 &(*ppNewMesh)->Index.pBuffer,
										 (void*)MESH_INFO.Indices.data());
		BREAK_IF_FAILED(hr);
		(*ppNewMesh)->Index.Count = (UINT)MESH_INFO.Indices.size();
	}
}

void SkinnedMeshModel::InitAnimationData(ResourceManager* pManager, const AnimationData& ANIM_DATA)
{
	if (!ANIM_DATA.Clips.empty())
	{
		AnimData = ANIM_DATA;

		// 여기서는 AnimationClip이 SkinnedMesh라고 가정.
		// ANIM_DATA.Clips[0].Keys.size() -> 뼈의 수.

		BoneTransforms.Initialize(pManager, (UINT)ANIM_DATA.Clips[0].Keys.size(), sizeof(Matrix));

		// 단위행렬로 초기화.
		Matrix* pBoneTransformConstData = (Matrix*)BoneTransforms.pData;
		for (UINT64 i = 0, size = ANIM_DATA.Clips[0].Keys.size(); i < size; ++i)
		{
			pBoneTransformConstData[i] = Matrix();
		}

		BoneTransforms.Upload();
	}
}

void SkinnedMeshModel::UpdateConstantBuffers()
{
	Model::UpdateConstantBuffers();
	m_pRightHand->MeshConstant.Upload();
	m_pLeftHand->MeshConstant.Upload();
	m_pRightToe->MeshConstant.Upload();
	m_pLeftToe->MeshConstant.Upload();
}

void SkinnedMeshModel::UpdateAnimation(int clipID, int frame)
{
	// 입력에 따른 변환행렬 업데이트.
	AnimData.Update(clipID, frame);

	// 버퍼 업데이트.
	Matrix* pBoneTransformConstData = (Matrix*)BoneTransforms.pData;
	for (UINT64 i = 0, size = BoneTransforms.ElementCount; i < size; ++i)
	{
		pBoneTransformConstData[i] = AnimData.Get(clipID, (UINT)i, frame).Transpose();
	}
	BoneTransforms.Upload();

	// root bone transform을 통해 bounding box 업데이트.
	// 캐릭터는 world 상 고정된 좌표에서 bone transform을 통해 애니메이션하고 있으므로,
	// world를 바꿔주게 되면 캐릭터 자체가 시야에서 없어져버림.
	// 현재, Model에서는 bounding box와 bounding sphere를 world에 맞춰 이동시키는데,
	// 캐릭터에서는 이를 방지하기 위해 bounding object만 따로 변환시킴.
	const UINT ROOT_BONE_ID = AnimData.BoneNameToID["mixamorig:Hips"];
	const Matrix ROOT_BONE_TRANSFORM = AnimData.Get(clipID, ROOT_BONE_ID, frame);
	const Matrix CORRECTION_CENTER = Matrix::CreateTranslation(Vector3(0.2f, 0.0f, 0.0f));
	MeshConstant* pBoxMeshConst = (MeshConstant*)m_pBoundingBoxMesh->MeshConstant.pData;
	MeshConstant* pSphereMeshConst = (MeshConstant*)m_pBoundingSphereMesh->MeshConstant.pData;

	pBoxMeshConst->World = (CORRECTION_CENTER * ROOT_BONE_TRANSFORM * World).Transpose();
	pSphereMeshConst->World = pBoxMeshConst->World;
	BoundingBox.Center = pBoxMeshConst->World.Transpose().Translation();
	BoundingSphere.Center = BoundingBox.Center;

	// end-effector update. right hand, left hand, right toe, left toe.
	{
		const UINT RIGHT_HAND_ID = AnimData.BoneNameToID["mixamorig:RightHand"];
		const Matrix RIGHT_HAND_TRANSFORM = AnimData.Get(clipID, RIGHT_HAND_ID, frame);
		const Matrix CORRECTION_ENDEFFECTOR_HAND = Matrix::CreateTranslation(Vector3(-0.2f, 0.32f, 0.06f));
		MeshConstant* pRightHandSphere = (MeshConstant*)m_pRightHand->MeshConstant.pData;
		
		pRightHandSphere->World = (CORRECTION_ENDEFFECTOR_HAND * RIGHT_HAND_TRANSFORM * World).Transpose();
		RightHand.Center = pRightHandSphere->World.Transpose().Translation();
	}
	{
		const UINT LEFT_HAND_ID = AnimData.BoneNameToID["mixamorig:LeftHand"];
		const Matrix LEFT_HAND_TRANSFORM = AnimData.Get(clipID, LEFT_HAND_ID, frame);
		const Matrix CORRECTION_ENDEFFECTOR_HAND = Matrix::CreateTranslation(Vector3(0.6f, 0.33f, 0.05f));
		MeshConstant* pLeftHandSphere = (MeshConstant*)m_pLeftHand->MeshConstant.pData;

		pLeftHandSphere->World = (CORRECTION_ENDEFFECTOR_HAND * LEFT_HAND_TRANSFORM * World).Transpose();
		LeftHand.Center = pLeftHandSphere->World.Transpose().Translation();
	}
	{
		const UINT RIGHT_TOE = AnimData.BoneNameToID["mixamorig:RightToeBase"];
		const Matrix RIGHT_TOE_TRANSFORM = AnimData.Get(clipID, RIGHT_TOE, frame);
		const Matrix CORRECTION_ENDEFFECTOR_TOE = Matrix::CreateTranslation(Vector3(0.15f, -0.45f, 0.0f));
		MeshConstant* pRightFootSphere = (MeshConstant*)m_pRightToe->MeshConstant.pData;

		pRightFootSphere->World = (CORRECTION_ENDEFFECTOR_TOE * RIGHT_TOE_TRANSFORM * World).Transpose();
		RightToe.Center = pRightFootSphere->World.Transpose().Translation();
	}
	{
		const UINT LEFT_TOE_ID = AnimData.BoneNameToID["mixamorig:LeftToeBase"];
		const Matrix LEFT_TOE_TRANSFORM = AnimData.Get(clipID, LEFT_TOE_ID, frame);
		const Matrix CORRECTION_ENDEFFECTOR_TOE = Matrix::CreateTranslation(Vector3(0.25f, -0.45f, 0.0f));
		MeshConstant* pLeftFootSphere = (MeshConstant*)m_pLeftToe->MeshConstant.pData;

		pLeftFootSphere->World = (CORRECTION_ENDEFFECTOR_TOE * LEFT_TOE_TRANSFORM * World).Transpose();
		LeftToe.Center = pLeftFootSphere->World.Transpose().Translation();
	}
	// other parts.
	{

	}
}

void SkinnedMeshModel::Render(ResourceManager* pManager, ePipelineStateSetting psoSetting)
{
	_ASSERT(pManager);

	HRESULT hr = S_OK;

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;
	bool bIsSkinnedPSO = false;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable;

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* const pCurMesh = Meshes[i];

		switch (psoSetting)
		{
			case Skinned: case ReflectionSkinned:
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 10);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, BoneTransforms.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// b2, b3
				pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(2, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t0 ~ t5
				pDevice->CopyDescriptorsSimple(6, dstHandle, pCurMesh->Material.Albedo.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(6, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t6
				pDevice->CopyDescriptorsSimple(1, dstHandle, pCurMesh->Material.Height.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

			}
			break;

			case DepthOnlySkinned: case DepthOnlyCubeSkinned: case DepthOnlyCascadeSkinned:
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 3);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, BoneTransforms.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// b2, b3
				pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			}
			break;

			default:
				__debugbreak();
				break;
		}

		pCommandList->IASetVertexBuffers(0, 1, &pCurMesh->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&pCurMesh->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(pCurMesh->Index.Count, 1, 0, 0, 0);
	}
}

void SkinnedMeshModel::RenderEndEffectorSphere(ResourceManager* pManager, ePipelineStateSetting psoSetting)
{
	_ASSERT(pManager);

	HRESULT hr = S_OK;

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();
	ID3D12DescriptorHeap* pCBVSRVHeap = pManager->m_pCBVSRVUAVHeap;
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable[4];
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable[4];
	CD3DX12_CPU_DESCRIPTOR_HANDLE nullHandle(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), 14, CBV_SRV_DESCRIPTOR_SIZE);
	
	// alloc descriptor handles for end-effector spheres.
	for (int i = 0; i < 4; ++i)
	{
		hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable[i], &gpuDescriptorTable[i], 3);
		BREAK_IF_FAILED(hr);
	}

	// right hand -> left hand -> right toe -> left toe.
	{
		// b2, b3
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[0], m_pRightHand->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuDescriptorTable[0].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[0], m_pRightHand->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuDescriptorTable[0].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[0], nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[0]);

		pCommandList->IASetVertexBuffers(0, 1, &m_pRightHand->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&m_pRightHand->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(m_pRightHand->Index.Count, 1, 0, 0, 0);
	}
	{
		// b2, b3
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[1], m_pLeftHand->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuDescriptorTable[1].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[1], m_pLeftHand->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuDescriptorTable[1].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[1], nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[1]);

		pCommandList->IASetVertexBuffers(0, 1, &m_pLeftHand->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&m_pLeftHand->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(m_pLeftHand->Index.Count, 1, 0, 0, 0);
	}
	{
		// b2, b3
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[2], m_pRightToe->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuDescriptorTable[2].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[2], m_pRightToe->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuDescriptorTable[2].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[2], nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[2]);

		pCommandList->IASetVertexBuffers(0, 1, &m_pRightToe->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&m_pRightToe->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(m_pRightToe->Index.Count, 1, 0, 0, 0);
	}
	{
		// b2, b3
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[3], m_pLeftToe->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuDescriptorTable[3].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[3], m_pLeftToe->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cpuDescriptorTable[3].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[3], nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[3]);

		pCommandList->IASetVertexBuffers(0, 1, &m_pLeftToe->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&m_pLeftToe->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(m_pLeftToe->Index.Count, 1, 0, 0, 0);
	}
	// other parts.
	{

	}
}

void SkinnedMeshModel::Clear()
{
	BoneTransforms.Clear();

	if (m_pRightHand)
	{
		delete m_pRightHand;
		m_pRightHand = nullptr;
	}
	if (m_pLeftHand)
	{
		delete m_pLeftHand;
		m_pLeftHand = nullptr;
	}
	if (m_pRightToe)
	{
		delete m_pRightToe;
		m_pRightToe = nullptr;
	}
	if (m_pLeftToe)
	{
		delete m_pLeftToe;
		m_pLeftToe = nullptr;
	}
	for (int i = 0; i < 3; ++i)
	{
		if (m_ppRightArm[i])
		{
			delete m_ppRightArm[i];
			m_ppRightArm[i] = nullptr;
		}
		if (m_ppLeftArm[i])
		{
			delete m_ppLeftArm[i];
			m_ppLeftArm[i] = nullptr;
		}
		if (m_ppRightLeg[i])
		{
			delete m_ppRightLeg[i];
			m_ppRightLeg[i] = nullptr;
		}
		if (m_ppLeftLeg[i])
		{
			delete m_ppLeftLeg[i];
			m_ppLeftLeg[i] = nullptr;
		}
	}
}

void SkinnedMeshModel::SetDescriptorHeap(ResourceManager* pManager)
{
	_ASSERT(pManager);

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12DescriptorHeap* pCBVSRVHeap = pManager->m_pCBVSRVUAVHeap;
	
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvLastHandle(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_CBVSRVUAVHeapSize, CBV_SRV_UAV_DESCRIPTOR_SIZE);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// BoneTransform buffer.
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC structuredSRVDesc;
		structuredSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		structuredSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		structuredSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		structuredSRVDesc.Buffer.FirstElement = 0;
		structuredSRVDesc.Buffer.NumElements = (UINT)(BoneTransforms.ElementCount);
		structuredSRVDesc.Buffer.StructureByteStride = (UINT)(BoneTransforms.ElementSize);
		structuredSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		pDevice->CreateShaderResourceView(BoneTransforms.GetResource(), &structuredSRVDesc, cbvSrvLastHandle);
		BoneTransforms.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);
	}

	// meshes.
	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];
		ConstantBuffer& meshConstant = pCurMesh->MeshConstant;
		ConstantBuffer& materialConstant = pCurMesh->MaterialConstant;
		Material* pMaterialBuffer = &pCurMesh->Material;

		cbvDesc.BufferLocation = meshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(meshConstant.GetBufferSize());
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		meshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = materialConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(materialConstant.GetBufferSize());
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		materialConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pDevice->CreateShaderResourceView(pMaterialBuffer->Albedo.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Albedo.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->Emissive.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Emissive.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		pDevice->CreateShaderResourceView(pMaterialBuffer->Normal.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Normal.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->Height.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Height.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->AmbientOcclusion.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->AmbientOcclusion.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->Metallic.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Metallic.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->Roughness.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Roughness.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);
	}

	// bounding box.
	cbvDesc.BufferLocation = m_pBoundingBoxMesh->MeshConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingBoxMesh->MeshConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingBoxMesh->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	cbvDesc.BufferLocation = m_pBoundingBoxMesh->MaterialConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingBoxMesh->MaterialConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingBoxMesh->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	// bounding sphere.
	cbvDesc.BufferLocation = m_pBoundingSphereMesh->MeshConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingSphereMesh->MeshConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingSphereMesh->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	cbvDesc.BufferLocation = m_pBoundingSphereMesh->MaterialConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingSphereMesh->MaterialConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingSphereMesh->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	// end-effector sphere.
	// right hand.
	cbvDesc.BufferLocation = m_pRightHand->MeshConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pRightHand->MeshConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pRightHand->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	cbvDesc.BufferLocation = m_pRightHand->MaterialConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pRightHand->MaterialConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pRightHand->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	// left hand.
	cbvDesc.BufferLocation = m_pLeftHand->MeshConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pLeftHand->MeshConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pLeftHand->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	cbvDesc.BufferLocation = m_pLeftHand->MaterialConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pLeftHand->MaterialConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pLeftHand->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	// right toe.
	cbvDesc.BufferLocation = m_pRightToe->MeshConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pRightToe->MeshConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pRightToe->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	cbvDesc.BufferLocation = m_pRightToe->MaterialConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pRightToe->MaterialConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pRightToe->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	// left toe.
	cbvDesc.BufferLocation = m_pLeftToe->MeshConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pLeftToe->MeshConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pLeftToe->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	cbvDesc.BufferLocation = m_pLeftToe->MaterialConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pLeftToe->MaterialConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pLeftToe->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	// other parts.
	for (int i = 0; i < 3; ++i)
	{
		Mesh** ppRightArmPart = &m_ppRightArm[i];
		Mesh** ppLeftArmPart = &m_ppLeftArm[i];
		Mesh** ppRightLegPart = &m_ppRightLeg[i];
		Mesh** ppLeftLegPart = &m_ppLeftLeg[i];

		cbvDesc.BufferLocation = (*ppRightArmPart)->MeshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppRightArmPart)->MeshConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppRightArmPart)->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppLeftArmPart)->MeshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppLeftArmPart)->MeshConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppLeftArmPart)->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppRightLegPart)->MeshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppRightLegPart)->MeshConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppRightLegPart)->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppLeftLegPart)->MeshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppLeftLegPart)->MeshConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppLeftLegPart)->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);
	}
}
