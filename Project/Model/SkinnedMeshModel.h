#pragma once

#include "Model.h"
#include "../Graphics/Texture.h"

class SkinnedMeshModel final : public Model
{
public:
	SkinnedMeshModel(Renderer* pRenderer, const std::vector<MeshInfo>& MESHES, const AnimationData& ANIM_DATA);
	~SkinnedMeshModel() { Cleanup(); }

	void Initialize(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS, const AnimationData& ANIM_DATA);
	void InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh* pNewMesh) override;
	void InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh** ppNewMesh);
	void InitAnimationData(Renderer* pRenderer, const AnimationData& ANIM_DATA);

	void UpdateConstantBuffers() override;
	void UpdateAnimation(int clipID, int frame) override;
	void UpdateCharacterIK(Vector3& target, int chainPart, int clipID, int frame, const float DELTA_TIME);

	void Render(Renderer* pRenderer, eRenderPSOType psoSetting) override;
	void Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ResourceManager* pManager, int psoSetting) override;
	void RenderBoundingCapsule(Renderer* pRenderer, eRenderPSOType psoSetting);
	void RenderJointSphere(Renderer* pRenderer, eRenderPSOType psoSetting);

	void Cleanup();

	inline Mesh** GetRightArmsMesh() { return m_ppRightArm; }
	inline Mesh** GetLeftArmsMesh() { return m_ppLeftArm; }
	inline Mesh** GetRightLegsMesh() { return m_ppRightLeg; }
	inline Mesh** GetLeftLegsMesh() { return m_ppLeftLeg; }

	void SetDescriptorHeap(Renderer* pRenderer) override;

protected:
	void initBoundingCapsule(Renderer* pRenderer);
	void initJointSpheres(Renderer* pRenderer);
	void initChain();

	void updateJointSpheres(int clipID, int frame);

public:
	NonImageTexture BoneTransforms;
	AnimationData CharacterAnimationData;

	DirectX::BoundingSphere RightHandMiddle;
	DirectX::BoundingSphere LeftHandMiddle;
	DirectX::BoundingSphere RightToe;
	DirectX::BoundingSphere LeftToe;

	Chain RightArm;
	Chain LeftArm;
	Chain RightLeg;
	Chain LeftLeg;

	// capsule ���� Ŭ���� �ʿ�.

private:
	Mesh* m_ppRightArm[4] = { nullptr, }; // right arm - right fore arm - right hand - right hand middle.
	Mesh* m_ppLeftArm[4] = { nullptr, }; // left arm - left fore arm - left hand - left hand middle.
	Mesh* m_ppRightLeg[4] = { nullptr, }; // right up leg - right leg - right foot - right toe.
	Mesh* m_ppLeftLeg[4] = { nullptr, }; // left up leg - left leg - left foot - left toe.
	Mesh* m_pBoundingCapsuleMesh = nullptr;
};
