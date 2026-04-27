param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$ExecutablePath = "",
    [int]$Frames = 90,
    [int]$TimeoutSeconds = 30,
    [double]$CpuFrameBudgetMs = 120.0,
    [double]$GpuFrameBudgetMs = 50.0,
    [double]$PassBudgetMs = 60.0,
    [string]$SuiteName = "Default",
    [string]$ReplayPath = "Assets/Verification/Prototype.dreplay",
    [string]$BaselinePath = "Assets/Verification/RuntimeBaseline.dverify",
    [string]$GoldenProfilePath = "Assets/Verification/GoldenProfiles/Default.dgoldenprofile",
    [string]$ReportSchemaPath = "Assets/Verification/RuntimeReportSchema.dschema",
    [string]$HistoryPath = "",
    [switch]$DisableCapture,
    [switch]$DisableInputPlayback,
    [switch]$DisableBudgets,
    [switch]$DisableBaseline,
    [switch]$DisablePerfHistory,
    [switch]$DisableGoldenComparison,
    [switch]$UpdateGoldenCapture
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $root "bin\x64\$Configuration\DisparityGame.exe"
}

if (!(Test-Path -LiteralPath $ExecutablePath)) {
    throw "DisparityGame.exe not found at $ExecutablePath"
}

$resolvedExecutable = (Resolve-Path -LiteralPath $ExecutablePath).Path
$workingDirectory = Split-Path -Parent $resolvedExecutable
$reportPath = Join-Path $workingDirectory "Saved\Verification\runtime_verify.txt"
$capturePath = Join-Path $workingDirectory "Saved\Verification\runtime_capture.ppm"
if ([string]::IsNullOrWhiteSpace($HistoryPath)) {
    $HistoryPath = Join-Path $workingDirectory "Saved\Verification\performance_history.csv"
}

function Resolve-VerificationPath {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $Path
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        if (Test-Path -LiteralPath $Path) {
            return (Resolve-Path -LiteralPath $Path).Path
        }
        return [System.IO.Path]::GetFullPath($Path)
    }

    $candidates = @(
        (Join-Path $workingDirectory $Path),
        (Join-Path $root $Path)
    )

    $cursor = $workingDirectory
    for ($i = 0; $i -lt 8 -and ![string]::IsNullOrWhiteSpace($cursor); $i++) {
        $candidates += Join-Path $cursor $Path
        $parent = Split-Path -Parent $cursor
        if ($parent -eq $cursor) {
            break
        }
        $cursor = $parent
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    if ($Path -match "^(Assets|Tools|Docs)[\\/]") {
        return Join-Path $root $Path
    }

    return Join-Path $workingDirectory $Path
}

function Read-BaselineValues {
    param([string]$Path)

    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
            continue
        }

        if ($trimmed -match "^([^=]+)=(.*)$") {
            $values[$Matches[1].Trim()] = $Matches[2].Trim()
        }
    }

    return $values
}

function Get-SafeProfileName {
    param([string]$Name)

    $safe = [regex]::Replace($Name.Trim(), "[^A-Za-z0-9_.-]", "_")
    if ([string]::IsNullOrWhiteSpace($safe)) {
        return "UnknownAdapter"
    }
    return $safe
}

function Get-DetectedAdapterName {
    try {
        $adapter = Get-CimInstance Win32_VideoController -ErrorAction Stop |
            Where-Object { ![string]::IsNullOrWhiteSpace($_.Name) } |
            Sort-Object AdapterRAM -Descending |
            Select-Object -First 1
        if ($adapter) {
            return [string]$adapter.Name
        }
    }
    catch {
    }

    return "UnknownAdapter"
}

function Get-BaselineInt {
    param(
        [hashtable]$Values,
        [string]$Name,
        [int]$DefaultValue
    )

    if ($Values.ContainsKey($Name)) {
        return [int]$Values[$Name]
    }

    return $DefaultValue
}

function Get-BaselineDouble {
    param(
        [hashtable]$Values,
        [string]$Name,
        [double]$DefaultValue
    )

    if ($Values.ContainsKey($Name)) {
        return [double]$Values[$Name]
    }

    return $DefaultValue
}

function Get-ProfiledInt {
    param(
        [hashtable]$BaselineValues,
        [hashtable]$ProfileValues,
        [string]$Name,
        [int]$DefaultValue
    )

    if ($ProfileValues.ContainsKey($Name)) {
        return [int]$ProfileValues[$Name]
    }
    return Get-BaselineInt -Values $BaselineValues -Name $Name -DefaultValue $DefaultValue
}

function Get-ProfiledDouble {
    param(
        [hashtable]$BaselineValues,
        [hashtable]$ProfileValues,
        [string]$Name,
        [double]$DefaultValue
    )

    if ($ProfileValues.ContainsKey($Name)) {
        return [double]$ProfileValues[$Name]
    }
    return Get-BaselineDouble -Values $BaselineValues -Name $Name -DefaultValue $DefaultValue
}

function Get-ReportMetrics {
    param([string]$Report)

    $metrics = @{}
    foreach ($line in ($Report -split "`r?`n")) {
        if ($line -match "^([^=]+)=(.*)$") {
            $metrics[$Matches[1]] = $Matches[2]
        }
    }

    return $metrics
}

