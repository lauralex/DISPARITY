#pragma once

#include "DisparityGame/GameRuntimeTypes.h"
#include "DisparityGame/GameEventRouteCatalog.h"
#include "Disparity/Editor/EditorPanelRegistry.h"
#include "Disparity/Runtime/RuntimeCommandRegistry.h"

#include <array>
#include <cstdint>
#include <iosfwd>

namespace DisparityGame
{
    struct V39RoadmapMetrics
    {
        uint32_t RuntimeCommandHistoryEntries = 0;
        uint32_t RuntimeCommandBindingConflicts = 0;
        uint32_t RuntimeCommandCategorySummaries = 0;
        uint32_t RuntimeCommandRequiredCategoriesSatisfied = 0;
        uint32_t EditorSavedDockLayouts = 0;
        uint32_t EditorTeamDefaultWorkspaces = 0;
        uint32_t EditorWorkspaceCommandBindings = 0;
        uint32_t EditorControllerNavigationHints = 0;
        uint32_t EditorToolbarCustomizationSlots = 0;
        uint32_t GameEventRouteReplayableRoutes = 0;
        uint32_t GameEventRouteSelectionLinkedRoutes = 0;
        uint32_t GameEventRouteBreadcrumbRoutes = 0;
        uint32_t GameEventRouteTraceChannels = 0;
        uint32_t GameEventRouteFailureRoutes = 0;
        uint32_t GameEventRouteObjectiveRoutes = 0;
        bool VersionReady = false;
        bool BaselineReady = false;
        bool SchemaReady = false;
        bool RuntimeWrapperReady = false;
    };

    struct V40DiversifiedMetrics
    {
        uint32_t RuntimeCommandHistorySuccesses = 0;
        uint32_t RuntimeCommandHistoryFailures = 0;
        uint32_t RuntimeCommandBoundCommands = 0;
        uint32_t RuntimeCommandUniqueBindings = 0;
        uint32_t RuntimeCommandDocumentedCommands = 0;
        uint32_t RuntimeCommandBindingConflicts = 0;
        uint32_t EditorWorkspaceMigrationReady = 0;
        uint32_t EditorWorkspaceFocusTargets = 0;
        uint32_t EditorWorkspaceGamepadReady = 0;
        uint32_t EditorWorkspaceToolbarProfiles = 0;
        uint32_t EditorWorkspaceCommandRoutes = 0;
        uint32_t GameEventRouteCheckpointLinks = 0;
        uint32_t GameEventRouteSaveRelevantRoutes = 0;
        uint32_t GameEventRouteChapterReplayRoutes = 0;
        uint32_t GameEventRouteAccessibilityRoutes = 0;
        uint32_t GameEventRouteHudVisibleRoutes = 0;
    };

    struct V41BreadthMetrics
    {
        uint32_t EngineEventBusFlushes = 0;
        uint32_t EngineSchedulerPhaseCoverage = 0;
        uint32_t EngineSceneQueryRaycasts = 0;
        uint32_t EngineStreamingScheduledRequests = 0;
        uint32_t EngineRenderGraphBudgetChecks = 0;
        uint32_t EngineModuleSmokeTests = 0;
        uint32_t EditorPickTests = 0;
        uint32_t EditorGizmoPickTests = 0;
        uint32_t EditorViewportCaptureTests = 0;
        uint32_t EditorPreferenceToolbarTests = 0;
        uint32_t EditorTransformHistoryTests = 0;
        uint32_t EditorShotWorkflowTests = 0;
        uint32_t GameObjectiveStages = 0;
        uint32_t GameTraversalCollisionTests = 0;
        uint32_t GameEnemyArchetypeTests = 0;
        uint32_t GameInputFailureTests = 0;
        uint32_t GameAudioAnimationTests = 0;
        bool SchemaReady = false;
        bool BaselineReady = false;
        bool ReleaseReady = false;
        bool PerformanceHistoryReady = false;
        bool DocsReady = false;
    };

    struct V42ProductionSurfaceMetrics
    {
        std::array<uint32_t, 6> EngineAssets = {};
        std::array<uint32_t, 6> EditorAssets = {};
        std::array<uint32_t, 6> GameAssets = {};
        std::array<uint32_t, 6> VerificationAssets = {};
    };

    struct V43LiveValidationMetrics
    {
        std::array<uint32_t, 6> EngineLiveAssets = {};
        std::array<uint32_t, 6> EditorEditableAssets = {};
        std::array<uint32_t, 6> GamePlayableAssets = {};
        std::array<uint32_t, 6> VerificationAssets = {};
    };

