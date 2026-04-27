param(
    [string]$PackagePath = "dist/DISPARITY-Release",
    [string]$InstallerPath = "dist/Installer",
    [string]$ObsProfilePath = "Saved/Trailer/OBS/DISPARITY-Trailer-Scene.json",
    [string]$RuntimeReportSchemaPath = "Assets/Verification/RuntimeReportSchema.dschema",
    [string]$ProductionBatchPath = "Assets/Verification/V25ProductionBatch.dfollowups",
    [string]$DiversifiedBatchPath = "Assets/Verification/V28DiversifiedBatch.dfollowups",
    [string]$PublicDemoBatchPath = "Assets/Verification/V29PublicDemo.dfollowups",
    [string]$VerticalSliceBatchPath = "Assets/Verification/V30VerticalSlice.dfollowups",
    [string]$OutputPath = "Saved/Release/release_readiness_manifest.json"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
function Resolve-RepoPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $root $Path
}

$PackagePath = Resolve-RepoPath -Path $PackagePath
$InstallerPath = Resolve-RepoPath -Path $InstallerPath
$ObsProfilePath = Resolve-RepoPath -Path $ObsProfilePath
$RuntimeReportSchemaPath = Resolve-RepoPath -Path $RuntimeReportSchemaPath
$ProductionBatchPath = Resolve-RepoPath -Path $ProductionBatchPath
$DiversifiedBatchPath = Resolve-RepoPath -Path $DiversifiedBatchPath
$PublicDemoBatchPath = Resolve-RepoPath -Path $PublicDemoBatchPath
$VerticalSliceBatchPath = Resolve-RepoPath -Path $VerticalSliceBatchPath
$OutputPath = Resolve-RepoPath -Path $OutputPath

$packageManifestPath = Join-Path $PackagePath "package_manifest.json"
$installerManifestPath = Join-Path $InstallerPath "DISPARITY-SetupManifest.json"
$bootstrapperPath = Join-Path $InstallerPath "DISPARITY-InstallerBootstrapper.cmd"
$symbolManifestPath = Join-Path $PackagePath "Symbols/symbols_manifest.json"

$checks = @(
    [pscustomobject]@{ name = "package_manifest"; path = $packageManifestPath; exists = Test-Path -LiteralPath $packageManifestPath },
    [pscustomobject]@{ name = "installer_manifest"; path = $installerManifestPath; exists = Test-Path -LiteralPath $installerManifestPath },
    [pscustomobject]@{ name = "bootstrapper_command"; path = $bootstrapperPath; exists = Test-Path -LiteralPath $bootstrapperPath },
    [pscustomobject]@{ name = "symbol_manifest"; path = $symbolManifestPath; exists = Test-Path -LiteralPath $symbolManifestPath },
    [pscustomobject]@{ name = "obs_profile"; path = $ObsProfilePath; exists = Test-Path -LiteralPath $ObsProfilePath },
    [pscustomobject]@{ name = "runtime_report_schema"; path = $RuntimeReportSchemaPath; exists = Test-Path -LiteralPath $RuntimeReportSchemaPath },
    [pscustomobject]@{ name = "production_batch_manifest"; path = $ProductionBatchPath; exists = Test-Path -LiteralPath $ProductionBatchPath },
    [pscustomobject]@{ name = "diversified_batch_manifest"; path = $DiversifiedBatchPath; exists = Test-Path -LiteralPath $DiversifiedBatchPath },
    [pscustomobject]@{ name = "public_demo_batch_manifest"; path = $PublicDemoBatchPath; exists = Test-Path -LiteralPath $PublicDemoBatchPath },
    [pscustomobject]@{ name = "vertical_slice_batch_manifest"; path = $VerticalSliceBatchPath; exists = Test-Path -LiteralPath $VerticalSliceBatchPath }
)

foreach ($check in $checks) {
    if (!$check.exists) {
        throw "Release readiness check '$($check.name)' failed; missing $($check.path)"
    }
}

$packageManifest = Get-Content -LiteralPath $packageManifestPath -Raw | ConvertFrom-Json
if (!$packageManifest.version -or [int]$packageManifest.files.Count -le 0) {
    throw "Package manifest is missing version or files."
}

$obsProfile = Get-Content -LiteralPath $ObsProfilePath -Raw | ConvertFrom-Json
if (!$obsProfile.recording.approval_required -or !$obsProfile.recording.metadata_sidecar) {
    throw "OBS profile is missing recording approval metadata."
}

$schemaMetrics = @(Get-Content -LiteralPath $RuntimeReportSchemaPath | Where-Object {
    $trimmed = $_.Trim()
    ![string]::IsNullOrWhiteSpace($trimmed) -and !$trimmed.StartsWith("#")
})
if ($schemaMetrics.Count -lt 170 -or !($schemaMetrics -contains "v25_production_points") -or !($schemaMetrics -contains "v28_diversified_points") -or !($schemaMetrics -contains "v29_public_demo_points") -or !($schemaMetrics -contains "v30_vertical_slice_points")) {
    throw "Runtime report schema does not include the v25/v28/v29/v30 production metrics."
}

$productionPoints = @(Get-Content -LiteralPath $ProductionBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($productionPoints.Count -ne 40) {
    throw "Production batch manifest does not define forty points."
}

$diversifiedPoints = @(Get-Content -LiteralPath $DiversifiedBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($diversifiedPoints.Count -ne 36) {
    throw "Diversified batch manifest does not define thirty-six points."
}

$publicDemoPoints = @(Get-Content -LiteralPath $PublicDemoBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($publicDemoPoints.Count -ne 30) {
    throw "Public demo batch manifest does not define thirty points."
}

$verticalSlicePoints = @(Get-Content -LiteralPath $VerticalSliceBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($verticalSlicePoints.Count -ne 36) {
    throw "Vertical slice batch manifest does not define thirty-six points."
}

$parent = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

[pscustomobject]@{
    schema = 1
    reviewed_utc = (Get-Date).ToUniversalTime().ToString("o")
    package_version = $packageManifest.version
    package_file_count = $packageManifest.files.Count
    runtime_schema_metric_count = $schemaMetrics.Count
    production_batch_point_count = $productionPoints.Count
    diversified_batch_point_count = $diversifiedPoints.Count
    public_demo_batch_point_count = $publicDemoPoints.Count
    vertical_slice_batch_point_count = $verticalSlicePoints.Count
    checks = $checks
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath

Write-Host "Release readiness manifest written to $OutputPath"
