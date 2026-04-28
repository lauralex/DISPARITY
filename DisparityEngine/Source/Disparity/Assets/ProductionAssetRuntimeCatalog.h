#pragma once

#include "Disparity/Assets/ProductionAssetValidator.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Disparity
{
    struct ProductionRuntimeAssetRule
    {
        ProductionAssetValidationRule Validation;
        std::string Domain;
        std::string Action;
    };

    struct ProductionRuntimeAssetField
    {
        std::string Key;
        std::string Value;
    };

    struct ProductionRuntimeAssetEntry
    {
        std::string Directive;
        std::string Name;
        std::vector<ProductionRuntimeAssetField> Fields;
        bool Activated = false;
    };

    struct ProductionRuntimeAsset
    {
        std::filesystem::path Path;
        std::string Domain;
        std::string Action;
        ProductionAssetValidationResult Validation;
        std::vector<ProductionRuntimeAssetEntry> Entries;
        uint32_t RequiredEntryMatches = 0;
        bool RuntimeReady = false;
    };

    struct ProductionRuntimeCatalogSummary
    {
        uint32_t AssetCount = 0;
        uint32_t ValidAssets = 0;
        uint32_t RuntimeReadyAssets = 0;
        uint32_t EntryCount = 0;
        uint32_t FieldCount = 0;
        uint32_t ActivationEntries = 0;
        uint32_t DomainCount = 0;
        uint32_t ActionCount = 0;
        uint32_t RequiredEntryMatches = 0;
        uint32_t MissingFields = 0;
        uint64_t CombinedHash = 1469598103934665603ull;
    };

    [[nodiscard]] std::vector<ProductionRuntimeAsset> LoadProductionRuntimeCatalog(
        const std::vector<ProductionRuntimeAssetRule>& rules);
    [[nodiscard]] ProductionRuntimeCatalogSummary SummarizeProductionRuntimeCatalog(
        const std::vector<ProductionRuntimeAsset>& catalog);
}
