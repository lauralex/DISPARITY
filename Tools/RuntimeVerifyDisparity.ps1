param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$ExecutablePath = "",
    [int]$Frames = 90,
    [int]$TimeoutSeconds = 30,
    [double]$CpuFrameBudgetMs = 120.0,
    [double]$GpuFrameBudgetMs = 50.0,
    [double]$PassBudgetMs = 60.0,
    [switch]$DisableCapture,
    [switch]$DisableInputPlayback,
    [switch]$DisableBudgets
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
    "--verify-pass-budget-ms=$PassBudgetMs"
)
if ($DisableCapture) { $runtimeArguments += "--verify-no-capture" }
if ($DisableInputPlayback) { $runtimeArguments += "--verify-no-input-playback" }
if ($DisableBudgets) { $runtimeArguments += "--verify-no-budgets" }

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

Write-Host "Runtime verification passed for $resolvedExecutable"
Write-Host $report
