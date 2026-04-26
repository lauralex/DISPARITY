#pragma once

namespace Disparity
{
    class TimeStep
    {
    public:
        TimeStep() = default;
        TimeStep(double deltaSeconds, double elapsedSeconds);

        [[nodiscard]] float DeltaSeconds() const;
        [[nodiscard]] double DeltaSecondsPrecise() const;
        [[nodiscard]] double ElapsedSeconds() const;

    private:
        double m_deltaSeconds = 0.0;
        double m_elapsedSeconds = 0.0;
    };

    class Clock
    {
    public:
        Clock();

        [[nodiscard]] TimeStep Tick();

    private:
        long long m_frequency = 0;
        long long m_lastCounter = 0;
        double m_elapsedSeconds = 0.0;
    };
}
