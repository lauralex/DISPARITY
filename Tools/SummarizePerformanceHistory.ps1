param(
    [string]$HistoryPath = "Saved/Verification/performance_history.csv",
    [int]$Recent = 8,
    [double]$CpuRegressionMs = 8.0,
    [double]$GpuRegressionMs = 8.0
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($HistoryPath)) {
    $HistoryPath = Join-Path $root $HistoryPath
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

Write-Host "Performance history: $HistoryPath"
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
    $latestLuma = [double]$latest.capture_average_luma
    $editorPicks = if ($latest.PSObject.Properties.Name -contains "editor_pick_tests" -and ![string]::IsNullOrWhiteSpace($latest.editor_pick_tests)) { [int]$latest.editor_pick_tests } else { 0 }
    $gizmoPicks = if ($latest.PSObject.Properties.Name -contains "gizmo_pick_tests" -and ![string]::IsNullOrWhiteSpace($latest.gizmo_pick_tests)) { [int]$latest.gizmo_pick_tests } else { 0 }

    Write-Host ("{0}: latest cpu_max={1:N3}ms gpu_max={2:N3}ms luma={3:N2} editor_picks={4} gizmo_picks={5}" -f $label, $latestCpu, $latestGpu, $latestLuma, $editorPicks, $gizmoPicks)

    if ($latestRows.Count -lt 2) {
        continue
    }

    $previous = $latestRows[-2]
    $cpuDelta = $latestCpu - [double]$previous.cpu_frame_max_ms
    $gpuDelta = $latestGpu - [double]$previous.gpu_frame_max_ms
    Write-Host ("  delta from previous: cpu={0:+0.000;-0.000;0.000}ms gpu={1:+0.000;-0.000;0.000}ms" -f $cpuDelta, $gpuDelta)

    if ($cpuDelta -gt $CpuRegressionMs) {
        throw "$label regressed CPU frame max by $([Math]::Round($cpuDelta, 3))ms."
    }

    if ($gpuDelta -gt $GpuRegressionMs) {
        throw "$label regressed GPU frame max by $([Math]::Round($gpuDelta, 3))ms."
    }
}
