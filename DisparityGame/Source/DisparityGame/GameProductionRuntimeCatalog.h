#pragma once

#include "DisparityGame/GameRuntimeTypes.h"
#include <Disparity/Disparity.h>

#include <cstdint>
#include <string>
#include <vector>

namespace DisparityGame
{
    struct ProductionCatalogSnapshot
    {
        std::vector<Disparity::ProductionRuntimeAsset> Assets;
        std::vector<Disparity::ProductionRuntimeBinding> Bindings;
        Disparity::ProductionRuntimeCatalogSummary Summary;
        Disparity::ProductionRuntimeCatalogDiagnostics Diagnostics;
        uint32_t NegativeFixtureRejected = 0;
    };

    [[nodiscard]] std::vector<Disparity::ProductionRuntimeAssetRule> BuildProductionRuntimeCatalogRules();
    [[nodiscard]] ProductionCatalogSnapshot BuildProductionCatalogSnapshot();
    [[nodiscard]] uint32_t CountProductionCatalogBindingsByAction(
        const ProductionCatalogSnapshot& snapshot,
        const std::string& action);

    void ApplyProductionCatalogSnapshotStats(
        const ProductionCatalogSnapshot& snapshot,
        EditorVerificationStats& stats);
    [[nodiscard]] bool DrawProductionCatalogSnapshotPanel(
        const ProductionCatalogSnapshot& snapshot,
        EditorVerificationStats& stats);
    [[nodiscard]] uint32_t DrawProductionCatalogWorldBeacons(
        Disparity::Renderer& renderer,
        const ProductionCatalogSnapshot& snapshot,
        float visualTime,
        const DirectX::XMFLOAT3& center,
        const Disparity::Material& baseMaterial,
        Disparity::MeshHandle mesh);
}
