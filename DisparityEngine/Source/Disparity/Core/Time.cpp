#include "Disparity/Core/Time.h"

#include <algorithm>
#include <windows.h>

namespace Disparity
{
    TimeStep::TimeStep(double deltaSeconds, double elapsedSeconds)
        : m_deltaSeconds(deltaSeconds)
        , m_elapsedSeconds(elapsedSeconds)
    {
    }

    float TimeStep::DeltaSeconds() const
    {
        return static_cast<float>(m_deltaSeconds);
    }

    double TimeStep::DeltaSecondsPrecise() const
    {
        return m_deltaSeconds;
    }

    double TimeStep::ElapsedSeconds() const
    {
        return m_elapsedSeconds;
    }

    Clock::Clock()
    {
        LARGE_INTEGER frequency = {};
        LARGE_INTEGER counter = {};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);

        m_frequency = frequency.QuadPart;
        m_lastCounter = counter.QuadPart;
    }

    TimeStep Clock::Tick()
    {
        LARGE_INTEGER counter = {};
        QueryPerformanceCounter(&counter);

        double deltaSeconds = static_cast<double>(counter.QuadPart - m_lastCounter) / static_cast<double>(m_frequency);
        deltaSeconds = std::clamp(deltaSeconds, 0.0, 0.1);

        m_lastCounter = counter.QuadPart;
        m_elapsedSeconds += deltaSeconds;

        return TimeStep(deltaSeconds, m_elapsedSeconds);
    }
}