function Get-RequiredReportMetrics {
    param([string]$SchemaPath)

    $defaults = @(
        "version",
        "frames",
        "viewport_overlay_tests",
        "high_res_resolve_tests",
        "gpu_pick_stale_frames",
        "high_res_resolve_filter",
        "high_res_resolve_samples"
    )

    $resolvedSchemaPath = Resolve-VerificationPath -Path $SchemaPath
    if (!(Test-Path -LiteralPath $resolvedSchemaPath)) {
        Write-Host "Runtime report schema manifest not found at $resolvedSchemaPath; using built-in defaults"
        return $defaults
    }

    $metrics = @()
    foreach ($line in Get-Content -LiteralPath $resolvedSchemaPath) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
            continue
        }

        $metrics += $trimmed
    }

    if ($metrics.Count -eq 0) {
        throw "Runtime report schema manifest $resolvedSchemaPath did not define any metrics."
    }

    return $metrics
}

function Assert-RuntimeReportSchema {
    param(
        [hashtable]$Metrics,
        [string[]]$RequiredMetrics
    )

    foreach ($metric in $requiredMetrics) {
        if (!$Metrics.ContainsKey($metric) -or [string]::IsNullOrWhiteSpace([string]$Metrics[$metric])) {
            throw "Runtime verification report is missing required schema metric '$metric'."
        }
    }

    if ([int]$Metrics["viewport_overlay_tests"] -lt 1) {
        throw "Runtime verification report did not record viewport overlay coverage."
    }
    if ([int]$Metrics["high_res_resolve_tests"] -lt 1) {
        throw "Runtime verification report did not record high-resolution resolve coverage."
    }
    if ([int]$Metrics["high_res_resolve_samples"] -lt 4) {
        throw "Runtime verification report recorded too few high-resolution resolve samples."
    }
    if ($Metrics["high_res_resolve_filter"] -ne "tent") {
        throw "Runtime verification report expected tent high-resolution resolve filter."
    }
    if ($Metrics.ContainsKey("v25_production_points") -and [int]$Metrics["v25_production_points"] -lt 40) {
        throw "Runtime verification report did not verify all forty v25 production points."
    }
    if ($Metrics.ContainsKey("editor_preference_persistence_tests") -and [int]$Metrics["editor_preference_persistence_tests"] -lt 1) {
        throw "Runtime verification report did not record editor preference persistence coverage."
    }
    if ($Metrics.ContainsKey("viewport_toolbar_tests") -and [int]$Metrics["viewport_toolbar_tests"] -lt 1) {
        throw "Runtime verification report did not record viewport toolbar coverage."
    }
    if ($Metrics.ContainsKey("viewport_toolbar_interactions") -and [int]$Metrics["viewport_toolbar_interactions"] -lt 5) {
        throw "Runtime verification report recorded too few viewport toolbar interactions."
    }
    if ($Metrics.ContainsKey("editor_preference_profile_tests") -and [int]$Metrics["editor_preference_profile_tests"] -lt 1) {
        throw "Runtime verification report did not record editor preference profile coverage."
    }
    if ($Metrics.ContainsKey("capture_preset_tests") -and [int]$Metrics["capture_preset_tests"] -lt 1) {
        throw "Runtime verification report did not record capture preset coverage."
    }
    if ($Metrics.ContainsKey("high_res_capture_async_compression_ready") -and $Metrics["high_res_capture_async_compression_ready"] -ne "true") {
        throw "Runtime verification report expected async capture compression readiness."
    }
    if ($Metrics.ContainsKey("vfx_emitter_profile_tests") -and [int]$Metrics["vfx_emitter_profile_tests"] -lt 1) {
        throw "Runtime verification report did not record VFX emitter profile coverage."
    }
    if ($Metrics.ContainsKey("rift_vfx_gpu_buffer_bytes") -and [int]$Metrics["rift_vfx_gpu_buffer_bytes"] -lt 1) {
        throw "Runtime verification report recorded no VFX GPU buffer bytes."
    }
    if ($Metrics.ContainsKey("cooked_dependency_preview_tests") -and [int]$Metrics["cooked_dependency_preview_tests"] -lt 1) {
        throw "Runtime verification report did not record cooked dependency preview coverage."
    }
    if ($Metrics.ContainsKey("cooked_package_reload_rollback_ready") -and $Metrics["cooked_package_reload_rollback_ready"] -ne "true") {
        throw "Runtime verification report expected cooked package reload rollback readiness."
    }
    if ($Metrics.ContainsKey("v28_diversified_points") -and [int]$Metrics["v28_diversified_points"] -lt 36) {
        throw "Runtime verification report did not verify all thirty-six v28 diversified points."
    }
    foreach ($metric in @(
        "editor_workflow_tests",
        "asset_pipeline_promotion_tests",
        "rendering_advanced_tests",
        "runtime_sequencer_feature_tests",
        "audio_production_feature_tests",
        "production_publishing_tests"
    )) {
        if ($Metrics.ContainsKey($metric) -and [int]$Metrics[$metric] -lt 1) {
            throw "Runtime verification report did not record required v28 category coverage for '$metric'."
        }
    }
    if ($Metrics.ContainsKey("editor_profile_diff_fields") -and [int]$Metrics["editor_profile_diff_fields"] -lt 1) {
        throw "Runtime verification report did not record editor profile diff fields."
    }
    if ($Metrics.ContainsKey("asset_streaming_priority_levels") -and [int]$Metrics["asset_streaming_priority_levels"] -lt 3) {
        throw "Runtime verification report recorded too few asset streaming priority levels."
    }
    if ($Metrics.ContainsKey("rendering_motion_vector_targets") -and [int]$Metrics["rendering_motion_vector_targets"] -lt 2) {
        throw "Runtime verification report recorded too few rendering motion-vector targets."
    }
    if ($Metrics.ContainsKey("runtime_keyboard_preview_bindings") -and [int]$Metrics["runtime_keyboard_preview_bindings"] -lt 4) {
        throw "Runtime verification report recorded too few runtime keyboard preview bindings."
    }
    if ($Metrics.ContainsKey("audio_content_pulse_inputs") -and [int]$Metrics["audio_content_pulse_inputs"] -lt 1) {
        throw "Runtime verification report recorded no audio content pulse inputs."
    }
    if ($Metrics.ContainsKey("production_obs_websocket_commands") -and [int]$Metrics["production_obs_websocket_commands"] -lt 1) {
        throw "Runtime verification report recorded no OBS automation command readiness."
    }
    if ($Metrics.ContainsKey("v29_public_demo_points") -and [int]$Metrics["v29_public_demo_points"] -lt 30) {
        throw "Runtime verification report did not verify all thirty v29 public demo points."
    }
    if ($Metrics.ContainsKey("public_demo_tests") -and [int]$Metrics["public_demo_tests"] -lt 1) {
        throw "Runtime verification report did not record public demo coverage."
    }
    if ($Metrics.ContainsKey("public_demo_shard_pickups") -and [int]$Metrics["public_demo_shard_pickups"] -lt 6) {
        throw "Runtime verification report recorded too few public demo shard pickups."
    }
    if ($Metrics.ContainsKey("public_demo_hud_frames") -and [int]$Metrics["public_demo_hud_frames"] -lt 1) {
        throw "Runtime verification report did not render the public demo HUD."
    }
    if ($Metrics.ContainsKey("public_demo_beacon_draws") -and [int]$Metrics["public_demo_beacon_draws"] -lt 1) {
        throw "Runtime verification report did not render public demo beacons."
    }
    if ($Metrics.ContainsKey("public_demo_extraction_completed") -and $Metrics["public_demo_extraction_completed"] -ne "true") {
        throw "Runtime verification report did not complete the public demo extraction loop."
    }
    if ($Metrics.ContainsKey("v30_vertical_slice_points") -and [int]$Metrics["v30_vertical_slice_points"] -lt 36) {
        throw "Runtime verification report did not verify all thirty-six v30 vertical slice points."
    }
    if ($Metrics.ContainsKey("public_demo_objective_stages") -and [int]$Metrics["public_demo_objective_stages"] -lt 3) {
        throw "Runtime verification report recorded too few public demo objective stages."
    }
    if ($Metrics.ContainsKey("public_demo_anchor_activations") -and [int]$Metrics["public_demo_anchor_activations"] -lt 3) {
        throw "Runtime verification report recorded too few public demo anchor activations."
    }
    if ($Metrics.ContainsKey("public_demo_retries") -and [int]$Metrics["public_demo_retries"] -lt 1) {
        throw "Runtime verification report did not exercise public demo retry behavior."
    }
    if ($Metrics.ContainsKey("public_demo_director_frames") -and [int]$Metrics["public_demo_director_frames"] -lt 1) {
        throw "Runtime verification report did not exercise the Demo Director surface."
    }
    if ($Metrics.ContainsKey("public_demo_anchor_puzzle_complete") -and $Metrics["public_demo_anchor_puzzle_complete"] -ne "true") {
        throw "Runtime verification report did not complete the phase-anchor puzzle."
    }
    if ($Metrics.ContainsKey("v31_diversified_points") -and [int]$Metrics["v31_diversified_points"] -lt 30) {
        throw "Runtime verification report did not verify all thirty v31 diversified roadmap points."
    }
    if ($Metrics.ContainsKey("public_demo_resonance_gates") -and [int]$Metrics["public_demo_resonance_gates"] -lt 2) {
        throw "Runtime verification report recorded too few public demo resonance gates."
    }
    if ($Metrics.ContainsKey("public_demo_pressure_hits") -and [int]$Metrics["public_demo_pressure_hits"] -lt 1) {
        throw "Runtime verification report did not exercise sentinel pressure hit behavior."
    }
    if ($Metrics.ContainsKey("public_demo_footstep_events") -and [int]$Metrics["public_demo_footstep_events"] -lt 1) {
        throw "Runtime verification report did not exercise footstep cadence behavior."
    }
    if ($Metrics.ContainsKey("public_demo_resonance_gate_complete") -and $Metrics["public_demo_resonance_gate_complete"] -ne "true") {
        throw "Runtime verification report did not complete the resonance gate objective."
    }
    if ($Metrics.ContainsKey("v32_roadmap_points") -and [int]$Metrics["v32_roadmap_points"] -lt 60) {
        throw "Runtime verification report did not verify all sixty v32 roadmap points."
    }
    if ($Metrics.ContainsKey("public_demo_phase_relays") -and [int]$Metrics["public_demo_phase_relays"] -lt 3) {
        throw "Runtime verification report recorded too few public demo phase relays."
    }
    if ($Metrics.ContainsKey("public_demo_relay_overcharge_windows") -and [int]$Metrics["public_demo_relay_overcharge_windows"] -lt 1) {
        throw "Runtime verification report did not exercise relay overcharge windows."
    }
    if ($Metrics.ContainsKey("public_demo_combo_steps") -and [int]$Metrics["public_demo_combo_steps"] -lt 12) {
        throw "Runtime verification report recorded too few public demo combo steps."
    }
    if ($Metrics.ContainsKey("public_demo_phase_relay_complete") -and $Metrics["public_demo_phase_relay_complete"] -ne "true") {
        throw "Runtime verification report did not complete the phase relay objective."
    }
    if ($Metrics.ContainsKey("public_demo_complex_route_ready") -and $Metrics["public_demo_complex_route_ready"] -ne "true") {
        throw "Runtime verification report did not mark the complex public demo route ready."
    }
    if ($Metrics.ContainsKey("v33_playable_demo_points") -and [int]$Metrics["v33_playable_demo_points"] -lt 50) {
        throw "Runtime verification report did not verify all fifty v33 playable demo points."
    }
    if ($Metrics.ContainsKey("public_demo_collision_solves") -and [int]$Metrics["public_demo_collision_solves"] -lt 1) {
        throw "Runtime verification report did not exercise public demo collision solves."
    }
    if ($Metrics.ContainsKey("public_demo_traversal_actions") -and [int]$Metrics["public_demo_traversal_actions"] -lt 1) {
        throw "Runtime verification report did not exercise public demo traversal actions."
    }
    if ($Metrics.ContainsKey("public_demo_enemy_chases") -and [int]$Metrics["public_demo_enemy_chases"] -lt 1) {
        throw "Runtime verification report did not exercise public demo enemy chase behavior."
    }
    if ($Metrics.ContainsKey("public_demo_enemy_evades") -and [int]$Metrics["public_demo_enemy_evades"] -lt 1) {
        throw "Runtime verification report did not exercise public demo enemy evasion."
    }
    if ($Metrics.ContainsKey("public_demo_gamepad_frames") -and [int]$Metrics["public_demo_gamepad_frames"] -lt 1) {
        throw "Runtime verification report did not simulate gamepad input frames."
    }
    if ($Metrics.ContainsKey("public_demo_menu_transitions") -and [int]$Metrics["public_demo_menu_transitions"] -lt 1) {
        throw "Runtime verification report did not exercise menu transitions."
    }
    if ($Metrics.ContainsKey("public_demo_failure_presentations") -and [int]$Metrics["public_demo_failure_presentations"] -lt 1) {
        throw "Runtime verification report did not exercise failure presentation frames."
    }
    if ($Metrics.ContainsKey("public_demo_content_audio_cues") -and [int]$Metrics["public_demo_content_audio_cues"] -lt 1) {
        throw "Runtime verification report did not exercise content-backed audio cues."
    }
    if ($Metrics.ContainsKey("public_demo_animation_state_changes") -and [int]$Metrics["public_demo_animation_state_changes"] -lt 1) {
        throw "Runtime verification report did not exercise animation state changes."
    }
    if ($Metrics.ContainsKey("public_demo_enemy_behavior_ready") -and $Metrics["public_demo_enemy_behavior_ready"] -ne "true") {
        throw "Runtime verification report did not mark enemy behavior ready."
    }
    if ($Metrics.ContainsKey("public_demo_gamepad_menu_ready") -and $Metrics["public_demo_gamepad_menu_ready"] -ne "true") {
        throw "Runtime verification report did not mark gamepad/menu behavior ready."
    }
    if ($Metrics.ContainsKey("public_demo_content_audio_ready") -and $Metrics["public_demo_content_audio_ready"] -ne "true") {
        throw "Runtime verification report did not mark content-backed audio ready."
    }
    if ($Metrics.ContainsKey("public_demo_animation_hook_ready") -and $Metrics["public_demo_animation_hook_ready"] -ne "true") {
        throw "Runtime verification report did not mark animation hooks ready."
    }
    if ($Metrics.ContainsKey("v34_aaa_foundation_points") -and [int]$Metrics["v34_aaa_foundation_points"] -lt 60) {
        throw "Runtime verification report did not verify all sixty v34 AAA foundation points."
    }
    if ($Metrics.ContainsKey("public_demo_enemy_archetypes") -and [int]$Metrics["public_demo_enemy_archetypes"] -lt 3) {
        throw "Runtime verification report recorded too few public demo enemy archetypes."
    }
    foreach ($metric in @(
        "public_demo_enemy_los_checks",
        "public_demo_enemy_telegraphs",
        "public_demo_enemy_hit_reactions",
        "public_demo_controller_grounded_frames",
        "public_demo_controller_slope_samples",
        "public_demo_controller_material_samples",
        "public_demo_controller_moving_platform_frames",
        "public_demo_controller_camera_collision_frames",
        "public_demo_animation_clip_events",
        "public_demo_animation_blend_samples",
        "public_demo_accessibility_toggles"
    )) {
        if ($Metrics.ContainsKey($metric) -and [int]$Metrics[$metric] -lt 1) {
            throw "Runtime verification report did not record required v34 coverage for '$metric'."
        }
    }
    foreach ($metric in @(
        "public_demo_enemy_archetype_ready",
        "public_demo_enemy_los_ready",
        "public_demo_enemy_telegraph_ready",
        "public_demo_enemy_hit_reaction_ready",
        "public_demo_controller_polish_ready",
        "public_demo_animation_blend_tree_ready",
        "public_demo_accessibility_ready",
        "rendering_aaa_readiness",
        "production_aaa_readiness"
    )) {
        if ($Metrics.ContainsKey($metric) -and $Metrics[$metric] -ne "true") {
            throw "Runtime verification report did not mark v34 readiness metric '$metric' ready."
        }
    }
    if ($Metrics.ContainsKey("rendering_pipeline_readiness_items") -and [int]$Metrics["rendering_pipeline_readiness_items"] -lt 6) {
        throw "Runtime verification report recorded too few rendering pipeline readiness items."
    }
    if ($Metrics.ContainsKey("production_pipeline_readiness_items") -and [int]$Metrics["production_pipeline_readiness_items"] -lt 6) {
        throw "Runtime verification report recorded too few production pipeline readiness items."
    }
    if ($Metrics.ContainsKey("public_demo_blend_tree_clips") -and [int]$Metrics["public_demo_blend_tree_clips"] -lt 4) {
        throw "Runtime verification report recorded too few public demo blend-tree clips."
    }
    if ($Metrics.ContainsKey("public_demo_blend_tree_transitions") -and [int]$Metrics["public_demo_blend_tree_transitions"] -lt 4) {
        throw "Runtime verification report recorded too few public demo blend-tree transitions."
    }
}

