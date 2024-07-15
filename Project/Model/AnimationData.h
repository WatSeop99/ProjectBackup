#pragma once

#include <directxtk12/SimpleMath.h>
#include <unordered_map>
#include <vector>
#include <string>

using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Quaternion;
using DirectX::SimpleMath::Vector3;

struct AnimationClip
{
	class Key
	{
	public:
		Key() = default;
		~Key() = default;

		Matrix GetTransform();

	public:
		Vector3 Position = Vector3(0.0f);
		Vector3 Scale = Vector3(1.0f);
		Quaternion Rotation = Quaternion();
		Quaternion UpdateRotation = Quaternion();
	};

	std::string Name;					 // Name of this animation clip.
	std::vector<std::vector<Key>> Keys;  // Keys[boneID][frame].
	int NumChannels;					 // Number of bones.
	int NumKeys;						 // Number of frames of this animation clip.
	double Duration;					 // Duration of animation in ticks.
	double TicksPerSec;					 // Frames per second.
};

class AnimationData
{
public:
	AnimationData() = default;
	~AnimationData() = default;

	void Update(int clipID, int frame);

	inline Matrix Get(int boneID)
	{
		return (InverseDefaultTransform * OffsetMatrices[boneID] * BoneTransforms[boneID] * DefaultTransform);
	}

public:
	std::unordered_map<std::string, int> BoneNameToID; // 뼈 이름과 인덱스 정수.
	std::vector<std::string> BoneIDToNames;				// BoneNameToID의 ID 순서대로 뼈 이름 저장.
	std::vector<int> BoneParents;					    // 부모 뼈의 인덱스.
	std::vector<Matrix> OffsetMatrices;					// root 뼈로부터 위치 offset 변환 행렬.
	std::vector<Matrix> BoneTransforms;					// 움직임에 따른 뼈의 변환 행렬.
	std::vector<AnimationClip> Clips;					// 애니메이션 동작.

	Matrix DefaultTransform = Matrix();			// normalizing을 위한 변환 행렬 [-1, 1]^3
	Matrix InverseDefaultTransform = Matrix();	// 모델 좌표계 복귀 변환 행렬.
	Matrix RootTransform = Matrix();
	Matrix AccumulatedRootTransform = Matrix();
	Vector3 PrevPos = Vector3(0.0f);
};

class Joint
{
public:
	Joint() = default;
	~Joint() = default;

	void Update(float deltaX, float deltaY, float deltaZ, std::vector<AnimationClip>* pClips, Matrix* pDefaultTransform, Matrix* pInverseDefaultTransform);

	void JacobianX(Vector3* pOutput, Vector3& parentPos);
	void JacobianY(Vector3* pOutput, Vector3& parentPos);
	void JacobianZ(Vector3* pOutput, Vector3& parentPos);

	inline Vector3 GetPos() { return pWorld->Translation(); }

public:
	UINT BoneID = 0xffffffff;

	Matrix* pWorld = nullptr; // World Matrix.
	Matrix* pOffset = nullptr; 
	Matrix* pParentMatrix = nullptr; // parent bone transform.
	Matrix* pJointTransform = nullptr; // bone transform.

	Matrix CharacterWorld = Matrix(); // 캐릭터 world.
	Matrix Correction = Matrix(); // world를 위한 보정값.
};
class Chain
{
public:
	Chain() = default;
	~Chain() = default;

	void SolveIK(Vector3& targetPos, const float DELTA_TIME);

public:
	std::vector<Joint> BodyChain; // root ~ child.
	std::vector<AnimationClip>* pAnimationClips = nullptr;
	Matrix DefaultTransform = Matrix();
	Matrix InverseDefaultTransform = Matrix();
};
