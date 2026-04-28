#include "DisparityGame/GameRoadmapBatch.h"
#include "DisparityGame/GameFollowupCatalog.h"

#include <algorithm>
#include <ostream>

namespace DisparityGame
{
    std::array<uint32_t, V39RoadmapPointCount> EvaluateV39RoadmapBatch(
        const V39RoadmapMetrics& metrics,
        const RuntimeBaseline& baseline)
    {
        std::array<uint32_t, V39RoadmapPointCount> results = {};
        results[0] = metrics.RuntimeCommandHistoryEntries >= baseline.MinRuntimeCommandHistoryEntries ? 1u : 0u;
        results[1] = metrics.RuntimeCommandBindingConflicts >= baseline.MinRuntimeCommandBindingConflicts ? 1u : 0u;
        results[2] = metrics.RuntimeCommandCategorySummaries >= 4 ? 1u : 0u;
        results[3] = metrics.RuntimeCommandRequiredCategoriesSatisfied >= 4 ? 1u : 0u;
        results[4] = results[0] != 0u && results[1] != 0u ? 1u : 0u;
        results[5] = metrics.EditorSavedDockLayouts >= baseline.MinEditorDockLayoutDescriptors ? 1u : 0u;
        results[6] = metrics.EditorTeamDefaultWorkspaces >= baseline.MinEditorTeamDefaultWorkspaces ? 1u : 0u;
        results[7] = metrics.EditorWorkspaceCommandBindings >= 6 ? 1u : 0u;
        results[8] = metrics.EditorControllerNavigationHints >= 4 ? 1u : 0u;
        results[9] = metrics.EditorToolbarCustomizationSlots >= 10 ? 1u : 0u;
        results[10] = metrics.GameEventRouteBreadcrumbRoutes >= baseline.MinGameReplayableEventRoutes &&
            metrics.GameEventRouteReplayableRoutes >= baseline.MinGameReplayableEventRoutes ? 1u : 0u;
        results[11] = metrics.GameEventRouteSelectionLinkedRoutes >= 6 ? 1u : 0u;
        results[12] = metrics.GameEventRouteTraceChannels >= 5 ? 1u : 0u;
        results[13] = metrics.GameEventRouteFailureRoutes >= 3 && metrics.GameEventRouteObjectiveRoutes >= 6 ? 1u : 0u;
        results[14] = metrics.VersionReady && metrics.BaselineReady && metrics.SchemaReady && metrics.RuntimeWrapperReady ? 1u : 0u;
        return results;
    }

    uint32_t CountReadyV39RoadmapPoints(const std::array<uint32_t, V39RoadmapPointCount>& results)
    {
        return static_cast<uint32_t>(std::count(results.begin(), results.end(), 1u));
    }

    void CaptureV39RoadmapStats(
        EditorVerificationStats& stats,
        const Disparity::RuntimeCommandRegistryDiagnostics& commandDiagnostics,
        const Disparity::EditorPanelRegistryDiagnostics& panelDiagnostics,
        const GameEventRouteDiagnostics& routeDiagnostics)
    {
        stats.RuntimeCommandHistoryEntries = commandDiagnostics.HistoryEntries;
        stats.RuntimeCommandBindingConflicts = commandDiagnostics.BindingConflicts;
        stats.RuntimeCommandCategorySummaries = commandDiagnostics.CategorySummaries;
        stats.RuntimeCommandRequiredCategoriesSatisfied = commandDiagnostics.RequiredCategoriesSatisfied;
        stats.EditorSavedDockLayouts = panelDiagnostics.SavedDockLayouts;
        stats.EditorTeamDefaultWorkspaces = panelDiagnostics.TeamDefaultWorkspaces;
        stats.EditorWorkspaceCommandBindings = panelDiagnostics.WorkspaceCommandBindings;
        stats.EditorControllerNavigationHints = panelDiagnostics.ControllerNavigationHints;
        stats.EditorToolbarCustomizationSlots = panelDiagnostics.ToolbarCustomizationSlots;
        stats.GameEventRouteReplayableRoutes = routeDiagnostics.ReplayableRoutes;
        stats.GameEventRouteSelectionLinkedRoutes = routeDiagnostics.SelectionLinkedRoutes;
        stats.GameEventRouteBreadcrumbRoutes = routeDiagnostics.BreadcrumbRoutes;
        stats.GameEventRouteTraceChannels = routeDiagnostics.TraceChannels;
        stats.GameEventRouteFailureRoutes = routeDiagnostics.FailureRoutes;
    }

