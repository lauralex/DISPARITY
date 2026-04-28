param(
    [string]$VerificationPath = "Assets/Verification",
    [string]$HistoryPath = "Saved/Verification/performance_history.csv",
    [string]$PerformanceBaselinePath = "Assets/Verification/PerformanceBaselines.dperf",
    [switch]$UpdatePerformanceBaseline,
    [switch]$ListGoldenProfiles
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($VerificationPath)) {
    $VerificationPath = Join-Path $root $VerificationPath
}
if (![System.IO.Path]::IsPathRooted($HistoryPath)) {
    $HistoryPath = Join-Path $root $HistoryPath
}
if (![System.IO.Path]::IsPathRooted($PerformanceBaselinePath)) {
    $PerformanceBaselinePath = Join-Path $root $PerformanceBaselinePath
}

$requiredKeys = @(
    "expected_capture_width",
    "expected_capture_height",
    "min_editor_pick_tests",
    "min_gizmo_pick_tests",
    "min_gizmo_drag_tests",
    "min_post_debug_views",
    "min_showcase_frames",
    "min_trailer_frames",
    "min_high_res_captures",
    "min_rift_vfx_draws",
    "min_audio_beat_pulses",
    "min_audio_snapshot_tests",
    "min_render_graph_allocations",
    "min_render_graph_aliased_resources",
    "min_render_graph_callbacks",
    "min_render_graph_barriers",
    "min_render_graph_resource_bindings",
    "min_async_io_tests",
    "min_material_texture_slot_tests",
    "min_prefab_variant_tests",
    "min_shot_director_tests",
    "min_shot_spline_tests",
    "min_audio_analysis_tests",
    "min_xaudio2_backend_tests",
    "min_vfx_system_tests",
    "min_gpu_vfx_simulation_tests",
    "min_animation_skinning_tests",
    "min_gpu_pick_hover_cache_tests",
    "min_gpu_pick_latency_histogram_tests",
    "min_shot_timeline_track_tests",
    "min_shot_thumbnail_tests",
    "min_shot_preview_scrub_tests",
    "min_graph_high_res_capture_tests",
    "min_cooked_package_tests",
    "min_asset_invalidation_tests",
    "min_nested_prefab_tests",
    "min_audio_production_tests",
    "min_viewport_overlay_tests",
    "min_high_res_resolve_tests",
    "min_viewport_hud_control_tests",
    "min_transform_precision_tests",
    "min_command_history_filter_tests",
    "min_runtime_schema_manifest_tests",
    "min_shot_sequencer_tests",
    "min_vfx_renderer_profile_tests",
    "min_cooked_gpu_resource_tests",
    "min_dependency_invalidation_tests",
    "min_audio_meter_calibration_tests",
    "min_release_readiness_tests",
    "min_v25_production_points",
    "min_editor_preference_persistence_tests",
    "min_viewport_toolbar_tests",
    "min_editor_preference_profile_tests",
    "min_capture_preset_tests",
    "min_vfx_emitter_profile_tests",
    "min_cooked_dependency_preview_tests",
    "min_editor_workflow_tests",
    "min_asset_pipeline_promotion_tests",
    "min_rendering_advanced_tests",
    "min_runtime_sequencer_feature_tests",
    "min_audio_production_feature_tests",
    "min_production_publishing_tests",
    "min_v28_diversified_points",
    "min_public_demo_tests",
    "min_public_demo_shard_pickups",
    "min_public_demo_hud_frames",
    "min_public_demo_beacon_draws",
    "min_v29_public_demo_points",
    "min_public_demo_objective_stages",
    "min_public_demo_anchor_activations",
    "min_public_demo_retries",
    "min_public_demo_checkpoints",
    "min_public_demo_director_frames",
    "min_v30_vertical_slice_points",
    "min_public_demo_resonance_gates",
    "min_public_demo_pressure_hits",
    "min_public_demo_footstep_events",
    "min_v31_diversified_points",
    "min_public_demo_phase_relays",
    "min_public_demo_relay_overcharge_windows",
    "min_public_demo_combo_steps",
    "min_v32_roadmap_points",
    "min_public_demo_collision_solves",
    "min_public_demo_traversal_actions",
    "min_public_demo_enemy_chases",
    "min_public_demo_enemy_evades",
    "min_public_demo_gamepad_frames",
    "min_public_demo_menu_transitions",
    "min_public_demo_failure_presentations",
    "min_public_demo_content_audio_cues",
    "min_public_demo_animation_state_changes",
    "min_v33_playable_demo_points",
    "min_public_demo_enemy_archetypes",
    "min_public_demo_enemy_los_checks",
    "min_public_demo_enemy_telegraphs",
    "min_public_demo_enemy_hit_reactions",
    "min_public_demo_controller_grounded_frames",
    "min_public_demo_controller_slope_samples",
    "min_public_demo_controller_material_samples",
    "min_public_demo_controller_moving_platform_frames",
    "min_public_demo_controller_camera_collision_frames",
    "min_public_demo_animation_clip_events",
    "min_public_demo_animation_blend_samples",
    "min_public_demo_accessibility_toggles",
    "min_rendering_pipeline_readiness_items",
    "min_production_pipeline_readiness_items",
    "min_v34_aaa_foundation_points",
    "min_engine_event_bus_deliveries",
    "min_engine_scheduler_tasks",
    "min_engine_scene_query_hits",
    "min_engine_streaming_scheduled_requests",
    "min_engine_render_graph_budget_checks",
    "min_engine_module_smoke_tests",
    "min_v35_engine_architecture_points",
    "min_engine_service_registry_services",
    "min_engine_telemetry_events",
    "min_engine_config_vars",
    "min_editor_panel_registry_panels",
    "min_game_module_split_files",
    "min_v36_mixed_batch_points",
    "min_runtime_command_registry_commands",
    "min_editor_workspace_registry_workspaces",
    "min_game_event_route_catalog_routes",
    "min_v38_diversified_points",
    "min_runtime_command_history_entries",
    "min_runtime_command_binding_conflicts",
    "min_editor_dock_layout_descriptors",
    "min_editor_team_default_workspaces",
    "min_game_replayable_event_routes",
    "min_v39_roadmap_points",
    "min_runtime_command_history_failures",
    "min_editor_workspace_migration_ready",
    "min_game_checkpoint_route_links",
    "min_v40_diversified_points",
    "min_v41_engine_phase_coverage",
    "min_v41_editor_pick_tests",
    "min_v41_game_objective_stages",
    "min_v41_verification_docs_points",
    "min_v41_breadth_points",
    "min_v42_engine_content_assets",
    "min_v42_editor_content_assets",
    "min_v42_game_content_assets",
    "min_v42_verification_content_assets",
    "min_v42_content_points",
    "min_v43_engine_live_assets",
    "min_v43_editor_editable_assets",
    "min_v43_game_playable_assets",
    "min_v43_verification_assets",
    "min_v43_validated_assets",
    "min_v43_activation_bindings",
    "max_v43_missing_fields",
    "min_v43_validation_points",
    "min_v44_engine_catalog_assets",
    "min_v44_editor_catalog_assets",
    "min_v44_game_catalog_assets",
    "min_v44_verification_assets",
    "min_v44_runtime_catalog_assets",
    "min_v44_runtime_catalog_activations",
    "min_v44_runtime_catalog_domains",
    "min_v44_runtime_catalog_actions",
    "max_v44_runtime_catalog_missing_fields",
    "min_v44_catalog_points",
    "min_v45_runtime_catalog_bindings",
    "min_v45_runtime_catalog_ready_bindings",
    "min_v45_engine_live_bindings",
    "min_v45_editor_live_bindings",
    "min_v45_game_live_bindings",
    "min_v45_catalog_panel_rows",
    "min_v45_catalog_visible_beacons",
    "min_v45_catalog_objective_bindings",
    "min_v45_catalog_encounter_bindings",
    "min_v45_catalog_negative_fixture_tests",
    "min_v45_verification_assets",
    "min_v45_live_catalog_points",
    "require_editor_gpu_pick_resources",
    "golden_capture_path",
    "golden_mean_tolerance",
    "golden_bad_pixel_ratio"
)

