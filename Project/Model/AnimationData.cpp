#include <Eigen/Dense>
#include "../pch.h"
#include "AnimationData.h"

void AnimationData::Update(int clipID, int frame)
{
	AnimationClip& clip = Clips[clipID];

	for (UINT64 boneID = 0, totalTransformSize = BoneTransforms.size(); boneID < totalTransformSize; ++boneID)
	{
		std::vector<AnimationClip::Key>& keys = clip.Keys[boneID];
		const UINT64 KEY_SIZE = keys.size();

		const int PARENT_ID = BoneParents[boneID];
		const Matrix& PARENT_MATRIX = (PARENT_ID >= 0 ? BoneTransforms[PARENT_ID] : AccumulatedRootTransform);
		AnimationClip::Key key = keys[frame % KEY_SIZE];

		// Root일 경우.
		if (PARENT_ID < 0)
		{
			if (frame != 0)
			{
				AccumulatedRootTransform = (Matrix::CreateTranslation(key.Position - PrevPos) * AccumulatedRootTransform); // root 뼈의 변환을 누적시킴.
			}
			else
			{
				Vector3 temp = AccumulatedRootTransform.Translation();
				temp.y = key.Position.y;
				AccumulatedRootTransform.Translation(temp);
			}

			PrevPos = key.Position;
			key.Position = Vector3(0.0f);
		}

		// BoneTransforms[boneID] = key.GetTransform() * PARENT_MATRIX;
		
		Quaternion originRot = key.Rotation;
		Quaternion updateRot = clip.UpdateRotations[boneID][frame % KEY_SIZE];
		Quaternion newRot = Quaternion::Concatenate(originRot, updateRot);
		Matrix adjustingTransform = Matrix::CreateScale(key.Scale) * Matrix::CreateFromQuaternion(newRot) * Matrix::CreateTranslation(key.Position);
		BoneTransforms[boneID] = adjustingTransform * PARENT_MATRIX;
		/*{
			std::string debugString;
			debugString = std::string("boneID: ") + std::to_string(boneID) + std::string("\n");
			OutputDebugStringA(debugString.c_str());

			debugString = std::string("key pos: ") + std::to_string(key.Position.x) + std::string(", ") + std::to_string(key.Position.y) + std::string(", ") + std::to_string(key.Position.z) + std::string("\n");
			OutputDebugStringA(debugString.c_str());

			debugString = std::string("key scale: ") + std::to_string(key.Scale.x) + std::string(", ") + std::to_string(key.Scale.y) + std::string(", ") + std::to_string(key.Scale.z) + std::string("\n");
			OutputDebugStringA(debugString.c_str());

			debugString = std::string("key rotation: ") + std::to_string(key.Rotation.x) + std::string(", ") + std::to_string(key.Rotation.y) + std::string(", ") + std::to_string(key.Rotation.z) + std::string("\n\n");
			OutputDebugStringA(debugString.c_str());
		}*/
	}
}

void Joint::Update(float deltaThetaX, float deltaThetaY, float deltaThetaZ, std::vector<AnimationClip>* pClips, Matrix* pDefaultTransform, Matrix* pInverseDefaultTransform, int clipID, int frame)
{
	_ASSERT(pClips);

	// 변환 각도.
	Matrix rot = Matrix::CreateFromYawPitchRoll(deltaThetaX, deltaThetaY, deltaThetaZ);
	Quaternion deltaRot = Quaternion::CreateFromRotationMatrix(rot);
	
	//// 원래 키 데이터에서의 변환 각도.
	//std::vector<AnimationClip::Key>& keys = (*pClips)[clipID].Keys[BoneID];
	//const UINT64 KEY_SIZE = keys.size();
	//AnimationClip::Key key = keys[frame % KEY_SIZE];
	//Quaternion originRot = key.Rotation;

	//// 원래 각도에서 변환 각도 적용.
	//Quaternion newRot = Quaternion::Concatenate(originRot, deltaRot);
	//Matrix newKeyTransform = Matrix::CreateScale(key.Scale) * Matrix::CreateFromQuaternion(newRot) * Matrix::CreateTranslation(key.Position);

	//// 적용.
	//*pJointTransform = newKeyTransform * (*pParentMatrix);
	//*pWorld = Correction * (*pInverseDefaultTransform) * (*pOffset) * (*pJointTransform) * (*pDefaultTransform) * CharacterWorld;

	std::vector<AnimationClip::Key>& keys = (*pClips)[clipID].Keys[BoneID];
	std::vector<Quaternion>& updateRots = (*pClips)[clipID].UpdateRotations[BoneID];
	const UINT64 KEY_SIZE = keys.size();

	updateRots[frame % KEY_SIZE] = deltaRot;
}

