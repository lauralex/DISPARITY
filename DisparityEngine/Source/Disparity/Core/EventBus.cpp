#include "Disparity/Core/EventBus.h"

#include <algorithm>

namespace Disparity
{
    uint32_t EventBus::Subscribe(std::string eventType, Callback callback)
    {
        if (eventType.empty() || !callback)
        {
            return 0;
        }

        const uint32_t id = m_nextSubscriptionId++;
        m_subscriptions.push_back({ id, std::move(eventType), std::move(callback) });
        m_diagnostics.SubscriberCount = static_cast<uint32_t>(m_subscriptions.size());
        return id;
    }

    void EventBus::Unsubscribe(uint32_t subscriptionId)
    {
        const size_t oldSize = m_subscriptions.size();
        m_subscriptions.erase(
            std::remove_if(m_subscriptions.begin(), m_subscriptions.end(), [subscriptionId](const Subscription& subscription) {
                return subscription.Id == subscriptionId;
            }),
            m_subscriptions.end());

        if (m_subscriptions.size() != oldSize)
        {
            m_diagnostics.SubscriberCount = static_cast<uint32_t>(m_subscriptions.size());
        }
    }

    void EventBus::Clear()
    {
        m_subscriptions.clear();
        m_queue.clear();
        m_diagnostics = {};
    }

    bool EventBus::Emit(const EngineEvent& event)
    {
        ++m_diagnostics.EmittedEvents;
        const uint32_t delivered = Deliver(event);
        if (delivered == 0)
        {
            ++m_diagnostics.DroppedEvents;
            return false;
        }

        m_diagnostics.DeliveredEvents += delivered;
        return true;
    }

    void EventBus::Queue(EngineEvent event)
    {
        m_queue.push_back(std::move(event));
        m_diagnostics.QueuedEvents = static_cast<uint32_t>(m_queue.size());
        m_diagnostics.MaxQueueDepth = std::max(m_diagnostics.MaxQueueDepth, m_diagnostics.QueuedEvents);
    }

    uint32_t EventBus::Flush(uint32_t maxEvents)
    {
        ++m_diagnostics.FlushCount;

        uint32_t flushed = 0;
        while (!m_queue.empty() && flushed < maxEvents)
        {
            EngineEvent event = std::move(m_queue.front());
            m_queue.erase(m_queue.begin());
            (void)Emit(event);
            ++flushed;
        }

        m_diagnostics.QueuedEvents = static_cast<uint32_t>(m_queue.size());
        return flushed;
    }

    EventBusDiagnostics EventBus::GetDiagnostics() const
    {
        EventBusDiagnostics diagnostics = m_diagnostics;
        diagnostics.SubscriberCount = static_cast<uint32_t>(m_subscriptions.size());
        diagnostics.QueuedEvents = static_cast<uint32_t>(m_queue.size());
        return diagnostics;
    }

    size_t EventBus::PendingEventCount() const
    {
        return m_queue.size();
    }

    uint32_t EventBus::Deliver(const EngineEvent& event)
    {
        uint32_t delivered = 0;
        for (const Subscription& subscription : m_subscriptions)
        {
            if (subscription.Type == event.Type || subscription.Type == "*")
            {
                subscription.Handler(event);
                ++delivered;
            }
        }
        return delivered;
    }
}