$baselineFiles = @(Get-ChildItem -LiteralPath $VerificationPath -Filter *Baseline.dverify -File)
foreach ($baseline in $baselineFiles) {
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $baseline.FullName) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
            continue
        }
        if ($trimmed -match "^([^=]+)=(.*)$") {
            $values[$Matches[1].Trim()] = $Matches[2].Trim()
        }
    }

    $missing = @($requiredKeys | Where-Object { !$values.ContainsKey($_) })
    if ($missing.Count -gt 0) {
        throw "$($baseline.Name) is missing key(s): $($missing -join ', ')"
    }

    Write-Host "Baseline OK: $($baseline.Name)"
}

if ($ListGoldenProfiles) {
    $profiles = @(Get-ChildItem -LiteralPath (Join-Path $VerificationPath "GoldenProfiles") -Filter *.dgoldenprofile -File -ErrorAction SilentlyContinue)
    foreach ($profile in $profiles) {
        Write-Host "Golden profile: $($profile.Name)"
    }
}

$summaryArguments = @{
    HistoryPath = $HistoryPath
    BaselinePath = $PerformanceBaselinePath
}
if ($UpdatePerformanceBaseline) {
    $summaryArguments["UpdateBaseline"] = $true
}

& (Join-Path $PSScriptRoot "SummarizePerformanceHistory.ps1") @summaryArguments
