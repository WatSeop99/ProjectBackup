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
	void InitMeshBuffers(ResourceManager* pManager, const MeshInfo& MESH_INFO, Mesh** ppNewMesh);
	void InitAnimationData(ResourceManager* pManager, const AnimationData& ANIM_DATA);

	void UpdateConstantBuffers() override;
	void UpdateAnimation(int clipID, int frame) override;

	void Render(ResourceManager* pManager, ePipelineStateSetting psoSetting) override;
	void RenderEndEffectorSphere(ResourceManager* pManager, ePipelineStateSetting psoSetting);

	void Clear();

	void SetDescriptorHeap(ResourceManager* pManager) override;

public:
	NonImageTexture BoneTransforms;
	AnimationData AnimData;

	DirectX::BoundingSphere RightHand;
	DirectX::BoundingSphere LeftHand;
	DirectX::BoundingSphere RightToe;
	DirectX::BoundingSphere LeftToe;

private:
	Mesh* m_pRightHand = nullptr;
	Mesh* m_pLeftHand = nullptr;
	Mesh* m_pRightToe = nullptr;
	Mesh* m_pLeftToe = nullptr;

	Mesh* m_ppRightArm[3] = { nullptr, }; // right arm - right fore arm - right hand.
	Mesh* m_ppLeftArm[3] = { nullptr, }; // left arm - left fore arm - left hand.
	Mesh* m_ppRightLeg[3] = { nullptr, }; // right up leg - right leg - right foot.
	Mesh* m_ppLeftLeg[3] = { nullptr, }; // left up leg - left leg - left foot.

	// InitAnimationData()에서 초기화 필요.
	Chain m_RightArm;
	Chain m_LeftArm;
	Chain m_RightLeg;
	Chain m_LeftLeg;
};
