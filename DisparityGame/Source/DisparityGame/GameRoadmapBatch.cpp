#include "DisparityGame/GameRoadmapBatch.h"
#include "DisparityGame/GameFollowupCatalog.h"
#include "DisparityGame/GameProductionRuntimeCatalog.h"
#include "Disparity/Assets/ProductionAssetRuntimeCatalog.h"
#include "Disparity/Assets/ProductionAssetValidator.h"
#include "Disparity/Core/FileSystem.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <vector>

namespace DisparityGame
{
    namespace
    {
        bool TextContains(const char* relativePath, const char* needle)
        {
            std::string text;
            return Disparity::FileSystem::ReadTextFile(relativePath, text) && text.find(needle) != std::string::npos;
        }

        struct V43AssetCheck
        {
            const char* MetricName = "";
            Disparity::ProductionAssetValidationRule Rule = {};
        };

        using V43AssetCheckGroup = std::array<V43AssetCheck, 6>;

        std::array<uint32_t, 6> ValidateV43AssetGroup(
            const V43AssetCheckGroup& checks,
            std::vector<Disparity::ProductionAssetValidationResult>& results)
        {
            std::array<uint32_t, 6> readiness = {};
            for (size_t index = 0; index < checks.size(); ++index)
            {
                const Disparity::ProductionAssetValidationResult result =
                    Disparity::ValidateProductionAsset(checks[index].Rule);
                readiness[index] = result.Valid ? 1u : 0u;
                results.push_back(result);
            }
            return readiness;
        }

        void WriteV43AssetValidationMetrics(
            std::ostream& report,
            const V43AssetCheckGroup& checks,
            const std::vector<Disparity::ProductionAssetValidationResult>& results,
            size_t offset)
        {
            for (size_t index = 0; index < checks.size(); ++index)
            {
                const Disparity::ProductionAssetValidationResult& result = results[offset + index];
                report << "v43_asset_" << checks[index].MetricName << "_version=43\n";
                report << "v43_asset_" << checks[index].MetricName << "_valid=" << (result.Valid ? 1u : 0u) << "\n";
                report << "v43_asset_" << checks[index].MetricName << "_hash_low="
                    << static_cast<uint32_t>(result.ContentHash & 0xffffffffull) << "\n";
            }
        }

        [[nodiscard]] std::array<uint32_t, 6> BuildRuntimeCatalogReadiness(
            const std::vector<Disparity::ProductionRuntimeAsset>& catalog,
            const char* domain)
        {
            std::array<uint32_t, 6> readiness = {};
            size_t outputIndex = 0;
            for (const Disparity::ProductionRuntimeAsset& asset : catalog)
            {
                if (asset.Domain == domain && outputIndex < readiness.size())
                {
                    readiness[outputIndex] = asset.RuntimeReady ? 1u : 0u;
                    ++outputIndex;
                }
            }
            return readiness;
        }
    }

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

    std::array<uint32_t, V41BreadthPointCount> EvaluateV41BreadthBatch(
        const V41BreadthMetrics& metrics)
    {
        std::array<uint32_t, V41BreadthPointCount> results = {};
        results[0] = metrics.EngineEventBusFlushes >= 1 ? 1u : 0u;
        results[1] = metrics.EngineSchedulerPhaseCoverage >= 7 ? 1u : 0u;
        results[2] = metrics.EngineSceneQueryRaycasts >= 1 ? 1u : 0u;
        results[3] = metrics.EngineStreamingScheduledRequests >= 2 ? 1u : 0u;
        results[4] = metrics.EngineRenderGraphBudgetChecks >= 1 && metrics.EngineModuleSmokeTests >= 5 ? 1u : 0u;
        results[5] = metrics.EditorPickTests >= 4 && metrics.EditorGizmoPickTests >= 3 ? 1u : 0u;
        results[6] = metrics.EditorViewportCaptureTests >= 2 ? 1u : 0u;
        results[7] = metrics.EditorPreferenceToolbarTests >= 3 ? 1u : 0u;
        results[8] = metrics.EditorTransformHistoryTests >= 2 ? 1u : 0u;
        results[9] = metrics.EditorShotWorkflowTests >= 2 ? 1u : 0u;
        results[10] = metrics.GameObjectiveStages >= 30 ? 1u : 0u;
        results[11] = metrics.GameTraversalCollisionTests >= 2 ? 1u : 0u;
        results[12] = metrics.GameEnemyArchetypeTests >= 4 ? 1u : 0u;
        results[13] = metrics.GameInputFailureTests >= 3 ? 1u : 0u;
        results[14] = metrics.GameAudioAnimationTests >= 2 ? 1u : 0u;
        results[15] = metrics.SchemaReady ? 1u : 0u;
        results[16] = metrics.BaselineReady ? 1u : 0u;
        results[17] = metrics.ReleaseReady ? 1u : 0u;
        results[18] = metrics.PerformanceHistoryReady ? 1u : 0u;
        results[19] = metrics.DocsReady ? 1u : 0u;
        return results;
    }

    uint32_t CountReadyV41BreadthPoints(const std::array<uint32_t, V41BreadthPointCount>& results)
    {
        return static_cast<uint32_t>(std::count(results.begin(), results.end(), 1u));
    }

    std::array<uint32_t, V42ProductionSurfacePointCount> EvaluateV42ProductionSurface(
        const V42ProductionSurfaceMetrics& metrics)
    {
        std::array<uint32_t, V42ProductionSurfacePointCount> results = {};
        for (size_t index = 0; index < metrics.EngineAssets.size(); ++index)
        {
            results[index] = metrics.EngineAssets[index] != 0u ? 1u : 0u;
        }
        for (size_t index = 0; index < metrics.EditorAssets.size(); ++index)
        {
            results[6 + index] = metrics.EditorAssets[index] != 0u ? 1u : 0u;
        }
        for (size_t index = 0; index < metrics.GameAssets.size(); ++index)
        {
            results[12 + index] = metrics.GameAssets[index] != 0u ? 1u : 0u;
        }
        for (size_t index = 0; index < metrics.VerificationAssets.size(); ++index)
        {
            results[18 + index] = metrics.VerificationAssets[index] != 0u ? 1u : 0u;
        }
        return results;
    }

