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
	std::unordered_map<std::string, int> BoneNameToID; // �� �̸��� �ε��� ����.
	std::vector<std::string> BoneIDToNames;				// BoneNameToID�� ID ������� �� �̸� ����.
	std::vector<int> BoneParents;					    // �θ� ���� �ε���.
	std::vector<Matrix> OffsetMatrices;					// root ���κ��� ��ġ offset ��ȯ ���.
	std::vector<Matrix> BoneTransforms;					// �����ӿ� ���� ���� ��ȯ ���.
	std::vector<AnimationClip> Clips;					// �ִϸ��̼� ����.

	Matrix DefaultTransform = Matrix();			// normalizing�� ���� ��ȯ ��� [-1, 1]^3
	Matrix InverseDefaultTransform = Matrix();	// �� ��ǥ�� ���� ��ȯ ���.
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

	Matrix CharacterWorld = Matrix(); // ĳ���� world.
	Matrix Correction = Matrix(); // world�� ���� ������.
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
