#include "../pch.h"
#include "Camera.h"

void Camera::UpdateViewDir()
{
	// �̵��� �� ������ �Ǵ� ����/������ ���� ���.
	m_ViewDirection = Vector3::Transform(Vector3(0.0f, 0.0f, 1.0f), Matrix::CreateRotationY(m_Yaw));
	m_RightDirection = m_UpDirection.Cross(m_ViewDirection);
}

void Camera::UpdateKeyboard(const float DELTA_TIME, Keyboard* pKeyboard)
{
	if (bUseFirstPersonView)
	{
		if (pKeyboard->bPressed['W'])
		{
			MoveForward(DELTA_TIME);
		}
		if (pKeyboard->bPressed['S'])
		{
			MoveForward(-DELTA_TIME);
		}
		if (pKeyboard->bPressed['D'])
		{
			MoveRight(DELTA_TIME);
		}
		if (pKeyboard->bPressed['A'])
		{
			MoveRight(-DELTA_TIME);
		}
		if (pKeyboard->bPressed['E'])
		{
			MoveUp(DELTA_TIME);
		}
		if (pKeyboard->bPressed['Q'])
		{
			MoveUp(-DELTA_TIME);
		}
	}
}

void Camera::UpdateMouse(float mouseNDCX, float mouseNDCY)
{
	if (bUseFirstPersonView)
	{
		// �󸶳� ȸ������ ���.
		m_Yaw = mouseNDCX * DirectX::XM_2PI;       // �¿� 360��.
		m_Pitch = -mouseNDCY * DirectX::XM_PIDIV2; // �� �Ʒ� 90��.
		UpdateViewDir();
	}
}

void Camera::MoveForward(float deltaTime)
{
	// �̵�����_��ġ = ����_��ġ + �̵����� * �ӵ� * �ð�����.
	m_Position += m_ViewDirection * m_Speed * deltaTime;
}

void Camera::MoveRight(float deltaTime)
{
	// �̵�����_��ġ = ����_��ġ + �̵����� * �ӵ� * �ð�����.
	m_Position += m_RightDirection * m_Speed * deltaTime;
}

void Camera::MoveUp(float deltaTime)
{
	// �̵�����_��ġ = ����_��ġ + �̵����� * �ӵ� * �ð�����.
	m_Position += m_UpDirection * m_Speed * deltaTime;
}
