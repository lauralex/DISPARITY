#include "Disparity/Rendering/RenderGraph.h"

#include <algorithm>
#include <chrono>
#include <queue>
#include <unordered_map>
#include <utility>

namespace Disparity
{
    void RenderGraph::Reset()
    {
        m_nextResourceId = 1;
        m_resources.clear();
        m_passes.clear();
        m_executionOrder.clear();
        m_resourceLifetimes.clear();
        m_barriers.clear();
        m_aliasCandidates.clear();
        m_resourceAllocations.clear();
        m_compileErrors.clear();
    }

    uint32_t RenderGraph::AddResource(std::string name, RenderGraphResourceKind kind)
    {
        const uint32_t id = m_nextResourceId++;
        m_resources.push_back(RenderGraphResource{ id, std::move(name), kind });
        return id;
    }

    uint32_t RenderGraph::AddPass(
        std::string name,
        std::vector<uint32_t> reads,
        std::vector<uint32_t> writes,
        std::function<void()> execute)
    {
        const uint32_t id = static_cast<uint32_t>(m_passes.size());
        RenderGraphPass pass;
        pass.Id = id;
        pass.Name = std::move(name);
        pass.Reads = std::move(reads);
        pass.Writes = std::move(writes);
        pass.Execute = std::move(execute);
        m_passes.push_back(std::move(pass));
        return id;
    }

    void RenderGraph::SetPassEnabled(uint32_t passId, bool enabled, std::string reason)
    {
        if (passId >= m_passes.size())
        {
            return;
        }

        RenderGraphPass& pass = m_passes[passId];
        pass.Enabled = enabled;
        pass.Culled = !enabled;
        pass.CullReason = enabled ? std::string{} : std::move(reason);
    }

    const std::vector<RenderGraphResource>& RenderGraph::GetResources() const
    {
        return m_resources;
    }

    const std::vector<RenderGraphPass>& RenderGraph::GetPasses() const
    {
        return m_passes;
    }

    const std::vector<uint32_t>& RenderGraph::GetExecutionOrder() const
    {
        return m_executionOrder;
    }

    const std::vector<RenderGraphResourceLifetime>& RenderGraph::GetResourceLifetimes() const
    {
        return m_resourceLifetimes;
    }

    const std::vector<RenderGraphBarrier>& RenderGraph::GetBarriers() const
    {
        return m_barriers;
    }

    const std::vector<RenderGraphAliasCandidate>& RenderGraph::GetAliasCandidates() const
    {
        return m_aliasCandidates;
    }

    const std::vector<RenderGraphResourceAllocation>& RenderGraph::GetResourceAllocations() const
    {
        return m_resourceAllocations;
    }

    std::vector<std::string> RenderGraph::Validate() const
    {
        std::vector<std::string> errors = m_compileErrors;
        for (const RenderGraphPass& pass : m_passes)
        {
            if (pass.Name.empty())
            {
                errors.push_back("Render graph pass has no name");
            }

            for (const uint32_t resource : pass.Reads)
            {
                if (!HasResource(resource))
                {
                    errors.push_back("Pass " + pass.Name + " reads unknown resource " + std::to_string(resource));
                }
            }

            for (const uint32_t resource : pass.Writes)
            {
                if (!HasResource(resource))
                {
                    errors.push_back("Pass " + pass.Name + " writes unknown resource " + std::to_string(resource));
                }
            }
        }

        return errors;
    }

