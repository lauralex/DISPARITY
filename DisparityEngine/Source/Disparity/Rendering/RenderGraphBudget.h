#pragma once

#include "Disparity/Rendering/RenderGraph.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Disparity
{
    struct RenderGraphBudget
    {
        uint32_t MaxPasses = 16;
        uint32_t MaxResources = 32;
        uint32_t MaxBarriers = 64;
        uint32_t MinAliasedResources = 0;
    };

    struct RenderGraphBudgetReport
    {
        bool WithinBudget = true;
        uint32_t Passes = 0;
        uint32_t Resources = 0;
        uint32_t Barriers = 0;
        uint32_t AliasedResources = 0;
        std::vector<std::string> Warnings;
    };

    [[nodiscard]] RenderGraphBudgetReport AnalyzeRenderGraphBudget(const RenderGraph& graph, const RenderGraphBudget& budget);
}
