#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Disparity
{
    struct Bounds3
    {
        DirectX::XMFLOAT3 Center = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Extents = { 0.5f, 0.5f, 0.5f };
    };

    struct SceneQueryEntry
    {
        uint64_t StableId = 0;
        std::string Name;
        Bounds3 Bounds;
        uint32_t LayerMask = 1u;
    };

    struct SceneQueryHit
    {
        uint64_t StableId = 0;
        std::string Name;
        float Distance = 0.0f;
        DirectX::XMFLOAT3 Point = { 0.0f, 0.0f, 0.0f };
    };

    struct SceneQueryDiagnostics
    {
        uint32_t Entries = 0;
        uint32_t SphereQueries = 0;
        uint32_t AabbQueries = 0;
        uint32_t Raycasts = 0;
        uint32_t Hits = 0;
        uint32_t LayerRejects = 0;
    };

    class SceneQueryWorld
    {
    public:
        void Clear();
        void AddOrUpdate(SceneQueryEntry entry);
        void Remove(uint64_t stableId);

        [[nodiscard]] std::vector<SceneQueryHit> QuerySphere(
            const DirectX::XMFLOAT3& center,
            float radius,
            uint32_t layerMask = UINT32_MAX);
        [[nodiscard]] std::vector<SceneQueryHit> QueryAabb(const Bounds3& bounds, uint32_t layerMask = UINT32_MAX);
        [[nodiscard]] bool Raycast(
            const DirectX::XMFLOAT3& origin,
            const DirectX::XMFLOAT3& direction,
            float maxDistance,
            SceneQueryHit& outHit,
            uint32_t layerMask = UINT32_MAX);

        [[nodiscard]] SceneQueryDiagnostics GetDiagnostics() const;
        [[nodiscard]] size_t Count() const;

    private:
        [[nodiscard]] bool AcceptLayer(const SceneQueryEntry& entry, uint32_t layerMask);

        std::vector<SceneQueryEntry> m_entries;
        SceneQueryDiagnostics m_diagnostics;
    };
}
