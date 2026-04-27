#include "Disparity/Animation/Animation.h"

#include <algorithm>
#include <cmath>

namespace Disparity
{
    namespace
    {
        DirectX::XMFLOAT3 Lerp3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t)
        {
            return {
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t
            };
        }
    }

    void TransformAnimation::AddKeyframe(const TransformKeyframe& keyframe)
    {
        m_keyframes.push_back(keyframe);
        std::sort(m_keyframes.begin(), m_keyframes.end(), [](const TransformKeyframe& a, const TransformKeyframe& b) {
            return a.Time < b.Time;
        });
    }

    Transform TransformAnimation::Sample(float timeSeconds, bool loop) const
    {
        if (m_keyframes.empty())
        {
            return {};
        }

        const float duration = GetDuration();
        if (loop && duration > 0.0f)
        {
            timeSeconds = std::fmod(timeSeconds, duration);
        }

        if (timeSeconds <= m_keyframes.front().Time)
        {
            return m_keyframes.front().Value;
        }

        if (timeSeconds >= m_keyframes.back().Time)
        {
            return m_keyframes.back().Value;
        }

        for (size_t index = 0; index + 1 < m_keyframes.size(); ++index)
        {
            const TransformKeyframe& a = m_keyframes[index];
            const TransformKeyframe& b = m_keyframes[index + 1];
            if (timeSeconds >= a.Time && timeSeconds <= b.Time)
            {
                const float t = (timeSeconds - a.Time) / (b.Time - a.Time);
                Transform result;
                result.Position = Lerp3(a.Value.Position, b.Value.Position, t);
                result.Rotation = Lerp3(a.Value.Rotation, b.Value.Rotation, t);
                result.Scale = Lerp3(a.Value.Scale, b.Value.Scale, t);
                return result;
            }
        }

        return m_keyframes.back().Value;
    }

    bool TransformAnimation::IsEmpty() const
    {
        return m_keyframes.empty();
    }

    float TransformAnimation::GetDuration() const
    {
        return m_keyframes.empty() ? 0.0f : m_keyframes.back().Time;
    }

    float BobAnimation::SampleOffset(float deltaSeconds)
    {
        Time += deltaSeconds;
        return Amplitude * std::sin(Time * Frequency);
    }

    Transform BlendTransforms(const Transform& a, const Transform& b, float weight)
    {
        const float t = std::clamp(weight, 0.0f, 1.0f);
        Transform result;
        result.Position = Lerp3(a.Position, b.Position, t);
        result.Rotation = Lerp3(a.Rotation, b.Rotation, t);
        result.Scale = Lerp3(a.Scale, b.Scale, t);
        return result;
    }

    void SkinningPalette::Resize(size_t jointCount)
    {
        JointMatrices.resize(jointCount);
        for (DirectX::XMFLOAT4X4& matrix : JointMatrices)
        {
            DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixIdentity());
        }
        UploadGeneration = 0;
    }

    void SkinningPalette::MarkUploaded()
    {
        if (!JointMatrices.empty())
        {
            ++UploadGeneration;
        }
    }

    bool SkinningPalette::IsReady() const
    {
        return !JointMatrices.empty() && UploadGeneration > 0;
    }
}
