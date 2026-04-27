#include "Disparity/Scene/SceneQuery.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Disparity
{
    namespace
    {
        float Clamp(float value, float minValue, float maxValue)
        {
            return std::max(minValue, std::min(value, maxValue));
        }

        float LengthSquared(const DirectX::XMFLOAT3& value)
        {
            return value.x * value.x + value.y * value.y + value.z * value.z;
        }

        DirectX::XMFLOAT3 Subtract(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
        {
            return { a.x - b.x, a.y - b.y, a.z - b.z };
        }

        float Dot(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        DirectX::XMFLOAT3 NormalizeOrForward(const DirectX::XMFLOAT3& value)
        {
            const float lengthSquared = LengthSquared(value);
            if (lengthSquared <= 0.000001f)
            {
                return { 0.0f, 0.0f, 1.0f };
            }

            const float invLength = 1.0f / std::sqrt(lengthSquared);
            return { value.x * invLength, value.y * invLength, value.z * invLength };
        }

        bool AabbOverlap(const Bounds3& a, const Bounds3& b)
        {
            return std::fabs(a.Center.x - b.Center.x) <= (a.Extents.x + b.Extents.x) &&
                std::fabs(a.Center.y - b.Center.y) <= (a.Extents.y + b.Extents.y) &&
                std::fabs(a.Center.z - b.Center.z) <= (a.Extents.z + b.Extents.z);
        }

        float DistanceToAabbSquared(const DirectX::XMFLOAT3& point, const Bounds3& bounds)
        {
            const float minX = bounds.Center.x - bounds.Extents.x;
            const float maxX = bounds.Center.x + bounds.Extents.x;
            const float minY = bounds.Center.y - bounds.Extents.y;
            const float maxY = bounds.Center.y + bounds.Extents.y;
            const float minZ = bounds.Center.z - bounds.Extents.z;
            const float maxZ = bounds.Center.z + bounds.Extents.z;
            const DirectX::XMFLOAT3 closest = {
                Clamp(point.x, minX, maxX),
                Clamp(point.y, minY, maxY),
                Clamp(point.z, minZ, maxZ)
            };
            return LengthSquared(Subtract(point, closest));
        }

        bool RayAabb(
            const DirectX::XMFLOAT3& origin,
            const DirectX::XMFLOAT3& direction,
            const Bounds3& bounds,
            float maxDistance,
            float& outDistance)
        {
            float tMin = 0.0f;
            float tMax = maxDistance;
            const float minValues[3] = {
                bounds.Center.x - bounds.Extents.x,
                bounds.Center.y - bounds.Extents.y,
                bounds.Center.z - bounds.Extents.z
            };
            const float maxValues[3] = {
                bounds.Center.x + bounds.Extents.x,
                bounds.Center.y + bounds.Extents.y,
                bounds.Center.z + bounds.Extents.z
            };
            const float origins[3] = { origin.x, origin.y, origin.z };
            const float directions[3] = { direction.x, direction.y, direction.z };

            for (int axis = 0; axis < 3; ++axis)
            {
                if (std::fabs(directions[axis]) < 0.000001f)
                {
                    if (origins[axis] < minValues[axis] || origins[axis] > maxValues[axis])
                    {
                        return false;
                    }
                    continue;
                }

                const float invDirection = 1.0f / directions[axis];
                float t1 = (minValues[axis] - origins[axis]) * invDirection;
                float t2 = (maxValues[axis] - origins[axis]) * invDirection;
                if (t1 > t2)
                {
                    std::swap(t1, t2);
                }

                tMin = std::max(tMin, t1);
                tMax = std::min(tMax, t2);
                if (tMin > tMax)
                {
                    return false;
                }
            }

            outDistance = tMin;
            return true;
        }
    }

    void SceneQueryWorld::Clear()
    {
        m_entries.clear();
        m_diagnostics = {};
    }

    void SceneQueryWorld::AddOrUpdate(SceneQueryEntry entry)
    {
        const auto found = std::find_if(m_entries.begin(), m_entries.end(), [&entry](const SceneQueryEntry& existing) {
            return existing.StableId == entry.StableId;
        });

        if (found != m_entries.end())
        {
            *found = std::move(entry);
        }
        else
        {
            m_entries.push_back(std::move(entry));
        }
        m_diagnostics.Entries = static_cast<uint32_t>(m_entries.size());
    }

    void SceneQueryWorld::Remove(uint64_t stableId)
    {
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(), [stableId](const SceneQueryEntry& entry) {
                return entry.StableId == stableId;
            }),
            m_entries.end());
        m_diagnostics.Entries = static_cast<uint32_t>(m_entries.size());
    }

    std::vector<SceneQueryHit> SceneQueryWorld::QuerySphere(
        const DirectX::XMFLOAT3& center,
        float radius,
        uint32_t layerMask)
    {
        ++m_diagnostics.SphereQueries;
        std::vector<SceneQueryHit> hits;
        const float radiusSquared = radius * radius;
        for (const SceneQueryEntry& entry : m_entries)
        {
            if (!AcceptLayer(entry, layerMask))
            {
                continue;
            }

            const float distanceSquared = DistanceToAabbSquared(center, entry.Bounds);
            if (distanceSquared <= radiusSquared)
            {
                const float distance = std::sqrt(distanceSquared);
                hits.push_back({ entry.StableId, entry.Name, distance, entry.Bounds.Center });
            }
        }

        m_diagnostics.Hits += static_cast<uint32_t>(hits.size());
        return hits;
    }

    std::vector<SceneQueryHit> SceneQueryWorld::QueryAabb(const Bounds3& bounds, uint32_t layerMask)
    {
        ++m_diagnostics.AabbQueries;
        std::vector<SceneQueryHit> hits;
        for (const SceneQueryEntry& entry : m_entries)
        {
            if (!AcceptLayer(entry, layerMask))
            {
                continue;
            }

            if (AabbOverlap(bounds, entry.Bounds))
            {
                hits.push_back({ entry.StableId, entry.Name, std::sqrt(LengthSquared(Subtract(bounds.Center, entry.Bounds.Center))), entry.Bounds.Center });
            }
        }

        m_diagnostics.Hits += static_cast<uint32_t>(hits.size());
        return hits;
    }

    bool SceneQueryWorld::Raycast(
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& direction,
        float maxDistance,
        SceneQueryHit& outHit,
        uint32_t layerMask)
    {
        ++m_diagnostics.Raycasts;
        const DirectX::XMFLOAT3 normalizedDirection = NormalizeOrForward(direction);
        bool hitAny = false;
        float bestDistance = std::numeric_limits<float>::max();

        for (const SceneQueryEntry& entry : m_entries)
        {
            if (!AcceptLayer(entry, layerMask))
            {
                continue;
            }

            float distance = 0.0f;
            if (RayAabb(origin, normalizedDirection, entry.Bounds, maxDistance, distance) && distance < bestDistance)
            {
                bestDistance = distance;
                outHit = {
                    entry.StableId,
                    entry.Name,
                    distance,
                    {
                        origin.x + normalizedDirection.x * distance,
                        origin.y + normalizedDirection.y * distance,
                        origin.z + normalizedDirection.z * distance
                    }
                };
                hitAny = true;
            }
        }

        if (hitAny)
        {
            ++m_diagnostics.Hits;
        }
        return hitAny;
    }

    SceneQueryDiagnostics SceneQueryWorld::GetDiagnostics() const
    {
        SceneQueryDiagnostics diagnostics = m_diagnostics;
        diagnostics.Entries = static_cast<uint32_t>(m_entries.size());
        return diagnostics;
    }

    size_t SceneQueryWorld::Count() const
    {
        return m_entries.size();
    }

    bool SceneQueryWorld::AcceptLayer(const SceneQueryEntry& entry, uint32_t layerMask)
    {
        if ((entry.LayerMask & layerMask) != 0)
        {
            return true;
        }

        ++m_diagnostics.LayerRejects;
        return false;
    }
}
