#include "Disparity/Assets/ProductionAssetRuntimeCatalog.h"
#include "Disparity/Core/FileSystem.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace Disparity
{
    namespace
    {
        constexpr uint64_t FnvPrime = 1099511628211ull;

        [[nodiscard]] uint64_t HashCombine(uint64_t seed, uint64_t value)
        {
            seed ^= value;
            seed *= FnvPrime;
            return seed;
        }

        [[nodiscard]] std::string Trim(std::string value)
        {
            const auto isNotSpace = [](unsigned char character)
            {
                return std::isspace(character) == 0;
            };

            value.erase(value.begin(), std::find_if(value.begin(), value.end(), isNotSpace));
            value.erase(std::find_if(value.rbegin(), value.rend(), isNotSpace).base(), value.end());
            return value;
        }

        [[nodiscard]] bool StartsWith(const std::string& text, const std::string& prefix)
        {
            return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
        }

        [[nodiscard]] std::vector<std::string> SplitSegments(const std::string& line)
        {
            std::vector<std::string> segments;
            std::istringstream stream(line);
            std::string segment;
            while (std::getline(stream, segment, '|'))
            {
                segments.push_back(Trim(segment));
            }
            return segments;
        }

        [[nodiscard]] ProductionRuntimeAssetField ParseField(std::string segment)
        {
            segment = Trim(std::move(segment));
            const size_t equals = segment.find('=');
            if (equals != std::string::npos)
            {
                return {
                    Trim(segment.substr(0, equals)),
                    Trim(segment.substr(equals + 1))
                };
            }

            const size_t space = segment.find_first_of(" \t");
            if (space == std::string::npos)
            {
                return { segment, {} };
            }

            return {
                Trim(segment.substr(0, space)),
                Trim(segment.substr(space + 1))
            };
        }

        [[nodiscard]] ProductionRuntimeAssetEntry ParseEntry(
            const std::string& trimmedLine,
            const ProductionAssetValidationRule& validation)
        {
            ProductionRuntimeAssetEntry entry = {};
            const std::vector<std::string> segments = SplitSegments(trimmedLine);
            if (segments.empty())
            {
                return entry;
            }

            const size_t firstSpace = segments.front().find_first_of(" \t");
            if (firstSpace == std::string::npos)
            {
                entry.Directive = segments.front();
            }
            else
            {
                entry.Directive = Trim(segments.front().substr(0, firstSpace));
                entry.Name = Trim(segments.front().substr(firstSpace + 1));
            }

            for (size_t index = 1; index < segments.size(); ++index)
            {
                if (!segments[index].empty())
                {
                    entry.Fields.push_back(ParseField(segments[index]));
                }
            }

            entry.Activated = !validation.ActivationToken.empty() &&
                trimmedLine.find(validation.ActivationToken) != std::string::npos;
            return entry;
        }

        [[nodiscard]] std::string MutationTargetForAction(const ProductionRuntimeActionPlan& plan)
        {
            if (plan.Action == "scheduler_budgets")
            {
                return "FrameScheduler.Rendering";
            }
            if (plan.Action == "streaming_budgets")
            {
                return "AssetStreaming.Critical";
            }
            if (plan.Action == "render_budget_classes")
            {
                return "RenderGraph.TrailerCapture";
            }
            if (plan.Action == "workspace_layouts")
            {
                return "EditorWorkspace.TrailerCapture";
            }
            if (plan.Action == "command_palette")
            {
                return "CommandPalette.disparity.capture.highres";
            }
            if (plan.Action == "event_trace_channels")
            {
                return "TraceChannel.gameplay.objective";
            }
            if (plan.Action == "objective_routes")
            {
                return "PublicDemo.Route.Extraction";
            }
            if (plan.Action == "encounter_plan")
            {
                return "Encounter.ResonanceAmbush";
            }
            if (plan.Action == "combat_sandbox")
            {
                return "CombatSandbox.RelayPressure";
            }
            return plan.Domain + "." + plan.Action;
        }
    }

    std::vector<ProductionRuntimeAsset> LoadProductionRuntimeCatalog(
        const std::vector<ProductionRuntimeAssetRule>& rules)
    {
        std::vector<ProductionRuntimeAsset> catalog;
        catalog.reserve(rules.size());

        for (const ProductionRuntimeAssetRule& rule : rules)
        {
            ProductionRuntimeAsset asset = {};
            asset.Path = rule.Validation.Path;
            asset.Domain = rule.Domain;
            asset.Action = rule.Action;
            asset.Validation = ValidateProductionAsset(rule.Validation);

            std::string text;
            if (asset.Validation.Loaded && FileSystem::ReadTextFile(rule.Validation.Path, text))
            {
                std::istringstream stream(text);
                std::string line;
                while (std::getline(stream, line))
                {
                    const std::string trimmed = Trim(line);
                    const size_t firstPipe = trimmed.find('|');
                    const std::string head = firstPipe == std::string::npos ? trimmed : trimmed.substr(0, firstPipe);
                    if (trimmed.empty() || StartsWith(trimmed, "#") || head.find('=') != std::string::npos)
                    {
                        continue;
                    }

                    ProductionRuntimeAssetEntry entry = ParseEntry(trimmed, rule.Validation);
                    if (!entry.Directive.empty())
                    {
                        if (!rule.Validation.RequiredPrefix.empty() &&
                            entry.Directive == rule.Validation.RequiredPrefix &&
                            trimmed.find(rule.Validation.RequiredToken) != std::string::npos)
                        {
                            ++asset.RequiredEntryMatches;
                        }
                        asset.Entries.push_back(std::move(entry));
                    }
                }
            }

            asset.RuntimeReady = asset.Validation.Valid &&
                !asset.Entries.empty() &&
                asset.RequiredEntryMatches > 0 &&
                (rule.Validation.ActivationToken.empty() || asset.Validation.ActivationMatches > 0);
            catalog.push_back(std::move(asset));
        }

        return catalog;
    }

    ProductionRuntimeCatalogSummary SummarizeProductionRuntimeCatalog(
        const std::vector<ProductionRuntimeAsset>& catalog)
    {
        ProductionRuntimeCatalogSummary summary = {};
        std::set<std::string> domains;
        std::set<std::string> actions;
        summary.AssetCount = static_cast<uint32_t>(catalog.size());

        for (const ProductionRuntimeAsset& asset : catalog)
        {
            summary.ValidAssets += asset.Validation.Valid ? 1u : 0u;
            summary.RuntimeReadyAssets += asset.RuntimeReady ? 1u : 0u;
            summary.RequiredEntryMatches += asset.RequiredEntryMatches;
            summary.MissingFields += asset.Validation.MissingFields;
            summary.EntryCount += static_cast<uint32_t>(asset.Entries.size());
            summary.CombinedHash = HashCombine(summary.CombinedHash, asset.Validation.ContentHash);

            if (!asset.Domain.empty())
            {
                domains.insert(asset.Domain);
            }
            if (!asset.Action.empty())
            {
                actions.insert(asset.Action);
            }

            for (const ProductionRuntimeAssetEntry& entry : asset.Entries)
            {
                summary.FieldCount += static_cast<uint32_t>(entry.Fields.size());
                summary.ActivationEntries += entry.Activated ? 1u : 0u;
            }
        }

        summary.DomainCount = static_cast<uint32_t>(domains.size());
        summary.ActionCount = static_cast<uint32_t>(actions.size());
        return summary;
    }

    std::vector<ProductionRuntimeBinding> BuildProductionRuntimeBindings(
        const std::vector<ProductionRuntimeAsset>& catalog)
    {
        std::vector<ProductionRuntimeBinding> bindings;
        bindings.reserve(catalog.size());

        for (const ProductionRuntimeAsset& asset : catalog)
        {
            for (const ProductionRuntimeAssetEntry& entry : asset.Entries)
            {
                if (!entry.Activated)
                {
                    continue;
                }

                ProductionRuntimeBinding binding = {};
                binding.SourcePath = asset.Path;
                binding.Domain = asset.Domain;
                binding.Action = asset.Action;
                binding.Directive = entry.Directive;
                binding.Name = entry.Name.empty() ? asset.Action : entry.Name;
                binding.FieldCount = static_cast<uint32_t>(entry.Fields.size());
                binding.Activated = entry.Activated;
                binding.RuntimeReady = asset.RuntimeReady;
                bindings.push_back(std::move(binding));
            }
        }

        return bindings;
    }

    std::vector<ProductionRuntimeActionPlan> BuildProductionRuntimeActionPlans(
        const std::vector<ProductionRuntimeBinding>& bindings)
    {
        std::vector<ProductionRuntimeActionPlan> plans;
        plans.reserve(bindings.size());

        uint32_t stageIndex = 0;
        for (const ProductionRuntimeBinding& binding : bindings)
        {
            ProductionRuntimeActionPlan plan = {};
            plan.SourcePath = binding.SourcePath;
            plan.Domain = binding.Domain;
            plan.Action = binding.Action;
            plan.Name = binding.Name;
            plan.StageIndex = stageIndex++;
            plan.RuntimeReady = binding.RuntimeReady;
            plan.HighImpact = binding.Action == "objective_routes" ||
                binding.Action == "encounter_plan" ||
                binding.Action == "combat_sandbox" ||
                binding.Action == "render_budget_classes" ||
                binding.Action == "scheduler_budgets";
            plan.EditorVisible = binding.Domain == "Editor" ||
                binding.Action == "command_palette" ||
                binding.Action == "workspace_layouts" ||
                binding.Action == "viewport_bookmarks";
            plan.Playable = binding.Domain == "Game" ||
                binding.Action == "objective_routes" ||
                binding.Action == "encounter_plan" ||
                binding.Action == "combat_sandbox";
            plan.PriorityScore = binding.FieldCount * 2u +
                (binding.RuntimeReady ? 8u : 0u) +
                (plan.HighImpact ? 6u : 0u) +
                (plan.EditorVisible ? 3u : 0u) +
                (plan.Playable ? 4u : 0u);
            plans.push_back(std::move(plan));
        }

        std::stable_sort(
            plans.begin(),
            plans.end(),
            [](const ProductionRuntimeActionPlan& left, const ProductionRuntimeActionPlan& right)
            {
                return left.PriorityScore > right.PriorityScore;
            });
        return plans;
    }

    std::vector<ProductionRuntimeMutationPlan> BuildProductionRuntimeMutationPlans(
        const std::vector<ProductionRuntimeActionPlan>& actionPlans)
    {
        std::vector<ProductionRuntimeMutationPlan> plans;
        plans.reserve(actionPlans.size());

        for (const ProductionRuntimeActionPlan& actionPlan : actionPlans)
        {
            ProductionRuntimeMutationPlan mutation = {};
            mutation.SourcePath = actionPlan.SourcePath;
            mutation.Domain = actionPlan.Domain;
            mutation.Action = actionPlan.Action;
            mutation.Name = actionPlan.Name;
            mutation.MutationTarget = MutationTargetForAction(actionPlan);
            mutation.StageIndex = actionPlan.StageIndex;
            mutation.MutatesRuntime = actionPlan.Domain == "Engine" ||
                actionPlan.Action == "event_trace_channels" ||
                actionPlan.Action == "scheduler_budgets" ||
                actionPlan.Action == "streaming_budgets" ||
                actionPlan.Action == "render_budget_classes";
            mutation.MutatesEditor = actionPlan.Domain == "Editor" ||
                actionPlan.Action == "workspace_layouts" ||
                actionPlan.Action == "command_palette" ||
                actionPlan.Action == "viewport_bookmarks";
            mutation.MutatesGameplay = actionPlan.Domain == "Game" ||
                actionPlan.Action == "objective_routes" ||
                actionPlan.Action == "encounter_plan" ||
                actionPlan.Action == "combat_sandbox";
            mutation.BudgetBound = actionPlan.Action == "scheduler_budgets" ||
                actionPlan.Action == "streaming_budgets" ||
                actionPlan.Action == "render_budget_classes";
            mutation.MutationCost = actionPlan.PriorityScore +
                (mutation.MutatesRuntime ? 3u : 0u) +
                (mutation.MutatesEditor ? 2u : 0u) +
                (mutation.MutatesGameplay ? 4u : 0u) +
                (mutation.BudgetBound ? 5u : 0u);
            plans.push_back(std::move(mutation));
        }

        std::stable_sort(
            plans.begin(),
            plans.end(),
            [](const ProductionRuntimeMutationPlan& left, const ProductionRuntimeMutationPlan& right)
            {
                return left.MutationCost > right.MutationCost;
            });
        return plans;
    }

    ProductionRuntimeCatalogDiagnostics DiagnoseProductionRuntimeCatalog(
        const std::vector<ProductionRuntimeAsset>& catalog)
    {
        ProductionRuntimeCatalogDiagnostics diagnostics = {};
        const std::vector<ProductionRuntimeBinding> bindings = BuildProductionRuntimeBindings(catalog);
        diagnostics.BindingCount = static_cast<uint32_t>(bindings.size());

        for (const ProductionRuntimeBinding& binding : bindings)
        {
            diagnostics.RuntimeReadyBindings += binding.RuntimeReady ? 1u : 0u;
            diagnostics.ActiveBindings += binding.Activated ? 1u : 0u;
            diagnostics.EngineBindings += binding.Domain == "Engine" ? 1u : 0u;
            diagnostics.EditorBindings += binding.Domain == "Editor" ? 1u : 0u;
            diagnostics.GameBindings += binding.Domain == "Game" ? 1u : 0u;
            diagnostics.EntriesWithFields += binding.FieldCount > 0 ? 1u : 0u;
            diagnostics.MaxFieldsPerEntry = std::max(diagnostics.MaxFieldsPerEntry, binding.FieldCount);
        }

        return diagnostics;
    }

    ProductionRuntimeActionPlanSummary SummarizeProductionRuntimeActionPlans(
        const std::vector<ProductionRuntimeActionPlan>& plans)
    {
        ProductionRuntimeActionPlanSummary summary = {};
        summary.ActionPlanCount = static_cast<uint32_t>(plans.size());
        for (const ProductionRuntimeActionPlan& plan : plans)
        {
            summary.RuntimeReadyPlans += plan.RuntimeReady ? 1u : 0u;
            summary.EnginePlans += plan.Domain == "Engine" ? 1u : 0u;
            summary.EditorPlans += plan.Domain == "Editor" ? 1u : 0u;
            summary.GamePlans += plan.Domain == "Game" ? 1u : 0u;
            summary.HighImpactPlans += plan.HighImpact ? 1u : 0u;
            summary.EditorVisiblePlans += plan.EditorVisible ? 1u : 0u;
            summary.PlayablePlans += plan.Playable ? 1u : 0u;
            summary.MaxPriorityScore = std::max(summary.MaxPriorityScore, plan.PriorityScore);
        }
        return summary;
    }

    ProductionRuntimeMutationPlanSummary SummarizeProductionRuntimeMutationPlans(
        const std::vector<ProductionRuntimeMutationPlan>& plans)
    {
        ProductionRuntimeMutationPlanSummary summary = {};
        summary.MutationPlanCount = static_cast<uint32_t>(plans.size());
        for (const ProductionRuntimeMutationPlan& plan : plans)
        {
            summary.RuntimeMutationPlans += plan.MutatesRuntime ? 1u : 0u;
            summary.EditorMutationPlans += plan.MutatesEditor ? 1u : 0u;
            summary.GameplayMutationPlans += plan.MutatesGameplay ? 1u : 0u;
            summary.BudgetBoundPlans += plan.BudgetBound ? 1u : 0u;
            summary.MaxMutationCost = std::max(summary.MaxMutationCost, plan.MutationCost);
        }
        return summary;
    }

    const ProductionRuntimeAssetField* FindProductionRuntimeField(
        const ProductionRuntimeAssetEntry& entry,
        const std::string& key)
    {
        const auto match = std::find_if(
            entry.Fields.begin(),
            entry.Fields.end(),
            [&key](const ProductionRuntimeAssetField& field)
            {
                return field.Key == key;
            });
        return match != entry.Fields.end() ? &(*match) : nullptr;
    }
}
