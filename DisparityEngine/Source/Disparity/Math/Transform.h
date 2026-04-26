#pragma once

#include <DirectXMath.h>

namespace Disparity
{
    struct Transform
    {
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Rotation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };

        [[nodiscard]] DirectX::XMMATRIX ToMatrix() const
        {
            const DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z);
            const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(Rotation.x, Rotation.y, Rotation.z);
            const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(Position.x, Position.y, Position.z);
            return scale * rotation * translation;
        }
    };
}
