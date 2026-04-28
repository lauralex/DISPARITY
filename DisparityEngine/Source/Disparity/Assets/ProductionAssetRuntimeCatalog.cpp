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
