#pragma once

#include <cstdint>
#include <functional>

namespace Disparity
{
    class JobSystem
    {
    public:
        static void Initialize(uint32_t workerCount = 0);
        static void Shutdown();
        static void Dispatch(std::function<void()> job);
        static void WaitIdle();
        [[nodiscard]] static uint32_t WorkerCount();
        [[nodiscard]] static bool IsRunning();
    };
}