if (Test-Path -LiteralPath $reportPath) {
    Remove-Item -LiteralPath $reportPath -Force -ErrorAction Ignore
}
if (Test-Path -LiteralPath $capturePath) {
    Remove-Item -LiteralPath $capturePath -Force -ErrorAction Ignore
}

$runtimeArguments = @(
    "--verify-runtime",
    "--verify-frames=$Frames",
    "--verify-cpu-budget-ms=$CpuFrameBudgetMs",
    "--verify-gpu-budget-ms=$GpuFrameBudgetMs",
    "--verify-pass-budget-ms=$PassBudgetMs",
    "--verify-replay=$ReplayPath",
    "--verify-baseline=$BaselinePath"
)
if ($DisableCapture) { $runtimeArguments += "--verify-no-capture" }
if ($DisableInputPlayback) { $runtimeArguments += "--verify-no-input-playback" }
if ($DisableBudgets) { $runtimeArguments += "--verify-no-budgets" }
if ($DisableBaseline) { $runtimeArguments += "--verify-no-baseline" }

$process = Start-Process -FilePath $resolvedExecutable -WorkingDirectory $workingDirectory -ArgumentList $runtimeArguments -PassThru
if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
    $process.Kill()
    $process.WaitForExit()
    throw "Runtime verification did not exit within $TimeoutSeconds second(s)."
}

