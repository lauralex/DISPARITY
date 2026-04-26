#pragma once

#include <DirectXMath.h>

namespace Disparity
{
    class Camera
    {
    public:
        void SetPerspective(float verticalFovRadians, float aspectRatio, float nearPlane, float farPlane);
        void LookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

        [[nodiscard]] DirectX::XMMATRIX GetViewMatrix() const;
        [[nodiscard]] DirectX::XMMATRIX GetProjectionMatrix() const;
        [[nodiscard]] DirectX::XMMATRIX GetViewProjectionMatrix() const;
        [[nodiscard]] DirectX::XMFLOAT3 GetPosition() const;

    private:
        DirectX::XMFLOAT3 m_position = { 0.0f, 2.0f, -6.0f };
        DirectX::XMFLOAT3 m_target = { 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT3 m_up = { 0.0f, 1.0f, 0.0f };
        float m_verticalFovRadians = DirectX::XMConvertToRadians(65.0f);
        float m_aspectRatio = 16.0f / 9.0f;
        float m_nearPlane = 0.1f;
        float m_farPlane = 500.0f;
    };
}
