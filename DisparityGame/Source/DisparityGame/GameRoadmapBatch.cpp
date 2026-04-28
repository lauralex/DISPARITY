#include "DisparityGame/GameRoadmapBatch.h"
#include "DisparityGame/GameFollowupCatalog.h"
#include "Disparity/Core/FileSystem.h"

#include <algorithm>
#include <ostream>
#include <string>

namespace DisparityGame
{
    namespace
    {
        bool TextContains(const char* relativePath, const char* needle)
        {
            std::string text;
            return Disparity::FileSystem::ReadTextFile(relativePath, text) && text.find(needle) != std::string::npos;
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
    }
}
