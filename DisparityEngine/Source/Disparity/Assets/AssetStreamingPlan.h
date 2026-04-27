#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Disparity
{
    struct AssetStreamingRequest
    {
        std::filesystem::path Path;
        uint32_t Priority = 0;
        uint64_t EstimatedBytes = 0;
        std::vector<std::filesystem::path> Dependencies;
        bool Pinned = false;
    };

    struct AssetStreamingBudget
    {
        uint32_t MaxRequestsPerTick = 4;
        uint64_t MaxBytesPerTick = 16ull * 1024ull * 1024ull;
    };

    struct AssetStreamingTickResult
    {
        uint32_t ScheduledRequests = 0;
        uint32_t DeferredRequests = 0;
        uint64_t ScheduledBytes = 0;
    };

    struct AssetStreamingPlanDiagnostics
    {
        uint32_t RequestsSubmitted = 0;
        uint32_t RequestsScheduled = 0;
        uint32_t RequestsDeferred = 0;
        uint32_t RequestsCancelled = 0;
        uint32_t PendingRequests = 0;
        uint32_t LoadedRequests = 0;
        uint32_t DependencyEdges = 0;
        uint64_t BytesScheduled = 0;
    };

    class AssetStreamingPlan
    {
    public:
        void Clear();
        void Submit(AssetStreamingRequest request);
        [[nodiscard]] bool Cancel(const std::filesystem::path& path);
        [[nodiscard]] AssetStreamingTickResult Tick(const AssetStreamingBudget& budget);

        [[nodiscard]] AssetStreamingPlanDiagnostics GetDiagnostics() const;
        [[nodiscard]] const std::vector<AssetStreamingRequest>& GetPendingRequests() const;
        [[nodiscard]] const std::vector<AssetStreamingRequest>& GetScheduledRequests() const;

    private:
        std::vector<AssetStreamingRequest> m_pending;
        std::vector<AssetStreamingRequest> m_scheduled;
        AssetStreamingPlanDiagnostics m_diagnostics;
    };
}