    bool RenderGraph::Compile()
    {
        m_executionOrder.clear();
        m_resourceLifetimes.clear();
        m_barriers.clear();
        m_aliasCandidates.clear();
        m_resourceAllocations.clear();
        m_compileErrors.clear();

        for (RenderGraphResource& resource : m_resources)
        {
            resource.PhysicalIndex = std::numeric_limits<uint32_t>::max();
            resource.Aliased = false;
        }

        const size_t passCount = m_passes.size();
        std::vector<std::vector<uint32_t>> dependencies(passCount);
        std::vector<std::vector<uint32_t>> dependents(passCount);
        std::unordered_map<uint32_t, uint32_t> latestWriter;
        std::unordered_map<uint32_t, std::vector<uint32_t>> latestReaders;

        const auto addDependency = [&](uint32_t passId, uint32_t dependencyId) {
            if (passId == dependencyId || passId >= passCount || dependencyId >= passCount)
            {
                return;
            }

            std::vector<uint32_t>& passDependencies = dependencies[passId];
            if (std::find(passDependencies.begin(), passDependencies.end(), dependencyId) == passDependencies.end())
            {
                passDependencies.push_back(dependencyId);
                dependents[dependencyId].push_back(passId);
            }
        };

        for (uint32_t passId = 0; passId < passCount; ++passId)
        {
            RenderGraphPass& mutablePass = m_passes[passId];
            mutablePass.ExecutionOrder = std::numeric_limits<uint32_t>::max();
            mutablePass.Culled = !mutablePass.Enabled;
            if (!mutablePass.Enabled)
            {
                if (mutablePass.CullReason.empty())
                {
                    mutablePass.CullReason = "Pass disabled by renderer settings";
                }
                continue;
            }

            const RenderGraphPass& pass = m_passes[passId];
            for (const uint32_t resource : pass.Reads)
            {
                if (!HasResource(resource))
                {
                    continue;
                }

                const auto writer = latestWriter.find(resource);
                if (writer != latestWriter.end())
                {
                    addDependency(passId, writer->second);
                    m_barriers.push_back(RenderGraphBarrier{ resource, writer->second, passId, "Write", "Read" });
                }
                latestReaders[resource].push_back(passId);
            }

            for (const uint32_t resource : pass.Writes)
            {
                if (!HasResource(resource))
                {
                    continue;
                }

                const auto writer = latestWriter.find(resource);
                if (writer != latestWriter.end())
                {
                    addDependency(passId, writer->second);
                    m_barriers.push_back(RenderGraphBarrier{ resource, writer->second, passId, "Write", "Write" });
                }

                const auto readers = latestReaders.find(resource);
                if (readers != latestReaders.end())
                {
                    for (const uint32_t reader : readers->second)
                    {
                        addDependency(passId, reader);
                    }
                    readers->second.clear();
                }

                latestWriter[resource] = passId;
            }
        }

        std::queue<uint32_t> ready;
        std::vector<uint32_t> indegree(passCount, 0);
        for (uint32_t passId = 0; passId < passCount; ++passId)
        {
            if (!m_passes[passId].Enabled)
            {
                continue;
            }

            indegree[passId] = static_cast<uint32_t>(dependencies[passId].size());
            if (indegree[passId] == 0)
            {
                ready.push(passId);
            }
        }

        while (!ready.empty())
        {
            const uint32_t passId = ready.front();
            ready.pop();
            m_executionOrder.push_back(passId);

            for (const uint32_t dependent : dependents[passId])
            {
                if (indegree[dependent] > 0)
                {
                    --indegree[dependent];
                    if (indegree[dependent] == 0)
                    {
                        ready.push(dependent);
                    }
                }
            }
        }

        const size_t enabledPassCount = static_cast<size_t>(std::count_if(m_passes.begin(), m_passes.end(), [](const RenderGraphPass& pass) {
            return pass.Enabled;
        }));
        if (m_executionOrder.size() != enabledPassCount)
        {
            m_compileErrors.push_back("Render graph scheduling failed; falling back to submission order");
            m_executionOrder.clear();
            for (uint32_t passId = 0; passId < passCount; ++passId)
            {
                if (m_passes[passId].Enabled)
                {
                    m_executionOrder.push_back(passId);
                }
            }
        }

        for (uint32_t orderIndex = 0; orderIndex < m_executionOrder.size(); ++orderIndex)
        {
            m_passes[m_executionOrder[orderIndex]].ExecutionOrder = orderIndex;
        }

        std::unordered_map<uint32_t, RenderGraphResourceLifetime> lifetimeByResource;
        for (const RenderGraphResource& resource : m_resources)
        {
            lifetimeByResource[resource.Id] = RenderGraphResourceLifetime{
                resource.Id,
                std::numeric_limits<uint32_t>::max(),
                0,
                resource.Kind == RenderGraphResourceKind::External
            };
        }

        for (uint32_t orderIndex = 0; orderIndex < m_executionOrder.size(); ++orderIndex)
        {
            const RenderGraphPass& pass = m_passes[m_executionOrder[orderIndex]];
            std::vector<uint32_t> touched = pass.Reads;
            touched.insert(touched.end(), pass.Writes.begin(), pass.Writes.end());
            for (const uint32_t resource : touched)
            {
                auto lifetime = lifetimeByResource.find(resource);
                if (lifetime == lifetimeByResource.end())
                {
                    continue;
                }

                lifetime->second.FirstPass = std::min(lifetime->second.FirstPass, orderIndex);
                lifetime->second.LastPass = std::max(lifetime->second.LastPass, orderIndex);
            }
        }

        for (const auto& [resourceId, lifetime] : lifetimeByResource)
        {
            (void)resourceId;
            if (lifetime.FirstPass != std::numeric_limits<uint32_t>::max())
            {
                m_resourceLifetimes.push_back(lifetime);
            }
        }

        std::sort(m_resourceLifetimes.begin(), m_resourceLifetimes.end(), [](const RenderGraphResourceLifetime& left, const RenderGraphResourceLifetime& right) {
            if (left.FirstPass == right.FirstPass)
            {
                return left.ResourceId < right.ResourceId;
            }
            return left.FirstPass < right.FirstPass;
        });

        for (size_t first = 0; first < m_resourceLifetimes.size(); ++first)
        {
            const RenderGraphResource* firstResource = FindResource(m_resourceLifetimes[first].ResourceId);
            if (!firstResource || firstResource->Kind == RenderGraphResourceKind::External)
            {
                continue;
            }

            for (size_t second = first + 1; second < m_resourceLifetimes.size(); ++second)
            {
                const RenderGraphResource* secondResource = FindResource(m_resourceLifetimes[second].ResourceId);
                if (!secondResource || secondResource->Kind != firstResource->Kind || secondResource->Kind == RenderGraphResourceKind::External)
                {
                    continue;
                }

                const RenderGraphResourceLifetime& a = m_resourceLifetimes[first];
                const RenderGraphResourceLifetime& b = m_resourceLifetimes[second];
                if (a.LastPass < b.FirstPass || b.LastPass < a.FirstPass)
                {
                    m_aliasCandidates.push_back(RenderGraphAliasCandidate{ a.ResourceId, b.ResourceId });
                }
            }
        }

        struct TransientHeapSlot
        {
            RenderGraphResourceKind Kind = RenderGraphResourceKind::Texture;
            uint32_t LastPass = 0;
        };

        std::vector<TransientHeapSlot> heapSlots;
        for (const RenderGraphResourceLifetime& lifetime : m_resourceLifetimes)
        {
            const RenderGraphResource* resource = FindResource(lifetime.ResourceId);
            if (!resource)
            {
                continue;
            }

            if (resource->Kind == RenderGraphResourceKind::External)
            {
                m_resourceAllocations.push_back(RenderGraphResourceAllocation{
                    lifetime.ResourceId,
                    std::numeric_limits<uint32_t>::max(),
                    true,
                    false
                });
                continue;
            }

            uint32_t heapIndex = std::numeric_limits<uint32_t>::max();
            bool aliased = false;
            for (uint32_t index = 0; index < static_cast<uint32_t>(heapSlots.size()); ++index)
            {
                TransientHeapSlot& slot = heapSlots[index];
                if (slot.Kind == resource->Kind && slot.LastPass < lifetime.FirstPass)
                {
                    heapIndex = index;
                    slot.LastPass = lifetime.LastPass;
                    aliased = true;
                    break;
                }
            }

            if (heapIndex == std::numeric_limits<uint32_t>::max())
            {
                heapIndex = static_cast<uint32_t>(heapSlots.size());
                heapSlots.push_back(TransientHeapSlot{ resource->Kind, lifetime.LastPass });
            }

            m_resourceAllocations.push_back(RenderGraphResourceAllocation{
                lifetime.ResourceId,
                heapIndex,
                false,
                aliased
            });

            auto mutableResource = std::find_if(m_resources.begin(), m_resources.end(), [resourceId = lifetime.ResourceId](const RenderGraphResource& candidate) {
                return candidate.Id == resourceId;
            });
            if (mutableResource != m_resources.end())
            {
                mutableResource->PhysicalIndex = heapIndex;
                mutableResource->Aliased = aliased;
            }
        }

        return m_compileErrors.empty();
    }

