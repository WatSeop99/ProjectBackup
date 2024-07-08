#pragma once

#include "AnimationData.h"
#include "../Util/LinkedList.h"
#include "Mesh.h"
#include "MeshInfo.h"

using DirectX::SimpleMath::Matrix;

enum eModelType
{
	DefaultModel = 0,
	SkinnedModel,
	SkyboxModel,
	MirrorModel,
	TotalModelType
};

class Model
{
public:
	Model() = default;
	Model(ResourceManager* pManager, std::wstring& basePath, std::wstring& fileName);
	Model(ResourceManager* pManager, const std::vector<MeshInfo>& MESH_INFOS);
	virtual ~Model() { Clear(); }

	void Initialize(ResourceManager* pManager, std::wstring& basePath, std::wstring& fileName);
	void Initialize(ResourceManager* pManager, const std::vector<MeshInfo>& MESH_INFOS);
	virtual void Initialize(ResourceManager* pManager) { }
	virtual void InitMeshBuffers(ResourceManager* pManager, const MeshInfo& MESH_INFO, Mesh* pNewMesh);

	virtual void UpdateConstantBuffers();
	void UpdateWorld(const Matrix& WORLD);
	virtual void UpdateAnimation(int clipID, int frame) { }

	virtual void Render(ResourceManager* pManager, ePipelineStateSetting psoSetting);
	void RenderBoundingBox(ResourceManager* pManager, ePipelineStateSetting psoSetting);
	void RenderBoundingSphere(ResourceManager* pManager, ePipelineStateSetting psoSetting);

	virtual void Clear();

	virtual void SetDescriptorHeap(ResourceManager* pManager); 

protected:
	void initBoundingBox(ResourceManager* pManager, const std::vector<MeshInfo>& MESH_INFOS);
	void initBoundingSphere(ResourceManager* pManager, const std::vector<MeshInfo>& MESH_INFOS);

	DirectX::BoundingBox getBoundingBox(const std::vector<Vertex>& VERTICES);
	void extendBoundingBox(const DirectX::BoundingBox& SRC_BOX, DirectX::BoundingBox* pDestBox);

public:
	Matrix World = Matrix();
	Matrix WorldInverseTranspose = Matrix();

	std::vector<Mesh*> Meshes;

	DirectX::BoundingBox BoundingBox;
	DirectX::BoundingSphere BoundingSphere;

	std::string Name;

	eModelType ModelType = DefaultModel;

	bool bIsVisible = true;
	bool bCastShadow = true;
	bool bIsPickable = false;

	// ListElem LinkToList = { nullptr, nullptr, this };

protected:
	Mesh* m_pBoundingBoxMesh = nullptr;
	Mesh* m_pBoundingSphereMesh = nullptr;
};
