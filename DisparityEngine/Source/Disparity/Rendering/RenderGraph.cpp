#include "Disparity/Rendering/RenderGraph.h"

#include <algorithm>
#include <utility>

namespace Disparity
{
    void RenderGraph::Reset()
    {
        m_nextResourceId = 1;
        m_resources.clear();
        m_passes.clear();
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
        m_passes.push_back(RenderGraphPass{ std::move(name), std::move(reads), std::move(writes), std::move(execute) });
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

    std::vector<std::string> RenderGraph::Validate() const
    {
        std::vector<std::string> errors;
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

    void RenderGraph::Execute() const
    {
        for (const RenderGraphPass& pass : m_passes)
        {
            if (pass.Execute)
            {
                pass.Execute();
            }
        }
    }

    bool RenderGraph::HasResource(uint32_t id) const
    {
        return std::any_of(m_resources.begin(), m_resources.end(), [id](const RenderGraphResource& resource) {
            return resource.Id == id;
        });
    }
}
