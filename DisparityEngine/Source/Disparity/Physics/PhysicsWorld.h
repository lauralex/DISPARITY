#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Disparity
{
    using PhysicsBodyId = uint32_t;
    inline constexpr PhysicsBodyId InvalidPhysicsBodyId = 0;

    enum class PhysicsMotionType : uint8_t
    {
        Static,
        Dynamic,
        Kinematic
    };

    enum class PhysicsShapeType : uint8_t
    {
        Box,
        Sphere,
        Capsule
    };

    struct PhysicsMaterial
    {
        float StaticFriction = 0.72f;
        float DynamicFriction = 0.48f;
        float Restitution = 0.08f;
        float Density = 1.0f;
    };

    struct PhysicsShape
    {
        PhysicsShapeType Type = PhysicsShapeType::Box;
        DirectX::XMFLOAT3 HalfExtents = { 0.5f, 0.5f, 0.5f };
        float Radius = 0.5f;
        float HalfHeight = 0.5f;
    };

    struct PhysicsBodyDesc
    {
        uint64_t StableId = 0;
        std::string Name;
        PhysicsMotionType Motion = PhysicsMotionType::Static;
        PhysicsShape Shape;
        PhysicsMaterial Material;
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Velocity = { 0.0f, 0.0f, 0.0f };
        float Mass = 1.0f;
        float GravityScale = 1.0f;
        uint32_t LayerMask = 1u;
        uint32_t CollidesWith = 0xffffffffu;
        bool Trigger = false;
        bool Sleeping = false;
    };

    struct PhysicsBodyState
    {
        PhysicsBodyId Id = InvalidPhysicsBodyId;
        uint64_t StableId = 0;
        std::string Name;
        PhysicsMotionType Motion = PhysicsMotionType::Static;
        PhysicsShape Shape;
        PhysicsMaterial Material;
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Velocity = { 0.0f, 0.0f, 0.0f };
        float Mass = 1.0f;
        float InverseMass = 0.0f;
        float GravityScale = 1.0f;
        uint32_t LayerMask = 1u;
        uint32_t CollidesWith = 0xffffffffu;
        bool Trigger = false;
        bool Sleeping = false;
    };

    struct PhysicsAabb
    {
        DirectX::XMFLOAT3 Center = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Extents = { 0.5f, 0.5f, 0.5f };
    };

    struct PhysicsRaycastHit
    {
        PhysicsBodyId Body = InvalidPhysicsBodyId;
        uint64_t StableId = 0;
        std::string Name;
        float Distance = 0.0f;
        DirectX::XMFLOAT3 Point = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Normal = { 0.0f, 1.0f, 0.0f };
    };

    struct PhysicsCharacterControllerConfig
    {
        float Radius = 0.38f;
        float HalfHeight = 0.82f;
        float StepHeight = 0.34f;
        float SlopeLimitDegrees = 48.0f;
        uint32_t LayerMask = 1u;
        uint32_t CollidesWith = 0xffffffffu;
    };

    struct PhysicsCharacterControllerState
    {
        DirectX::XMFLOAT3 Position = { 0.0f, 1.2f, 0.0f };
        DirectX::XMFLOAT3 Velocity = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 GroundNormal = { 0.0f, 1.0f, 0.0f };
        bool Grounded = false;
        bool HitWall = false;
        uint32_t GroundBody = InvalidPhysicsBodyId;
    };

    struct PhysicsDebugLine
    {
        DirectX::XMFLOAT3 Start = {};
        DirectX::XMFLOAT3 End = {};
        DirectX::XMFLOAT3 Color = { 0.22f, 0.86f, 1.0f };
    };

    struct PhysicsWorldSettings
    {
        DirectX::XMFLOAT3 Gravity = { 0.0f, -9.81f, 0.0f };
        float FixedTimeStep = 1.0f / 60.0f;
        uint32_t MaxSubSteps = 4;
        float SleepVelocityThreshold = 0.035f;
        float GroundPlaneY = 0.0f;
        bool EnableGroundPlane = true;
    };

    struct PhysicsWorldDiagnostics
    {
        uint32_t BodyCount = 0;
        uint32_t StaticBodies = 0;
        uint32_t DynamicBodies = 0;
        uint32_t KinematicBodies = 0;
        uint32_t TriggerBodies = 0;
        uint32_t ActiveBodies = 0;
        uint32_t SleepingBodies = 0;
        uint32_t StepCount = 0;
        uint32_t SubStepCount = 0;
        uint32_t BroadphasePairs = 0;
        uint32_t ContactPairs = 0;
        uint32_t TriggerOverlaps = 0;
        uint32_t GroundContacts = 0;
        uint32_t Raycasts = 0;
        uint32_t SweepTests = 0;
        uint32_t OverlapTests = 0;
        uint32_t CharacterMoves = 0;
        uint32_t CharacterGroundedMoves = 0;
        uint32_t CharacterStepSolves = 0;
        uint32_t CharacterSlopeSamples = 0;
        uint32_t LayerRejects = 0;
        uint32_t Impulses = 0;
        uint32_t DebugLines = 0;
        float SimulatedSeconds = 0.0f;
    };

    class PhysicsWorld
    {
    public:
        void Configure(const PhysicsWorldSettings& settings);
        void Clear();

        [[nodiscard]] PhysicsBodyId AddBody(const PhysicsBodyDesc& desc);
        [[nodiscard]] bool RemoveBody(PhysicsBodyId id);
        [[nodiscard]] PhysicsBodyState* FindBody(PhysicsBodyId id);
        [[nodiscard]] const PhysicsBodyState* FindBody(PhysicsBodyId id) const;

        void SetBodyPosition(PhysicsBodyId id, const DirectX::XMFLOAT3& position);
        void SetBodyVelocity(PhysicsBodyId id, const DirectX::XMFLOAT3& velocity);
        void AddImpulse(PhysicsBodyId id, const DirectX::XMFLOAT3& impulse);

        void Step(float deltaSeconds);
        [[nodiscard]] bool Raycast(
            const DirectX::XMFLOAT3& origin,
            const DirectX::XMFLOAT3& direction,
            float maxDistance,
            PhysicsRaycastHit& outHit,
            uint32_t layerMask = 0xffffffffu);
        [[nodiscard]] std::vector<PhysicsBodyId> OverlapSphere(
            const DirectX::XMFLOAT3& center,
            float radius,
            uint32_t layerMask = 0xffffffffu);
        [[nodiscard]] std::vector<PhysicsBodyId> OverlapAabb(
            const PhysicsAabb& bounds,
            uint32_t layerMask = 0xffffffffu);
        [[nodiscard]] bool SweepSphere(
            const DirectX::XMFLOAT3& origin,
            float radius,
            const DirectX::XMFLOAT3& direction,
            float maxDistance,
            PhysicsRaycastHit& outHit,
            uint32_t layerMask = 0xffffffffu);

        void MoveCharacter(
            const PhysicsCharacterControllerConfig& config,
            PhysicsCharacterControllerState& state,
            const DirectX::XMFLOAT3& desiredDisplacement,
            float deltaSeconds);

        [[nodiscard]] PhysicsWorldDiagnostics GetDiagnostics() const;
        [[nodiscard]] const std::vector<PhysicsBodyState>& GetBodies() const;
        [[nodiscard]] const std::vector<PhysicsDebugLine>& GetDebugLines() const;

    private:
        [[nodiscard]] PhysicsAabb ComputeAabb(const PhysicsBodyState& body) const;
        [[nodiscard]] bool AcceptLayer(const PhysicsBodyState& body, uint32_t layerMask);
        void RebuildDiagnostics();
        void RebuildDebugLines();

        PhysicsWorldSettings m_settings;
        std::vector<PhysicsBodyState> m_bodies;
        std::vector<PhysicsDebugLine> m_debugLines;
        PhysicsWorldDiagnostics m_diagnostics;
        PhysicsBodyId m_nextBodyId = 1u;
        float m_accumulator = 0.0f;
    };
}
