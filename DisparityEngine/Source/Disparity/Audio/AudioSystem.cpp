#include "Disparity/Audio/AudioSystem.h"

#include "Disparity/Core/FileSystem.h"
#include "Disparity/Core/Log.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <vector>

namespace Disparity
{
    namespace
    {
        std::atomic_bool g_tonePlaying = false;
        std::atomic<float> g_masterVolume = 0.8f;
        std::mutex g_audioMutex;
        std::unordered_map<std::string, AudioBus> g_buses;
        DirectX::XMFLOAT3 g_listenerPosition = {};

        void EnsureDefaultBuses()
        {
            std::scoped_lock lock(g_audioMutex);
            if (!g_buses.empty())
            {
                return;
            }

            g_buses.emplace("Master", AudioBus{ "Master", 1.0f, false });
            g_buses.emplace("SFX", AudioBus{ "SFX", 0.9f, false });
            g_buses.emplace("UI", AudioBus{ "UI", 0.85f, false });
            g_buses.emplace("Music", AudioBus{ "Music", 0.7f, false });
            g_buses.emplace("Ambience", AudioBus{ "Ambience", 0.65f, false });
        }

        float GetBusGainUnlocked(const std::string& busName)
        {
            const auto master = g_buses.find("Master");
            const float masterBusGain = master == g_buses.end() || master->second.Muted ? 1.0f : std::clamp(master->second.Volume, 0.0f, 1.0f);

            const auto bus = g_buses.find(busName);
            if (bus == g_buses.end())
            {
                return masterBusGain;
            }

            if (bus->second.Muted)
            {
                return 0.0f;
            }

            return masterBusGain * std::clamp(bus->second.Volume, 0.0f, 1.0f);
        }

        float GetBusGain(const std::string& busName)
        {
            EnsureDefaultBuses();
            std::scoped_lock lock(g_audioMutex);
            return GetBusGainUnlocked(busName);
        }

        void PlayGeneratedTone(float frequency, float durationSeconds, float volume, std::string busName, float pan)
        {
            if (g_tonePlaying.exchange(true))
            {
                return;
            }

            constexpr DWORD SampleRate = 44100;
            constexpr double Pi = 3.14159265358979323846;
            const size_t sampleCount = static_cast<size_t>(static_cast<float>(SampleRate) * std::clamp(durationSeconds, 0.04f, 2.0f));
            const float gain =
                std::clamp(volume, 0.0f, 1.0f) *
                std::clamp(g_masterVolume.load(), 0.0f, 1.0f) *
                GetBusGain(busName);
            const float clampedPan = std::clamp(pan, -1.0f, 1.0f);
            const float leftGain = std::sqrt(0.5f * (1.0f - clampedPan));
            const float rightGain = std::sqrt(0.5f * (1.0f + clampedPan));

            std::vector<short> samples(sampleCount * 2u);
            for (size_t index = 0; index < sampleCount; ++index)
            {
                const double t = static_cast<double>(index) / static_cast<double>(SampleRate);
                const double envelope = 1.0 - (static_cast<double>(index) / static_cast<double>(sampleCount));
                const float sample = static_cast<float>(std::sin(t * static_cast<double>(frequency) * 2.0 * Pi) * envelope * 14000.0 * gain);
                samples[index * 2u] = static_cast<short>(sample * leftGain);
                samples[index * 2u + 1u] = static_cast<short>(sample * rightGain);
            }

            WAVEFORMATEX format = {};
            format.wFormatTag = WAVE_FORMAT_PCM;
            format.nChannels = 2;
            format.nSamplesPerSec = SampleRate;
            format.wBitsPerSample = 16;
            format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
            format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

            HWAVEOUT output = nullptr;
            if (waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
            {
                g_tonePlaying = false;
                return;
            }

            WAVEHDR header = {};
            header.lpData = reinterpret_cast<LPSTR>(samples.data());
            header.dwBufferLength = static_cast<DWORD>(samples.size() * sizeof(short));

            if (waveOutPrepareHeader(output, &header, sizeof(header)) == MMSYSERR_NOERROR)
            {
                waveOutWrite(output, &header, sizeof(header));

                for (int attempts = 0; attempts < 100 && (header.dwFlags & WHDR_DONE) == 0; ++attempts)
                {
                    Sleep(10);
                }

                waveOutUnprepareHeader(output, &header, sizeof(header));
            }

            waveOutClose(output);
            g_tonePlaying = false;
        }
    }

    bool AudioSystem::Initialize()
    {
        EnsureDefaultBuses();
        return true;
    }

    void AudioSystem::Shutdown()
    {
        StopAll();
    }

