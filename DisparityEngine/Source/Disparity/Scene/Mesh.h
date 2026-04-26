#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace Disparity
{
    struct Vertex
    {
        DirectX::XMFLOAT3 Position = {};
        DirectX::XMFLOAT3 Normal = {};
        DirectX::XMFLOAT2 TexCoord = {};
        DirectX::XMUINT4 Joints = {};
        DirectX::XMFLOAT4 Weights = {};
    };

    struct MeshData
    {
        std::vector<Vertex> Vertices;
        std::vector<uint32_t> Indices;
    };

    using MeshHandle = uint32_t;
}
