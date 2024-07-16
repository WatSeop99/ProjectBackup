#include <Eigen/Dense>
#include "../pch.h"
#include "AnimationData.h"

Matrix AnimationClip::Key::GetTransform()
{
	Quaternion newRot = Quaternion::Concatenate(Rotation, UpdateRotation);
	return (Matrix::CreateScale(Scale) * Matrix::CreateFromQuaternion(newRot) * Matrix::CreateTranslation(Position));
}

void AnimationData::Update(int clipID, int frame)
{
	AnimationClip& clip = Clips[clipID];

	// root bone id�� 0(�ƴ� �� ����).
	// root bone�� ���� bone transform update.
	{
		const int ROOT_BONE_ID = 0;
		std::vector<AnimationClip::Key>& keys = clip.Keys[ROOT_BONE_ID];
		const UINT64 KEY_SIZE = keys.size();

		const int PARENT_ID = BoneParents[ROOT_BONE_ID];
		const Matrix& PARENT_MATRIX = AccumulatedRootTransform;
		AnimationClip::Key& key = keys[frame % KEY_SIZE];

		if (frame != 0)
		{
			AccumulatedRootTransform = (Matrix::CreateTranslation(key.Position - PrevPos) * AccumulatedRootTransform); // root ���� ��ȯ�� ������Ŵ.
		}
		else
		{
			Vector3 rootBoneTranslation = AccumulatedRootTransform.Translation();
			rootBoneTranslation.y = key.Position.y;
			AccumulatedRootTransform.Translation(rootBoneTranslation);
		}

		PrevPos = key.Position;
		// key.Position = Vector3(0.0f);

		Quaternion newRot = Quaternion::Concatenate(key.Rotation, key.UpdateRotation);
		BoneTransforms[ROOT_BONE_ID] = Matrix::CreateScale(key.Scale) * Matrix::CreateFromQuaternion(newRot) * Matrix::CreateTranslation(Vector3(0.0f)) * PARENT_MATRIX;
	}

	// ������ bone transform ������Ʈ.
	for (UINT64 boneID = 1, totalTransformSize = BoneTransforms.size(); boneID < totalTransformSize; ++boneID)
	{
		std::vector<AnimationClip::Key>& keys = clip.Keys[boneID];
		const UINT64 KEY_SIZE = keys.size();

		const int PARENT_ID = BoneParents[boneID];
		const Matrix& PARENT_MATRIX = BoneTransforms[PARENT_ID];
		AnimationClip::Key& key = keys[frame % KEY_SIZE];

		BoneTransforms[boneID] = key.GetTransform() * PARENT_MATRIX;
		/*{
			char debugString[256];

			sprintf_s(debugString, 256, "boneID: %ud\n", boneID);
			OutputDebugStringA(debugString);

			sprintf_s(debugString, 256, "key pos: %f, %f, %f\n", key.Position.x, key.Position.y, key.Position.z);
			OutputDebugStringA(debugString);

			sprintf_s(debugString, 256, "key scale: %f, %f, %f\n", key.Scale.x, key.Scale.y, key.Scale.z);
			OutputDebugStringA(debugString);

			sprintf_s(debugString, 256, "key rotation: %f, %f, %f, %f\n\n", key.Rotation.x, key.Rotation.y, key.Rotation.z, key.Rotation.z);
			OutputDebugStringA(debugString);
		}*/
	}
}

Matrix AnimationData::GetBonePositionMatrix(int boneID, int clipID, int frame)
{
	std::vector<AnimationClip::Key>& keys = Clips[clipID].Keys[boneID];
	const UINT64 KEY_SIZE = keys.size();
	AnimationClip::Key& key = keys[frame % KEY_SIZE];

	return (InverseDefaultTransform * OffsetMatrices[boneID] * key.GetTransform() * DefaultTransform);
}

void Joint::Update(float deltaThetaX, float deltaThetaY, float deltaThetaZ, std::vector<AnimationClip>* pClips, Matrix* pDefaultTransform, Matrix* pInverseDefaultTransform, int clipID, int frame)
{
	_ASSERT(pClips);

	// ��ȯ ����.
	Matrix rot = Matrix::CreateFromYawPitchRoll(deltaThetaX, deltaThetaY, deltaThetaZ);
	Quaternion deltaRot = Quaternion::CreateFromRotationMatrix(rot);
	
	// ���� Ű �����Ϳ����� ��ȯ ����.
	std::vector<AnimationClip::Key>& keys = (*pClips)[clipID].Keys[BoneID];
	const UINT64 KEY_SIZE = keys.size();

	for (UINT64 i = 0; i < KEY_SIZE; ++i)
	{
		// ���� Ű �����Ϳ����� ��ȯ ����.
		AnimationClip::Key& key = keys[frame % KEY_SIZE];
		Quaternion originRot = key.Rotation;
		Quaternion prevUpdateRot = key.UpdateRotation;

		// ���� �������� ��ȯ ���� ����.
		Quaternion newUpdateRot = Quaternion::Concatenate(prevUpdateRot, deltaRot);
		key.UpdateRotation = newUpdateRot;
	}
}

