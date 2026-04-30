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

    struct ProductionRuntimeBinding
    {
        std::filesystem::path SourcePath;
        std::string Domain;
        std::string Action;
        std::string Directive;
        std::string Name;
        uint32_t FieldCount = 0;
        bool Activated = false;
        bool RuntimeReady = false;
    };

    struct ProductionRuntimeActionPlan
    {
        std::filesystem::path SourcePath;
        std::string Domain;
        std::string Action;
        std::string Name;
        uint32_t StageIndex = 0;
        uint32_t PriorityScore = 0;
        bool RuntimeReady = false;
        bool HighImpact = false;
        bool EditorVisible = false;
        bool Playable = false;
    };

    struct ProductionRuntimeCatalogDiagnostics
    {
        uint32_t BindingCount = 0;
        uint32_t RuntimeReadyBindings = 0;
        uint32_t ActiveBindings = 0;
        uint32_t EngineBindings = 0;
        uint32_t EditorBindings = 0;
        uint32_t GameBindings = 0;
        uint32_t EntriesWithFields = 0;
        uint32_t MaxFieldsPerEntry = 0;
    };

    struct ProductionRuntimeActionPlanSummary
    {
        uint32_t ActionPlanCount = 0;
        uint32_t RuntimeReadyPlans = 0;
        uint32_t EnginePlans = 0;
        uint32_t EditorPlans = 0;
        uint32_t GamePlans = 0;
        uint32_t HighImpactPlans = 0;
        uint32_t EditorVisiblePlans = 0;
        uint32_t PlayablePlans = 0;
        uint32_t MaxPriorityScore = 0;
    };

    struct ProductionRuntimeMutationPlan
    {
        std::filesystem::path SourcePath;
        std::string Domain;
        std::string Action;
        std::string Name;
        std::string MutationTarget;
        uint32_t StageIndex = 0;
        uint32_t MutationCost = 0;
        bool MutatesRuntime = false;
        bool MutatesEditor = false;
        bool MutatesGameplay = false;
        bool BudgetBound = false;
    };

    struct ProductionRuntimeMutationPlanSummary
    {
        uint32_t MutationPlanCount = 0;
        uint32_t RuntimeMutationPlans = 0;
        uint32_t EditorMutationPlans = 0;
        uint32_t GameplayMutationPlans = 0;
        uint32_t BudgetBoundPlans = 0;
        uint32_t MaxMutationCost = 0;
    };

    [[nodiscard]] std::vector<ProductionRuntimeAsset> LoadProductionRuntimeCatalog(
        const std::vector<ProductionRuntimeAssetRule>& rules);
    [[nodiscard]] ProductionRuntimeCatalogSummary SummarizeProductionRuntimeCatalog(
        const std::vector<ProductionRuntimeAsset>& catalog);
    [[nodiscard]] std::vector<ProductionRuntimeBinding> BuildProductionRuntimeBindings(
        const std::vector<ProductionRuntimeAsset>& catalog);
    [[nodiscard]] std::vector<ProductionRuntimeActionPlan> BuildProductionRuntimeActionPlans(
        const std::vector<ProductionRuntimeBinding>& bindings);
    [[nodiscard]] std::vector<ProductionRuntimeMutationPlan> BuildProductionRuntimeMutationPlans(
        const std::vector<ProductionRuntimeActionPlan>& actionPlans);
    [[nodiscard]] ProductionRuntimeCatalogDiagnostics DiagnoseProductionRuntimeCatalog(
        const std::vector<ProductionRuntimeAsset>& catalog);
    [[nodiscard]] ProductionRuntimeActionPlanSummary SummarizeProductionRuntimeActionPlans(
        const std::vector<ProductionRuntimeActionPlan>& plans);
    [[nodiscard]] ProductionRuntimeMutationPlanSummary SummarizeProductionRuntimeMutationPlans(
        const std::vector<ProductionRuntimeMutationPlan>& plans);
    [[nodiscard]] const ProductionRuntimeAssetField* FindProductionRuntimeField(
        const ProductionRuntimeAssetEntry& entry,
        const std::string& key);
}
