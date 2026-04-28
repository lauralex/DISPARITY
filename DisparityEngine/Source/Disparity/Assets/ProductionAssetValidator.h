#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Disparity
{
    struct ProductionAssetValidationRule
    {
        std::filesystem::path Path;
        std::string RequiredPrefix;
        std::string RequiredToken;
        std::string ActivationToken;
    };

    struct ProductionAssetValidationResult
    {
        std::filesystem::path Path;
        uint32_t LineCount = 0;
        uint32_t DirectiveCount = 0;
        uint32_t RequiredMatches = 0;
        uint32_t ActivationMatches = 0;
        uint32_t MissingFields = 0;
        uint64_t ContentHash = 0;
        bool Loaded = false;
        bool Valid = false;
    };

    struct ProductionAssetValidationSummary
    {
        uint32_t AssetCount = 0;
        uint32_t LoadedAssets = 0;
        uint32_t ValidAssets = 0;
        uint32_t DirectiveCount = 0;
        uint32_t ActivationCount = 0;
        uint32_t MissingFields = 0;
        uint64_t CombinedHash = 1469598103934665603ull;
    };

    [[nodiscard]] ProductionAssetValidationResult ValidateProductionAsset(
        const ProductionAssetValidationRule& rule);
    [[nodiscard]] ProductionAssetValidationSummary SummarizeProductionAssetValidation(
        const std::vector<ProductionAssetValidationResult>& results);
}
