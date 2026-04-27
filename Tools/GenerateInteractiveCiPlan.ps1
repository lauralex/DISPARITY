param(
    [string]$OutputPath = "Saved/CI/interactive_ci_plan.json",
    [string]$RuntimeSuiteFile = "Assets/Verification/RuntimeSuites.dverify"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}
if (![System.IO.Path]::IsPathRooted($RuntimeSuiteFile)) {
    $RuntimeSuiteFile = Join-Path $root $RuntimeSuiteFile
}

$suites = @()
if (Test-Path -LiteralPath $RuntimeSuiteFile) {
    foreach ($line in Get-Content -LiteralPath $RuntimeSuiteFile) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
            continue
        }
        $parts = $trimmed -split "\|"
        if ($parts.Count -eq 4) {
            $suites += [ordered]@{
                name = $parts[0].Trim()
                frames = [int]$parts[1].Trim()
                replay = $parts[2].Trim()
                baseline = $parts[3].Trim()
            }
        }
    }
}

$plan = [ordered]@{
    schema = 1
    generated_utc = (Get-Date).ToUniversalTime().ToString("o")
    runner = "windows self-hosted interactive GPU runner"
    required_display = $true
    static_gate = "Tools/VerifyDisparity.ps1 -SkipRuntime"
    runtime_gate = "Tools/VerifyDisparity.ps1 -SkipCodeAnalysis -RuntimeFrames 90"
    packaged_smoke = "Tools/SmokeTestDisparity.ps1 -ExecutablePath dist/DISPARITY-Release/DisparityGame.exe -ExerciseWindow"
    package_artifacts = @(
        "dist/DISPARITY-Release.zip",
        "dist/Installer/DISPARITY-InstallerPayload.zip",
        "Symbols/symbols_manifest.json",
        "Saved/Trailer/trailer_launch_preset.json",
        "Saved/Verification/performance_history.csv"
    )
    trailer_capture = [ordered]@{
        preset = "Tools/LaunchTrailerCapture.ps1 -Configuration Release -NoLaunch"
        obs_profile = "DISPARITY-Trailer"
        required_hotkeys = @("F7 showcase", "F8 authored trailer", "F9 capture")
    }
    runtime_suites = $suites
}

$parent = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

$plan | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
Write-Host "Interactive CI plan written: $OutputPath"
