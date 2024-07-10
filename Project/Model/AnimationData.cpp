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

		const int PARENT_IDX = BoneParents[boneID];
		const Matrix& PARENT_MATRIX = (PARENT_IDX >= 0 ? BoneTransforms[PARENT_IDX] : AccumulatedRootTransform);

		AnimationClip::Key key = (KEY_SIZE > 0 ? keys[frame % KEY_SIZE] : AnimationClip::Key());

		// Root일 경우.
		if (PARENT_IDX < 0)
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
	}
}

void Joint::Update(float deltaX, float deltaY, float deltaZ)
{
	Matrix delta = Matrix::CreateFromYawPitchRoll(deltaX, deltaY, deltaZ);
	Quaternion originQuat = Quaternion::CreateFromRotationMatrix(JointTransform);
	Quaternion deltaQuat = Quaternion::CreateFromRotationMatrix(delta);
	originQuat = Quaternion::Concatenate(originQuat, deltaQuat);

	JointTransform = Matrix::CreateFromQuaternion(originQuat);
	// World matrix update.
	World = Correction * Offset * JointTransform;
}

void Joint::JacobianX(Vector3* pOutput, Vector3& TARGET)
{
	Vector3 xAxis(1.0f, 0.0f, 0.0f);
	Vector3 diff = TARGET - Vector3::Transform(Vector3(0.0f), World);
	
	xAxis = Vector3::Transform(xAxis, World);
	*pOutput = xAxis.Cross(diff);
}

void Joint::JacobianY(Vector3* pOutput, Vector3& TARGET)
{
	Vector3 yAxis(0.0f, 1.0f, 0.0f);
	Vector3 diff = TARGET - Vector3::Transform(Vector3(0.0f), World);

	yAxis = Vector3::Transform(yAxis, World);
	*pOutput = yAxis.Cross(diff);
}

void Joint::JacobianZ(Vector3* pOutput, Vector3& TARGET)
{
	Vector3 zAxis(0.0f, 0.0f, 1.0f);
	Vector3 diff = TARGET - Vector3::Transform(Vector3(0.0f), World);

	zAxis = Vector3::Transform(zAxis, World);
	*pOutput = zAxis.Cross(diff);
}

void Chain::Update(Vector3 targetPos)
{
	_ASSERT(BodyChain.size() > 0);

	UINT64 totalJoint = BodyChain.size() - 1;
	Eigen::MatrixXf J(3, 3 * totalJoint); // 3차원, 관여 joint 3개임.
	Eigen::Vector3f target(targetPos.x, targetPos.y, targetPos.z);
	Eigen::MatrixXf deltaQ;

	UINT column = 0;
	for (UINT64 i = 0, size = BodyChain.size() - 1; i < size; ++i)
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

	Eigen::JacobiSVD<Eigen::MatrixXf> svdJ(J);
	// 반복..?
	deltaQ = svdJ.solve(target);

	
}
