#pragma once

#include "Disparity/Math/Transform.h"

#include <DirectXMath.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Disparity
{
    struct TransformKeyframe
    {
        float Time = 0.0f;
        Transform Value;
    };

    class TransformAnimation
    {
    public:
        void AddKeyframe(const TransformKeyframe& keyframe);
        [[nodiscard]] Transform Sample(float timeSeconds, bool loop) const;
        [[nodiscard]] bool IsEmpty() const;
        [[nodiscard]] float GetDuration() const;

    private:
        std::vector<TransformKeyframe> m_keyframes;
    };

    struct BobAnimation
    {
        float Time = 0.0f;
        float Amplitude = 0.04f;
        float Frequency = 5.0f;

        [[nodiscard]] float SampleOffset(float deltaSeconds);
    };

    [[nodiscard]] Transform BlendTransforms(const Transform& a, const Transform& b, float weight);

    struct SkinningPalette
    {
        std::vector<DirectX::XMFLOAT4X4> JointMatrices;
        uint32_t UploadGeneration = 0;

        void Resize(size_t jointCount);
        void MarkUploaded();
        [[nodiscard]] bool IsReady() const;
    };
}
