param(
    [string]$PackagePath = "dist/DISPARITY-Release",
    [string]$InstallerPath = "dist/Installer",
    [string]$ObsProfilePath = "Saved/Trailer/OBS/DISPARITY-Trailer-Scene.json",
    [string]$RuntimeReportSchemaPath = "Assets/Verification/RuntimeReportSchema.dschema",
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
    [pscustomobject]@{ name = "runtime_report_schema"; path = $RuntimeReportSchemaPath; exists = Test-Path -LiteralPath $RuntimeReportSchemaPath }
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
if ($schemaMetrics.Count -lt 20 -or !($schemaMetrics -contains "release_readiness_tests")) {
    throw "Runtime report schema does not include the v24 production metrics."
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
    checks = $checks
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath

Write-Host "Release readiness manifest written to $OutputPath"
