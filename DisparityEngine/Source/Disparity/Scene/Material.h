#pragma once

#include <DirectXMath.h>

namespace Disparity
{
    using TextureHandle = unsigned int;

    struct Material
    {
        DirectX::XMFLOAT3 Albedo = { 1.0f, 1.0f, 1.0f };
        float Roughness = 0.65f;
        float Metallic = 0.0f;
        float Alpha = 1.0f;
        DirectX::XMFLOAT3 Emissive = { 0.0f, 0.0f, 0.0f };
        float EmissiveIntensity = 0.0f;
        TextureHandle BaseColorTexture = 0;
        bool DoubleSided = false;
    };
}