    uint32_t CountReadyV42ProductionSurfacePoints(
        const std::array<uint32_t, V42ProductionSurfacePointCount>& results)
    {
        return static_cast<uint32_t>(std::count(results.begin(), results.end(), 1u));
    }

    std::array<uint32_t, V43LiveValidationPointCount> EvaluateV43LiveValidation(
        const V43LiveValidationMetrics& metrics)
    {
        std::array<uint32_t, V43LiveValidationPointCount> results = {};
        for (size_t index = 0; index < metrics.EngineLiveAssets.size(); ++index)
        {
            results[index] = metrics.EngineLiveAssets[index] != 0u ? 1u : 0u;
        }
        for (size_t index = 0; index < metrics.EditorEditableAssets.size(); ++index)
        {
            results[6 + index] = metrics.EditorEditableAssets[index] != 0u ? 1u : 0u;
        }
        for (size_t index = 0; index < metrics.GamePlayableAssets.size(); ++index)
        {
            results[12 + index] = metrics.GamePlayableAssets[index] != 0u ? 1u : 0u;
        }
        for (size_t index = 0; index < metrics.VerificationAssets.size(); ++index)
        {
            results[18 + index] = metrics.VerificationAssets[index] != 0u ? 1u : 0u;
        }
        return results;
    }

    uint32_t CountReadyV43LiveValidationPoints(
        const std::array<uint32_t, V43LiveValidationPointCount>& results)
    {
        return static_cast<uint32_t>(std::count(results.begin(), results.end(), 1u));
    }

    std::array<uint32_t, V44RuntimeCatalogPointCount> EvaluateV44RuntimeCatalog(
        const V44RuntimeCatalogMetrics& metrics)
    {
        std::array<uint32_t, V44RuntimeCatalogPointCount> results = {};
        for (size_t index = 0; index < metrics.EngineCatalogAssets.size(); ++index)
        {
            results[index] = metrics.EngineCatalogAssets[index] != 0u ? 1u : 0u;
        }
        for (size_t index = 0; index < metrics.EditorCatalogAssets.size(); ++index)
        {
            results[6 + index] = metrics.EditorCatalogAssets[index] != 0u ? 1u : 0u;
        }
        for (size_t index = 0; index < metrics.GameCatalogAssets.size(); ++index)
        {
            results[12 + index] = metrics.GameCatalogAssets[index] != 0u ? 1u : 0u;
        }
        for (size_t index = 0; index < metrics.VerificationAssets.size(); ++index)
        {
            results[18 + index] = metrics.VerificationAssets[index] != 0u ? 1u : 0u;
        }
        return results;
    }

    uint32_t CountReadyV44RuntimeCatalogPoints(
        const std::array<uint32_t, V44RuntimeCatalogPointCount>& results)
    {
        return static_cast<uint32_t>(std::count(results.begin(), results.end(), 1u));
    }