    struct V44RuntimeCatalogMetrics
    {
        std::array<uint32_t, 6> EngineCatalogAssets = {};
        std::array<uint32_t, 6> EditorCatalogAssets = {};
        std::array<uint32_t, 6> GameCatalogAssets = {};
        std::array<uint32_t, 6> VerificationAssets = {};
    };

    struct V45LiveCatalogMetrics
    {
        uint32_t RuntimeBindings = 0;
        uint32_t RuntimeReadyBindings = 0;
        uint32_t EngineBindings = 0;
        uint32_t EditorBindings = 0;
        uint32_t GameBindings = 0;
        uint32_t CatalogPanelRows = 0;
        uint32_t VisibleBeacons = 0;
        uint32_t ObjectiveBindings = 0;
        uint32_t EncounterBindings = 0;
        uint32_t NegativeFixtureTests = 0;
        std::array<uint32_t, 6> VerificationAssets = {};
    };

    struct V46CatalogActionPreviewMetrics
    {
        uint32_t SelectableRows = 0;
        uint32_t PreviewSelections = 0;
        uint32_t PreviewCycles = 0;
        uint32_t PreviewClears = 0;
        uint32_t PreviewDetails = 0;
        uint32_t FocusedBeacons = 0;
        uint32_t EnginePreviewBindings = 0;
        uint32_t EditorPreviewBindings = 0;
        uint32_t GamePreviewBindings = 0;
        uint32_t RuntimeActionCommands = 0;
        std::array<uint32_t, 6> VerificationAssets = {};
    };

    struct V47CatalogExecutionMetrics
    {
        uint32_t ExecuteRequests = 0;
        uint32_t ExecutionStops = 0;
        uint32_t ExecutionPulses = 0;
        uint32_t EngineExecutableBindings = 0;
        uint32_t EditorExecutableBindings = 0;
        uint32_t GameExecutableBindings = 0;
        uint32_t EngineExecutionOverlays = 0;
        uint32_t EditorExecutionOverlays = 0;
        uint32_t GameExecutionOverlays = 0;
        uint32_t WorldExecutionMarkers = 0;
        uint32_t ActionRouteBeams = 0;
        uint32_t ExecutionDetailRows = 0;
        std::array<uint32_t, 6> VerificationAssets = {};
    };

    struct V48ActionDirectorMetrics
    {
        uint32_t RuntimeActionPlans = 0;
        uint32_t RuntimeReadyActionPlans = 0;
        uint32_t HighImpactActionPlans = 0;
        uint32_t EditorVisibleActionPlans = 0;
        uint32_t PlayableActionPlans = 0;
        uint32_t ActionDirectorRequests = 0;
        uint32_t ActionDirectorQueueDepth = 0;
        uint32_t ActionDirectorHistoryRows = 0;
        uint32_t DirectorCinematicBursts = 0;
        uint32_t DirectorRouteRibbons = 0;
        uint32_t DirectorEncounterGhosts = 0;
        uint32_t DirectorEditorQueueRows = 0;
        uint32_t DirectorPlanSummaryRows = 0;
        std::array<uint32_t, 6> VerificationAssets = {};
    };

    struct V49ActionMutationMetrics
    {
        uint32_t RuntimeMutationPlans = 0;
        uint32_t RuntimeMutationRuntimePlans = 0;
        uint32_t EditorMutationPlans = 0;
        uint32_t GameplayMutationPlans = 0;
        uint32_t BudgetBoundMutationPlans = 0;
        uint32_t ActionMutationRequests = 0;
        uint32_t MutationQueueDepth = 0;
        uint32_t EngineBudgetMutations = 0;
        uint32_t SchedulerBudgetMutations = 0;
        uint32_t StreamingBudgetMutations = 0;
        uint32_t RenderBudgetMutations = 0;
        uint32_t EditorWorkspaceMutations = 0;
        uint32_t EditorCommandMutations = 0;
        uint32_t TraceEventRows = 0;
        uint32_t GameSpawnedEncounterWaves = 0;
        uint32_t GameObjectiveRouteMutations = 0;
        uint32_t GameCombatSandboxMutations = 0;
        uint32_t MutationWorldBursts = 0;
        uint32_t MutationWorldPillars = 0;
        uint32_t MutationWaveGhosts = 0;
        uint32_t MutationPanelRows = 0;
        std::array<uint32_t, 6> VerificationAssets = {};
    };

