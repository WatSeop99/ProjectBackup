#pragma once

#include "Model.h"
#include "../Graphics/Texture.h"

class SkinnedMeshModel final : public Model
{
public:
	SkinnedMeshModel(ResourceManager* pManager, const std::vector<MeshInfo>& MESHES, const AnimationData& ANIM_DATA);
	~SkinnedMeshModel() { Clear(); }

	void Initialize(ResourceManager* pManager, const std::vector<MeshInfo>& MESHES, const AnimationData& ANIM_DATA);
	void InitMeshBuffers(ResourceManager* pManager, const MeshInfo& MESH_INFO, Mesh* pNewMesh) override;
	void InitAnimationData(ResourceManager* pManager, const AnimationData& ANIM_DATA);

	void UpdateAnimation(int clipID, int frame) override;

	void Render(ResourceManager* pManager, ePipelineStateSetting psoSetting) override;

	void Clear();

	void SetDescriptorHeap(ResourceManager* pManager) override;

public:
	NonImageTexture BoneTransforms;
	AnimationData AnimData;
};
