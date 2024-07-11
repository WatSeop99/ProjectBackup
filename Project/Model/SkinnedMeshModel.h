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
	void UpdateCharacter();
	void UpdateCharacter(Vector3& target, int chainPart);

	void Render(ResourceManager* pManager, ePipelineStateSetting psoSetting) override;
	void RenderEndEffectorSphere(ResourceManager* pManager, ePipelineStateSetting psoSetting);

	void Clear();

	void SetDescriptorHeap(ResourceManager* pManager) override;

	inline Mesh** GetRightArmsMesh() { return m_ppRightArm; }
	inline Mesh** GetLeftArmsMesh() { return m_ppLeftArm; }
	inline Mesh** GetRightLegsMesh() { return m_ppRightLeg; }
	inline Mesh** GetLeftLegsMesh() { return m_ppLeftLeg; }

protected:
	void initJointSpheres(ResourceManager* pManager);
	void initChain();

	void updateJointSpheres(int clipID, int frame);

public:
	NonImageTexture BoneTransforms;
	AnimationData AnimData;

	DirectX::BoundingSphere RightHandMiddle;
	DirectX::BoundingSphere LeftHandMiddle;
	DirectX::BoundingSphere RightToe;
	DirectX::BoundingSphere LeftToe;

	Chain RightArm;
	Chain LeftArm;
	Chain RightLeg;
	Chain LeftLeg;

private:
	Mesh* m_ppRightArm[4] = { nullptr, }; // right arm - right fore arm - right hand - right hand middle.
	Mesh* m_ppLeftArm[4] = { nullptr, }; // left arm - left fore arm - left hand - left hand middle.
	Mesh* m_ppRightLeg[4] = { nullptr, }; // right up leg - right leg - right foot - right toe.
	Mesh* m_ppLeftLeg[4] = { nullptr, }; // left up leg - left leg - left foot - left toe.

	// InitAnimationData()에서 초기화 필요.
	Chain m_RightArm;
	Chain m_LeftArm;
	Chain m_RightLeg;
	Chain m_LeftLeg;
};
