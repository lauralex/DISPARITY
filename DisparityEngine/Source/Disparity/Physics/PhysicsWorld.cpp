#include "Disparity/Physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Disparity
{
    namespace
    {
        [[nodiscard]] DirectX::XMFLOAT3 Add(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
        {
            return { left.x + right.x, left.y + right.y, left.z + right.z };
        }

        [[nodiscard]] DirectX::XMFLOAT3 Subtract(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
        {
            return { left.x - right.x, left.y - right.y, left.z - right.z };
        }

        [[nodiscard]] DirectX::XMFLOAT3 Scale(const DirectX::XMFLOAT3& value, float scalar)
        {
            return { value.x * scalar, value.y * scalar, value.z * scalar };
        }

        [[nodiscard]] float Dot(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
        {
            return left.x * right.x + left.y * right.y + left.z * right.z;
        }

        [[nodiscard]] float LengthSquared(const DirectX::XMFLOAT3& value)
        {
            return Dot(value, value);
        }

        [[nodiscard]] float Length(const DirectX::XMFLOAT3& value)
        {
            return std::sqrt(LengthSquared(value));
        }

        [[nodiscard]] DirectX::XMFLOAT3 NormalizeOrForward(const DirectX::XMFLOAT3& value)
        {
            const float length = Length(value);
            if (length <= 0.000001f)
            {
                return { 0.0f, 0.0f, 1.0f };
            }

            return Scale(value, 1.0f / length);
        }

        [[nodiscard]] bool AabbOverlap(const PhysicsAabb& left, const PhysicsAabb& right)
        {
            return std::fabs(left.Center.x - right.Center.x) <= (left.Extents.x + right.Extents.x) &&
                std::fabs(left.Center.y - right.Center.y) <= (left.Extents.y + right.Extents.y) &&
                std::fabs(left.Center.z - right.Center.z) <= (left.Extents.z + right.Extents.z);
        }

        [[nodiscard]] PhysicsAabb ExpandAabb(const PhysicsAabb& bounds, float amount)
        {
            return {
                bounds.Center,
                {
                    bounds.Extents.x + amount,
                    bounds.Extents.y + amount,
                    bounds.Extents.z + amount
                }
            };
        }

        [[nodiscard]] float DistanceToAabbSquared(const DirectX::XMFLOAT3& point, const PhysicsAabb& bounds)
        {
            const DirectX::XMFLOAT3 minPoint = {
                bounds.Center.x - bounds.Extents.x,
                bounds.Center.y - bounds.Extents.y,
                bounds.Center.z - bounds.Extents.z
            };
            const DirectX::XMFLOAT3 maxPoint = {
                bounds.Center.x + bounds.Extents.x,
                bounds.Center.y + bounds.Extents.y,
                bounds.Center.z + bounds.Extents.z
            };
            const DirectX::XMFLOAT3 closest = {
                std::clamp(point.x, minPoint.x, maxPoint.x),
                std::clamp(point.y, minPoint.y, maxPoint.y),
                std::clamp(point.z, minPoint.z, maxPoint.z)
            };
            return LengthSquared(Subtract(point, closest));
        }

        [[nodiscard]] bool RayAabb(
            const DirectX::XMFLOAT3& origin,
            const DirectX::XMFLOAT3& direction,
            const PhysicsAabb& bounds,
            float maxDistance,
            float& outDistance,
            DirectX::XMFLOAT3& outNormal)
        {
            float tMin = 0.0f;
            float tMax = maxDistance;
            int hitAxis = 1;
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

                if (t1 > tMin)
                {
                    tMin = t1;
                    hitAxis = axis;
                }
                tMax = std::min(tMax, t2);
                if (tMin > tMax)
                {
                    return false;
                }
            }

            outDistance = tMin;
            outNormal = { 0.0f, 0.0f, 0.0f };
            if (hitAxis == 0)
            {
                outNormal.x = direction.x > 0.0f ? -1.0f : 1.0f;
            }
            else if (hitAxis == 1)
            {
                outNormal.y = direction.y > 0.0f ? -1.0f : 1.0f;
            }
            else
            {
                outNormal.z = direction.z > 0.0f ? -1.0f : 1.0f;
            }
            return true;
        }

        [[nodiscard]] DirectX::XMFLOAT3 MinimumPushOut(const PhysicsAabb& moving, const PhysicsAabb& obstacle)
        {
            const float pushX = (moving.Extents.x + obstacle.Extents.x) - std::fabs(moving.Center.x - obstacle.Center.x);
            const float pushY = (moving.Extents.y + obstacle.Extents.y) - std::fabs(moving.Center.y - obstacle.Center.y);
            const float pushZ = (moving.Extents.z + obstacle.Extents.z) - std::fabs(moving.Center.z - obstacle.Center.z);
            if (pushX <= pushY && pushX <= pushZ)
            {
                return { moving.Center.x < obstacle.Center.x ? -pushX : pushX, 0.0f, 0.0f };
            }
            if (pushY <= pushZ)
            {
                return { 0.0f, moving.Center.y < obstacle.Center.y ? -pushY : pushY, 0.0f };
            }
            return { 0.0f, 0.0f, moving.Center.z < obstacle.Center.z ? -pushZ : pushZ };
        }
    }

    void PhysicsWorld::Configure(const PhysicsWorldSettings& settings)
    {
        m_settings = settings;
        if (m_settings.FixedTimeStep <= 0.0f)
        {
            m_settings.FixedTimeStep = 1.0f / 60.0f;
        }
        m_settings.MaxSubSteps = std::max(1u, m_settings.MaxSubSteps);
    }

    void PhysicsWorld::Clear()
    {
        m_bodies.clear();
        m_debugLines.clear();
        m_diagnostics = {};
        m_accumulator = 0.0f;
        m_nextBodyId = 1u;
    }

    PhysicsBodyId PhysicsWorld::AddBody(const PhysicsBodyDesc& desc)
    {
        PhysicsBodyState body;
        body.Id = m_nextBodyId++;
        body.StableId = desc.StableId;
        body.Name = desc.Name;
        body.Motion = desc.Motion;
        body.Shape = desc.Shape;
        body.Material = desc.Material;
        body.Position = desc.Position;
        body.Velocity = desc.Velocity;
        body.Mass = std::max(0.001f, desc.Mass);
        body.InverseMass = body.Motion == PhysicsMotionType::Dynamic ? 1.0f / body.Mass : 0.0f;
        body.GravityScale = desc.GravityScale;
        body.LayerMask = desc.LayerMask;
        body.CollidesWith = desc.CollidesWith;
        body.Trigger = desc.Trigger;
        body.Sleeping = desc.Sleeping;
        m_bodies.push_back(body);
        RebuildDiagnostics();
        return body.Id;
    }

    bool PhysicsWorld::RemoveBody(PhysicsBodyId id)
    {
        const auto oldSize = m_bodies.size();
        m_bodies.erase(std::remove_if(m_bodies.begin(), m_bodies.end(), [id](const PhysicsBodyState& body) {
            return body.Id == id;
        }), m_bodies.end());
        RebuildDiagnostics();
        return m_bodies.size() != oldSize;
    }

    PhysicsBodyState* PhysicsWorld::FindBody(PhysicsBodyId id)
    {
        const auto found = std::find_if(m_bodies.begin(), m_bodies.end(), [id](const PhysicsBodyState& body) {
            return body.Id == id;
        });
        return found != m_bodies.end() ? &(*found) : nullptr;
    }

    const PhysicsBodyState* PhysicsWorld::FindBody(PhysicsBodyId id) const
    {
        const auto found = std::find_if(m_bodies.begin(), m_bodies.end(), [id](const PhysicsBodyState& body) {
            return body.Id == id;
        });
        return found != m_bodies.end() ? &(*found) : nullptr;
    }

    void PhysicsWorld::SetBodyPosition(PhysicsBodyId id, const DirectX::XMFLOAT3& position)
    {
        if (PhysicsBodyState* body = FindBody(id))
        {
            body->Position = position;
            body->Sleeping = false;
        }
    }

    void PhysicsWorld::SetBodyVelocity(PhysicsBodyId id, const DirectX::XMFLOAT3& velocity)
    {
        if (PhysicsBodyState* body = FindBody(id))
        {
            body->Velocity = velocity;
            body->Sleeping = false;
        }
    }

    void PhysicsWorld::AddImpulse(PhysicsBodyId id, const DirectX::XMFLOAT3& impulse)
    {
        if (PhysicsBodyState* body = FindBody(id);
            body && body->Motion == PhysicsMotionType::Dynamic && body->InverseMass > 0.0f)
        {
            body->Velocity = Add(body->Velocity, Scale(impulse, body->InverseMass));
            body->Sleeping = false;
            ++m_diagnostics.Impulses;
        }
    }

    void PhysicsWorld::Step(float deltaSeconds)
    {
        if (deltaSeconds <= 0.0f)
        {
            return;
        }

        m_accumulator += std::min(deltaSeconds, m_settings.FixedTimeStep * static_cast<float>(m_settings.MaxSubSteps));
        uint32_t steps = 0;
        while (m_accumulator >= m_settings.FixedTimeStep && steps < m_settings.MaxSubSteps)
        {
            ++steps;
            ++m_diagnostics.SubStepCount;
            m_diagnostics.SimulatedSeconds += m_settings.FixedTimeStep;

            for (PhysicsBodyState& body : m_bodies)
            {
                if (body.Motion != PhysicsMotionType::Dynamic || body.Sleeping || body.Trigger)
                {
                    continue;
                }

                body.Velocity = Add(body.Velocity, Scale(m_settings.Gravity, body.GravityScale * m_settings.FixedTimeStep));
                body.Position = Add(body.Position, Scale(body.Velocity, m_settings.FixedTimeStep));
                const PhysicsAabb bounds = ComputeAabb(body);
                if (m_settings.EnableGroundPlane)
                {
                    const float minY = bounds.Center.y - bounds.Extents.y;
                    if (minY < m_settings.GroundPlaneY)
                    {
                        body.Position.y += m_settings.GroundPlaneY - minY;
                        if (body.Velocity.y < 0.0f)
                        {
                            body.Velocity.y = -body.Velocity.y * body.Material.Restitution;
                            body.Velocity.x *= 1.0f - body.Material.DynamicFriction * 0.18f;
                            body.Velocity.z *= 1.0f - body.Material.DynamicFriction * 0.18f;
                        }
                        ++m_diagnostics.GroundContacts;
                    }
                }
            }

            for (PhysicsBodyState& moving : m_bodies)
            {
                if (moving.Motion != PhysicsMotionType::Dynamic || moving.Sleeping)
                {
                    continue;
                }

                for (const PhysicsBodyState& obstacle : m_bodies)
                {
                    if (moving.Id == obstacle.Id || obstacle.Motion == PhysicsMotionType::Dynamic)
                    {
                        continue;
                    }
                    if (!AcceptLayer(moving, obstacle.LayerMask) || !AcceptLayer(obstacle, moving.LayerMask))
                    {
                        continue;
                    }

                    ++m_diagnostics.BroadphasePairs;
                    PhysicsAabb movingBounds = ComputeAabb(moving);
                    const PhysicsAabb obstacleBounds = ComputeAabb(obstacle);
                    if (!AabbOverlap(movingBounds, obstacleBounds))
                    {
                        continue;
                    }

                    if (moving.Trigger || obstacle.Trigger)
                    {
                        ++m_diagnostics.TriggerOverlaps;
                        continue;
                    }

                    const DirectX::XMFLOAT3 push = MinimumPushOut(movingBounds, obstacleBounds);
                    moving.Position = Add(moving.Position, push);
                    movingBounds.Center = Add(movingBounds.Center, push);
                    if (std::fabs(push.x) > 0.0f)
                    {
                        moving.Velocity.x = -moving.Velocity.x * moving.Material.Restitution;
                    }
                    if (std::fabs(push.y) > 0.0f)
                    {
                        moving.Velocity.y = -moving.Velocity.y * moving.Material.Restitution;
                    }
                    if (std::fabs(push.z) > 0.0f)
                    {
                        moving.Velocity.z = -moving.Velocity.z * moving.Material.Restitution;
                    }
                    ++m_diagnostics.ContactPairs;
                }

                if (LengthSquared(moving.Velocity) < m_settings.SleepVelocityThreshold * m_settings.SleepVelocityThreshold)
                {
                    moving.Sleeping = true;
                    moving.Velocity = { 0.0f, 0.0f, 0.0f };
                }
            }

            m_accumulator -= m_settings.FixedTimeStep;
        }

        if (steps > 0)
        {
            ++m_diagnostics.StepCount;
            RebuildDiagnostics();
            RebuildDebugLines();
        }
    }

    bool PhysicsWorld::Raycast(
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& direction,
        float maxDistance,
        PhysicsRaycastHit& outHit,
        uint32_t layerMask)
    {
        ++m_diagnostics.Raycasts;
        const DirectX::XMFLOAT3 normalizedDirection = NormalizeOrForward(direction);
        bool hitAny = false;
        float bestDistance = std::numeric_limits<float>::max();
        for (const PhysicsBodyState& body : m_bodies)
        {
            if (!AcceptLayer(body, layerMask))
            {
                continue;
            }

            float distance = 0.0f;
            DirectX::XMFLOAT3 normal = {};
            if (RayAabb(origin, normalizedDirection, ComputeAabb(body), maxDistance, distance, normal) && distance < bestDistance)
            {
                bestDistance = distance;
                outHit = {
                    body.Id,
                    body.StableId,
                    body.Name,
                    distance,
                    Add(origin, Scale(normalizedDirection, distance)),
                    normal
                };
                hitAny = true;
            }
        }
        return hitAny;
    }

    std::vector<PhysicsBodyId> PhysicsWorld::OverlapSphere(
        const DirectX::XMFLOAT3& center,
        float radius,
        uint32_t layerMask)
    {
        ++m_diagnostics.OverlapTests;
        std::vector<PhysicsBodyId> hits;
        const float radiusSquared = radius * radius;
        for (const PhysicsBodyState& body : m_bodies)
        {
            if (AcceptLayer(body, layerMask) && DistanceToAabbSquared(center, ComputeAabb(body)) <= radiusSquared)
            {
                hits.push_back(body.Id);
            }
        }
        return hits;
    }

    std::vector<PhysicsBodyId> PhysicsWorld::OverlapAabb(const PhysicsAabb& bounds, uint32_t layerMask)
    {
        ++m_diagnostics.OverlapTests;
        std::vector<PhysicsBodyId> hits;
        for (const PhysicsBodyState& body : m_bodies)
        {
            if (AcceptLayer(body, layerMask) && AabbOverlap(bounds, ComputeAabb(body)))
            {
                hits.push_back(body.Id);
            }
        }
        return hits;
    }

    bool PhysicsWorld::SweepSphere(
        const DirectX::XMFLOAT3& origin,
        float radius,
        const DirectX::XMFLOAT3& direction,
        float maxDistance,
        PhysicsRaycastHit& outHit,
        uint32_t layerMask)
    {
        ++m_diagnostics.SweepTests;
        const DirectX::XMFLOAT3 normalizedDirection = NormalizeOrForward(direction);
        bool hitAny = false;
        float bestDistance = std::numeric_limits<float>::max();
        for (const PhysicsBodyState& body : m_bodies)
        {
            if (!AcceptLayer(body, layerMask))
            {
                continue;
            }

            float distance = 0.0f;
            DirectX::XMFLOAT3 normal = {};
            const PhysicsAabb expanded = ExpandAabb(ComputeAabb(body), radius);
            if (RayAabb(origin, normalizedDirection, expanded, maxDistance, distance, normal) && distance < bestDistance)
            {
                bestDistance = distance;
                outHit = {
                    body.Id,
                    body.StableId,
                    body.Name,
                    distance,
                    Add(origin, Scale(normalizedDirection, distance)),
                    normal
                };
                hitAny = true;
            }
        }
        return hitAny;
    }

    void PhysicsWorld::MoveCharacter(
        const PhysicsCharacterControllerConfig& config,
        PhysicsCharacterControllerState& state,
        const DirectX::XMFLOAT3& desiredDisplacement,
        float deltaSeconds)
    {
        ++m_diagnostics.CharacterMoves;
        ++m_diagnostics.CharacterSlopeSamples;
        state.HitWall = false;
        state.Grounded = false;
        state.GroundBody = InvalidPhysicsBodyId;
        state.Velocity = Add(state.Velocity, Scale(m_settings.Gravity, deltaSeconds));
        state.Position = Add(state.Position, Add(desiredDisplacement, Scale(state.Velocity, deltaSeconds)));

        PhysicsAabb characterBounds = {
            state.Position,
            { config.Radius, config.HalfHeight + config.Radius, config.Radius }
        };
        if (m_settings.EnableGroundPlane)
        {
            const float bottom = characterBounds.Center.y - characterBounds.Extents.y;
            if (bottom <= m_settings.GroundPlaneY + config.StepHeight)
            {
                state.Position.y += (m_settings.GroundPlaneY + characterBounds.Extents.y) - bottom;
                state.Velocity.y = std::max(0.0f, state.Velocity.y);
                state.Grounded = true;
                state.GroundNormal = { 0.0f, 1.0f, 0.0f };
                ++m_diagnostics.CharacterGroundedMoves;
                ++m_diagnostics.GroundContacts;
            }
        }

        characterBounds.Center = state.Position;
        for (const PhysicsBodyState& body : m_bodies)
        {
            if (body.Motion == PhysicsMotionType::Dynamic || body.Trigger || !AcceptLayer(body, config.CollidesWith))
            {
                continue;
            }

            const PhysicsAabb obstacleBounds = ComputeAabb(body);
            if (!AabbOverlap(characterBounds, obstacleBounds))
            {
                continue;
            }

            const DirectX::XMFLOAT3 push = MinimumPushOut(characterBounds, obstacleBounds);
            if (std::fabs(push.y) > std::fabs(push.x) && std::fabs(push.y) > std::fabs(push.z) && push.y > 0.0f && push.y <= config.StepHeight)
            {
                ++m_diagnostics.CharacterStepSolves;
                state.Grounded = true;
                state.GroundBody = body.Id;
            }
            else if (std::fabs(push.x) > 0.0f || std::fabs(push.z) > 0.0f)
            {
                state.HitWall = true;
                state.Velocity.x = 0.0f;
                state.Velocity.z = 0.0f;
            }
            state.Position = Add(state.Position, push);
            characterBounds.Center = state.Position;
        }
    }

    PhysicsWorldDiagnostics PhysicsWorld::GetDiagnostics() const
    {
        PhysicsWorldDiagnostics diagnostics = m_diagnostics;
        diagnostics.BodyCount = static_cast<uint32_t>(m_bodies.size());
        diagnostics.DebugLines = static_cast<uint32_t>(m_debugLines.size());
        return diagnostics;
    }

    const std::vector<PhysicsBodyState>& PhysicsWorld::GetBodies() const
    {
        return m_bodies;
    }

    const std::vector<PhysicsDebugLine>& PhysicsWorld::GetDebugLines() const
    {
        return m_debugLines;
    }

    PhysicsAabb PhysicsWorld::ComputeAabb(const PhysicsBodyState& body) const
    {
        DirectX::XMFLOAT3 extents = body.Shape.HalfExtents;
        if (body.Shape.Type == PhysicsShapeType::Sphere)
        {
            extents = { body.Shape.Radius, body.Shape.Radius, body.Shape.Radius };
        }
        else if (body.Shape.Type == PhysicsShapeType::Capsule)
        {
            extents = { body.Shape.Radius, body.Shape.HalfHeight + body.Shape.Radius, body.Shape.Radius };
        }
        return { body.Position, extents };
    }

    bool PhysicsWorld::AcceptLayer(const PhysicsBodyState& body, uint32_t layerMask)
    {
        if ((body.LayerMask & layerMask) != 0)
        {
            return true;
        }

        ++m_diagnostics.LayerRejects;
        return false;
    }

    void PhysicsWorld::RebuildDiagnostics()
    {
        uint32_t staticBodies = 0;
        uint32_t dynamicBodies = 0;
        uint32_t kinematicBodies = 0;
        uint32_t triggerBodies = 0;
        uint32_t sleepingBodies = 0;
        uint32_t activeBodies = 0;
        for (const PhysicsBodyState& body : m_bodies)
        {
            staticBodies += body.Motion == PhysicsMotionType::Static ? 1u : 0u;
            dynamicBodies += body.Motion == PhysicsMotionType::Dynamic ? 1u : 0u;
            kinematicBodies += body.Motion == PhysicsMotionType::Kinematic ? 1u : 0u;
            triggerBodies += body.Trigger ? 1u : 0u;
            sleepingBodies += body.Sleeping ? 1u : 0u;
            activeBodies += !body.Sleeping ? 1u : 0u;
        }

        m_diagnostics.BodyCount = static_cast<uint32_t>(m_bodies.size());
        m_diagnostics.StaticBodies = staticBodies;
        m_diagnostics.DynamicBodies = dynamicBodies;
        m_diagnostics.KinematicBodies = kinematicBodies;
        m_diagnostics.TriggerBodies = triggerBodies;
        m_diagnostics.SleepingBodies = sleepingBodies;
        m_diagnostics.ActiveBodies = activeBodies;
        m_diagnostics.DebugLines = static_cast<uint32_t>(m_debugLines.size());
    }

    void PhysicsWorld::RebuildDebugLines()
    {
        m_debugLines.clear();
        for (const PhysicsBodyState& body : m_bodies)
        {
            const PhysicsAabb bounds = ComputeAabb(body);
            const DirectX::XMFLOAT3 color = body.Trigger
                ? DirectX::XMFLOAT3{ 1.0f, 0.28f, 0.88f }
                : (body.Motion == PhysicsMotionType::Dynamic
                    ? DirectX::XMFLOAT3{ 0.22f, 0.86f, 1.0f }
                    : DirectX::XMFLOAT3{ 1.0f, 0.72f, 0.20f });
            const DirectX::XMFLOAT3 minPoint = Subtract(bounds.Center, bounds.Extents);
            const DirectX::XMFLOAT3 maxPoint = Add(bounds.Center, bounds.Extents);
            m_debugLines.push_back({ { minPoint.x, minPoint.y, minPoint.z }, { maxPoint.x, minPoint.y, minPoint.z }, color });
            m_debugLines.push_back({ { minPoint.x, minPoint.y, minPoint.z }, { minPoint.x, maxPoint.y, minPoint.z }, color });
            m_debugLines.push_back({ { minPoint.x, minPoint.y, minPoint.z }, { minPoint.x, minPoint.y, maxPoint.z }, color });
        }
        m_diagnostics.DebugLines = static_cast<uint32_t>(m_debugLines.size());
    }
}