    struct V50PhysicsFoundationMetrics
    {
        uint32_t Bodies = 0;
        uint32_t DynamicBodies = 0;
        uint32_t StaticBodies = 0;
        uint32_t TriggerBodies = 0;
        uint32_t Steps = 0;
        uint32_t Substeps = 0;
        uint32_t Contacts = 0;
        uint32_t TriggerOverlaps = 0;
        uint32_t Raycasts = 0;
        uint32_t Sweeps = 0;
        uint32_t Overlaps = 0;
        uint32_t CharacterMoves = 0;
        uint32_t CharacterGroundedMoves = 0;
        uint32_t DebugLines = 0;
        uint32_t VisibleBodies = 0;
        uint32_t PanelRows = 0;
        std::array<uint32_t, 6> VerificationAssets = {};
    };

    [[nodiscard]] std::array<uint32_t, V39RoadmapPointCount> EvaluateV39RoadmapBatch(
        const V39RoadmapMetrics& metrics,
        const RuntimeBaseline& baseline);
    [[nodiscard]] uint32_t CountReadyV39RoadmapPoints(const std::array<uint32_t, V39RoadmapPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V40DiversifiedPointCount> EvaluateV40DiversifiedBatch(
        const V40DiversifiedMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV40DiversifiedPoints(const std::array<uint32_t, V40DiversifiedPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V41BreadthPointCount> EvaluateV41BreadthBatch(
        const V41BreadthMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV41BreadthPoints(const std::array<uint32_t, V41BreadthPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V42ProductionSurfacePointCount> EvaluateV42ProductionSurface(
        const V42ProductionSurfaceMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV42ProductionSurfacePoints(
        const std::array<uint32_t, V42ProductionSurfacePointCount>& results);
    [[nodiscard]] std::array<uint32_t, V43LiveValidationPointCount> EvaluateV43LiveValidation(
        const V43LiveValidationMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV43LiveValidationPoints(
        const std::array<uint32_t, V43LiveValidationPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V44RuntimeCatalogPointCount> EvaluateV44RuntimeCatalog(
        const V44RuntimeCatalogMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV44RuntimeCatalogPoints(
        const std::array<uint32_t, V44RuntimeCatalogPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V45LiveCatalogPointCount> EvaluateV45LiveCatalog(
        const V45LiveCatalogMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV45LiveCatalogPoints(
        const std::array<uint32_t, V45LiveCatalogPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V46CatalogActionPreviewPointCount> EvaluateV46CatalogActionPreview(
        const V46CatalogActionPreviewMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV46CatalogActionPreviewPoints(
        const std::array<uint32_t, V46CatalogActionPreviewPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V47CatalogExecutionPointCount> EvaluateV47CatalogExecution(
        const V47CatalogExecutionMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV47CatalogExecutionPoints(
        const std::array<uint32_t, V47CatalogExecutionPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V48ActionDirectorPointCount> EvaluateV48ActionDirector(
        const V48ActionDirectorMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV48ActionDirectorPoints(
        const std::array<uint32_t, V48ActionDirectorPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V49ActionMutationPointCount> EvaluateV49ActionMutation(
        const V49ActionMutationMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV49ActionMutationPoints(
        const std::array<uint32_t, V49ActionMutationPointCount>& results);
    [[nodiscard]] std::array<uint32_t, V50PhysicsFoundationPointCount> EvaluateV50PhysicsFoundation(
        const V50PhysicsFoundationMetrics& metrics);
    [[nodiscard]] uint32_t CountReadyV50PhysicsFoundationPoints(
        const std::array<uint32_t, V50PhysicsFoundationPointCount>& results);
    void CaptureV39RoadmapStats(
        EditorVerificationStats& stats,
        const Disparity::RuntimeCommandRegistryDiagnostics& commandDiagnostics,
        const Disparity::EditorPanelRegistryDiagnostics& panelDiagnostics,
        const GameEventRouteDiagnostics& routeDiagnostics);
    [[nodiscard]] V39RoadmapMetrics BuildV39RoadmapMetrics(
        const EditorVerificationStats& stats,
        const GameEventRouteDiagnostics& routeDiagnostics,
        bool versionReady,
        bool baselineReady,
        bool schemaReady,
        bool runtimeWrapperReady);
    void WriteV39RoadmapReport(
        std::ostream& report,
        const EditorVerificationStats& stats,
        const std::array<uint32_t, V39RoadmapPointCount>& results);
}
