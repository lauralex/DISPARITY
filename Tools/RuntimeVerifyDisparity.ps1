param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$ExecutablePath = "",
    [int]$Frames = 90,
    [int]$TimeoutSeconds = 30,
    [double]$CpuFrameBudgetMs = 120.0,
    [double]$GpuFrameBudgetMs = 50.0,
    [double]$PassBudgetMs = 60.0,
    [string]$ReplayPath = "Assets/Verification/Prototype.dreplay",
    [string]$BaselinePath = "Assets/Verification/RuntimeBaseline.dverify",
    [string]$HistoryPath = "",
    [switch]$DisableCapture,
    [switch]$DisableInputPlayback,
    [switch]$DisableBudgets,
    [switch]$DisableBaseline,
    [switch]$DisablePerfHistory
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

if (Test-Path -LiteralPath $reportPath) {
    Remove-Item -LiteralPath $reportPath -Force
}
if (Test-Path -LiteralPath $capturePath) {
    Remove-Item -LiteralPath $capturePath -Force
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

    if (!(Test-Path -LiteralPath $HistoryPath)) {
        "timestamp,executable,version,frames,cpu_frame_max_ms,cpu_frame_avg_ms,gpu_frame_max_ms,gpu_frame_avg_ms,pass_cpu_max_ms,pass_cpu_max_name,pass_gpu_max_ms,pass_gpu_max_name,capture_average_luma,capture_checksum,playback_distance" |
            Set-Content -LiteralPath $HistoryPath
    }

    $row = @(
        (Get-Date).ToString("o"),
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
        $metrics["playback_distance"]
    ) | ForEach-Object {
        '"' + ([string]$_ -replace '"', '""') + '"'
    }
    Add-Content -LiteralPath $HistoryPath -Value ($row -join ",")
}

Write-Host "Runtime verification passed for $resolvedExecutable"
Write-Host $report