if ($process.ExitCode -ne 0) {
    if (Test-Path -LiteralPath $reportPath) {
        Write-Host (Get-Content -LiteralPath $reportPath -Raw)
    }
    throw "Runtime verification exited with code $($process.ExitCode)."
}

if (!(Test-Path -LiteralPath $reportPath)) {
    throw "Runtime verification report was not written to $reportPath"
}

$report = Get-Content -LiteralPath $reportPath -Raw
if (!$report.StartsWith("PASS")) {
    Write-Host $report
    throw "Runtime verification report did not pass."
}

if (!$DisableCapture -and !(Test-Path -LiteralPath $capturePath)) {
    Write-Host $report
    throw "Runtime verification capture was not written to $capturePath"
}

$metrics = Get-ReportMetrics -Report $report
$requiredReportMetrics = Get-RequiredReportMetrics -SchemaPath $ReportSchemaPath
Assert-RuntimeReportSchema -Metrics $metrics -RequiredMetrics $requiredReportMetrics

if (!$DisableCapture -and !$DisableGoldenComparison) {
    $resolvedBaselinePath = Resolve-VerificationPath -Path $BaselinePath
    if (!(Test-Path -LiteralPath $resolvedBaselinePath)) {
        throw "Runtime verification baseline was not found at $resolvedBaselinePath"
    }

    $baselineValues = Read-BaselineValues -Path $resolvedBaselinePath
    $profileValues = @{}
    $detectedAdapterName = Get-DetectedAdapterName
    $defaultGoldenProfilePath = "Assets/Verification/GoldenProfiles/Default.dgoldenprofile"
    if ($GoldenProfilePath -eq $defaultGoldenProfilePath) {
        $safeAdapterName = Get-SafeProfileName -Name $detectedAdapterName
        $adapterProfileCandidate = Join-Path $root "Assets\Verification\GoldenProfiles\$safeAdapterName.dgoldenprofile"
        if (Test-Path -LiteralPath $adapterProfileCandidate) {
            $GoldenProfilePath = $adapterProfileCandidate
            Write-Host "Detected adapter '$detectedAdapterName'; using adapter golden tolerance profile $adapterProfileCandidate"
        }
        else {
            Write-Host "Detected adapter '$detectedAdapterName'; using default golden tolerance profile"
        }
    }
    $resolvedGoldenProfilePath = Resolve-VerificationPath -Path $GoldenProfilePath
    if (Test-Path -LiteralPath $resolvedGoldenProfilePath) {
        $profileValues = Read-BaselineValues -Path $resolvedGoldenProfilePath
        Write-Host "Using golden tolerance profile $resolvedGoldenProfilePath"
    }
    if (!$baselineValues.ContainsKey("golden_capture_path")) {
        throw "Runtime verification baseline $resolvedBaselinePath does not define golden_capture_path."
    }

    $goldenPath = Resolve-VerificationPath -Path $baselineValues["golden_capture_path"]
    $thumbnailWidth = Get-ProfiledInt -BaselineValues $baselineValues -ProfileValues $profileValues -Name "golden_thumbnail_width" -DefaultValue 64
    $thumbnailHeight = Get-ProfiledInt -BaselineValues $baselineValues -ProfileValues $profileValues -Name "golden_thumbnail_height" -DefaultValue 36
    $meanTolerance = Get-ProfiledDouble -BaselineValues $baselineValues -ProfileValues $profileValues -Name "golden_mean_tolerance" -DefaultValue 18.0
    $badPixelDelta = Get-ProfiledDouble -BaselineValues $baselineValues -ProfileValues $profileValues -Name "golden_bad_pixel_delta" -DefaultValue 42.0
    $badPixelRatio = Get-ProfiledDouble -BaselineValues $baselineValues -ProfileValues $profileValues -Name "golden_bad_pixel_ratio" -DefaultValue 0.35
    $safeSuiteName = [regex]::Replace($SuiteName.Trim(), "[^A-Za-z0-9_.-]", "_")
    if ([string]::IsNullOrWhiteSpace($safeSuiteName)) {
        $safeSuiteName = "Default"
    }

    $thumbnailPath = Join-Path $workingDirectory "Saved\Verification\runtime_capture_$safeSuiteName.ppm"
    $diffPath = Join-Path $workingDirectory "Saved\Verification\runtime_capture_${safeSuiteName}_diff.ppm"
    $compareArguments = @{
        CapturePath = $capturePath
        GoldenPath = $goldenPath
        ThumbnailPath = $thumbnailPath
        DiffPath = $diffPath
        ThumbnailWidth = $thumbnailWidth
        ThumbnailHeight = $thumbnailHeight
        MeanTolerance = $meanTolerance
        BadPixelDelta = $badPixelDelta
        BadPixelRatio = $badPixelRatio
    }
    if ($UpdateGoldenCapture) {
        $compareArguments["UpdateGolden"] = $true
    }

    & (Join-Path $PSScriptRoot "CompareCaptureDisparity.ps1") @compareArguments
}