void Joint::JacobianX(Vector3* pOutput, Vector3& parentPos)
{
	Vector3 xAxis(1.0f, 0.0f, 0.0f);
	Vector3 curJointPos = pWorld->Translation();
	Vector3 diff = curJointPos - parentPos;
	
	// xAxis = Vector3::Transform(xAxis, *pWorld);
	*pOutput = xAxis.Cross(diff);
}

void Joint::JacobianY(Vector3* pOutput, Vector3& parentPos)
{
	Vector3 yAxis(0.0f, 1.0f, 0.0f);
	Vector3 curJointPos = pWorld->Translation();
	Vector3 diff = curJointPos - parentPos;

	// yAxis = Vector3::Transform(yAxis, *pWorld);
	*pOutput = yAxis.Cross(diff);
}

void Joint::JacobianZ(Vector3* pOutput, Vector3& parentPos)
{
	Vector3 zAxis(0.0f, 0.0f, 1.0f);
	Vector3 curJointPos = pWorld->Translation();
	Vector3 diff = curJointPos - parentPos;

	// zAxis = Vector3::Transform(zAxis, *pWorld);
	*pOutput = zAxis.Cross(diff);
}

void Chain::SolveIK(Vector3& targetPos, int clipID, int frame, const float DELTA_TIME)
{
	_ASSERT(BodyChain.size() > 0);

	const UINT64 TOTAL_JOINT = BodyChain.size();
	Eigen::MatrixXf J(3, 3 * TOTAL_JOINT); // 3차원, 관여 joint 3개임.
	Eigen::MatrixXf b(3, 1);
	Eigen::VectorXf deltaTheta(TOTAL_JOINT * 3);
	Joint& endEffector = BodyChain[TOTAL_JOINT - 1];

	{
		std::string debugString;
		debugString = std::string("targetPos: ") + std::to_string(targetPos.x) + std::string(", ") + std::to_string(targetPos.y) + std::string(", ") + std::to_string(targetPos.z) + std::string("\n");
		OutputDebugStringA(debugString.c_str());
	}

	for (int step = 0; step < 100; ++step)
	{
		Vector3 deltaPos = targetPos - endEffector.GetPos();
		b << deltaPos.x, deltaPos.y, deltaPos.z;

		int columnIndex = 0;
		for (UINT64 i = 0; i < TOTAL_JOINT; ++i)
		{
			Joint* pJoint = &BodyChain[i];
			Vector3 jointPos = pJoint->GetPos();
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
			std::string debugString;
			debugString = std::string("Theta 1: ") + std::to_string(deltaTheta[0]) + std::string(", ") + std::to_string(deltaTheta[1]) + std::string(", ") + std::to_string(deltaTheta[2]) + std::string("\n");
			OutputDebugStringA(debugString.c_str());

			debugString = std::string("Theta 2: ") + std::to_string(deltaTheta[3]) + std::string(", ") + std::to_string(deltaTheta[4]) + std::string(", ") + std::to_string(deltaTheta[5]) + std::string("\n");
			OutputDebugStringA(debugString.c_str());

			debugString = std::string("Theta 3: ") + std::to_string(deltaTheta[6]) + std::string(", ") + std::to_string(deltaTheta[7]) + std::string(", ") + std::to_string(deltaTheta[8]) + std::string("\n");
			OutputDebugStringA(debugString.c_str());
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