void Joint::JacobianX(Vector3* pOutput, Vector3& parentPos)
{
	Vector3 xAxis(1.0f, 0.0f, 0.0f);
	Vector3 diff = Position - parentPos;
	
	*pOutput = xAxis.Cross(diff);
}

void Joint::JacobianY(Vector3* pOutput, Vector3& parentPos)
{
	Vector3 yAxis(0.0f, 1.0f, 0.0f);
	Vector3 diff = Position - parentPos;

	*pOutput = yAxis.Cross(diff);
}

void Joint::JacobianZ(Vector3* pOutput, Vector3& parentPos)
{
	Vector3 zAxis(0.0f, 0.0f, 1.0f);
	Vector3 diff = Position - parentPos;

	*pOutput = zAxis.Cross(diff);
}

void Chain::SolveIK(Vector3& targetPos, int clipID, int frame, const float DELTA_TIME)
{
	_ASSERT(BodyChain.size() > 0);

	const UINT64 TOTAL_JOINT = BodyChain.size();
	Eigen::MatrixXf J(3, 3 * TOTAL_JOINT); // 3����, ���� joint 3����.
	Eigen::MatrixXf b(3, 1);
	Eigen::VectorXf deltaTheta(TOTAL_JOINT * 3);
	Joint& endEffector = BodyChain[TOTAL_JOINT - 1];

	/*{
		std::string debugString;
		debugString = std::string("targetPos: ") + std::to_string(targetPos.x) + std::string(", ") + std::to_string(targetPos.y) + std::string(", ") + std::to_string(targetPos.z) + std::string("\n");
		OutputDebugStringA(debugString.c_str());

		debugString = std::string("delta time: ") + std::to_string(DELTA_TIME) + std::string("\n");
		OutputDebugStringA(debugString.c_str());
	}*/

	for (int step = 0; step < 100; ++step)
	{
		Vector3 deltaPos = targetPos - endEffector.Position;
		float deltaPosLength = deltaPos.Length();

		/*{
			char debugString[256];
			Vector3 pos = endEffector.Position;
			sprintf_s(debugString, "deltaPosLength: %f\n", deltaPosLength);
			// sprintf_s(debugString, "deltaPos pos: %f, %f, %f\n", deltaPos.x, deltaPos.y, deltaPos.z);
			OutputDebugStringA(debugString);
		}*/

		if (deltaPosLength <= 0.0001f || deltaPosLength >= 1.0f)
		{
			break;
		}

		b << deltaPos.x, deltaPos.y, deltaPos.z;

		int columnIndex = 0;
		for (UINT64 i = 0; i < TOTAL_JOINT; ++i)
		{
			Joint* pJoint = &BodyChain[i];
			Vector3 jointPos = pJoint->Position;
			Vector3 partialX;
			Vector3 partialY;
			Vector3 partialZ;

			endEffector.JacobianX(&partialX, jointPos);
			endEffector.JacobianY(&partialY, jointPos);
			endEffector.JacobianZ(&partialZ, jointPos);

			J.col(columnIndex) << partialX.x, partialX.y, partialX.z;
			J.col(columnIndex + 1) << partialY.x, partialY.y, partialY.z;
			J.col(columnIndex + 2) << partialZ.x, partialZ.y, partialZ.z;
			
			columnIndex += 3;
		}

		deltaTheta = J.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b);
		deltaTheta *= DELTA_TIME;
		{
			char debugString[256];

			sprintf_s(debugString, "Theta 1: %f, %f, %f\n", deltaTheta[0], deltaTheta[1], deltaTheta[2]);
			OutputDebugStringA(debugString);

			sprintf_s(debugString, "Theta 2: %f, %f, %f\n", deltaTheta[3], deltaTheta[4], deltaTheta[5]);
			OutputDebugStringA(debugString);

			sprintf_s(debugString, "Theta 3: %f, %f, %f\n\n", deltaTheta[6], deltaTheta[7], deltaTheta[8]);
			OutputDebugStringA(debugString);
		}
		columnIndex = 0;
		for (UINT64 i = 0; i < TOTAL_JOINT; ++i)
		{
			Joint* pJoint = &BodyChain[i];
			pJoint->Update(deltaTheta[columnIndex], deltaTheta[columnIndex + 1], deltaTheta[columnIndex + 2], pAnimationClips, &DefaultTransform, &InverseDefaultTransform, clipID, frame);
			columnIndex += 3;
		}
	}
}
