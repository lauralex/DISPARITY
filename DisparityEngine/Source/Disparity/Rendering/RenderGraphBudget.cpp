#include "Disparity/Rendering/RenderGraphBudget.h"

namespace Disparity
{
    RenderGraphBudgetReport AnalyzeRenderGraphBudget(const RenderGraph& graph, const RenderGraphBudget& budget)
    {
        RenderGraphBudgetReport report;
        report.Passes = static_cast<uint32_t>(graph.GetPasses().size());
        report.Resources = static_cast<uint32_t>(graph.GetResources().size());
        report.Barriers = static_cast<uint32_t>(graph.GetBarriers().size());

        for (const RenderGraphResourceAllocation& allocation : graph.GetResourceAllocations())
        {
            if (allocation.Aliased)
            {
                ++report.AliasedResources;
            }
        }

        if (report.Passes > budget.MaxPasses)
        {
            report.WithinBudget = false;
            report.Warnings.push_back("render graph pass count exceeds budget");
        }
        if (report.Resources > budget.MaxResources)
        {
            report.WithinBudget = false;
            report.Warnings.push_back("render graph resource count exceeds budget");
        }
        if (report.Barriers > budget.MaxBarriers)
        {
            report.WithinBudget = false;
            report.Warnings.push_back("render graph transition barrier count exceeds budget");
        }
        if (report.AliasedResources < budget.MinAliasedResources)
        {
            report.WithinBudget = false;
            report.Warnings.push_back("render graph alias reuse is below expectation");
        }

        return report;
    }
}
