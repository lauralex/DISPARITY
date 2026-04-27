#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace Disparity
{
    struct TelemetryEvent
    {
        std::string Channel;
        std::string Name;
        double Value = 0.0;
        uint32_t Frame = 0;
        std::string Payload;
    };

    struct TelemetryStreamDiagnostics
    {
        uint32_t CounterCount = 0;
        uint32_t GaugeCount = 0;
        uint32_t EventCount = 0;
        uint32_t TotalEvents = 0;
        uint32_t DroppedEvents = 0;
        uint32_t MaxEvents = 0;
    };

    class TelemetryStream
    {
    public:
        explicit TelemetryStream(uint32_t maxEvents = 128);

        void IncrementCounter(const std::string& name, double amount = 1.0);
        void SetGauge(const std::string& name, double value);
        void PushEvent(TelemetryEvent event);
        void Clear();

        [[nodiscard]] double GetCounter(const std::string& name, double fallback = 0.0) const;
        [[nodiscard]] double GetGauge(const std::string& name, double fallback = 0.0) const;
        [[nodiscard]] std::vector<TelemetryEvent> GetEventsForChannel(const std::string& channel) const;
        [[nodiscard]] const std::deque<TelemetryEvent>& GetEvents() const;
        [[nodiscard]] TelemetryStreamDiagnostics GetDiagnostics() const;

    private:
        uint32_t m_maxEvents = 128;
        uint32_t m_totalEvents = 0;
        uint32_t m_droppedEvents = 0;
        std::unordered_map<std::string, double> m_counters;
        std::unordered_map<std::string, double> m_gauges;
        std::deque<TelemetryEvent> m_events;
    };
}
