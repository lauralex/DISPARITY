param(
    [string]$HistoryPath = "Saved/Verification/performance_history.csv",
    [string]$BaselinePath = "Assets/Verification/PerformanceBaselines.dperf",
    [int]$Recent = 8,
    [double]$CpuRegressionMs = 8.0,
    [double]$GpuRegressionMs = 8.0,
    [switch]$DisableBaselineComparison,
    [switch]$UpdateBaseline
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($HistoryPath)) {
    $HistoryPath = Join-Path $root $HistoryPath
}
if (![System.IO.Path]::IsPathRooted($BaselinePath)) {
    $BaselinePath = Join-Path $root $BaselinePath
}

if (!(Test-Path -LiteralPath $HistoryPath)) {
    Write-Host "No performance history found at $HistoryPath"
    return
}

$rows = @(Import-Csv -LiteralPath $HistoryPath)
if ($rows.Count -eq 0) {
    Write-Host "Performance history is empty at $HistoryPath"
    return
}

function Get-Median {
    param([double[]]$Values)

    if ($Values.Count -eq 0) {
        return 0.0
    }

    $ordered = @($Values | Sort-Object)
    $middle = [int]($ordered.Count / 2)
    if (($ordered.Count % 2) -eq 1) {
        return [double]$ordered[$middle]
    }

    return ([double]$ordered[$middle - 1] + [double]$ordered[$middle]) / 2.0
}

Write-Host "Performance history: $HistoryPath"
$baselines = @{}
if (!$DisableBaselineComparison -and (Test-Path -LiteralPath $BaselinePath)) {
    foreach ($line in Get-Content -LiteralPath $BaselinePath) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
            continue
        }

        $parts = $trimmed -split "\|"
        if ($parts.Count -lt 6) {
            throw "Invalid performance baseline line in ${BaselinePath}: $line"
        }

        $baselines[$parts[0].Trim()] = [pscustomobject]@{
            CpuMaxMs = [double]$parts[1].Trim()
            GpuMaxMs = [double]$parts[2].Trim()
            PassCpuMaxMs = [double]$parts[3].Trim()
            CpuRegressionMs = [double]$parts[4].Trim()
            GpuRegressionMs = [double]$parts[5].Trim()
        }
    }
    Write-Host "Performance baseline: $BaselinePath"
}

$latestBySuite = @{}
$groups = $rows | Group-Object {
    $suite = $_.suite
    if ([string]::IsNullOrWhiteSpace($suite)) {
        $suite = "Default"
    }

    $executable = $_.executable
    if ([string]::IsNullOrWhiteSpace($executable)) {
        $executable = "Unknown"
    }

    "$suite|$executable"
}

