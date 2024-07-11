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

		const int PARENT_ID = (int)BoneParents[boneID];
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

		BoneTransforms[boneID] = key.GetTransform() * PARENT_MATRIX;

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
	Quaternion deltaRot(deltaThetaX, deltaThetaY, deltaThetaZ, 0.0f);
	
	// 원래 키 데이터에서의 변환 각도.
	std::vector<AnimationClip::Key>& keys = (*pClips)[clipID].Keys[BoneID];
	const UINT64 KEY_SIZE = keys.size();
	AnimationClip::Key key = keys[frame % KEY_SIZE];
	Quaternion originRot = key.Rotation;

	// 원래 각도에서 변환 각도 적용.
	Quaternion newRot = Quaternion::Concatenate(originRot, deltaRot);
	Matrix newKeyTransform = Matrix::CreateScale(key.Scale) * Matrix::CreateFromQuaternion(newRot) * Matrix::CreateTranslation(key.Position);

	// 적용.
	*pJointTransform = newKeyTransform * (*pParentMatrix);
	*pWorld = Correction * (*pInverseDefaultTransform) * (*pOffset) * (*pJointTransform) * (*pDefaultTransform) * CharacterWorld;
}

void Joint::JacobianX(Vector3* pOutput, Vector3& target)
{
	Vector3 xAxis(1.0f, 0.0f, 0.0f);
	Vector3 diff = target - (*pWorld).Translation();
	// diff.Normalize();
	
	xAxis = Vector3::Transform(xAxis, *pWorld);
	*pOutput = xAxis.Cross(diff);
}

void Joint::JacobianY(Vector3* pOutput, Vector3& target)
{
	Vector3 yAxis(0.0f, 1.0f, 0.0f);
	Vector3 diff = target - (*pWorld).Translation();
	// diff.Normalize();

	yAxis = Vector3::Transform(yAxis, *pWorld);
	*pOutput = yAxis.Cross(diff);
}

void Joint::JacobianZ(Vector3* pOutput, Vector3& target)
{
	Vector3 zAxis(0.0f, 0.0f, 1.0f);
	Vector3 diff = target - (*pWorld).Translation();
	// diff.Normalize();

	zAxis = Vector3::Transform(zAxis, *pWorld);
	*pOutput = zAxis.Cross(diff);
}

void Chain::Update(Vector3 targetPos, int clipID, int frame)
{
	_ASSERT(BodyChain.size() > 0);

	UINT64 totalJoint = BodyChain.size() - 1; // end-effector 제외.
	Eigen::MatrixXf J(3, 3 * totalJoint); // 3차원, 관여 joint 3개임.
	Eigen::Vector3f target(targetPos.x, targetPos.y, targetPos.z);
	Eigen::MatrixXf deltaTheta;

	// 자코비안 행렬 구성.
	UINT column = 0;
	J.setZero();
	for (UINT64 i = 0; i < totalJoint; ++i)
	{
		Joint& joint = BodyChain[i];
		Vector3 partialToX(0.0f);
		Vector3 partialToY(0.0f);
		Vector3 partialToZ(0.0f);

		joint.JacobianX(&partialToX, targetPos);
		joint.JacobianY(&partialToY, targetPos);
		joint.JacobianZ(&partialToZ, targetPos);

		J(0, column) = partialToX.x;
		J(1, column) = partialToX.y;
		J(2, column) = partialToX.z;
		J(0, column + 1) = partialToY.x;
		J(1, column + 1) = partialToY.y;
		J(2, column + 1) = partialToY.z;
		J(0, column + 2) = partialToZ.x;
		J(1, column + 2) = partialToZ.y;
		J(2, column + 2) = partialToZ.z;

		column += 3;
	}

	// 자코비안 역행렬 계산.
	Eigen::JacobiSVD<Eigen::MatrixXf> svd(J, Eigen::ComputeThinU | Eigen::ComputeThinV);
	// 반복..?
	// Eigen::MatrixXf inverseJ = svd.matrixV() * (svd.singularValues().array().inverse().matrix().asDiagonal()) * svd.matrixU().adjoint();
	// deltaTheta = inverseJ * target;
	deltaTheta = svd.solve(target); // 9 x 1 ? 
	{
		std::string debugString;
		debugString = std::string("Theta 1: ") + std::to_string(deltaTheta(0, 0)) + std::string(", ") + std::to_string(deltaTheta(1, 0)) + std::string(", ") + std::to_string(deltaTheta(2, 0)) + std::string("\n");
		OutputDebugStringA(debugString.c_str());

		debugString = std::string("Theta 2: ") + std::to_string(deltaTheta(3, 0)) + std::string(", ") + std::to_string(deltaTheta(4, 0)) + std::string(", ") + std::to_string(deltaTheta(5, 0)) + std::string("\n");
		OutputDebugStringA(debugString.c_str());

		debugString = std::string("Theta 3: ") + std::to_string(deltaTheta(6, 0)) + std::string(", ") + std::to_string(deltaTheta(7, 0)) + std::string(", ") + std::to_string(deltaTheta(8, 0)) + std::string("\n");
		OutputDebugStringA(debugString.c_str());
	}

	UINT row = 0;
	for (UINT64 i = 0; i < totalJoint; ++i)
	{
		Joint& joint = BodyChain[i];
		joint.Update(deltaTheta(row, 0), deltaTheta(row + 1, 0), deltaTheta(row + 2, 0), pAnimationClips, &DefaultTransform, &InverseDefaultTransform, clipID, frame);
		row += 3;
	}

	Joint& endEffector = BodyChain[BodyChain.size() - 1];
	endEffector.Update(0.0f, 0.0f, 0.0f, pAnimationClips, &DefaultTransform, &InverseDefaultTransform, clipID, frame);
}
