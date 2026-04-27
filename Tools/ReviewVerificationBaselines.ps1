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
