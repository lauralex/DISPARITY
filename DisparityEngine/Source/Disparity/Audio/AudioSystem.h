#pragma once

#include <DirectXMath.h>
#include <cstdint>
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
        float Send = 0.0f;
    };

    struct AudioBusMeter
    {
        std::string BusName;
        float Peak = 0.0f;
        float Rms = 0.0f;
        uint32_t ActiveVoices = 0;
    };

    struct AudioSnapshot
    {
        float MasterVolume = 1.0f;
        std::vector<AudioBus> Buses;
    };

    struct AudioBackendInfo
    {
        std::string ActiveBackend;
        bool XAudio2Available = false;
        bool XAudio2Preferred = false;
    };

    struct AudioAnalysis
    {
        float Peak = 0.0f;
        float Rms = 0.0f;
        float BeatEnvelope = 0.0f;
        uint32_t ActiveVoices = 0;
        bool ContentDriven = false;
    };

    class AudioSystem
    {
    public:
        static bool Initialize();
        static void Shutdown();
        [[nodiscard]] static const char* GetBackendName();
        [[nodiscard]] static AudioBackendInfo GetBackendInfo();
        [[nodiscard]] static bool IsXAudio2Available();
        static void PreferXAudio2(bool preferred);
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
        static void SetBusSend(const std::string& busName, float send);
        [[nodiscard]] static float GetBusSend(const std::string& busName);
        [[nodiscard]] static std::vector<AudioBus> GetBuses();
        [[nodiscard]] static std::vector<AudioBusMeter> GetMeters();
        [[nodiscard]] static AudioAnalysis GetAnalysis();
        [[nodiscard]] static AudioSnapshot CaptureSnapshot();
        static void ApplySnapshot(const AudioSnapshot& snapshot);
        static void SetListenerPosition(const DirectX::XMFLOAT3& position);
        static void SetListenerOrientation(
            const DirectX::XMFLOAT3& position,
            const DirectX::XMFLOAT3& forward,
            const DirectX::XMFLOAT3& up);
    };
}
