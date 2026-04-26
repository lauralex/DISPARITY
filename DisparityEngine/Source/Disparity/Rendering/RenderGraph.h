#pragma once

#include <cstdint>
#include <functional>
#include <limits>
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
        uint32_t Id = 0;
        std::string Name;
        std::vector<uint32_t> Reads;
        std::vector<uint32_t> Writes;
        std::function<void()> Execute;
        bool Enabled = true;
        bool Culled = false;
        std::string CullReason;
        uint32_t ExecutionOrder = std::numeric_limits<uint32_t>::max();
        double LastCpuMilliseconds = 0.0;
        double LastGpuMilliseconds = 0.0;
    };

    struct RenderGraphResourceLifetime
    {
        uint32_t ResourceId = 0;
        uint32_t FirstPass = std::numeric_limits<uint32_t>::max();
        uint32_t LastPass = 0;
        bool External = false;
    };

    struct RenderGraphBarrier
    {
        uint32_t ResourceId = 0;
        uint32_t FromPass = std::numeric_limits<uint32_t>::max();
        uint32_t ToPass = std::numeric_limits<uint32_t>::max();
        std::string FromAccess;
        std::string ToAccess;
    };

    struct RenderGraphAliasCandidate
    {
        uint32_t FirstResourceId = 0;
        uint32_t SecondResourceId = 0;
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
        void SetPassEnabled(uint32_t passId, bool enabled, std::string reason = {});

        [[nodiscard]] const std::vector<RenderGraphResource>& GetResources() const;
        [[nodiscard]] const std::vector<RenderGraphPass>& GetPasses() const;
        [[nodiscard]] const std::vector<uint32_t>& GetExecutionOrder() const;
        [[nodiscard]] const std::vector<RenderGraphResourceLifetime>& GetResourceLifetimes() const;
        [[nodiscard]] const std::vector<RenderGraphBarrier>& GetBarriers() const;
        [[nodiscard]] const std::vector<RenderGraphAliasCandidate>& GetAliasCandidates() const;
        [[nodiscard]] std::vector<std::string> Validate() const;
        [[nodiscard]] bool Compile();
        void Execute();
        void RecordPassCpuTime(uint32_t passId, double milliseconds);
        void RecordPassGpuTime(uint32_t passId, double milliseconds);

    private:
        [[nodiscard]] bool HasResource(uint32_t id) const;
        [[nodiscard]] const RenderGraphResource* FindResource(uint32_t id) const;

        uint32_t m_nextResourceId = 1;
        std::vector<RenderGraphResource> m_resources;
        std::vector<RenderGraphPass> m_passes;
        std::vector<uint32_t> m_executionOrder;
        std::vector<RenderGraphResourceLifetime> m_resourceLifetimes;
        std::vector<RenderGraphBarrier> m_barriers;
        std::vector<RenderGraphAliasCandidate> m_aliasCandidates;
        std::vector<std::string> m_compileErrors;
    };
}