    std::array<uint32_t, V45LiveCatalogPointCount> EvaluateV45LiveCatalog(
        const V45LiveCatalogMetrics& metrics)
    {
        std::array<uint32_t, V45LiveCatalogPointCount> results = {};
        results[0] = metrics.RuntimeBindings >= 18 ? 1u : 0u;
        results[1] = metrics.EngineBindings >= 6 && metrics.EditorBindings >= 6 && metrics.GameBindings >= 6 ? 1u : 0u;
        results[2] = metrics.RuntimeReadyBindings >= 18 ? 1u : 0u;
        results[3] = metrics.RuntimeReadyBindings == metrics.RuntimeBindings ? 1u : 0u;
        results[4] = metrics.NegativeFixtureTests >= 1 ? 1u : 0u;
        results[5] = TextContains("DisparityEngine/Source/Disparity/Assets/ProductionAssetRuntimeCatalog.h", "ProductionRuntimeBinding") ? 1u : 0u;
        results[6] = metrics.CatalogPanelRows >= 8 ? 1u : 0u;
        results[7] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "ProductionCatalogBindings##EngineServices") ? 1u : 0u;
        results[8] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "Reload Catalog##ProductionCatalogPanel") ? 1u : 0u;
        results[9] = TextContains("DisparityGame/Source/DisparityGame.cpp", "catalog.reload") ? 1u : 0u;
        results[10] = TextContains("DisparityGame/Source/DisparityGame.cpp", "ProductionRuntimeCatalog") ? 1u : 0u;
        results[11] = TextContains("Tools/TestImGuiIds.ps1", "duplicate") &&
            TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "##ProductionCatalogPanel") ? 1u : 0u;
        results[12] = metrics.GameBindings >= 6 ? 1u : 0u;
        results[13] = metrics.ObjectiveBindings >= 1 ? 1u : 0u;
        results[14] = metrics.EncounterBindings >= 1 ? 1u : 0u;
        results[15] = metrics.VisibleBeacons >= 1 ? 1u : 0u;
        results[16] = TextContains("DisparityGame/Source/DisparityGame.cpp", "assets.production_catalog_bindings") ? 1u : 0u;
        results[17] = TextContains("DisparityGame/Source/DisparityGame.cpp", "DrawProductionCatalogSnapshotPanel") ? 1u : 0u;
        for (size_t index = 0; index < metrics.VerificationAssets.size(); ++index)
        {
            results[18 + index] = metrics.VerificationAssets[index] != 0u ? 1u : 0u;
        }
        return results;
    }

    uint32_t CountReadyV45LiveCatalogPoints(
        const std::array<uint32_t, V45LiveCatalogPointCount>& results)
    {
        return static_cast<uint32_t>(std::count(results.begin(), results.end(), 1u));
    }

    std::array<uint32_t, V46CatalogActionPreviewPointCount> EvaluateV46CatalogActionPreview(
        const V46CatalogActionPreviewMetrics& metrics)
    {
        std::array<uint32_t, V46CatalogActionPreviewPointCount> results = {};
        results[0] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.h", "ProductionCatalogPreviewState") ? 1u : 0u;
        results[1] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "ClampPreviewSelection") ? 1u : 0u;
        results[2] = metrics.EnginePreviewBindings >= 6 && metrics.EditorPreviewBindings >= 6 && metrics.GamePreviewBindings >= 6 ? 1u : 0u;
        results[3] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "PrimeProductionCatalogPreview") ? 1u : 0u;
        results[4] = metrics.RuntimeActionCommands >= 1 ? 1u : 0u;
        results[5] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.h", "ApplyProductionCatalogPreviewStats") ? 1u : 0u;
        results[6] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "Production Catalogs v46") ? 1u : 0u;
        results[7] = metrics.SelectableRows >= 8 && TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "ImGui::Selectable") ? 1u : 0u;
        results[8] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "Preview First##ProductionCatalogPreview") ? 1u : 0u;
        results[9] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "Next##ProductionCatalogPreview") ? 1u : 0u;
        results[10] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "Clear##ProductionCatalogPreview") ? 1u : 0u;
        results[11] = metrics.PreviewDetails >= 1 ? 1u : 0u;
        results[12] = metrics.FocusedBeacons >= 1 ? 1u : 0u;
        results[13] = metrics.PreviewSelections >= 1 && TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "objective_routes") ? 1u : 0u;
        results[14] = TextContains("DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.cpp", "DomainColor") ? 1u : 0u;
        results[15] = metrics.PreviewSelections >= 1 && metrics.PreviewCycles >= 1 ? 1u : 0u;
        results[16] = metrics.EnginePreviewBindings >= 1 && metrics.EditorPreviewBindings >= 1 && metrics.GamePreviewBindings >= 1 ? 1u : 0u;
        results[17] = metrics.PreviewClears >= 1 && TextContains("DisparityGame/Source/DisparityGame.cpp", "RefreshProductionCatalogPreview") ? 1u : 0u;
        for (size_t index = 0; index < metrics.VerificationAssets.size(); ++index)
        {
            results[18 + index] = metrics.VerificationAssets[index] != 0u ? 1u : 0u;
        }
        return results;
    }

    uint32_t CountReadyV46CatalogActionPreviewPoints(
        const std::array<uint32_t, V46CatalogActionPreviewPointCount>& results)
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

        const uint32_t editorViewportCaptureTests = stats.ViewportOverlayTests + stats.HighResResolveTests;
        const uint32_t editorPreferenceToolbarTests = stats.EditorPreferencePersistenceTests +
            stats.ViewportToolbarTests +
            stats.EditorPreferenceProfileTests;
        const uint32_t editorTransformHistoryTests = stats.TransformPrecisionTests + stats.CommandHistoryFilterTests;
        const uint32_t editorShotWorkflowTests = stats.ShotDirectorTests + stats.ShotSequencerTests;
        const uint32_t gameTraversalCollisionTests = stats.PublicDemoCollisionSolves + stats.PublicDemoTraversalActions;
        const uint32_t gameEnemyArchetypeTests = stats.PublicDemoEnemyArchetypes + (stats.PublicDemoEnemyChases > 0 ? 1u : 0u);
        const uint32_t gameInputFailureTests = stats.PublicDemoGamepadFrames +
            stats.PublicDemoMenuTransitions +
            stats.PublicDemoFailurePresentations;
        const uint32_t gameAudioAnimationTests = stats.PublicDemoContentAudioCues + stats.PublicDemoAnimationStateChanges;
        const bool schemaReady = stats.RuntimeSchemaManifestTests >= 1 &&
            TextContains("Assets/Verification/RuntimeReportSchema.dschema", "v41_breadth_points");
        const bool baselineReady =
            TextContains("Assets/Verification/RuntimeBaseline.dverify", "min_v41_breadth_points") &&
            TextContains("Assets/Verification/CameraSweepBaseline.dverify", "min_v41_breadth_points") &&
            TextContains("Assets/Verification/EditorPrecisionBaseline.dverify", "min_v41_breadth_points") &&
            TextContains("Assets/Verification/PostDebugBaseline.dverify", "min_v41_breadth_points") &&
            TextContains("Assets/Verification/AssetReloadBaseline.dverify", "min_v41_breadth_points") &&
            TextContains("Assets/Verification/GizmoDragBaseline.dverify", "min_v41_breadth_points");
        const bool releaseReady = stats.ReleaseReadinessTests >= 1 &&
            TextContains("Tools/ReviewReleaseReadiness.ps1", "V41BreadthBatchPath");
        const bool performanceHistoryReady =
            TextContains("Tools/RuntimeVerifyDisparity.ps1", "v41_breadth_points") &&
            TextContains("Tools/SummarizePerformanceHistory.ps1", "v41_breadth_points");
        const bool docsReady =
            TextContains("README.md", "Engine v41 Breadth Batch Implemented") &&
            TextContains("Docs/ROADMAP.md", "v41 Completed Breadth Batch") &&
            TextContains("Docs/ENGINE_FEATURES.md", "v41_breadth_points") &&
            TextContains("AGENTS.md", "Editor/runtime v41");

        const V41BreadthMetrics v41Metrics = {
            stats.EngineEventBusFlushes,
            stats.EngineSchedulerPhases,
            stats.EngineSceneQueryRaycasts,
            stats.EngineStreamingScheduledRequests,
            stats.EngineRenderGraphBudgetChecks,
            stats.EngineModuleSmokeTests,
            stats.ObjectPickTests,
            stats.GizmoPickTests,
            editorViewportCaptureTests,
            editorPreferenceToolbarTests,
            editorTransformHistoryTests,
            editorShotWorkflowTests,
            stats.PublicDemoObjectiveStages,
            gameTraversalCollisionTests,
            gameEnemyArchetypeTests,
            gameInputFailureTests,
            gameAudioAnimationTests,
            schemaReady,
            baselineReady,
            releaseReady,
            performanceHistoryReady,
            docsReady
        };
        const auto v41Results = EvaluateV41BreadthBatch(v41Metrics);
        const uint32_t v41ReadyPoints = CountReadyV41BreadthPoints(v41Results);

        report << "v41_engine_event_bus_flushes=" << stats.EngineEventBusFlushes << "\n";
        report << "v41_engine_scheduler_phase_coverage=" << stats.EngineSchedulerPhases << "\n";
        report << "v41_engine_scene_query_raycasts=" << stats.EngineSceneQueryRaycasts << "\n";
        report << "v41_engine_streaming_scheduled_requests=" << stats.EngineStreamingScheduledRequests << "\n";
        report << "v41_engine_render_graph_budget_checks=" << stats.EngineRenderGraphBudgetChecks << "\n";
        report << "v41_editor_pick_tests=" << stats.ObjectPickTests << "\n";
        report << "v41_editor_viewport_capture_tests=" << editorViewportCaptureTests << "\n";
        report << "v41_editor_preference_toolbar_tests=" << editorPreferenceToolbarTests << "\n";
        report << "v41_editor_transform_history_tests=" << editorTransformHistoryTests << "\n";
        report << "v41_editor_shot_workflow_tests=" << editorShotWorkflowTests << "\n";
        report << "v41_game_objective_stages=" << stats.PublicDemoObjectiveStages << "\n";
        report << "v41_game_traversal_collision_tests=" << gameTraversalCollisionTests << "\n";
        report << "v41_game_enemy_archetype_tests=" << gameEnemyArchetypeTests << "\n";
        report << "v41_game_input_failure_tests=" << gameInputFailureTests << "\n";
        report << "v41_game_audio_animation_tests=" << gameAudioAnimationTests << "\n";
        report << "v41_verification_schema_ready=" << (schemaReady ? 1u : 0u) << "\n";
        report << "v41_verification_baseline_ready=" << (baselineReady ? 1u : 0u) << "\n";
        report << "v41_verification_release_ready=" << (releaseReady ? 1u : 0u) << "\n";
        report << "v41_verification_performance_history_ready=" << (performanceHistoryReady ? 1u : 0u) << "\n";
        report << "v41_docs_ready=" << (docsReady ? 1u : 0u) << "\n";
        report << "v41_breadth_points=" << v41ReadyPoints << "\n";

        const auto& v41Points = GetV41BreadthBatchPoints();
        for (size_t index = 0; index < v41Points.size(); ++index)
        {
            report << v41Points[index].Key << "=" << v41Results[index] << "\n";
        }

        const std::array<uint32_t, 6> v42EngineAssets = {
            TextContains("Assets/Runtime/EventTraceChannels.deventschema", "channel gameplay.objective") ? 1u : 0u,
            TextContains("Assets/Runtime/SchedulerBudgets.dscheduler", "phase Rendering") ? 1u : 0u,
            TextContains("Assets/SceneSchemas/SceneQueryLayers.dqueryschema", "layer EnemyPerception") ? 1u : 0u,
            TextContains("Assets/Streaming/StreamingBudgets.dstreaming", "priority Critical") ? 1u : 0u,
            TextContains("Assets/Rendering/RenderBudgetClasses.drenderbudget", "class TrailerCapture") ? 1u : 0u,
            TextContains("Assets/Runtime/FrameTaskGraph.dtaskgraph", "edge Simulation->Physics") ? 1u : 0u
        };
        const std::array<uint32_t, 6> v42EditorAssets = {
            TextContains("Assets/Editor/WorkspaceLayouts.dworkspace", "workspace TrailerCapture") ? 1u : 0u,
            TextContains("Assets/Editor/CommandPalette.dcommands", "command disparity.capture.highres") ? 1u : 0u,
            TextContains("Assets/Editor/ViewportBookmarks.dbookmarks", "bookmark RiftHero") ? 1u : 0u,
            TextContains("Assets/Editor/InspectorPresets.dinspector", "preset BeaconMaterial") ? 1u : 0u,
            TextContains("Assets/Editor/DockMigrationPlan.ddockplan", "migration v42") ? 1u : 0u,
            TextContains("Assets/Cinematics/ShotTrackValidation.dshotcheck", "track camera_spline") ? 1u : 0u
        };
        const std::array<uint32_t, 6> v42GameAssets = {
            TextContains("Assets/Gameplay/PublicDemoEncounterPlan.dencounter", "encounter ResonanceAmbush") ? 1u : 0u,
            TextContains("Assets/Gameplay/PublicDemoControllerFeel.dcontroller", "preset PublicDemoTuned") ? 1u : 0u,
            TextContains("Assets/Gameplay/PublicDemoObjectiveRoutes.droute", "route extraction_complete") ? 1u : 0u,
            TextContains("Assets/Gameplay/PublicDemoAccessibility.daccess", "option high_contrast_rift") ? 1u : 0u,
            TextContains("Assets/Gameplay/PublicDemoSaveSlots.dsaveplan", "slot public_demo_checkpoint") ? 1u : 0u,
            TextContains("Assets/Gameplay/PublicDemoCombatSandbox.dcombat", "sandbox RelayPressure") ? 1u : 0u
        };
        const std::array<uint32_t, 6> v42VerificationAssets = {
            TextContains("Assets/Verification/V42ProductionSurface.dfollowups", "v42_point_24_docs_agent_roadmap_gate") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeReportSchema.dschema", "v42_content_points") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeBaseline.dverify", "min_v42_content_points") &&
                TextContains("Assets/Verification/CameraSweepBaseline.dverify", "min_v42_content_points") &&
                TextContains("Assets/Verification/EditorPrecisionBaseline.dverify", "min_v42_content_points") &&
                TextContains("Assets/Verification/PostDebugBaseline.dverify", "min_v42_content_points") &&
                TextContains("Assets/Verification/AssetReloadBaseline.dverify", "min_v42_content_points") &&
                TextContains("Assets/Verification/GizmoDragBaseline.dverify", "min_v42_content_points") ? 1u : 0u,
            TextContains("Tools/ReviewReleaseReadiness.ps1", "V42ProductionSurfacePath") ? 1u : 0u,
            TextContains("Tools/RuntimeVerifyDisparity.ps1", "v42_content_points") &&
                TextContains("Tools/SummarizePerformanceHistory.ps1", "v42_content_points") ? 1u : 0u,
            TextContains("README.md", "Engine v42 Production Surface Implemented") &&
                TextContains("Docs/ROADMAP.md", "v42 Completed Production Surface Batch") &&
                TextContains("Docs/ENGINE_FEATURES.md", "v42_content_points") &&
                TextContains("AGENTS.md", "Editor/runtime v42") ? 1u : 0u
        };
        const V42ProductionSurfaceMetrics v42Metrics = {
            v42EngineAssets,
            v42EditorAssets,
            v42GameAssets,
            v42VerificationAssets
        };
        const auto v42Results = EvaluateV42ProductionSurface(v42Metrics);
        const uint32_t v42EngineReady = static_cast<uint32_t>(std::count(v42EngineAssets.begin(), v42EngineAssets.end(), 1u));
        const uint32_t v42EditorReady = static_cast<uint32_t>(std::count(v42EditorAssets.begin(), v42EditorAssets.end(), 1u));
        const uint32_t v42GameReady = static_cast<uint32_t>(std::count(v42GameAssets.begin(), v42GameAssets.end(), 1u));
        const uint32_t v42VerificationReady = static_cast<uint32_t>(std::count(v42VerificationAssets.begin(), v42VerificationAssets.end(), 1u));
        const uint32_t v42ReadyPoints = CountReadyV42ProductionSurfacePoints(v42Results);

        report << "v42_engine_manifest_assets=" << v42EngineReady << "\n";
        report << "v42_editor_manifest_assets=" << v42EditorReady << "\n";
        report << "v42_game_manifest_assets=" << v42GameReady << "\n";
        report << "v42_verification_manifest_assets=" << v42VerificationReady << "\n";
        report << "v42_docs_ready=" << v42VerificationAssets[5] << "\n";
        report << "v42_content_points=" << v42ReadyPoints << "\n";

        const auto& v42Points = GetV42ProductionSurfacePoints();
        for (size_t index = 0; index < v42Points.size(); ++index)
        {
            report << v42Points[index].Key << "=" << v42Results[index] << "\n";
        }

        const V43AssetCheckGroup v43EngineChecks = {{
            { "event_trace_channels", { "Assets/Runtime/EventTraceChannels.deventschema", "channel", "gameplay.objective", "capture=true" } },
            { "scheduler_budgets", { "Assets/Runtime/SchedulerBudgets.dscheduler", "phase", "Rendering", "enforce=true" } },
            { "scene_query_layers", { "Assets/SceneSchemas/SceneQueryLayers.dqueryschema", "layer", "EnemyPerception", "broadphase=true" } },
            { "streaming_budgets", { "Assets/Streaming/StreamingBudgets.dstreaming", "priority", "Critical", "async=true" } },
            { "render_budget_classes", { "Assets/Rendering/RenderBudgetClasses.drenderbudget", "class", "TrailerCapture", "enforce=true" } },
            { "frame_task_graph", { "Assets/Runtime/FrameTaskGraph.dtaskgraph", "edge", "Simulation->Physics", "active=true" } }
        }};
        const V43AssetCheckGroup v43EditorChecks = {{
            { "workspace_layouts", { "Assets/Editor/WorkspaceLayouts.dworkspace", "workspace", "TrailerCapture", "editable=true" } },
            { "command_palette", { "Assets/Editor/CommandPalette.dcommands", "command", "disparity.capture.highres", "search=true" } },
            { "viewport_bookmarks", { "Assets/Editor/ViewportBookmarks.dbookmarks", "bookmark", "RiftHero", "editable=true" } },
            { "inspector_presets", { "Assets/Editor/InspectorPresets.dinspector", "preset", "BeaconMaterial", "apply=true" } },
            { "dock_migration_plan", { "Assets/Editor/DockMigrationPlan.ddockplan", "migration", "v42", "migrate=true" } },
            { "shot_track_validation", { "Assets/Cinematics/ShotTrackValidation.dshotcheck", "track", "camera_spline", "validate=true" } }
        }};
        const V43AssetCheckGroup v43GameChecks = {{
            { "encounter_plan", { "Assets/Gameplay/PublicDemoEncounterPlan.dencounter", "encounter", "ResonanceAmbush", "spawn=true" } },
            { "controller_feel", { "Assets/Gameplay/PublicDemoControllerFeel.dcontroller", "preset", "PublicDemoTuned", "load=true" } },
            { "objective_routes", { "Assets/Gameplay/PublicDemoObjectiveRoutes.droute", "route", "extraction_complete", "trigger=true" } },
            { "accessibility", { "Assets/Gameplay/PublicDemoAccessibility.daccess", "option", "high_contrast_rift", "toggle=true" } },
            { "save_slots", { "Assets/Gameplay/PublicDemoSaveSlots.dsaveplan", "slot", "public_demo_checkpoint", "checkpoint=true" } },
            { "combat_sandbox", { "Assets/Gameplay/PublicDemoCombatSandbox.dcombat", "sandbox", "RelayPressure", "wave=true" } }
        }};

        std::vector<Disparity::ProductionAssetValidationResult> v43AssetResults;
        v43AssetResults.reserve(v43EngineChecks.size() + v43EditorChecks.size() + v43GameChecks.size());
        const std::array<uint32_t, 6> v43EngineLiveAssets = ValidateV43AssetGroup(v43EngineChecks, v43AssetResults);
        const std::array<uint32_t, 6> v43EditorEditableAssets = ValidateV43AssetGroup(v43EditorChecks, v43AssetResults);
        const std::array<uint32_t, 6> v43GamePlayableAssets = ValidateV43AssetGroup(v43GameChecks, v43AssetResults);
        const Disparity::ProductionAssetValidationSummary v43Summary =
            Disparity::SummarizeProductionAssetValidation(v43AssetResults);

        const std::array<uint32_t, 6> v43VerificationAssets = {
            TextContains("Assets/Verification/V43LiveProductionValidation.dfollowups", "v43_point_24_docs_agent_roadmap_gate") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeReportSchema.dschema", "v43_validation_points") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeBaseline.dverify", "min_v43_validation_points") &&
                TextContains("Assets/Verification/CameraSweepBaseline.dverify", "min_v43_validation_points") &&
                TextContains("Assets/Verification/EditorPrecisionBaseline.dverify", "min_v43_validation_points") &&
                TextContains("Assets/Verification/PostDebugBaseline.dverify", "min_v43_validation_points") &&
                TextContains("Assets/Verification/AssetReloadBaseline.dverify", "min_v43_validation_points") &&
                TextContains("Assets/Verification/GizmoDragBaseline.dverify", "min_v43_validation_points") ? 1u : 0u,
            TextContains("Tools/ReviewReleaseReadiness.ps1", "V43LiveValidationPath") ? 1u : 0u,
            TextContains("Tools/RuntimeVerifyDisparity.ps1", "v43_validation_points") &&
                TextContains("Tools/SummarizePerformanceHistory.ps1", "v43_validation_points") ? 1u : 0u,
            TextContains("README.md", "Engine v43 Live Production Validation Implemented") &&
                TextContains("Docs/ROADMAP.md", "v43 Completed Live Production Validation Batch") &&
                TextContains("Docs/ENGINE_FEATURES.md", "v43_validation_points") &&
                TextContains("AGENTS.md", "Editor/runtime v43") ? 1u : 0u
        };
        const V43LiveValidationMetrics v43Metrics = {
            v43EngineLiveAssets,
            v43EditorEditableAssets,
            v43GamePlayableAssets,
            v43VerificationAssets
        };
        const auto v43Results = EvaluateV43LiveValidation(v43Metrics);
        const uint32_t v43EngineReady = static_cast<uint32_t>(std::count(v43EngineLiveAssets.begin(), v43EngineLiveAssets.end(), 1u));
        const uint32_t v43EditorReady = static_cast<uint32_t>(std::count(v43EditorEditableAssets.begin(), v43EditorEditableAssets.end(), 1u));
        const uint32_t v43GameReady = static_cast<uint32_t>(std::count(v43GamePlayableAssets.begin(), v43GamePlayableAssets.end(), 1u));
        const uint32_t v43VerificationReady = static_cast<uint32_t>(std::count(v43VerificationAssets.begin(), v43VerificationAssets.end(), 1u));
        const uint32_t v43ReadyPoints = CountReadyV43LiveValidationPoints(v43Results);

        report << "v43_engine_live_assets=" << v43EngineReady << "\n";
        report << "v43_editor_editable_assets=" << v43EditorReady << "\n";
        report << "v43_game_playable_assets=" << v43GameReady << "\n";
        report << "v43_verification_assets=" << v43VerificationReady << "\n";
        report << "v43_validated_assets=" << v43Summary.ValidAssets << "\n";
        report << "v43_validation_directives=" << v43Summary.DirectiveCount << "\n";
        report << "v43_activation_bindings=" << v43Summary.ActivationCount << "\n";
        report << "v43_missing_fields=" << v43Summary.MissingFields << "\n";
        report << "v43_asset_hash_low=" << static_cast<uint32_t>(v43Summary.CombinedHash & 0xffffffffull) << "\n";
        report << "v43_docs_ready=" << v43VerificationAssets[5] << "\n";
        report << "v43_validation_points=" << v43ReadyPoints << "\n";

        WriteV43AssetValidationMetrics(report, v43EngineChecks, v43AssetResults, 0);
        WriteV43AssetValidationMetrics(report, v43EditorChecks, v43AssetResults, v43EngineChecks.size());
        WriteV43AssetValidationMetrics(
            report,
            v43GameChecks,
            v43AssetResults,
            v43EngineChecks.size() + v43EditorChecks.size());

        const auto& v43Points = GetV43LiveValidationPoints();
        for (size_t index = 0; index < v43Points.size(); ++index)
        {
            report << v43Points[index].Key << "=" << v43Results[index] << "\n";
        }

        const std::vector<Disparity::ProductionRuntimeAssetRule> v44RuntimeRules = {
            { v43EngineChecks[0].Rule, "Engine", "event_trace_channels" },
            { v43EngineChecks[1].Rule, "Engine", "scheduler_budgets" },
            { v43EngineChecks[2].Rule, "Engine", "scene_query_layers" },
            { v43EngineChecks[3].Rule, "Engine", "streaming_budgets" },
            { v43EngineChecks[4].Rule, "Engine", "render_budget_classes" },
            { v43EngineChecks[5].Rule, "Engine", "frame_task_graph" },
            { v43EditorChecks[0].Rule, "Editor", "workspace_layouts" },
            { v43EditorChecks[1].Rule, "Editor", "command_palette" },
            { v43EditorChecks[2].Rule, "Editor", "viewport_bookmarks" },
            { v43EditorChecks[3].Rule, "Editor", "inspector_presets" },
            { v43EditorChecks[4].Rule, "Editor", "dock_migration_plan" },
            { v43EditorChecks[5].Rule, "Editor", "shot_track_validation" },
            { v43GameChecks[0].Rule, "Game", "encounter_plan" },
            { v43GameChecks[1].Rule, "Game", "controller_feel" },
            { v43GameChecks[2].Rule, "Game", "objective_routes" },
            { v43GameChecks[3].Rule, "Game", "accessibility" },
            { v43GameChecks[4].Rule, "Game", "save_slots" },
            { v43GameChecks[5].Rule, "Game", "combat_sandbox" }
        };
        const std::vector<Disparity::ProductionRuntimeAsset> v44Catalog =
            Disparity::LoadProductionRuntimeCatalog(v44RuntimeRules);
        const Disparity::ProductionRuntimeCatalogSummary v44Summary =
            Disparity::SummarizeProductionRuntimeCatalog(v44Catalog);

        const std::array<uint32_t, 6> v44EngineCatalogAssets =
            BuildRuntimeCatalogReadiness(v44Catalog, "Engine");
        const std::array<uint32_t, 6> v44EditorCatalogAssets =
            BuildRuntimeCatalogReadiness(v44Catalog, "Editor");
        const std::array<uint32_t, 6> v44GameCatalogAssets =
            BuildRuntimeCatalogReadiness(v44Catalog, "Game");
        const std::array<uint32_t, 6> v44VerificationAssets = {
            TextContains("Assets/Verification/V44RuntimeCatalogActivation.dfollowups", "v44_point_24_docs_agent_roadmap_gate") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeReportSchema.dschema", "v44_catalog_points") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeBaseline.dverify", "min_v44_catalog_points") &&
                TextContains("Assets/Verification/CameraSweepBaseline.dverify", "min_v44_catalog_points") &&
                TextContains("Assets/Verification/EditorPrecisionBaseline.dverify", "min_v44_catalog_points") &&
                TextContains("Assets/Verification/PostDebugBaseline.dverify", "min_v44_catalog_points") &&
                TextContains("Assets/Verification/AssetReloadBaseline.dverify", "min_v44_catalog_points") &&
                TextContains("Assets/Verification/GizmoDragBaseline.dverify", "min_v44_catalog_points") ? 1u : 0u,
            TextContains("Tools/ReviewReleaseReadiness.ps1", "V44RuntimeCatalogPath") ? 1u : 0u,
            TextContains("Tools/RuntimeVerifyDisparity.ps1", "v44_catalog_points") &&
                TextContains("Tools/SummarizePerformanceHistory.ps1", "v44_catalog_points") ? 1u : 0u,
            TextContains("README.md", "Engine v44 Runtime Catalog Activation Implemented") &&
                TextContains("Docs/ROADMAP.md", "v44 Completed Runtime Catalog Activation Batch") &&
                TextContains("Docs/ENGINE_FEATURES.md", "v44_catalog_points") &&
                TextContains("AGENTS.md", "Editor/runtime v44") ? 1u : 0u
        };
        const V44RuntimeCatalogMetrics v44Metrics = {
            v44EngineCatalogAssets,
            v44EditorCatalogAssets,
            v44GameCatalogAssets,
            v44VerificationAssets
        };
        const auto v44Results = EvaluateV44RuntimeCatalog(v44Metrics);
        const uint32_t v44EngineReady = static_cast<uint32_t>(std::count(v44EngineCatalogAssets.begin(), v44EngineCatalogAssets.end(), 1u));
        const uint32_t v44EditorReady = static_cast<uint32_t>(std::count(v44EditorCatalogAssets.begin(), v44EditorCatalogAssets.end(), 1u));
        const uint32_t v44GameReady = static_cast<uint32_t>(std::count(v44GameCatalogAssets.begin(), v44GameCatalogAssets.end(), 1u));
        const uint32_t v44VerificationReady = static_cast<uint32_t>(std::count(v44VerificationAssets.begin(), v44VerificationAssets.end(), 1u));
        const uint32_t v44ReadyPoints = CountReadyV44RuntimeCatalogPoints(v44Results);

        report << "v44_engine_catalog_assets=" << v44EngineReady << "\n";
        report << "v44_editor_catalog_assets=" << v44EditorReady << "\n";
        report << "v44_game_catalog_assets=" << v44GameReady << "\n";
        report << "v44_verification_assets=" << v44VerificationReady << "\n";
        report << "v44_runtime_catalog_assets=" << v44Summary.RuntimeReadyAssets << "\n";
        report << "v44_runtime_catalog_entries=" << v44Summary.EntryCount << "\n";
        report << "v44_runtime_catalog_fields=" << v44Summary.FieldCount << "\n";
        report << "v44_runtime_catalog_activations=" << v44Summary.ActivationEntries << "\n";
        report << "v44_runtime_catalog_domains=" << v44Summary.DomainCount << "\n";
        report << "v44_runtime_catalog_actions=" << v44Summary.ActionCount << "\n";
        report << "v44_runtime_catalog_required_matches=" << v44Summary.RequiredEntryMatches << "\n";
        report << "v44_runtime_catalog_missing_fields=" << v44Summary.MissingFields << "\n";
        report << "v44_runtime_catalog_hash_low=" << static_cast<uint32_t>(v44Summary.CombinedHash & 0xffffffffull) << "\n";
        report << "v44_docs_ready=" << v44VerificationAssets[5] << "\n";
        report << "v44_catalog_points=" << v44ReadyPoints << "\n";

        const auto& v44Points = GetV44RuntimeCatalogPoints();
        for (size_t index = 0; index < v44Points.size(); ++index)
        {
            report << v44Points[index].Key << "=" << v44Results[index] << "\n";
        }

        const ProductionCatalogSnapshot v45Snapshot = BuildProductionCatalogSnapshot();
        const std::array<uint32_t, 6> v45VerificationAssets = {
            TextContains("Assets/Verification/V45LiveProductionCatalog.dfollowups", "v45_point_24_docs_agent_roadmap_gate") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeReportSchema.dschema", "v45_live_catalog_points") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeBaseline.dverify", "min_v45_live_catalog_points") &&
                TextContains("Assets/Verification/CameraSweepBaseline.dverify", "min_v45_live_catalog_points") &&
                TextContains("Assets/Verification/EditorPrecisionBaseline.dverify", "min_v45_live_catalog_points") &&
                TextContains("Assets/Verification/PostDebugBaseline.dverify", "min_v45_live_catalog_points") &&
                TextContains("Assets/Verification/AssetReloadBaseline.dverify", "min_v45_live_catalog_points") &&
                TextContains("Assets/Verification/GizmoDragBaseline.dverify", "min_v45_live_catalog_points") ? 1u : 0u,
            TextContains("Tools/ReviewReleaseReadiness.ps1", "V45LiveCatalogPath") ? 1u : 0u,
            TextContains("Tools/RuntimeVerifyDisparity.ps1", "v45_live_catalog_points") &&
                TextContains("Tools/SummarizePerformanceHistory.ps1", "v45_live_catalog_points") ? 1u : 0u,
            TextContains("README.md", "Engine v45 Live Production Catalog Implemented") &&
                TextContains("Docs/ROADMAP.md", "v45 Completed Live Production Catalog Batch") &&
                TextContains("Docs/ENGINE_FEATURES.md", "v45_live_catalog_points") &&
                TextContains("AGENTS.md", "Editor/runtime v45") ? 1u : 0u
        };
        const V45LiveCatalogMetrics v45Metrics = {
            v45Snapshot.Diagnostics.BindingCount,
            v45Snapshot.Diagnostics.RuntimeReadyBindings,
            v45Snapshot.Diagnostics.EngineBindings,
            v45Snapshot.Diagnostics.EditorBindings,
            v45Snapshot.Diagnostics.GameBindings,
            std::max(stats.V45CatalogPanelRows, static_cast<uint32_t>(std::min<size_t>(v45Snapshot.Bindings.size(), 10u))),
            stats.V45CatalogVisibleBeacons,
            CountProductionCatalogBindingsByAction(v45Snapshot, "objective_routes"),
            CountProductionCatalogBindingsByAction(v45Snapshot, "encounter_plan"),
            v45Snapshot.NegativeFixtureRejected,
            v45VerificationAssets
        };
        const auto v45Results = EvaluateV45LiveCatalog(v45Metrics);
        const uint32_t v45VerificationReady = static_cast<uint32_t>(std::count(v45VerificationAssets.begin(), v45VerificationAssets.end(), 1u));
        const uint32_t v45ReadyPoints = CountReadyV45LiveCatalogPoints(v45Results);

        report << "v45_runtime_catalog_bindings=" << v45Metrics.RuntimeBindings << "\n";
        report << "v45_runtime_catalog_ready_bindings=" << v45Metrics.RuntimeReadyBindings << "\n";
        report << "v45_engine_live_bindings=" << v45Metrics.EngineBindings << "\n";
        report << "v45_editor_live_bindings=" << v45Metrics.EditorBindings << "\n";
        report << "v45_game_live_bindings=" << v45Metrics.GameBindings << "\n";
        report << "v45_catalog_panel_rows=" << v45Metrics.CatalogPanelRows << "\n";
        report << "v45_catalog_visible_beacons=" << v45Metrics.VisibleBeacons << "\n";
        report << "v45_catalog_objective_bindings=" << v45Metrics.ObjectiveBindings << "\n";
        report << "v45_catalog_encounter_bindings=" << v45Metrics.EncounterBindings << "\n";
        report << "v45_catalog_negative_fixture_tests=" << v45Metrics.NegativeFixtureTests << "\n";
        report << "v45_verification_assets=" << v45VerificationReady << "\n";
        report << "v45_runtime_catalog_hash_low=" << static_cast<uint32_t>(v45Snapshot.Summary.CombinedHash & 0xffffffffull) << "\n";
        report << "v45_docs_ready=" << v45VerificationAssets[5] << "\n";
        report << "v45_live_catalog_points=" << v45ReadyPoints << "\n";

        const auto& v45Points = GetV45LiveCatalogPoints();
        for (size_t index = 0; index < v45Points.size(); ++index)
        {
            report << v45Points[index].Key << "=" << v45Results[index] << "\n";
        }

        const std::array<uint32_t, 6> v46VerificationAssets = {
            TextContains("Assets/Verification/V46CatalogActionPreview.dfollowups", "v46_point_24_docs_agent_roadmap_gate") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeReportSchema.dschema", "v46_catalog_action_preview_points") ? 1u : 0u,
            TextContains("Assets/Verification/RuntimeBaseline.dverify", "min_v46_catalog_action_preview_points") &&
                TextContains("Assets/Verification/CameraSweepBaseline.dverify", "min_v46_catalog_action_preview_points") &&
                TextContains("Assets/Verification/EditorPrecisionBaseline.dverify", "min_v46_catalog_action_preview_points") &&
                TextContains("Assets/Verification/PostDebugBaseline.dverify", "min_v46_catalog_action_preview_points") &&
                TextContains("Assets/Verification/AssetReloadBaseline.dverify", "min_v46_catalog_action_preview_points") &&
                TextContains("Assets/Verification/GizmoDragBaseline.dverify", "min_v46_catalog_action_preview_points") ? 1u : 0u,
            TextContains("Tools/ReviewReleaseReadiness.ps1", "V46CatalogActionPreviewPath") ? 1u : 0u,
            TextContains("Tools/RuntimeVerifyDisparity.ps1", "v46_catalog_action_preview_points") &&
                TextContains("Tools/SummarizePerformanceHistory.ps1", "v46_catalog_action_preview_points") ? 1u : 0u,
            TextContains("README.md", "Engine v46 Catalog Action Preview Implemented") &&
                TextContains("Docs/ROADMAP.md", "v46 Completed Catalog Action Preview Batch") &&
                TextContains("Docs/ENGINE_FEATURES.md", "v46_catalog_action_preview_points") &&
                TextContains("AGENTS.md", "Editor/runtime v46") ? 1u : 0u
        };
        const V46CatalogActionPreviewMetrics v46Metrics = {
            std::max(stats.V46CatalogSelectableRows, static_cast<uint32_t>(std::min<size_t>(v45Snapshot.Bindings.size(), 10u))),
            stats.V46CatalogPreviewSelections,
            stats.V46CatalogPreviewCycles,
            stats.V46CatalogPreviewClears,
            stats.V46CatalogPreviewDetails,
            stats.V46CatalogFocusedBeacons,
            std::max(stats.V46EnginePreviewBindings, v45Snapshot.Diagnostics.EngineBindings),
            std::max(stats.V46EditorPreviewBindings, v45Snapshot.Diagnostics.EditorBindings),
            std::max(stats.V46GamePreviewBindings, v45Snapshot.Diagnostics.GameBindings),
            stats.V46RuntimeActionCommands,
            v46VerificationAssets
        };
        const auto v46Results = EvaluateV46CatalogActionPreview(v46Metrics);
        const uint32_t v46VerificationReady = static_cast<uint32_t>(std::count(v46VerificationAssets.begin(), v46VerificationAssets.end(), 1u));
        const uint32_t v46ReadyPoints = CountReadyV46CatalogActionPreviewPoints(v46Results);

        report << "v46_catalog_selectable_rows=" << v46Metrics.SelectableRows << "\n";
        report << "v46_catalog_preview_selections=" << v46Metrics.PreviewSelections << "\n";
        report << "v46_catalog_preview_cycles=" << v46Metrics.PreviewCycles << "\n";
        report << "v46_catalog_preview_clears=" << v46Metrics.PreviewClears << "\n";
        report << "v46_catalog_preview_details=" << v46Metrics.PreviewDetails << "\n";
        report << "v46_catalog_focused_beacons=" << v46Metrics.FocusedBeacons << "\n";
        report << "v46_engine_preview_bindings=" << v46Metrics.EnginePreviewBindings << "\n";
        report << "v46_editor_preview_bindings=" << v46Metrics.EditorPreviewBindings << "\n";
        report << "v46_game_preview_bindings=" << v46Metrics.GamePreviewBindings << "\n";
        report << "v46_runtime_action_commands=" << v46Metrics.RuntimeActionCommands << "\n";
        report << "v46_verification_assets=" << v46VerificationReady << "\n";
        report << "v46_docs_ready=" << v46VerificationAssets[5] << "\n";
        report << "v46_catalog_action_preview_points=" << v46ReadyPoints << "\n";

        const auto& v46Points = GetV46CatalogActionPreviewPoints();
        for (size_t index = 0; index < v46Points.size(); ++index)
        {
            report << v46Points[index].Key << "=" << v46Results[index] << "\n";
        }
    }
}