foreach ($group in $groups) {
    $orderedRows = @($group.Group | Sort-Object { [datetime]$_.timestamp })
    $latestRows = @($orderedRows | Select-Object -Last $Recent)
    $latest = $latestRows[-1]
    $suiteName = $latest.suite
    if ([string]::IsNullOrWhiteSpace($suiteName)) {
        $suiteName = "Default"
    }

    $label = "$suiteName :: $($latest.executable)"
    $latestCpu = [double]$latest.cpu_frame_max_ms
    $latestGpu = [double]$latest.gpu_frame_max_ms
    $latestPassCpu = if ($latest.PSObject.Properties.Name -contains "pass_cpu_max_ms" -and ![string]::IsNullOrWhiteSpace($latest.pass_cpu_max_ms)) { [double]$latest.pass_cpu_max_ms } else { 0.0 }
    $latestLuma = [double]$latest.capture_average_luma
    $editorPicks = if ($latest.PSObject.Properties.Name -contains "editor_pick_tests" -and ![string]::IsNullOrWhiteSpace($latest.editor_pick_tests)) { [int]$latest.editor_pick_tests } else { 0 }
    $gizmoPicks = if ($latest.PSObject.Properties.Name -contains "gizmo_pick_tests" -and ![string]::IsNullOrWhiteSpace($latest.gizmo_pick_tests)) { [int]$latest.gizmo_pick_tests } else { 0 }
    $gizmoDrags = if ($latest.PSObject.Properties.Name -contains "gizmo_drag_tests" -and ![string]::IsNullOrWhiteSpace($latest.gizmo_drag_tests)) { [int]$latest.gizmo_drag_tests } else { 0 }
    $postDebugViews = if ($latest.PSObject.Properties.Name -contains "post_debug_view_tests" -and ![string]::IsNullOrWhiteSpace($latest.post_debug_view_tests)) { [int]$latest.post_debug_view_tests } else { 0 }
    $showcaseFrames = if ($latest.PSObject.Properties.Name -contains "showcase_frames" -and ![string]::IsNullOrWhiteSpace($latest.showcase_frames)) { [int]$latest.showcase_frames } else { 0 }
    $trailerFrames = if ($latest.PSObject.Properties.Name -contains "trailer_frames" -and ![string]::IsNullOrWhiteSpace($latest.trailer_frames)) { [int]$latest.trailer_frames } else { 0 }
    $highResCaptures = if ($latest.PSObject.Properties.Name -contains "high_res_capture_tests" -and ![string]::IsNullOrWhiteSpace($latest.high_res_capture_tests)) { [int]$latest.high_res_capture_tests } else { 0 }
    $riftVfxDraws = if ($latest.PSObject.Properties.Name -contains "rift_vfx_draws" -and ![string]::IsNullOrWhiteSpace($latest.rift_vfx_draws)) { [int]$latest.rift_vfx_draws } else { 0 }
    $audioBeatPulses = if ($latest.PSObject.Properties.Name -contains "audio_beat_pulses" -and ![string]::IsNullOrWhiteSpace($latest.audio_beat_pulses)) { [int]$latest.audio_beat_pulses } else { 0 }
    $asyncIoTests = if ($latest.PSObject.Properties.Name -contains "async_io_tests" -and ![string]::IsNullOrWhiteSpace($latest.async_io_tests)) { [int]$latest.async_io_tests } else { 0 }
    $shotDirectorTests = if ($latest.PSObject.Properties.Name -contains "shot_director_tests" -and ![string]::IsNullOrWhiteSpace($latest.shot_director_tests)) { [int]$latest.shot_director_tests } else { 0 }
    $animationSkinningTests = if ($latest.PSObject.Properties.Name -contains "animation_skinning_tests" -and ![string]::IsNullOrWhiteSpace($latest.animation_skinning_tests)) { [int]$latest.animation_skinning_tests } else { 0 }
    $graphCallbacks = if ($latest.PSObject.Properties.Name -contains "render_graph_callbacks_executed" -and ![string]::IsNullOrWhiteSpace($latest.render_graph_callbacks_executed)) { [int]$latest.render_graph_callbacks_executed } else { 0 }
    $viewportOverlayTests = if ($latest.PSObject.Properties.Name -contains "viewport_overlay_tests" -and ![string]::IsNullOrWhiteSpace($latest.viewport_overlay_tests)) { [int]$latest.viewport_overlay_tests } else { 0 }
    $highResResolveTests = if ($latest.PSObject.Properties.Name -contains "high_res_resolve_tests" -and ![string]::IsNullOrWhiteSpace($latest.high_res_resolve_tests)) { [int]$latest.high_res_resolve_tests } else { 0 }
    $runtimeSchemaTests = if ($latest.PSObject.Properties.Name -contains "runtime_schema_manifest_tests" -and ![string]::IsNullOrWhiteSpace($latest.runtime_schema_manifest_tests)) { [int]$latest.runtime_schema_manifest_tests } else { 0 }
    $releaseReadinessTests = if ($latest.PSObject.Properties.Name -contains "release_readiness_tests" -and ![string]::IsNullOrWhiteSpace($latest.release_readiness_tests)) { [int]$latest.release_readiness_tests } else { 0 }
    $v25ProductionPoints = if ($latest.PSObject.Properties.Name -contains "v25_production_points" -and ![string]::IsNullOrWhiteSpace($latest.v25_production_points)) { [int]$latest.v25_production_points } else { 0 }
    $editorPreferenceTests = if ($latest.PSObject.Properties.Name -contains "editor_preference_persistence_tests" -and ![string]::IsNullOrWhiteSpace($latest.editor_preference_persistence_tests)) { [int]$latest.editor_preference_persistence_tests } else { 0 }
    $viewportToolbarTests = if ($latest.PSObject.Properties.Name -contains "viewport_toolbar_tests" -and ![string]::IsNullOrWhiteSpace($latest.viewport_toolbar_tests)) { [int]$latest.viewport_toolbar_tests } else { 0 }
    $viewportToolbarInteractions = if ($latest.PSObject.Properties.Name -contains "viewport_toolbar_interactions" -and ![string]::IsNullOrWhiteSpace($latest.viewport_toolbar_interactions)) { [int]$latest.viewport_toolbar_interactions } else { 0 }
    $editorPreferenceProfiles = if ($latest.PSObject.Properties.Name -contains "editor_preference_profile_tests" -and ![string]::IsNullOrWhiteSpace($latest.editor_preference_profile_tests)) { [int]$latest.editor_preference_profile_tests } else { 0 }
    $capturePresetTests = if ($latest.PSObject.Properties.Name -contains "capture_preset_tests" -and ![string]::IsNullOrWhiteSpace($latest.capture_preset_tests)) { [int]$latest.capture_preset_tests } else { 0 }
    $vfxEmitterProfiles = if ($latest.PSObject.Properties.Name -contains "vfx_emitter_profile_tests" -and ![string]::IsNullOrWhiteSpace($latest.vfx_emitter_profile_tests)) { [int]$latest.vfx_emitter_profile_tests } else { 0 }
    $cookedDependencyPreviews = if ($latest.PSObject.Properties.Name -contains "cooked_dependency_preview_tests" -and ![string]::IsNullOrWhiteSpace($latest.cooked_dependency_preview_tests)) { [int]$latest.cooked_dependency_preview_tests } else { 0 }
    $v28DiversifiedPoints = if ($latest.PSObject.Properties.Name -contains "v28_diversified_points" -and ![string]::IsNullOrWhiteSpace($latest.v28_diversified_points)) { [int]$latest.v28_diversified_points } else { 0 }
    $editorWorkflowTests = if ($latest.PSObject.Properties.Name -contains "editor_workflow_tests" -and ![string]::IsNullOrWhiteSpace($latest.editor_workflow_tests)) { [int]$latest.editor_workflow_tests } else { 0 }
    $assetPipelinePromotionTests = if ($latest.PSObject.Properties.Name -contains "asset_pipeline_promotion_tests" -and ![string]::IsNullOrWhiteSpace($latest.asset_pipeline_promotion_tests)) { [int]$latest.asset_pipeline_promotion_tests } else { 0 }
    $renderingAdvancedTests = if ($latest.PSObject.Properties.Name -contains "rendering_advanced_tests" -and ![string]::IsNullOrWhiteSpace($latest.rendering_advanced_tests)) { [int]$latest.rendering_advanced_tests } else { 0 }
    $runtimeSequencerFeatureTests = if ($latest.PSObject.Properties.Name -contains "runtime_sequencer_feature_tests" -and ![string]::IsNullOrWhiteSpace($latest.runtime_sequencer_feature_tests)) { [int]$latest.runtime_sequencer_feature_tests } else { 0 }
    $audioProductionFeatureTests = if ($latest.PSObject.Properties.Name -contains "audio_production_feature_tests" -and ![string]::IsNullOrWhiteSpace($latest.audio_production_feature_tests)) { [int]$latest.audio_production_feature_tests } else { 0 }
    $productionPublishingTests = if ($latest.PSObject.Properties.Name -contains "production_publishing_tests" -and ![string]::IsNullOrWhiteSpace($latest.production_publishing_tests)) { [int]$latest.production_publishing_tests } else { 0 }
    $v29PublicDemoPoints = if ($latest.PSObject.Properties.Name -contains "v29_public_demo_points" -and ![string]::IsNullOrWhiteSpace($latest.v29_public_demo_points)) { [int]$latest.v29_public_demo_points } else { 0 }
    $publicDemoTests = if ($latest.PSObject.Properties.Name -contains "public_demo_tests" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_tests)) { [int]$latest.public_demo_tests } else { 0 }
    $publicDemoPickups = if ($latest.PSObject.Properties.Name -contains "public_demo_shard_pickups" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_shard_pickups)) { [int]$latest.public_demo_shard_pickups } else { 0 }
    $publicDemoCompletions = if ($latest.PSObject.Properties.Name -contains "public_demo_completions" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_completions)) { [int]$latest.public_demo_completions } else { 0 }
    $v30VerticalSlicePoints = if ($latest.PSObject.Properties.Name -contains "v30_vertical_slice_points" -and ![string]::IsNullOrWhiteSpace($latest.v30_vertical_slice_points)) { [int]$latest.v30_vertical_slice_points } else { 0 }
    $publicDemoStages = if ($latest.PSObject.Properties.Name -contains "public_demo_objective_stages" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_objective_stages)) { [int]$latest.public_demo_objective_stages } else { 0 }
    $publicDemoAnchors = if ($latest.PSObject.Properties.Name -contains "public_demo_anchor_activations" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_anchor_activations)) { [int]$latest.public_demo_anchor_activations } else { 0 }
    $publicDemoRetries = if ($latest.PSObject.Properties.Name -contains "public_demo_retries" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_retries)) { [int]$latest.public_demo_retries } else { 0 }
    $v31DiversifiedPoints = if ($latest.PSObject.Properties.Name -contains "v31_diversified_points" -and ![string]::IsNullOrWhiteSpace($latest.v31_diversified_points)) { [int]$latest.v31_diversified_points } else { 0 }
    $publicDemoResonanceGates = if ($latest.PSObject.Properties.Name -contains "public_demo_resonance_gates" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_resonance_gates)) { [int]$latest.public_demo_resonance_gates } else { 0 }
    $publicDemoPressureHits = if ($latest.PSObject.Properties.Name -contains "public_demo_pressure_hits" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_pressure_hits)) { [int]$latest.public_demo_pressure_hits } else { 0 }
    $publicDemoFootsteps = if ($latest.PSObject.Properties.Name -contains "public_demo_footstep_events" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_footstep_events)) { [int]$latest.public_demo_footstep_events } else { 0 }
    $v32RoadmapPoints = if ($latest.PSObject.Properties.Name -contains "v32_roadmap_points" -and ![string]::IsNullOrWhiteSpace($latest.v32_roadmap_points)) { [int]$latest.v32_roadmap_points } else { 0 }
    $publicDemoPhaseRelays = if ($latest.PSObject.Properties.Name -contains "public_demo_phase_relays" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_phase_relays)) { [int]$latest.public_demo_phase_relays } else { 0 }
    $publicDemoRelayOvercharge = if ($latest.PSObject.Properties.Name -contains "public_demo_relay_overcharge_windows" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_relay_overcharge_windows)) { [int]$latest.public_demo_relay_overcharge_windows } else { 0 }
    $publicDemoComboSteps = if ($latest.PSObject.Properties.Name -contains "public_demo_combo_steps" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_combo_steps)) { [int]$latest.public_demo_combo_steps } else { 0 }
    $v33PlayableDemoPoints = if ($latest.PSObject.Properties.Name -contains "v33_playable_demo_points" -and ![string]::IsNullOrWhiteSpace($latest.v33_playable_demo_points)) { [int]$latest.v33_playable_demo_points } else { 0 }
    $publicDemoCollisionSolves = if ($latest.PSObject.Properties.Name -contains "public_demo_collision_solves" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_collision_solves)) { [int]$latest.public_demo_collision_solves } else { 0 }
    $publicDemoTraversalActions = if ($latest.PSObject.Properties.Name -contains "public_demo_traversal_actions" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_traversal_actions)) { [int]$latest.public_demo_traversal_actions } else { 0 }
    $publicDemoEnemyChases = if ($latest.PSObject.Properties.Name -contains "public_demo_enemy_chases" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_enemy_chases)) { [int]$latest.public_demo_enemy_chases } else { 0 }
    $publicDemoGamepadFrames = if ($latest.PSObject.Properties.Name -contains "public_demo_gamepad_frames" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_gamepad_frames)) { [int]$latest.public_demo_gamepad_frames } else { 0 }
    $publicDemoContentAudioCues = if ($latest.PSObject.Properties.Name -contains "public_demo_content_audio_cues" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_content_audio_cues)) { [int]$latest.public_demo_content_audio_cues } else { 0 }
    $v34AAAFoundationPoints = if ($latest.PSObject.Properties.Name -contains "v34_aaa_foundation_points" -and ![string]::IsNullOrWhiteSpace($latest.v34_aaa_foundation_points)) { [int]$latest.v34_aaa_foundation_points } else { 0 }
    $publicDemoEnemyArchetypes = if ($latest.PSObject.Properties.Name -contains "public_demo_enemy_archetypes" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_enemy_archetypes)) { [int]$latest.public_demo_enemy_archetypes } else { 0 }
    $publicDemoEnemyTelegraphs = if ($latest.PSObject.Properties.Name -contains "public_demo_enemy_telegraphs" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_enemy_telegraphs)) { [int]$latest.public_demo_enemy_telegraphs } else { 0 }
    $publicDemoControllerGrounded = if ($latest.PSObject.Properties.Name -contains "public_demo_controller_grounded_frames" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_controller_grounded_frames)) { [int]$latest.public_demo_controller_grounded_frames } else { 0 }
    $publicDemoAnimationClipEvents = if ($latest.PSObject.Properties.Name -contains "public_demo_animation_clip_events" -and ![string]::IsNullOrWhiteSpace($latest.public_demo_animation_clip_events)) { [int]$latest.public_demo_animation_clip_events } else { 0 }
    $renderingReadinessItems = if ($latest.PSObject.Properties.Name -contains "rendering_pipeline_readiness_items" -and ![string]::IsNullOrWhiteSpace($latest.rendering_pipeline_readiness_items)) { [int]$latest.rendering_pipeline_readiness_items } else { 0 }
    $productionReadinessItems = if ($latest.PSObject.Properties.Name -contains "production_pipeline_readiness_items" -and ![string]::IsNullOrWhiteSpace($latest.production_pipeline_readiness_items)) { [int]$latest.production_pipeline_readiness_items } else { 0 }
    $v35EngineArchitecturePoints = if ($latest.PSObject.Properties.Name -contains "v35_engine_architecture_points" -and ![string]::IsNullOrWhiteSpace($latest.v35_engine_architecture_points)) { [int]$latest.v35_engine_architecture_points } else { 0 }
    $engineSchedulerTasks = if ($latest.PSObject.Properties.Name -contains "engine_scheduler_tasks" -and ![string]::IsNullOrWhiteSpace($latest.engine_scheduler_tasks)) { [int]$latest.engine_scheduler_tasks } else { 0 }
    $engineSceneQueryHits = if ($latest.PSObject.Properties.Name -contains "engine_scene_query_hits" -and ![string]::IsNullOrWhiteSpace($latest.engine_scene_query_hits)) { [int]$latest.engine_scene_query_hits } else { 0 }
    $engineGraphBudgetChecks = if ($latest.PSObject.Properties.Name -contains "engine_render_graph_budget_checks" -and ![string]::IsNullOrWhiteSpace($latest.engine_render_graph_budget_checks)) { [int]$latest.engine_render_graph_budget_checks } else { 0 }

    Write-Host ("{0}: latest cpu_max={1:N3}ms gpu_max={2:N3}ms pass_cpu_max={3:N3}ms luma={4:N2} editor_picks={5} gizmo_picks={6} gizmo_drags={7} post_debug={8} showcase_frames={9} trailer_frames={10} high_res={11} rift_vfx={12} beat_pulses={13} async_io={14} shot_director={15} animation_skinning={16} graph_callbacks={17} viewport_overlay={18} high_resolve={19} schema={20} release={21} v25={22} prefs={23} toolbar={24}/{25} profiles={26} capture_presets={27} vfx_emitters={28} cookdeps={29} v28={30} editor_workflow={31} asset_promo={32} render_adv={33} runtime_seq={34} audio_prod={35} publish={36} v29={37} demo={38} pickups={39} completions={40} v30={41} stages={42} anchors={43} retries={44} v31={45} gates={46} pressure={47} footsteps={48} v32={49} relays={50} overcharge={51} combo={52} v33={53} collision={54} traversal={55} enemy={56} gamepad={57} cues={58} v34={59} archetypes={60} telegraphs={61} controller={62} anim_events={63} render_ready={64} production_ready={65} v35={66} scheduler={67} scene_queries={68} graph_budget={69}" -f $label, $latestCpu, $latestGpu, $latestPassCpu, $latestLuma, $editorPicks, $gizmoPicks, $gizmoDrags, $postDebugViews, $showcaseFrames, $trailerFrames, $highResCaptures, $riftVfxDraws, $audioBeatPulses, $asyncIoTests, $shotDirectorTests, $animationSkinningTests, $graphCallbacks, $viewportOverlayTests, $highResResolveTests, $runtimeSchemaTests, $releaseReadinessTests, $v25ProductionPoints, $editorPreferenceTests, $viewportToolbarTests, $viewportToolbarInteractions, $editorPreferenceProfiles, $capturePresetTests, $vfxEmitterProfiles, $cookedDependencyPreviews, $v28DiversifiedPoints, $editorWorkflowTests, $assetPipelinePromotionTests, $renderingAdvancedTests, $runtimeSequencerFeatureTests, $audioProductionFeatureTests, $productionPublishingTests, $v29PublicDemoPoints, $publicDemoTests, $publicDemoPickups, $publicDemoCompletions, $v30VerticalSlicePoints, $publicDemoStages, $publicDemoAnchors, $publicDemoRetries, $v31DiversifiedPoints, $publicDemoResonanceGates, $publicDemoPressureHits, $publicDemoFootsteps, $v32RoadmapPoints, $publicDemoPhaseRelays, $publicDemoRelayOvercharge, $publicDemoComboSteps, $v33PlayableDemoPoints, $publicDemoCollisionSolves, $publicDemoTraversalActions, $publicDemoEnemyChases, $publicDemoGamepadFrames, $publicDemoContentAudioCues, $v34AAAFoundationPoints, $publicDemoEnemyArchetypes, $publicDemoEnemyTelegraphs, $publicDemoControllerGrounded, $publicDemoAnimationClipEvents, $renderingReadinessItems, $productionReadinessItems, $v35EngineArchitecturePoints, $engineSchedulerTasks, $engineSceneQueryHits, $engineGraphBudgetChecks)
    if (!$latestBySuite.ContainsKey($suiteName)) {
        $latestBySuite[$suiteName] = $latest
    }

    if ($latestRows.Count -lt 2) {
        if ($baselines.ContainsKey($suiteName)) {
            $baseline = $baselines[$suiteName]
            if ($latestCpu -gt $baseline.CpuMaxMs) {
                throw "$label exceeded committed CPU baseline $($baseline.CpuMaxMs)ms."
            }
            if ($latestGpu -gt $baseline.GpuMaxMs) {
                throw "$label exceeded committed GPU baseline $($baseline.GpuMaxMs)ms."
            }
            if ($latestPassCpu -gt $baseline.PassCpuMaxMs) {
                throw "$label exceeded committed pass CPU baseline $($baseline.PassCpuMaxMs)ms."
            }
        }
        continue
    }

    $previous = $latestRows[-2]
    $cpuDelta = $latestCpu - [double]$previous.cpu_frame_max_ms
    $gpuDelta = $latestGpu - [double]$previous.gpu_frame_max_ms
    Write-Host ("  delta from previous: cpu={0:+0.000;-0.000;0.000}ms gpu={1:+0.000;-0.000;0.000}ms" -f $cpuDelta, $gpuDelta)

    $referenceRows = @($latestRows | Select-Object -First ($latestRows.Count - 1))
    $cpuTrendReference = Get-Median -Values @($referenceRows | ForEach-Object { [double]$_.cpu_frame_max_ms })
    $gpuTrendReference = Get-Median -Values @($referenceRows | ForEach-Object { [double]$_.gpu_frame_max_ms })
    $cpuTrendDelta = $latestCpu - $cpuTrendReference
    $gpuTrendDelta = $latestGpu - $gpuTrendReference
    Write-Host ("  delta from recent median: cpu={0:+0.000;-0.000;0.000}ms gpu={1:+0.000;-0.000;0.000}ms" -f $cpuTrendDelta, $gpuTrendDelta)

    # CPU max-frame samples can spike when Windows schedules background work during an otherwise healthy run.
    # Gate on both previous-sample and recent-median deltas so one isolated spike is reported but not promoted to a failure.
    if ($cpuDelta -gt $CpuRegressionMs -and $cpuTrendDelta -gt $CpuRegressionMs) {
        throw "$label regressed CPU frame max by $([Math]::Round($cpuTrendDelta, 3))ms against recent median."
    }

    if ($gpuDelta -gt $GpuRegressionMs -and $gpuTrendDelta -gt $GpuRegressionMs) {
        throw "$label regressed GPU frame max by $([Math]::Round($gpuTrendDelta, 3))ms against recent median."
    }

    if ($baselines.ContainsKey($suiteName)) {
        $baseline = $baselines[$suiteName]
        if ($latestCpu -gt $baseline.CpuMaxMs) {
            throw "$label exceeded committed CPU baseline $($baseline.CpuMaxMs)ms."
        }
        if ($latestGpu -gt $baseline.GpuMaxMs) {
            throw "$label exceeded committed GPU baseline $($baseline.GpuMaxMs)ms."
        }
        if ($latestPassCpu -gt $baseline.PassCpuMaxMs) {
            throw "$label exceeded committed pass CPU baseline $($baseline.PassCpuMaxMs)ms."
        }
        if ($cpuDelta -gt $baseline.CpuRegressionMs -and $cpuTrendDelta -gt $baseline.CpuRegressionMs) {
            throw "$label regressed CPU frame max by $([Math]::Round($cpuTrendDelta, 3))ms against committed threshold."
        }
        if ($gpuDelta -gt $baseline.GpuRegressionMs -and $gpuTrendDelta -gt $baseline.GpuRegressionMs) {
            throw "$label regressed GPU frame max by $([Math]::Round($gpuTrendDelta, 3))ms against committed threshold."
        }
    }
}

if ($UpdateBaseline) {
    $parent = Split-Path -Parent $BaselinePath
    if (![string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    $lines = @("# suite|cpu_frame_max_ms|gpu_frame_max_ms|pass_cpu_max_ms|cpu_regression_ms|gpu_regression_ms")
    foreach ($suite in ($latestBySuite.Keys | Sort-Object)) {
        $row = $latestBySuite[$suite]
        $cpu = [double]$row.cpu_frame_max_ms
        $gpu = [double]$row.gpu_frame_max_ms
        $passCpu = if ($row.PSObject.Properties.Name -contains "pass_cpu_max_ms" -and ![string]::IsNullOrWhiteSpace($row.pass_cpu_max_ms)) { [double]$row.pass_cpu_max_ms } else { 60.0 }
        $lines += ("{0}|{1:N3}|{2:N3}|{3:N3}|{4:N3}|{5:N3}" -f $suite, [Math]::Max(40.0, $cpu + 8.0), [Math]::Max(20.0, $gpu + 8.0), [Math]::Max(30.0, $passCpu + 8.0), $CpuRegressionMs, $GpuRegressionMs)
    }
    $lines | Set-Content -LiteralPath $BaselinePath
    Write-Host "Updated performance baseline at $BaselinePath"
}
