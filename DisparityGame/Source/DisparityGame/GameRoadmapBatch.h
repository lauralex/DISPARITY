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
