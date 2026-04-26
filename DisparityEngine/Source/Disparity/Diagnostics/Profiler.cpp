#include "Disparity/Diagnostics/Profiler.h"

#include <mutex>
#include <utility>

namespace Disparity
{
    namespace
    {
        std::mutex g_profilerMutex;
        ProfileSnapshot g_snapshot;
        std::vector<ProfileRecord> g_currentRecords;
        std::chrono::steady_clock::time_point g_frameStart;
    }

    void Profiler::BeginFrame()
    {
        std::scoped_lock lock(g_profilerMutex);
        g_currentRecords.clear();
        g_frameStart = std::chrono::steady_clock::now();
    }

    void Profiler::EndFrame()
    {
        const auto frameEnd = std::chrono::steady_clock::now();
        const double frameMs = std::chrono::duration<double, std::milli>(frameEnd - g_frameStart).count();

        std::scoped_lock lock(g_profilerMutex);
        g_snapshot.FrameMilliseconds = frameMs;
        g_snapshot.FramesPerSecond = frameMs > 0.0001 ? 1000.0 / frameMs : 0.0;
        g_snapshot.Records = g_currentRecords;
    }

    void Profiler::Record(std::string name, double milliseconds)
    {
        std::scoped_lock lock(g_profilerMutex);
        g_currentRecords.push_back({ std::move(name), milliseconds });
    }

    ProfileSnapshot Profiler::GetSnapshot()
    {
        std::scoped_lock lock(g_profilerMutex);
        return g_snapshot;
    }

    ScopedProfile::ScopedProfile(std::string name)
        : m_name(std::move(name))
        , m_start(std::chrono::steady_clock::now())
    {
    }

    ScopedProfile::~ScopedProfile()
    {
        const auto end = std::chrono::steady_clock::now();
        Profiler::Record(m_name, std::chrono::duration<double, std::milli>(end - m_start).count());
    }
}
