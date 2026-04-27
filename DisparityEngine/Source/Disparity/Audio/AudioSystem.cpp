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
        std::unordered_map<std::string, AudioBusMeter> g_meters;
        bool g_xaudio2Available = false;
        bool g_preferXAudio2 = false;
        std::string g_backendName = "WinMM prototype mixer";
        DirectX::XMFLOAT3 g_listenerPosition = {};
        DirectX::XMFLOAT3 g_listenerForward = { 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT3 g_listenerUp = { 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT3 g_listenerRight = { 1.0f, 0.0f, 0.0f };

        float Length(const DirectX::XMFLOAT3& value)
        {
            return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
        }

        DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& fallback)
        {
            const float length = Length(value);
            if (length <= 0.0001f)
            {
                return fallback;
            }

            return { value.x / length, value.y / length, value.z / length };
        }

        DirectX::XMFLOAT3 Cross(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

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

        bool DetectXAudio2()
        {
            HMODULE xaudio = LoadLibraryW(L"XAudio2_9.dll");
            if (!xaudio)
            {
                xaudio = LoadLibraryW(L"XAudio2_8.dll");
            }
            if (!xaudio)
            {
                return false;
            }

            FreeLibrary(xaudio);
            return true;
        }

        void RefreshBackendName()
        {
            g_backendName = (g_preferXAudio2 && g_xaudio2Available)
                ? "XAudio2 available (WinMM render path)"
                : "WinMM prototype mixer";
        }

        void BeginVoiceMeter(const std::string& busName, float peak, float rms)
        {
            std::scoped_lock lock(g_audioMutex);
            AudioBusMeter& meter = g_meters[busName];
            meter.BusName = busName;
            meter.Peak = std::max(meter.Peak * 0.82f, std::clamp(peak, 0.0f, 1.0f));
            meter.Rms = std::max(meter.Rms * 0.82f, std::clamp(rms, 0.0f, 1.0f));
            ++meter.ActiveVoices;
        }

        void EndVoiceMeter(const std::string& busName)
        {
            std::scoped_lock lock(g_audioMutex);
            AudioBusMeter& meter = g_meters[busName];
            meter.BusName = busName;
            if (meter.ActiveVoices > 0)
            {
                --meter.ActiveVoices;
            }
            meter.Peak *= 0.65f;
            meter.Rms *= 0.65f;
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
            BeginVoiceMeter(busName, gain, gain * 0.707f);
            const float clampedPan = std::clamp(pan, -1.0f, 1.0f);
            const float leftGain = std::sqrt(0.5f * (1.0f - clampedPan));
            const float rightGain = std::sqrt(0.5f * (1.0f + clampedPan));

            std::vector<short> samples(sampleCount * 2u);
            const double toneDuration = static_cast<double>(sampleCount) / static_cast<double>(SampleRate);
            for (size_t index = 0; index < sampleCount; ++index)
            {
                const double t = static_cast<double>(index) / static_cast<double>(SampleRate);
                const double attack = std::min(1.0, t / 0.006);
                const double release = std::min(1.0, std::max(0.0, toneDuration - t) / 0.018);
                const double envelope = attack * release;
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
                EndVoiceMeter(busName);
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
            EndVoiceMeter(busName);
            g_tonePlaying = false;
        }
    }

    bool AudioSystem::Initialize()
    {
        EnsureDefaultBuses();
        g_xaudio2Available = DetectXAudio2();
        RefreshBackendName();
        return true;
    }

    const char* AudioSystem::GetBackendName()
    {
        RefreshBackendName();
        return g_backendName.c_str();
    }

    AudioBackendInfo AudioSystem::GetBackendInfo()
    {
        RefreshBackendName();
        return AudioBackendInfo{ g_backendName, g_xaudio2Available, g_preferXAudio2 };
    }

    bool AudioSystem::IsXAudio2Available()
    {
        return g_xaudio2Available;
    }

    void AudioSystem::PreferXAudio2(bool preferred)
    {
        g_preferXAudio2 = preferred;
        RefreshBackendName();
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
        DirectX::XMFLOAT3 listenerRight = {};
        {
            std::scoped_lock lock(g_audioMutex);
            listener = g_listenerPosition;
            listenerRight = g_listenerRight;
        }

        const float dx = worldPosition.x - listener.x;
        const float dy = worldPosition.y - listener.y;
        const float dz = worldPosition.z - listener.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float attenuation = 1.0f / (1.0f + distance * 0.18f);
        const float projectedRight = dx * listenerRight.x + dy * listenerRight.y + dz * listenerRight.z;
        const float pan = std::clamp(projectedRight / 8.0f, -1.0f, 1.0f);

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

    void AudioSystem::SetBusSend(const std::string& busName, float send)
    {
        EnsureDefaultBuses();
        std::scoped_lock lock(g_audioMutex);
        AudioBus& bus = g_buses[busName];
        bus.Name = busName;
        bus.Send = std::clamp(send, 0.0f, 1.0f);
    }

    float AudioSystem::GetBusSend(const std::string& busName)
    {
        EnsureDefaultBuses();
        std::scoped_lock lock(g_audioMutex);
        const auto found = g_buses.find(busName);
        return found == g_buses.end() ? 0.0f : found->second.Send;
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

    std::vector<AudioBusMeter> AudioSystem::GetMeters()
    {
        EnsureDefaultBuses();
        std::scoped_lock lock(g_audioMutex);
        std::vector<AudioBusMeter> meters;
        meters.reserve(g_buses.size());
        for (const auto& [name, bus] : g_buses)
        {
            (void)bus;
            const auto found = g_meters.find(name);
            if (found != g_meters.end())
            {
                meters.push_back(found->second);
            }
            else
            {
                meters.push_back(AudioBusMeter{ name, 0.0f, 0.0f, 0 });
            }
        }
        std::sort(meters.begin(), meters.end(), [](const AudioBusMeter& a, const AudioBusMeter& b) {
            return a.BusName < b.BusName;
        });
        return meters;
    }

    AudioSnapshot AudioSystem::CaptureSnapshot()
    {
        AudioSnapshot snapshot;
        snapshot.MasterVolume = GetMasterVolume();
        snapshot.Buses = GetBuses();
        return snapshot;
    }

    void AudioSystem::ApplySnapshot(const AudioSnapshot& snapshot)
    {
        SetMasterVolume(snapshot.MasterVolume);
        for (const AudioBus& bus : snapshot.Buses)
        {
            SetBusVolume(bus.Name, bus.Volume);
            SetBusMuted(bus.Name, bus.Muted);
            SetBusSend(bus.Name, bus.Send);
        }
    }

    void AudioSystem::SetListenerPosition(const DirectX::XMFLOAT3& position)
    {
        std::scoped_lock lock(g_audioMutex);
        g_listenerPosition = position;
    }

    void AudioSystem::SetListenerOrientation(
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& forward,
        const DirectX::XMFLOAT3& up)
    {
        std::scoped_lock lock(g_audioMutex);
        g_listenerPosition = position;
        g_listenerForward = Normalize(forward, { 0.0f, 0.0f, 1.0f });
        g_listenerUp = Normalize(up, { 0.0f, 1.0f, 0.0f });
        g_listenerRight = Normalize(Cross(g_listenerUp, g_listenerForward), { 1.0f, 0.0f, 0.0f });
    }
}