if (!$DisablePerfHistory) {
    $historyParent = Split-Path -Parent $HistoryPath
    if (![string]::IsNullOrWhiteSpace($historyParent)) {
        New-Item -ItemType Directory -Force -Path $historyParent | Out-Null
    }

    $historyHeader = "timestamp,suite,executable,version,frames,cpu_frame_max_ms,cpu_frame_avg_ms,gpu_frame_max_ms,gpu_frame_avg_ms,pass_cpu_max_ms,pass_cpu_max_name,pass_gpu_max_ms,pass_gpu_max_name,capture_average_luma,capture_checksum,playback_distance,playback_net_distance,editor_pick_tests,editor_pick_failures,gizmo_pick_tests,gizmo_pick_failures,gizmo_drag_tests,gizmo_drag_failures,scene_reload_tests,scene_save_tests,post_debug_view_tests,showcase_frames,trailer_frames,high_res_capture_tests,rift_vfx_draws,rift_vfx_gpu_simulation_batches,rift_vfx_motion_vector_candidates,rift_vfx_temporal_reprojection_samples,rift_vfx_depth_fade_particles,audio_beat_pulses,audio_snapshot_tests,async_io_tests,material_texture_slot_tests,prefab_variant_tests,shot_director_tests,shot_spline_tests,shot_timeline_track_tests,shot_thumbnail_tests,shot_preview_scrub_tests,audio_analysis_tests,xaudio2_backend_tests,vfx_system_tests,gpu_vfx_simulation_tests,animation_skinning_tests,gpu_pick_hover_cache_tests,gpu_pick_latency_histogram_tests,graph_high_res_capture_tests,cooked_package_tests,asset_invalidation_tests,nested_prefab_tests,audio_production_tests,viewport_overlay_tests,high_res_resolve_tests,viewport_hud_control_tests,transform_precision_tests,command_history_filter_tests,runtime_schema_manifest_tests,shot_sequencer_tests,vfx_renderer_profile_tests,cooked_gpu_resource_tests,dependency_invalidation_tests,audio_meter_calibration_tests,release_readiness_tests,v25_production_points,editor_preference_persistence_tests,viewport_toolbar_tests,viewport_toolbar_interactions,editor_preference_profile_tests,capture_preset_tests,vfx_emitter_profile_tests,cooked_dependency_preview_tests,v28_diversified_points,editor_workflow_tests,asset_pipeline_promotion_tests,rendering_advanced_tests,runtime_sequencer_feature_tests,audio_production_feature_tests,production_publishing_tests,editor_profile_diff_fields,asset_streaming_priority_levels,rendering_motion_vector_targets,runtime_keyboard_preview_bindings,audio_content_pulse_inputs,production_obs_websocket_commands,rift_vfx_emitter_count,rift_vfx_gpu_buffer_bytes,cooked_package_dependency_preview_count,render_graph_allocations,render_graph_aliased_resources,render_graph_barriers,render_graph_resource_bindings,render_graph_bind_hits,render_graph_bind_misses,render_graph_callbacks_bound,render_graph_callbacks_executed,render_graph_dispatch_valid,object_id_readback_ring_size,object_id_readback_pending,object_id_readback_requests,object_id_readback_completions,object_id_readback_latency_frames,object_id_readback_busy_skips,gpu_pick_cache_hits,gpu_pick_latency_samples,gpu_pick_stale_frames,graph_high_res_capture_targets,graph_high_res_capture_tiles,graph_high_res_capture_msaa_samples,graph_high_res_capture_passes,high_res_capture_preset,high_res_resolve_filter,high_res_resolve_samples,editor_viewport_ready,editor_viewport_presented_in_imgui,editor_object_id_ready,editor_object_depth_ready,audio_xaudio2_available,audio_xaudio2_initialized,audio_analysis_peak,audio_analysis_beat_envelope,audio_mixer_voices_created,audio_spatial_emitters,audio_analysis_content_pulses,v29_public_demo_points,public_demo_tests,public_demo_shard_pickups,public_demo_completions,public_demo_hud_frames,public_demo_beacon_draws,public_demo_sentinel_ticks,public_demo_stability,public_demo_focus,v30_vertical_slice_points,public_demo_objective_stages,public_demo_anchor_activations,public_demo_retries,public_demo_checkpoints,public_demo_director_frames,public_demo_anchors_activated,public_demo_event_count,public_demo_objective_distance,v31_diversified_points,public_demo_resonance_gates,public_demo_pressure_hits,public_demo_footstep_events,public_demo_resonance_gates_tuned,public_demo_gameplay_event_routes,v32_roadmap_points,public_demo_phase_relays,public_demo_relay_overcharge_windows,public_demo_combo_steps,public_demo_phase_relays_stabilized,public_demo_relay_bridge_draws,public_demo_combo_chain_steps,v33_playable_demo_points,public_demo_collision_solves,public_demo_traversal_actions,public_demo_enemy_chases,public_demo_enemy_evades,public_demo_gamepad_frames,public_demo_menu_transitions,public_demo_failure_presentations,public_demo_content_audio_cues,public_demo_animation_state_changes,v34_aaa_foundation_points,public_demo_enemy_archetypes,public_demo_enemy_los_checks,public_demo_enemy_telegraphs,public_demo_enemy_hit_reactions,public_demo_controller_grounded_frames,public_demo_controller_slope_samples,public_demo_controller_material_samples,public_demo_controller_moving_platform_frames,public_demo_controller_camera_collision_frames,public_demo_animation_clip_events,public_demo_animation_blend_samples,public_demo_accessibility_toggles,rendering_pipeline_readiness_items,production_pipeline_readiness_items"
    if (Test-Path -LiteralPath $HistoryPath) {
        $currentHeader = Get-Content -LiteralPath $HistoryPath -First 1
        if ($currentHeader -ne $historyHeader) {
            $legacyPath = "$HistoryPath.legacy"
            Copy-Item -LiteralPath $HistoryPath -Destination $legacyPath -Force
            Write-Host "Archived legacy performance history at $legacyPath"
            $historyHeader | Set-Content -LiteralPath $HistoryPath
        }
    }
    else {
        $historyHeader |
            Set-Content -LiteralPath $HistoryPath
    }

    $row = @(
        (Get-Date).ToString("o"),
        $SuiteName,
        $resolvedExecutable,
        $metrics["version"],
        $metrics["frames"],
        $metrics["cpu_frame_max_ms"],
        $metrics["cpu_frame_avg_ms"],
        $metrics["gpu_frame_max_ms"],
        $metrics["gpu_frame_avg_ms"],
        $metrics["pass_cpu_max_ms"],
        $metrics["pass_cpu_max_name"],
        $metrics["pass_gpu_max_ms"],
        $metrics["pass_gpu_max_name"],
        $metrics["capture_average_luma"],
        $metrics["capture_checksum"],
        $metrics["playback_distance"],
        $metrics["playback_net_distance"],
        $metrics["editor_pick_tests"],
        $metrics["editor_pick_failures"],
        $metrics["gizmo_pick_tests"],
        $metrics["gizmo_pick_failures"],
        $metrics["gizmo_drag_tests"],
        $metrics["gizmo_drag_failures"],
        $metrics["scene_reload_tests"],
        $metrics["scene_save_tests"],
        $metrics["post_debug_view_tests"],
        $metrics["showcase_frames"],
        $metrics["trailer_frames"],
        $metrics["high_res_capture_tests"],
        $metrics["rift_vfx_draws"],
        $metrics["rift_vfx_gpu_simulation_batches"],
        $metrics["rift_vfx_motion_vector_candidates"],
        $metrics["rift_vfx_temporal_reprojection_samples"],
        $metrics["rift_vfx_depth_fade_particles"],
        $metrics["audio_beat_pulses"],
        $metrics["audio_snapshot_tests"],
        $metrics["async_io_tests"],
        $metrics["material_texture_slot_tests"],
        $metrics["prefab_variant_tests"],
        $metrics["shot_director_tests"],
        $metrics["shot_spline_tests"],
        $metrics["shot_timeline_track_tests"],
        $metrics["shot_thumbnail_tests"],
        $metrics["shot_preview_scrub_tests"],
        $metrics["audio_analysis_tests"],
        $metrics["xaudio2_backend_tests"],
        $metrics["vfx_system_tests"],
        $metrics["gpu_vfx_simulation_tests"],
        $metrics["animation_skinning_tests"],
        $metrics["gpu_pick_hover_cache_tests"],
        $metrics["gpu_pick_latency_histogram_tests"],
        $metrics["graph_high_res_capture_tests"],
        $metrics["cooked_package_tests"],
        $metrics["asset_invalidation_tests"],
        $metrics["nested_prefab_tests"],
        $metrics["audio_production_tests"],
        $metrics["viewport_overlay_tests"],
        $metrics["high_res_resolve_tests"],
        $metrics["viewport_hud_control_tests"],
        $metrics["transform_precision_tests"],
        $metrics["command_history_filter_tests"],
        $metrics["runtime_schema_manifest_tests"],
        $metrics["shot_sequencer_tests"],
        $metrics["vfx_renderer_profile_tests"],
        $metrics["cooked_gpu_resource_tests"],
        $metrics["dependency_invalidation_tests"],
        $metrics["audio_meter_calibration_tests"],
        $metrics["release_readiness_tests"],
        $metrics["v25_production_points"],
        $metrics["editor_preference_persistence_tests"],
        $metrics["viewport_toolbar_tests"],
        $metrics["viewport_toolbar_interactions"],
        $metrics["editor_preference_profile_tests"],
        $metrics["capture_preset_tests"],
        $metrics["vfx_emitter_profile_tests"],
        $metrics["cooked_dependency_preview_tests"],
        $metrics["v28_diversified_points"],
        $metrics["editor_workflow_tests"],
        $metrics["asset_pipeline_promotion_tests"],
        $metrics["rendering_advanced_tests"],
        $metrics["runtime_sequencer_feature_tests"],
        $metrics["audio_production_feature_tests"],
        $metrics["production_publishing_tests"],
        $metrics["editor_profile_diff_fields"],
        $metrics["asset_streaming_priority_levels"],
        $metrics["rendering_motion_vector_targets"],
        $metrics["runtime_keyboard_preview_bindings"],
        $metrics["audio_content_pulse_inputs"],
        $metrics["production_obs_websocket_commands"],
        $metrics["rift_vfx_emitter_count"],
        $metrics["rift_vfx_gpu_buffer_bytes"],
        $metrics["cooked_package_dependency_preview_count"],
        $metrics["render_graph_allocations"],
        $metrics["render_graph_aliased_resources"],
        $metrics["render_graph_barriers"],
        $metrics["render_graph_resource_bindings"],
        $metrics["render_graph_bind_hits"],
        $metrics["render_graph_bind_misses"],
        $metrics["render_graph_callbacks_bound"],
        $metrics["render_graph_callbacks_executed"],
        $metrics["render_graph_dispatch_valid"],
        $metrics["object_id_readback_ring_size"],
        $metrics["object_id_readback_pending"],
        $metrics["object_id_readback_requests"],
        $metrics["object_id_readback_completions"],
        $metrics["object_id_readback_latency_frames"],
        $metrics["object_id_readback_busy_skips"],
        $metrics["gpu_pick_cache_hits"],
        $metrics["gpu_pick_latency_samples"],
        $metrics["gpu_pick_stale_frames"],
        $metrics["graph_high_res_capture_targets"],
        $metrics["graph_high_res_capture_tiles"],
        $metrics["graph_high_res_capture_msaa_samples"],
        $metrics["graph_high_res_capture_passes"],
        $metrics["high_res_capture_preset"],
        $metrics["high_res_resolve_filter"],
        $metrics["high_res_resolve_samples"],
        $metrics["editor_viewport_ready"],
        $metrics["editor_viewport_presented_in_imgui"],
        $metrics["editor_object_id_ready"],
        $metrics["editor_object_depth_ready"],
        $metrics["audio_xaudio2_available"],
        $metrics["audio_xaudio2_initialized"],
        $metrics["audio_analysis_peak"],
        $metrics["audio_analysis_beat_envelope"],
        $metrics["audio_mixer_voices_created"],
        $metrics["audio_spatial_emitters"],
        $metrics["audio_analysis_content_pulses"],
        $metrics["v29_public_demo_points"],
        $metrics["public_demo_tests"],
        $metrics["public_demo_shard_pickups"],
        $metrics["public_demo_completions"],
        $metrics["public_demo_hud_frames"],
        $metrics["public_demo_beacon_draws"],
        $metrics["public_demo_sentinel_ticks"],
        $metrics["public_demo_stability"],
        $metrics["public_demo_focus"],
        $metrics["v30_vertical_slice_points"],
        $metrics["public_demo_objective_stages"],
        $metrics["public_demo_anchor_activations"],
        $metrics["public_demo_retries"],
        $metrics["public_demo_checkpoints"],
        $metrics["public_demo_director_frames"],
        $metrics["public_demo_anchors_activated"],
        $metrics["public_demo_event_count"],
        $metrics["public_demo_objective_distance"],
        $metrics["v31_diversified_points"],
        $metrics["public_demo_resonance_gates"],
        $metrics["public_demo_pressure_hits"],
        $metrics["public_demo_footstep_events"],
        $metrics["public_demo_resonance_gates_tuned"],
        $metrics["public_demo_gameplay_event_routes"],
        $metrics["v32_roadmap_points"],
        $metrics["public_demo_phase_relays"],
        $metrics["public_demo_relay_overcharge_windows"],
        $metrics["public_demo_combo_steps"],
        $metrics["public_demo_phase_relays_stabilized"],
        $metrics["public_demo_relay_bridge_draws"],
        $metrics["public_demo_combo_chain_steps"],
        $metrics["v33_playable_demo_points"],
        $metrics["public_demo_collision_solves"],
        $metrics["public_demo_traversal_actions"],
        $metrics["public_demo_enemy_chases"],
        $metrics["public_demo_enemy_evades"],
        $metrics["public_demo_gamepad_frames"],
        $metrics["public_demo_menu_transitions"],
        $metrics["public_demo_failure_presentations"],
        $metrics["public_demo_content_audio_cues"],
        $metrics["public_demo_animation_state_changes"],
        $metrics["v34_aaa_foundation_points"],
        $metrics["public_demo_enemy_archetypes"],
        $metrics["public_demo_enemy_los_checks"],
        $metrics["public_demo_enemy_telegraphs"],
        $metrics["public_demo_enemy_hit_reactions"],
        $metrics["public_demo_controller_grounded_frames"],
        $metrics["public_demo_controller_slope_samples"],
        $metrics["public_demo_controller_material_samples"],
        $metrics["public_demo_controller_moving_platform_frames"],
        $metrics["public_demo_controller_camera_collision_frames"],
        $metrics["public_demo_animation_clip_events"],
        $metrics["public_demo_animation_blend_samples"],
        $metrics["public_demo_accessibility_toggles"],
        $metrics["rendering_pipeline_readiness_items"],
        $metrics["production_pipeline_readiness_items"]
    ) | ForEach-Object {
        '"' + ([string]$_ -replace '"', '""') + '"'
    }
    Add-Content -LiteralPath $HistoryPath -Value ($row -join ",")
}

Write-Host "Runtime verification passed for $resolvedExecutable"
Write-Host $report