    V39RoadmapMetrics BuildV39RoadmapMetrics(
        const EditorVerificationStats& stats,
        const GameEventRouteDiagnostics& routeDiagnostics,
        bool versionReady,
        bool baselineReady,
        bool schemaReady,
        bool runtimeWrapperReady)
    {
        return {
            stats.RuntimeCommandHistoryEntries,
            stats.RuntimeCommandBindingConflicts,
            stats.RuntimeCommandCategorySummaries,
            stats.RuntimeCommandRequiredCategoriesSatisfied,
            stats.EditorSavedDockLayouts,
            stats.EditorTeamDefaultWorkspaces,
            stats.EditorWorkspaceCommandBindings,
            stats.EditorControllerNavigationHints,
            stats.EditorToolbarCustomizationSlots,
            stats.GameEventRouteReplayableRoutes,
            stats.GameEventRouteSelectionLinkedRoutes,
            stats.GameEventRouteBreadcrumbRoutes,
            stats.GameEventRouteTraceChannels,
            stats.GameEventRouteFailureRoutes,
            routeDiagnostics.ObjectiveRoutes,
            versionReady,
            baselineReady,
            schemaReady,
            runtimeWrapperReady
        };
    }

    void WriteV39RoadmapReport(
        std::ostream& report,
        const EditorVerificationStats& stats,
        const std::array<uint32_t, V39RoadmapPointCount>& results)
    {
        report << "runtime_command_history_entries=" << stats.RuntimeCommandHistoryEntries << "\n";
        report << "runtime_command_binding_conflicts=" << stats.RuntimeCommandBindingConflicts << "\n";
        report << "runtime_command_category_summaries=" << stats.RuntimeCommandCategorySummaries << "\n";
        report << "runtime_command_required_categories_satisfied=" << stats.RuntimeCommandRequiredCategoriesSatisfied << "\n";
        report << "editor_saved_dock_layouts=" << stats.EditorSavedDockLayouts << "\n";
        report << "editor_team_default_workspaces=" << stats.EditorTeamDefaultWorkspaces << "\n";
        report << "editor_workspace_command_bindings=" << stats.EditorWorkspaceCommandBindings << "\n";
        report << "editor_controller_navigation_hints=" << stats.EditorControllerNavigationHints << "\n";
        report << "editor_toolbar_customization_slots=" << stats.EditorToolbarCustomizationSlots << "\n";
        report << "game_event_route_replayable_routes=" << stats.GameEventRouteReplayableRoutes << "\n";
        report << "game_event_route_selection_linked_routes=" << stats.GameEventRouteSelectionLinkedRoutes << "\n";
        report << "game_event_route_breadcrumb_routes=" << stats.GameEventRouteBreadcrumbRoutes << "\n";
        report << "game_event_route_trace_channels=" << stats.GameEventRouteTraceChannels << "\n";
        report << "game_event_route_failure_routes=" << stats.GameEventRouteFailureRoutes << "\n";
        report << "v39_roadmap_points=" << stats.V39RoadmapPointTests << "\n";

        const auto& points = GetV39RoadmapBatchPoints();
        for (size_t index = 0; index < points.size(); ++index)
        {
            report << points[index].Key << "=" << results[index] << "\n";
        }
    }
}
