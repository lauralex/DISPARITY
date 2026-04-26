param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$AssetsPath = "Assets",
    [string]$OutputPath = "Saved/CookedAssets",
    [switch]$BinaryPackages,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($AssetsPath)) {
    $AssetsPath = Join-Path $root $AssetsPath
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}

if (!(Test-Path -LiteralPath $AssetsPath)) {
    throw "Assets folder not found at $AssetsPath"
}
if ($Clean -and (Test-Path -LiteralPath $OutputPath)) {
    Remove-Item -LiteralPath $OutputPath -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
if ($BinaryPackages) {
    New-Item -ItemType Directory -Force -Path (Join-Path $OutputPath "Binary") | Out-Null
}

function Get-RelativePath {
    param(
        [string]$BasePath,
        [string]$Path
    )

    $baseFullPath = [System.IO.Path]::GetFullPath($BasePath)
    if (!$baseFullPath.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $baseFullPath += [System.IO.Path]::DirectorySeparatorChar
    }

    $baseUri = New-Object System.Uri($baseFullPath)
    $pathUri = New-Object System.Uri([System.IO.Path]::GetFullPath($Path))
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($pathUri).ToString()).Replace("/", "\")
}

function Get-AssetKind {
    param([string]$Extension)

    switch ($Extension.ToLowerInvariant()) {
        ".gltf" { return "gltf_scene" }
        ".glb" { return "glb_scene" }
        ".dmat" { return "material" }
        ".dprefab" { return "prefab" }
        ".dscript" { return "script" }
        ".dscene" { return "scene" }
        ".hlsl" { return "shader" }
        ".dverify" { return "verification_baseline" }
        ".dreplay" { return "verification_replay" }
        ".dgoldenprofile" { return "golden_profile" }
        ".dperf" { return "performance_profile" }
        default { return "source" }
    }
}

$records = @()
foreach ($file in Get-ChildItem -LiteralPath $AssetsPath -Recurse -File) {
    $relativePath = (Get-RelativePath -BasePath $root -Path $file.FullName).Replace("\", "/")
    $assetRelativePath = (Get-RelativePath -BasePath $AssetsPath -Path $file.FullName).Replace("\", "/")
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $kind = Get-AssetKind -Extension $file.Extension
    $metadataName = ($assetRelativePath -replace "[\\/:\*\?`"<>|]", "_") + ".dcooked"
    $metadataPath = Join-Path $OutputPath $metadataName
    $binaryName = ($assetRelativePath -replace "[\\/:\*\?`"<>|]", "_") + ".dassetbin"
    $binaryPath = Join-Path (Join-Path $OutputPath "Binary") $binaryName
    $importSettings = Join-Path $root ("Assets/ImportSettings/" + $relativePath + ".dimport")
    $dependencies = @()
    if (Test-Path -LiteralPath $importSettings) {
        $dependencies += (Get-RelativePath -BasePath $root -Path $importSettings).Replace("\", "/")
    }

    $metadata = [pscustomobject]@{
        source = $relativePath
        kind = $kind
        configuration = $Configuration
        source_sha256 = $hash
        source_bytes = $file.Length
        cooked_utc = (Get-Date).ToUniversalTime().ToString("o")
        dependencies = $dependencies
    }
    $metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $metadataPath

    $binaryRelativePath = ""
    $binaryBytes = 0
    $binaryHash = ""
    if ($BinaryPackages) {
        $sourceBytes = [System.IO.File]::ReadAllBytes($file.FullName)
        $metadataJson = $metadata | ConvertTo-Json -Depth 4 -Compress
        $header = [System.Text.Encoding]::UTF8.GetBytes("DSPK1`n$metadataJson`n---SOURCE---`n")
        $payload = New-Object byte[] ($header.Length + $sourceBytes.Length)
        [System.Buffer]::BlockCopy($header, 0, $payload, 0, $header.Length)
        [System.Buffer]::BlockCopy($sourceBytes, 0, $payload, $header.Length, $sourceBytes.Length)
        [System.IO.File]::WriteAllBytes($binaryPath, $payload)
        $binaryRelativePath = (Get-RelativePath -BasePath $root -Path $binaryPath).Replace("\", "/")
        $binaryBytes = (Get-Item -LiteralPath $binaryPath).Length
        $binaryHash = (Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }

    $records += [pscustomobject]@{
        source = $relativePath
        kind = $kind
        hash = $hash
        bytes = $file.Length
        cooked_metadata = (Get-RelativePath -BasePath $root -Path $metadataPath).Replace("\", "/")
        cooked_binary = $binaryRelativePath
        binary_bytes = $binaryBytes
        binary_sha256 = $binaryHash
        dependencies = $dependencies
    }
}

$manifest = [pscustomobject]@{
    name = "DISPARITY cooked asset manifest"
    configuration = $Configuration
    created_utc = (Get-Date).ToUniversalTime().ToString("o")
    asset_count = $records.Count
    binary_packages = [bool]$BinaryPackages
    records = $records
}

$manifestPath = Join-Path $OutputPath "manifest.dcook"
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath

if ($BinaryPackages) {
    Write-Host "Cooked $($records.Count) asset metadata and binary package record(s) to $manifestPath"
}
else {
    Write-Host "Cooked $($records.Count) asset metadata record(s) to $manifestPath"
}
