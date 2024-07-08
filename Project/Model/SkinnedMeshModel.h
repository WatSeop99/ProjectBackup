#pragma once

#include "Model.h"
#include "../Graphics/Texture.h"

class SkinnedMeshModel final : public Model
{
public:
	SkinnedMeshModel(ResourceManager* pManager, const std::vector<MeshInfo>& MESHES, const AnimationData& ANIM_DATA);
	~SkinnedMeshModel() { Clear(); }

	void Initialize(ResourceManager* pManager, const std::vector<MeshInfo>& MESH_INFOS, const AnimationData& ANIM_DATA);
	void InitMeshBuffers(ResourceManager* pManager, const MeshInfo& MESH_INFO, Mesh* pNewMesh) override;
	void InitAnimationData(ResourceManager* pManager, const AnimationData& ANIM_DATA);

	void UpdateConstantBuffers() override;
	void UpdateAnimation(int clipID, int frame) override;

	void Render(ResourceManager* pManager, ePipelineStateSetting psoSetting) override;
	void RenderEndEffectorSphere(ResourceManager* pManager, ePipelineStateSetting psoSetting);

	void Clear();

	void SetDescriptorHeap(ResourceManager* pManager) override;

protected:
	

public:
	NonImageTexture BoneTransforms;
	AnimationData AnimData;

	DirectX::BoundingSphere RightHand;
	DirectX::BoundingSphere LeftHand;
	DirectX::BoundingSphere RightFoot;
	DirectX::BoundingSphere LeftFoot;

private:
	Mesh* m_pRightHand = nullptr;
	Mesh* m_pLeftHand = nullptr;
	Mesh* m_pRightFoot = nullptr;
	Mesh* m_pLeftFoot = nullptr;
};
