#include "Disparity/Scene/Camera.h"

namespace Disparity
{
    void Camera::SetPerspective(float verticalFovRadians, float aspectRatio, float nearPlane, float farPlane)
    {
        m_verticalFovRadians = verticalFovRadians;
        m_aspectRatio = aspectRatio;
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;
    }

    void Camera::LookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up)
    {
        m_position = position;
        m_target = target;
        m_up = up;
    }

    DirectX::XMMATRIX Camera::GetViewMatrix() const
    {
        return DirectX::XMMatrixLookAtLH(
            DirectX::XMLoadFloat3(&m_position),
            DirectX::XMLoadFloat3(&m_target),
            DirectX::XMLoadFloat3(&m_up));
    }

    DirectX::XMMATRIX Camera::GetProjectionMatrix() const
    {
        return DirectX::XMMatrixPerspectiveFovLH(m_verticalFovRadians, m_aspectRatio, m_nearPlane, m_farPlane);
    }

    DirectX::XMMATRIX Camera::GetViewProjectionMatrix() const
    {
        return GetViewMatrix() * GetProjectionMatrix();
    }

    DirectX::XMFLOAT3 Camera::GetPosition() const
    {
        return m_position;
    }
}
