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
