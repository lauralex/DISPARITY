param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$ExecutablePath = "",
    [int]$Frames = 90,
    [int]$TimeoutSeconds = 30
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

if (Test-Path -LiteralPath $reportPath) {
    Remove-Item -LiteralPath $reportPath -Force
}

$process = Start-Process -FilePath $resolvedExecutable -WorkingDirectory $workingDirectory -ArgumentList "--verify-runtime --verify-frames=$Frames" -PassThru
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

Write-Host "Runtime verification passed for $resolvedExecutable"
Write-Host $report
