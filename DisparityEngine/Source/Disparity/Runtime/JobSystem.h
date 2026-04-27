#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Disparity
{
    class JobSystem
    {
    public:
        struct AsyncTextReadResult
        {
            std::filesystem::path Path;
            bool Success = false;
            std::string Text;
        };

        static void Initialize(uint32_t workerCount = 0);
        static void Shutdown();
        static void Dispatch(std::function<void()> job);
        static void DispatchReadTextFile(std::filesystem::path path, std::function<void(AsyncTextReadResult)> callback);
        static void WaitIdle();
        [[nodiscard]] static uint32_t WorkerCount();
        [[nodiscard]] static bool IsRunning();
    };
}
