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
        m_compileErrors.clear();

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

        if (m_executionOrder.size() != passCount)
        {
            m_compileErrors.push_back("Render graph scheduling failed; falling back to submission order");
            m_executionOrder.clear();
            for (uint32_t passId = 0; passId < passCount; ++passId)
            {
                m_executionOrder.push_back(passId);
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
