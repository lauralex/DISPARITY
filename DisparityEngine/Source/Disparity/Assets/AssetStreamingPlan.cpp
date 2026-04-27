#include "Disparity/Assets/AssetStreamingPlan.h"

#include <algorithm>
#include <utility>

namespace Disparity
{
    void AssetStreamingPlan::Clear()
    {
        m_pending.clear();
        m_scheduled.clear();
        m_diagnostics = {};
    }

    void AssetStreamingPlan::Submit(AssetStreamingRequest request)
    {
        if (request.Path.empty())
        {
            return;
        }

        m_diagnostics.DependencyEdges += static_cast<uint32_t>(request.Dependencies.size());
        ++m_diagnostics.RequestsSubmitted;
        m_pending.push_back(std::move(request));
        m_diagnostics.PendingRequests = static_cast<uint32_t>(m_pending.size());
    }

    bool AssetStreamingPlan::Cancel(const std::filesystem::path& path)
    {
        const auto found = std::find_if(m_pending.begin(), m_pending.end(), [&path](const AssetStreamingRequest& request) {
            return request.Path == path;
        });
        if (found == m_pending.end() || found->Pinned)
        {
            return false;
        }

        m_pending.erase(found);
        ++m_diagnostics.RequestsCancelled;
        m_diagnostics.PendingRequests = static_cast<uint32_t>(m_pending.size());
        return true;
    }

    AssetStreamingTickResult AssetStreamingPlan::Tick(const AssetStreamingBudget& budget)
    {
        std::stable_sort(m_pending.begin(), m_pending.end(), [](const AssetStreamingRequest& a, const AssetStreamingRequest& b) {
            if (a.Pinned != b.Pinned)
            {
                return a.Pinned;
            }
            return a.Priority > b.Priority;
        });

        AssetStreamingTickResult result;
        std::vector<AssetStreamingRequest> remaining;
        remaining.reserve(m_pending.size());

        for (AssetStreamingRequest& request : m_pending)
        {
            const bool requestLimitReached = result.ScheduledRequests >= budget.MaxRequestsPerTick;
            const bool byteLimitReached = result.ScheduledBytes + request.EstimatedBytes > budget.MaxBytesPerTick && !request.Pinned;
            if (requestLimitReached || byteLimitReached)
            {
                remaining.push_back(std::move(request));
                ++result.DeferredRequests;
                continue;
            }

            result.ScheduledBytes += request.EstimatedBytes;
            ++result.ScheduledRequests;
            m_scheduled.push_back(std::move(request));
        }

        m_pending = std::move(remaining);
        m_diagnostics.RequestsScheduled += result.ScheduledRequests;
        m_diagnostics.RequestsDeferred += result.DeferredRequests;
        m_diagnostics.BytesScheduled += result.ScheduledBytes;
        m_diagnostics.PendingRequests = static_cast<uint32_t>(m_pending.size());
        m_diagnostics.LoadedRequests = static_cast<uint32_t>(m_scheduled.size());
        return result;
    }

    AssetStreamingPlanDiagnostics AssetStreamingPlan::GetDiagnostics() const
    {
        AssetStreamingPlanDiagnostics diagnostics = m_diagnostics;
        diagnostics.PendingRequests = static_cast<uint32_t>(m_pending.size());
        diagnostics.LoadedRequests = static_cast<uint32_t>(m_scheduled.size());
        return diagnostics;
    }

    const std::vector<AssetStreamingRequest>& AssetStreamingPlan::GetPendingRequests() const
    {
        return m_pending;
    }

    const std::vector<AssetStreamingRequest>& AssetStreamingPlan::GetScheduledRequests() const
    {
        return m_scheduled;
    }
}
