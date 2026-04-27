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
    $metrics = @{}
    foreach ($line in ($report -split "`r?`n")) {
        if ($line -match "^([^=]+)=(.*)$") {
            $metrics[$Matches[1]] = $Matches[2]
        }
    }

    $historyParent = Split-Path -Parent $HistoryPath
    if (![string]::IsNullOrWhiteSpace($historyParent)) {
        New-Item -ItemType Directory -Force -Path $historyParent | Out-Null
    }

    $historyHeader = "timestamp,suite,executable,version,frames,cpu_frame_max_ms,cpu_frame_avg_ms,gpu_frame_max_ms,gpu_frame_avg_ms,pass_cpu_max_ms,pass_cpu_max_name,pass_gpu_max_ms,pass_gpu_max_name,capture_average_luma,capture_checksum,playback_distance,playback_net_distance,editor_pick_tests,editor_pick_failures,gizmo_pick_tests,gizmo_pick_failures,gizmo_drag_tests,gizmo_drag_failures,scene_reload_tests,scene_save_tests,post_debug_view_tests,showcase_frames,trailer_frames,high_res_capture_tests,rift_vfx_draws,rift_vfx_gpu_simulation_batches,rift_vfx_motion_vector_candidates,rift_vfx_temporal_reprojection_samples,audio_beat_pulses,audio_snapshot_tests,async_io_tests,material_texture_slot_tests,prefab_variant_tests,shot_director_tests,shot_spline_tests,audio_analysis_tests,xaudio2_backend_tests,vfx_system_tests,gpu_vfx_simulation_tests,animation_skinning_tests,render_graph_allocations,render_graph_aliased_resources,render_graph_barriers,render_graph_resource_bindings,render_graph_bind_hits,render_graph_bind_misses,render_graph_callbacks_bound,render_graph_callbacks_executed,render_graph_dispatch_valid,object_id_readback_ring_size,object_id_readback_pending,object_id_readback_requests,object_id_readback_completions,object_id_readback_latency_frames,object_id_readback_busy_skips,editor_viewport_ready,editor_viewport_presented_in_imgui,editor_object_id_ready,editor_object_depth_ready,audio_xaudio2_available,audio_xaudio2_initialized,audio_analysis_peak,audio_analysis_beat_envelope"
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
        $metrics["audio_beat_pulses"],
        $metrics["audio_snapshot_tests"],
        $metrics["async_io_tests"],
        $metrics["material_texture_slot_tests"],
        $metrics["prefab_variant_tests"],
        $metrics["shot_director_tests"],
        $metrics["shot_spline_tests"],
        $metrics["audio_analysis_tests"],
        $metrics["xaudio2_backend_tests"],
        $metrics["vfx_system_tests"],
        $metrics["gpu_vfx_simulation_tests"],
        $metrics["animation_skinning_tests"],
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
        $metrics["editor_viewport_ready"],
        $metrics["editor_viewport_presented_in_imgui"],
        $metrics["editor_object_id_ready"],
        $metrics["editor_object_depth_ready"],
        $metrics["audio_xaudio2_available"],
        $metrics["audio_xaudio2_initialized"],
        $metrics["audio_analysis_peak"],
        $metrics["audio_analysis_beat_envelope"]
    ) | ForEach-Object {
        '"' + ([string]$_ -replace '"', '""') + '"'
    }
    Add-Content -LiteralPath $HistoryPath -Value ($row -join ",")
}

Write-Host "Runtime verification passed for $resolvedExecutable"
Write-Host $report