    void AudioSystem::PlayNotification()
    {
        PlayToneOnBus("UI", 660.0f, 0.18f, 1.0f);
    }

    void AudioSystem::PlayTone(float frequencyHz, float durationSeconds, float volume)
    {
        PlayToneOnBus("SFX", frequencyHz, durationSeconds, volume);
    }

    void AudioSystem::PlayToneOnBus(const std::string& busName, float frequencyHz, float durationSeconds, float volume)
    {
        EnsureDefaultBuses();
        std::thread(PlayGeneratedTone, frequencyHz, durationSeconds, volume, busName, 0.0f).detach();
    }

    void AudioSystem::PlaySpatialTone(
        const std::string& busName,
        float frequencyHz,
        float durationSeconds,
        float volume,
        const DirectX::XMFLOAT3& worldPosition)
    {
        EnsureDefaultBuses();

        DirectX::XMFLOAT3 listener = {};
        {
            std::scoped_lock lock(g_audioMutex);
            listener = g_listenerPosition;
        }

        const float dx = worldPosition.x - listener.x;
        const float dy = worldPosition.y - listener.y;
        const float dz = worldPosition.z - listener.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float attenuation = 1.0f / (1.0f + distance * 0.18f);
        const float pan = std::clamp(dx / 8.0f, -1.0f, 1.0f);

        std::thread(PlayGeneratedTone, frequencyHz, durationSeconds, volume * attenuation, busName, pan).detach();
    }

    bool AudioSystem::PlayWaveFileAsync(const std::filesystem::path& path, const std::string& busName, bool loop)
    {
        EnsureDefaultBuses();
        if (GetBusGain(busName) <= 0.001f || g_masterVolume.load() <= 0.001f)
        {
            return true;
        }

        const std::filesystem::path resolvedPath = FileSystem::FindAssetPath(path);
        if (!std::filesystem::exists(resolvedPath))
        {
            Log(LogLevel::Warning, "Audio file not found: " + resolvedPath.string());
            return false;
        }

        UINT flags = SND_ASYNC | SND_FILENAME | SND_NODEFAULT;
        if (loop)
        {
            flags |= SND_LOOP;
        }

        return PlaySoundW(resolvedPath.c_str(), nullptr, flags) == TRUE;
    }

    bool AudioSystem::StreamMusic(const std::filesystem::path& path, bool loop)
    {
        return PlayWaveFileAsync(path, "Music", loop);
    }

    void AudioSystem::StopAll()
    {
        PlaySoundW(nullptr, nullptr, SND_PURGE);
    }

    void AudioSystem::SetMasterVolume(float volume)
    {
        g_masterVolume = std::clamp(volume, 0.0f, 1.0f);
    }

    float AudioSystem::GetMasterVolume()
    {
        return g_masterVolume.load();
    }

    void AudioSystem::SetBusVolume(const std::string& busName, float volume)
    {
        EnsureDefaultBuses();
        std::scoped_lock lock(g_audioMutex);
        AudioBus& bus = g_buses[busName];
        bus.Name = busName;
        bus.Volume = std::clamp(volume, 0.0f, 1.0f);
    }

    float AudioSystem::GetBusVolume(const std::string& busName)
    {
        EnsureDefaultBuses();
        std::scoped_lock lock(g_audioMutex);
        const auto found = g_buses.find(busName);
        return found == g_buses.end() ? 1.0f : found->second.Volume;
    }

    void AudioSystem::SetBusMuted(const std::string& busName, bool muted)
    {
        EnsureDefaultBuses();
        std::scoped_lock lock(g_audioMutex);
        AudioBus& bus = g_buses[busName];
        bus.Name = busName;
        bus.Muted = muted;
    }

    bool AudioSystem::IsBusMuted(const std::string& busName)
    {
        EnsureDefaultBuses();
        std::scoped_lock lock(g_audioMutex);
        const auto found = g_buses.find(busName);
        return found != g_buses.end() && found->second.Muted;
    }

    std::vector<AudioBus> AudioSystem::GetBuses()
    {
        EnsureDefaultBuses();
        std::scoped_lock lock(g_audioMutex);
        std::vector<AudioBus> buses;
        buses.reserve(g_buses.size());
        for (const auto& [name, bus] : g_buses)
        {
            (void)name;
            buses.push_back(bus);
        }
        std::sort(buses.begin(), buses.end(), [](const AudioBus& a, const AudioBus& b) {
            return a.Name < b.Name;
        });
        return buses;
    }

    void AudioSystem::SetListenerPosition(const DirectX::XMFLOAT3& position)
    {
        std::scoped_lock lock(g_audioMutex);
        g_listenerPosition = position;
    }
}
