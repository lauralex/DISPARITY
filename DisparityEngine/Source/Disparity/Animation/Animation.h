#pragma once

#include "Disparity/Math/Transform.h"

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
}
