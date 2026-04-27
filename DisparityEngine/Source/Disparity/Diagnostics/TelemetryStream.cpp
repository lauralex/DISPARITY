#include "Disparity/Diagnostics/TelemetryStream.h"

#include <utility>

namespace Disparity
{
    TelemetryStream::TelemetryStream(uint32_t maxEvents)
        : m_maxEvents(maxEvents == 0 ? 1u : maxEvents)
    {
    }

    void TelemetryStream::IncrementCounter(const std::string& name, double amount)
    {
        if (!name.empty())
        {
            m_counters[name] += amount;
        }
    }

    void TelemetryStream::SetGauge(const std::string& name, double value)
    {
        if (!name.empty())
        {
            m_gauges[name] = value;
        }
    }

    void TelemetryStream::PushEvent(TelemetryEvent event)
    {
        if (event.Name.empty())
        {
            return;
        }

        if (m_events.size() >= m_maxEvents)
        {
            m_events.pop_front();
            ++m_droppedEvents;
        }

        m_events.push_back(std::move(event));
        ++m_totalEvents;
    }

    void TelemetryStream::Clear()
    {
        m_counters.clear();
        m_gauges.clear();
        m_events.clear();
        m_totalEvents = 0;
        m_droppedEvents = 0;
    }

    double TelemetryStream::GetCounter(const std::string& name, double fallback) const
    {
        const auto found = m_counters.find(name);
        return found == m_counters.end() ? fallback : found->second;
    }

    double TelemetryStream::GetGauge(const std::string& name, double fallback) const
    {
        const auto found = m_gauges.find(name);
        return found == m_gauges.end() ? fallback : found->second;
    }

    std::vector<TelemetryEvent> TelemetryStream::GetEventsForChannel(const std::string& channel) const
    {
        std::vector<TelemetryEvent> events;
        for (const TelemetryEvent& event : m_events)
        {
            if (event.Channel == channel)
            {
                events.push_back(event);
            }
        }
        return events;
    }

    const std::deque<TelemetryEvent>& TelemetryStream::GetEvents() const
    {
        return m_events;
    }

    TelemetryStreamDiagnostics TelemetryStream::GetDiagnostics() const
    {
        return {
            static_cast<uint32_t>(m_counters.size()),
            static_cast<uint32_t>(m_gauges.size()),
            static_cast<uint32_t>(m_events.size()),
            m_totalEvents,
            m_droppedEvents,
            m_maxEvents
        };
    }
}
