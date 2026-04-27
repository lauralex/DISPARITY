#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Disparity
{
    struct EngineEvent
    {
        std::string Type;
        uint64_t EntityId = 0;
        std::string Payload;
        uint32_t Frame = 0;
    };

    struct EventBusDiagnostics
    {
        uint32_t SubscriberCount = 0;
        uint32_t QueuedEvents = 0;
        uint32_t EmittedEvents = 0;
        uint32_t DeliveredEvents = 0;
        uint32_t DroppedEvents = 0;
        uint32_t FlushCount = 0;
        uint32_t MaxQueueDepth = 0;
    };

    class EventBus
    {
    public:
        using Callback = std::function<void(const EngineEvent&)>;

        [[nodiscard]] uint32_t Subscribe(std::string eventType, Callback callback);
        void Unsubscribe(uint32_t subscriptionId);
        void Clear();

        [[nodiscard]] bool Emit(const EngineEvent& event);
        void Queue(EngineEvent event);
        [[nodiscard]] uint32_t Flush(uint32_t maxEvents = UINT32_MAX);

        [[nodiscard]] EventBusDiagnostics GetDiagnostics() const;
        [[nodiscard]] size_t PendingEventCount() const;

    private:
        struct Subscription
        {
            uint32_t Id = 0;
            std::string Type;
            Callback Handler;
        };

        [[nodiscard]] uint32_t Deliver(const EngineEvent& event);

        uint32_t m_nextSubscriptionId = 1;
        std::vector<Subscription> m_subscriptions;
        std::vector<EngineEvent> m_queue;
        EventBusDiagnostics m_diagnostics;
    };
}
