#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace Disparity
{
    struct ProfileRecord
    {
        std::string Name;
        double Milliseconds = 0.0;
    };

    struct ProfileSnapshot
    {
        double FrameMilliseconds = 0.0;
        double FramesPerSecond = 0.0;
        std::vector<ProfileRecord> Records;
    };

    class Profiler
    {
    public:
        static void BeginFrame();
        static void EndFrame();
        static void Record(std::string name, double milliseconds);
        [[nodiscard]] static ProfileSnapshot GetSnapshot();
    };

    class ScopedProfile
    {
    public:
        explicit ScopedProfile(std::string name);
        ~ScopedProfile();

        ScopedProfile(const ScopedProfile&) = delete;
        ScopedProfile& operator=(const ScopedProfile&) = delete;

    private:
        std::string m_name;
        std::chrono::steady_clock::time_point m_start;
    };
}

#define DISPARITY_PROFILE_SCOPE(name) ::Disparity::ScopedProfile disparityProfileScope##__LINE__(name)
