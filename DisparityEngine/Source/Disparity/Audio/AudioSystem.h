#pragma once

#include <DirectXMath.h>
#include <filesystem>
#include <string>
#include <vector>

namespace Disparity
{
    struct AudioBus
    {
        std::string Name;
        float Volume = 1.0f;
        bool Muted = false;
    };

    class AudioSystem
    {
    public:
        static bool Initialize();
        static void Shutdown();
        static void PlayNotification();
        static void PlayTone(float frequencyHz, float durationSeconds, float volume);
        static void PlayToneOnBus(const std::string& busName, float frequencyHz, float durationSeconds, float volume);
        static void PlaySpatialTone(
            const std::string& busName,
            float frequencyHz,
            float durationSeconds,
            float volume,
            const DirectX::XMFLOAT3& worldPosition);
        static bool PlayWaveFileAsync(const std::filesystem::path& path, const std::string& busName = "SFX", bool loop = false);
        static bool StreamMusic(const std::filesystem::path& path, bool loop);
        static void StopAll();
        static void SetMasterVolume(float volume);
        [[nodiscard]] static float GetMasterVolume();
        static void SetBusVolume(const std::string& busName, float volume);
        [[nodiscard]] static float GetBusVolume(const std::string& busName);
        static void SetBusMuted(const std::string& busName, bool muted);
        [[nodiscard]] static bool IsBusMuted(const std::string& busName);
        [[nodiscard]] static std::vector<AudioBus> GetBuses();
        static void SetListenerPosition(const DirectX::XMFLOAT3& position);
    };
}
