#pragma once

#include "DisparityGame/GameRuntimeTypes.h"
#include <Disparity/Disparity.h>

#include <cstddef>
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

    struct ProductionCatalogPreviewState
    {
        size_t SelectedBindingIndex = 0;
        uint32_t PreviewRequests = 0;
        uint32_t PreviewCycles = 0;
        uint32_t ClearRequests = 0;
        uint32_t DetailViews = 0;
        uint32_t FocusedBeaconDraws = 0;
        uint32_t ExecuteRequests = 0;
        uint32_t StopRequests = 0;
        uint32_t ExecutionPulses = 0;
        uint32_t ExecutionDetailRows = 0;
        uint32_t WorldExecutionMarkers = 0;
        uint32_t ActionRouteBeams = 0;
        bool PreviewActive = true;
        bool ExecutionActive = false;
    };

    [[nodiscard]] std::vector<Disparity::ProductionRuntimeAssetRule> BuildProductionRuntimeCatalogRules();
    [[nodiscard]] ProductionCatalogSnapshot BuildProductionCatalogSnapshot();
    [[nodiscard]] uint32_t CountProductionCatalogBindingsByAction(
        const ProductionCatalogSnapshot& snapshot,
        const std::string& action);

    void ApplyProductionCatalogSnapshotStats(
        const ProductionCatalogSnapshot& snapshot,
        EditorVerificationStats& stats);
    void PrimeProductionCatalogPreview(
        const ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats);
    void RefreshProductionCatalogPreview(
        ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats);
    void ApplyProductionCatalogPreviewStats(
        const ProductionCatalogSnapshot& snapshot,
        const ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats);
    void ExecuteProductionCatalogPreview(
        const ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats);
    void StopProductionCatalogExecution(
        ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats);
    [[nodiscard]] bool DrawProductionCatalogSnapshotPanel(
        const ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& preview,
        EditorVerificationStats& stats);
    [[nodiscard]] uint32_t DrawProductionCatalogWorldBeacons(
        Disparity::Renderer& renderer,
        const ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& preview,
        EditorVerificationStats& stats,
        float visualTime,
        const DirectX::XMFLOAT3& center,
        const Disparity::Material& baseMaterial,
        Disparity::MeshHandle mesh);
}