    void RenderGraph::Execute()
    {
        if (m_executionOrder.empty())
        {
            (void)Compile();
        }

        for (const uint32_t passId : m_executionOrder)
        {
            if (passId >= m_passes.size())
            {
                continue;
            }

            RenderGraphPass& pass = m_passes[passId];
            if (pass.Execute)
            {
                const auto start = std::chrono::steady_clock::now();
                pass.Execute();
                const auto end = std::chrono::steady_clock::now();
                pass.LastCpuMilliseconds = std::chrono::duration<double, std::milli>(end - start).count();
            }
        }
    }

    void RenderGraph::RecordPassCpuTime(uint32_t passId, double milliseconds)
    {
        if (passId < m_passes.size())
        {
            m_passes[passId].LastCpuMilliseconds = milliseconds;
        }
    }

    void RenderGraph::RecordPassGpuTime(uint32_t passId, double milliseconds)
    {
        if (passId < m_passes.size())
        {
            m_passes[passId].LastGpuMilliseconds = milliseconds;
        }
    }

    bool RenderGraph::HasResource(uint32_t id) const
    {
        return FindResource(id) != nullptr;
    }

    const RenderGraphResource* RenderGraph::FindResource(uint32_t id) const
    {
        const auto found = std::find_if(m_resources.begin(), m_resources.end(), [id](const RenderGraphResource& resource) {
            return resource.Id == id;
        });
        return found == m_resources.end() ? nullptr : &(*found);
    }
}
