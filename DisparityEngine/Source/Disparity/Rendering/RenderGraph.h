#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Disparity
{
    enum class RenderGraphResourceKind
    {
        Texture,
        Buffer,
        External
    };

    struct RenderGraphResource
    {
        uint32_t Id = 0;
        std::string Name;
        RenderGraphResourceKind Kind = RenderGraphResourceKind::Texture;
    };

    struct RenderGraphPass
    {
        std::string Name;
        std::vector<uint32_t> Reads;
        std::vector<uint32_t> Writes;
        std::function<void()> Execute;
    };

    class RenderGraph
    {
    public:
        void Reset();
        [[nodiscard]] uint32_t AddResource(std::string name, RenderGraphResourceKind kind);
        [[nodiscard]] uint32_t AddPass(
            std::string name,
            std::vector<uint32_t> reads,
            std::vector<uint32_t> writes,
            std::function<void()> execute = {});

        [[nodiscard]] const std::vector<RenderGraphResource>& GetResources() const;
        [[nodiscard]] const std::vector<RenderGraphPass>& GetPasses() const;
        [[nodiscard]] std::vector<std::string> Validate() const;
        void Execute() const;

    private:
        [[nodiscard]] bool HasResource(uint32_t id) const;

        uint32_t m_nextResourceId = 1;
        std::vector<RenderGraphResource> m_resources;
        std::vector<RenderGraphPass> m_passes;
    };
}
