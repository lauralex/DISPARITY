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

if (Test-Path -LiteralPath $reportPath) {
    Remove-Item -LiteralPath $reportPath -Force -ErrorAction SilentlyContinue
}
if (Test-Path -LiteralPath $capturePath) {
    Remove-Item -LiteralPath $capturePath -Force -ErrorAction SilentlyContinue
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
    if (!$baselineValues.ContainsKey("golden_capture_path")) {
        throw "Runtime verification baseline $resolvedBaselinePath does not define golden_capture_path."
    }

    $goldenPath = Resolve-VerificationPath -Path $baselineValues["golden_capture_path"]
    $thumbnailWidth = Get-BaselineInt -Values $baselineValues -Name "golden_thumbnail_width" -DefaultValue 64
    $thumbnailHeight = Get-BaselineInt -Values $baselineValues -Name "golden_thumbnail_height" -DefaultValue 36
    $meanTolerance = Get-BaselineDouble -Values $baselineValues -Name "golden_mean_tolerance" -DefaultValue 18.0
    $badPixelDelta = Get-BaselineDouble -Values $baselineValues -Name "golden_bad_pixel_delta" -DefaultValue 42.0
    $badPixelRatio = Get-BaselineDouble -Values $baselineValues -Name "golden_bad_pixel_ratio" -DefaultValue 0.35
    $safeSuiteName = [regex]::Replace($SuiteName.Trim(), "[^A-Za-z0-9_.-]", "_")
    if ([string]::IsNullOrWhiteSpace($safeSuiteName)) {
        $safeSuiteName = "Default"
    }

    $thumbnailPath = Join-Path $workingDirectory "Saved\Verification\runtime_capture_$safeSuiteName.ppm"
    $compareArguments = @{
        CapturePath = $capturePath
        GoldenPath = $goldenPath
        ThumbnailPath = $thumbnailPath
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

    $historyHeader = "timestamp,suite,executable,version,frames,cpu_frame_max_ms,cpu_frame_avg_ms,gpu_frame_max_ms,gpu_frame_avg_ms,pass_cpu_max_ms,pass_cpu_max_name,pass_gpu_max_ms,pass_gpu_max_name,capture_average_luma,capture_checksum,playback_distance,editor_pick_tests,editor_pick_failures,gizmo_pick_tests,gizmo_pick_failures"
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
        $metrics["editor_pick_tests"],
        $metrics["editor_pick_failures"],
        $metrics["gizmo_pick_tests"],
        $metrics["gizmo_pick_failures"]
    ) | ForEach-Object {
        '"' + ([string]$_ -replace '"', '""') + '"'
    }
    Add-Content -LiteralPath $HistoryPath -Value ($row -join ",")
}

Write-Host "Runtime verification passed for $resolvedExecutable"
Write-Host $report
