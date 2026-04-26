#include "Disparity/Scene/PrimitiveFactory.h"

#include <algorithm>
#include <cmath>

namespace Disparity::PrimitiveFactory
{
    namespace
    {
        constexpr float Pi = 3.1415926535f;

        float HeightAt(float x, float z, float heightScale)
        {
            return (std::sin(x * 0.35f) * std::cos(z * 0.27f) + std::sin((x + z) * 0.18f) * 0.5f) * heightScale;
        }
    }

    MeshData CreateCube()
    {
        MeshData mesh;
        mesh.Vertices = {
            // Front
            { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },
            { { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
            { {  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
            { {  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },

            // Back
            { {  0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
            { {  0.5f,  0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
            { { -0.5f,  0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
            { { -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },

            // Left
            { { -0.5f, -0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
            { { -0.5f,  0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
            { { -0.5f,  0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
            { { -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },

            // Right
            { { 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
            { { 0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
            { { 0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
            { { 0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },

            // Top
            { { -0.5f, 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
            { { -0.5f, 0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
            { {  0.5f, 0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
            { {  0.5f, 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },

            // Bottom
            { { -0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
            { { -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
            { {  0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
            { {  0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
        };

        mesh.Indices = {
             0,  1,  2,  0,  2,  3,
             4,  5,  6,  4,  6,  7,
             8,  9, 10,  8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23,
        };

        return mesh;
    }

    MeshData CreatePlane(float size)
    {
        const float halfSize = size * 0.5f;
        MeshData mesh;

        mesh.Vertices = {
            { { -halfSize, 0.0f, -halfSize }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
            { { -halfSize, 0.0f,  halfSize }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
            { {  halfSize, 0.0f,  halfSize }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
            { {  halfSize, 0.0f, -halfSize }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
        };

        mesh.Indices = { 0, 1, 2, 0, 2, 3 };
        return mesh;
    }

    MeshData CreateTorus(float majorRadius, float minorRadius, unsigned int majorSegments, unsigned int minorSegments)
    {
        majorSegments = std::max(majorSegments, 8u);
        minorSegments = std::max(minorSegments, 4u);
        majorRadius = std::max(majorRadius, 0.01f);
        minorRadius = std::max(minorRadius, 0.002f);

        MeshData mesh;
        mesh.Vertices.reserve(static_cast<size_t>(majorSegments) * static_cast<size_t>(minorSegments));
        mesh.Indices.reserve(static_cast<size_t>(majorSegments) * static_cast<size_t>(minorSegments) * 6u);

        for (unsigned int major = 0; major < majorSegments; ++major)
        {
            const float u = static_cast<float>(major) / static_cast<float>(majorSegments);
            const float theta = u * Pi * 2.0f;
            const float cosTheta = std::cos(theta);
            const float sinTheta = std::sin(theta);

            for (unsigned int minor = 0; minor < minorSegments; ++minor)
            {
                const float v = static_cast<float>(minor) / static_cast<float>(minorSegments);
                const float phi = v * Pi * 2.0f;
                const float cosPhi = std::cos(phi);
                const float sinPhi = std::sin(phi);
                const float ringRadius = majorRadius + minorRadius * cosPhi;

                mesh.Vertices.push_back({
                    { ringRadius * cosTheta, ringRadius * sinTheta, minorRadius * sinPhi },
                    { cosTheta * cosPhi, sinTheta * cosPhi, sinPhi },
                    { u, v }
                });
            }
        }

        for (unsigned int major = 0; major < majorSegments; ++major)
        {
            const unsigned int nextMajor = (major + 1u) % majorSegments;
            for (unsigned int minor = 0; minor < minorSegments; ++minor)
            {
                const unsigned int nextMinor = (minor + 1u) % minorSegments;
                const uint32_t a = major * minorSegments + minor;
                const uint32_t b = nextMajor * minorSegments + minor;
                const uint32_t c = nextMajor * minorSegments + nextMinor;
                const uint32_t d = major * minorSegments + nextMinor;

                mesh.Indices.push_back(a);
                mesh.Indices.push_back(b);
                mesh.Indices.push_back(c);
                mesh.Indices.push_back(a);
                mesh.Indices.push_back(c);
                mesh.Indices.push_back(d);
            }
        }

        return mesh;
    }

    MeshData CreateTerrainGrid(unsigned int cellsPerSide, float size, float heightScale)
    {
        if (cellsPerSide < 2)
        {
            cellsPerSide = 2;
        }

        MeshData mesh;
        const unsigned int verticesPerSide = cellsPerSide + 1;
        const float halfSize = size * 0.5f;
        const float cellSize = size / static_cast<float>(cellsPerSide);

        mesh.Vertices.reserve(static_cast<size_t>(verticesPerSide) * static_cast<size_t>(verticesPerSide));
        mesh.Indices.reserve(static_cast<size_t>(cellsPerSide) * static_cast<size_t>(cellsPerSide) * 6);

        for (unsigned int z = 0; z < verticesPerSide; ++z)
        {
            for (unsigned int x = 0; x < verticesPerSide; ++x)
            {
                const float worldX = -halfSize + static_cast<float>(x) * cellSize;
                const float worldZ = -halfSize + static_cast<float>(z) * cellSize;
                const float height = HeightAt(worldX, worldZ, heightScale);

                const float leftHeight = HeightAt(worldX - cellSize, worldZ, heightScale);
                const float rightHeight = HeightAt(worldX + cellSize, worldZ, heightScale);
                const float backHeight = HeightAt(worldX, worldZ - cellSize, heightScale);
                const float forwardHeight = HeightAt(worldX, worldZ + cellSize, heightScale);
                DirectX::XMFLOAT3 normal = {
                    leftHeight - rightHeight,
                    2.0f * cellSize,
                    backHeight - forwardHeight
                };

                const DirectX::XMVECTOR normalVector = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&normal));
                DirectX::XMStoreFloat3(&normal, normalVector);

                mesh.Vertices.push_back({
                    { worldX, height, worldZ },
                    normal,
                    { static_cast<float>(x) / static_cast<float>(cellsPerSide), static_cast<float>(z) / static_cast<float>(cellsPerSide) }
                });
            }
        }

        for (unsigned int z = 0; z < cellsPerSide; ++z)
        {
            for (unsigned int x = 0; x < cellsPerSide; ++x)
            {
                const uint32_t topLeft = z * verticesPerSide + x;
                const uint32_t topRight = topLeft + 1;
                const uint32_t bottomLeft = (z + 1) * verticesPerSide + x;
                const uint32_t bottomRight = bottomLeft + 1;

                mesh.Indices.push_back(topLeft);
                mesh.Indices.push_back(bottomLeft);
                mesh.Indices.push_back(bottomRight);
                mesh.Indices.push_back(topLeft);
                mesh.Indices.push_back(bottomRight);
                mesh.Indices.push_back(topRight);
            }
        }

        return mesh;
    }
}
