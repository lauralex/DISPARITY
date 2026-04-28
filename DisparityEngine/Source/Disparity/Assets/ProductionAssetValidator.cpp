#include "Disparity/Assets/ProductionAssetValidator.h"
#include "Disparity/Core/FileSystem.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace Disparity
{
    namespace
    {
        constexpr uint64_t FnvOffset = 1469598103934665603ull;
        constexpr uint64_t FnvPrime = 1099511628211ull;

        [[nodiscard]] uint64_t HashText(const std::string& text)
        {
            uint64_t hash = FnvOffset;
            for (const char value : text)
            {
                hash ^= static_cast<unsigned char>(value);
                hash *= FnvPrime;
            }
            return hash;
        }

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
    }

    ProductionAssetValidationResult ValidateProductionAsset(const ProductionAssetValidationRule& rule)
    {
        ProductionAssetValidationResult result = {};
        result.Path = rule.Path;

        std::string text;
        result.Loaded = FileSystem::ReadTextFile(rule.Path, text);
        result.ContentHash = HashText(text);
        if (!result.Loaded)
        {
            result.MissingFields = 1;
            return result;
        }

        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line))
        {
            ++result.LineCount;

            const std::string trimmed = Trim(line);
            if (trimmed.empty() || StartsWith(trimmed, "#"))
            {
                continue;
            }

            ++result.DirectiveCount;
            if (!rule.RequiredPrefix.empty() &&
                StartsWith(trimmed, rule.RequiredPrefix) &&
                trimmed.find(rule.RequiredToken) != std::string::npos)
            {
                ++result.RequiredMatches;
            }

            if (!rule.ActivationToken.empty() &&
                trimmed.find(rule.ActivationToken) != std::string::npos)
            {
                ++result.ActivationMatches;
            }
        }

        if (result.DirectiveCount == 0)
        {
            ++result.MissingFields;
        }
        if (!rule.RequiredToken.empty() && result.RequiredMatches == 0)
        {
            ++result.MissingFields;
        }
        if (!rule.ActivationToken.empty() && result.ActivationMatches == 0)
        {
            ++result.MissingFields;
        }

        result.Valid = result.MissingFields == 0;
        return result;
    }

    ProductionAssetValidationSummary SummarizeProductionAssetValidation(
        const std::vector<ProductionAssetValidationResult>& results)
    {
        ProductionAssetValidationSummary summary = {};
        summary.AssetCount = static_cast<uint32_t>(results.size());

        for (const ProductionAssetValidationResult& result : results)
        {
            summary.LoadedAssets += result.Loaded ? 1u : 0u;
            summary.ValidAssets += result.Valid ? 1u : 0u;
            summary.DirectiveCount += result.DirectiveCount;
            summary.ActivationCount += result.ActivationMatches;
            summary.MissingFields += result.MissingFields;
            summary.CombinedHash = HashCombine(summary.CombinedHash, result.ContentHash);
        }

        return summary;
    }
}
