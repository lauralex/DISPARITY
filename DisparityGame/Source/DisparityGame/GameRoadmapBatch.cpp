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

    std::array<uint32_t, V40DiversifiedPointCount> EvaluateV40DiversifiedBatch(
        const V40DiversifiedMetrics& metrics)
    {
        std::array<uint32_t, V40DiversifiedPointCount> results = {};
        results[0] = metrics.RuntimeCommandHistorySuccesses >= 5 ? 1u : 0u;
        results[1] = metrics.RuntimeCommandHistoryFailures >= 1 ? 1u : 0u;
        results[2] = metrics.RuntimeCommandBoundCommands >= 4 && metrics.RuntimeCommandUniqueBindings >= 3 ? 1u : 0u;
        results[3] = metrics.RuntimeCommandDocumentedCommands >= 7 ? 1u : 0u;
        results[4] = metrics.RuntimeCommandBindingConflicts >= 1 && metrics.RuntimeCommandUniqueBindings >= 3 ? 1u : 0u;
        results[5] = metrics.EditorWorkspaceMigrationReady >= 3 ? 1u : 0u;
        results[6] = metrics.EditorWorkspaceFocusTargets >= 3 ? 1u : 0u;
        results[7] = metrics.EditorWorkspaceGamepadReady >= 3 ? 1u : 0u;
        results[8] = metrics.EditorWorkspaceToolbarProfiles >= 3 ? 1u : 0u;
        results[9] = metrics.EditorWorkspaceCommandRoutes >= 3 ? 1u : 0u;
        results[10] = metrics.GameEventRouteCheckpointLinks >= 5 ? 1u : 0u;
        results[11] = metrics.GameEventRouteSaveRelevantRoutes >= 8 ? 1u : 0u;
        results[12] = metrics.GameEventRouteChapterReplayRoutes >= 10 ? 1u : 0u;
        results[13] = metrics.GameEventRouteAccessibilityRoutes >= 6 ? 1u : 0u;
        results[14] = metrics.GameEventRouteHudVisibleRoutes >= 10 ? 1u : 0u;
        return results;
    }

    uint32_t CountReadyV40DiversifiedPoints(const std::array<uint32_t, V40DiversifiedPointCount>& results)
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
        stats.RuntimeCommandHistorySuccesses = commandDiagnostics.HistorySuccesses;
        stats.RuntimeCommandHistoryFailures = commandDiagnostics.HistoryFailures;
        stats.RuntimeCommandBoundCommands = commandDiagnostics.BoundCommands;
        stats.RuntimeCommandUniqueBindings = commandDiagnostics.UniqueBindings;
        stats.RuntimeCommandDocumentedCommands = commandDiagnostics.DocumentedCommands;
        stats.EditorSavedDockLayouts = panelDiagnostics.SavedDockLayouts;
        stats.EditorTeamDefaultWorkspaces = panelDiagnostics.TeamDefaultWorkspaces;
        stats.EditorWorkspaceCommandBindings = panelDiagnostics.WorkspaceCommandBindings;
        stats.EditorControllerNavigationHints = panelDiagnostics.ControllerNavigationHints;
        stats.EditorToolbarCustomizationSlots = panelDiagnostics.ToolbarCustomizationSlots;
        stats.EditorWorkspaceMigrationReady = panelDiagnostics.MigrationReadyWorkspaces;
        stats.EditorWorkspaceFocusTargets = panelDiagnostics.FocusTargetWorkspaces;
        stats.EditorWorkspaceGamepadReady = panelDiagnostics.GamepadNavigableWorkspaces;
        stats.EditorWorkspaceToolbarProfiles = panelDiagnostics.ToolbarProfileWorkspaces;
        stats.EditorWorkspaceCommandRoutes = panelDiagnostics.CommandRoutedWorkspaces;
        stats.GameEventRouteReplayableRoutes = routeDiagnostics.ReplayableRoutes;
        stats.GameEventRouteSelectionLinkedRoutes = routeDiagnostics.SelectionLinkedRoutes;
        stats.GameEventRouteBreadcrumbRoutes = routeDiagnostics.BreadcrumbRoutes;
        stats.GameEventRouteTraceChannels = routeDiagnostics.TraceChannels;
        stats.GameEventRouteFailureRoutes = routeDiagnostics.FailureRoutes;
        stats.GameEventRouteCheckpointLinks = routeDiagnostics.CheckpointLinkedRoutes;
        stats.GameEventRouteSaveRelevantRoutes = routeDiagnostics.SaveRelevantRoutes;
        stats.GameEventRouteChapterReplayRoutes = routeDiagnostics.ChapterReplayRoutes;
        stats.GameEventRouteAccessibilityRoutes = routeDiagnostics.AccessibilityCriticalRoutes;
        stats.GameEventRouteHudVisibleRoutes = routeDiagnostics.HudVisibleRoutes;
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

        const V40DiversifiedMetrics v40Metrics = {
            stats.RuntimeCommandHistorySuccesses,
            stats.RuntimeCommandHistoryFailures,
            stats.RuntimeCommandBoundCommands,
            stats.RuntimeCommandUniqueBindings,
            stats.RuntimeCommandDocumentedCommands,
            stats.RuntimeCommandBindingConflicts,
            stats.EditorWorkspaceMigrationReady,
            stats.EditorWorkspaceFocusTargets,
            stats.EditorWorkspaceGamepadReady,
            stats.EditorWorkspaceToolbarProfiles,
            stats.EditorWorkspaceCommandRoutes,
            stats.GameEventRouteCheckpointLinks,
            stats.GameEventRouteSaveRelevantRoutes,
            stats.GameEventRouteChapterReplayRoutes,
            stats.GameEventRouteAccessibilityRoutes,
            stats.GameEventRouteHudVisibleRoutes
        };
        const auto v40Results = EvaluateV40DiversifiedBatch(v40Metrics);
        const uint32_t v40ReadyPoints = CountReadyV40DiversifiedPoints(v40Results);

        report << "runtime_command_history_successes=" << stats.RuntimeCommandHistorySuccesses << "\n";
        report << "runtime_command_history_failures=" << stats.RuntimeCommandHistoryFailures << "\n";
        report << "runtime_command_bound_commands=" << stats.RuntimeCommandBoundCommands << "\n";
        report << "runtime_command_unique_bindings=" << stats.RuntimeCommandUniqueBindings << "\n";
        report << "runtime_command_documented_commands=" << stats.RuntimeCommandDocumentedCommands << "\n";
        report << "editor_workspace_migration_ready=" << stats.EditorWorkspaceMigrationReady << "\n";
        report << "editor_workspace_focus_targets=" << stats.EditorWorkspaceFocusTargets << "\n";
        report << "editor_workspace_gamepad_ready=" << stats.EditorWorkspaceGamepadReady << "\n";
        report << "editor_workspace_toolbar_profiles=" << stats.EditorWorkspaceToolbarProfiles << "\n";
        report << "editor_workspace_command_routes=" << stats.EditorWorkspaceCommandRoutes << "\n";
        report << "game_event_route_checkpoint_links=" << stats.GameEventRouteCheckpointLinks << "\n";
        report << "game_event_route_save_relevant_routes=" << stats.GameEventRouteSaveRelevantRoutes << "\n";
        report << "game_event_route_chapter_replay_routes=" << stats.GameEventRouteChapterReplayRoutes << "\n";
        report << "game_event_route_accessibility_routes=" << stats.GameEventRouteAccessibilityRoutes << "\n";
        report << "game_event_route_hud_visible_routes=" << stats.GameEventRouteHudVisibleRoutes << "\n";
        report << "v40_diversified_points=" << v40ReadyPoints << "\n";

        const auto& v40Points = GetV40DiversifiedBatchPoints();
        for (size_t index = 0; index < v40Points.size(); ++index)
        {
            report << v40Points[index].Key << "=" << v40Results[index] << "\n";
        }
    }
}
