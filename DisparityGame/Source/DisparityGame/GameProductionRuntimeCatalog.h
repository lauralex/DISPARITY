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
        std::vector<Disparity::ProductionRuntimeActionPlan> ActionPlans;
        std::vector<Disparity::ProductionRuntimeMutationPlan> MutationPlans;
        Disparity::ProductionRuntimeCatalogSummary Summary;
        Disparity::ProductionRuntimeCatalogDiagnostics Diagnostics;
        Disparity::ProductionRuntimeActionPlanSummary ActionPlanSummary;
        Disparity::ProductionRuntimeMutationPlanSummary MutationPlanSummary;
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
        uint32_t ActionDirectorRequests = 0;
        uint32_t ActionDirectorQueueDepth = 0;
        uint32_t ActionDirectorHistoryRows = 0;
        uint32_t DirectorCinematicBursts = 0;
        uint32_t DirectorRouteRibbons = 0;
        uint32_t DirectorEncounterGhosts = 0;
        uint32_t DirectorEditorQueueRows = 0;
        uint32_t DirectorPlanSummaryRows = 0;
        uint32_t ActionMutationRequests = 0;
        uint32_t MutationQueueDepth = 0;
        uint32_t EngineBudgetMutations = 0;
        uint32_t EngineSchedulerBudgetMutations = 0;
        uint32_t EngineStreamingBudgetMutations = 0;
        uint32_t EngineRenderBudgetMutations = 0;
        uint32_t EditorWorkspaceMutations = 0;
        uint32_t EditorCommandMutations = 0;
        uint32_t EditorTraceEventRows = 0;
        uint32_t GameSpawnedEncounterWaves = 0;
        uint32_t GameObjectiveRouteMutations = 0;
        uint32_t GameCombatSandboxMutations = 0;
        uint32_t MutationWorldBursts = 0;
        uint32_t MutationWorldPillars = 0;
        uint32_t MutationWaveGhosts = 0;
        uint32_t MutationPanelRows = 0;
        uint32_t PhysicsPreviewFrames = 0;
        uint32_t PhysicsVisibleBodies = 0;
        uint32_t PhysicsPanelRows = 0;
        bool PhysicsPreviewInitialized = false;
        bool PreviewActive = true;
        bool ExecutionActive = false;
        bool DirectorActive = false;
        bool MutationsActive = false;
        std::string ActiveWorkspacePreset;
        std::string ActiveCommandName;
        std::string ActiveTraceChannel;
        Disparity::PhysicsWorld PhysicsPreviewWorld;
        Disparity::PhysicsCharacterControllerState PhysicsCharacter;
        Disparity::PhysicsWorldDiagnostics PhysicsDiagnostics;
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
