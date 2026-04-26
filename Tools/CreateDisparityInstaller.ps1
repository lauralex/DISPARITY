param(
    [string]$PackagePath = "dist/DISPARITY-Release",
    [string]$OutputPath = "dist/Installer",
    [switch]$CreateArchive
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($PackagePath)) {
    $PackagePath = Join-Path $root $PackagePath
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}

if (!(Test-Path -LiteralPath $PackagePath)) {
    throw "Package path not found at $PackagePath"
}

$packageManifestPath = Join-Path $PackagePath "package_manifest.json"
if (!(Test-Path -LiteralPath $packageManifestPath)) {
    throw "Package manifest not found at $packageManifestPath"
}

$packageManifest = Get-Content -LiteralPath $packageManifestPath -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null

$installerManifest = [pscustomobject]@{
    name = "DISPARITY setup manifest"
    product = $packageManifest.name
    version = $packageManifest.version
    configuration = $packageManifest.configuration
    source_package = $PackagePath
    created_utc = (Get-Date).ToUniversalTime().ToString("o")
    install_root = "%LOCALAPPDATA%/DISPARITY"
    entry_point = "DisparityGame.exe"
    file_count = $packageManifest.files.Count
    files = $packageManifest.files
}
$installerManifestPath = Join-Path $OutputPath "DISPARITY-SetupManifest.json"
$installerManifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $installerManifestPath

if ($CreateArchive) {
    $archivePath = Join-Path $OutputPath "DISPARITY-InstallerPayload.zip"
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    Compress-Archive -Path (Join-Path $PackagePath "*") -DestinationPath $archivePath -Force
    Write-Host "Created installer payload archive $archivePath"
}

Write-Host "Created installer manifest $installerManifestPath"
