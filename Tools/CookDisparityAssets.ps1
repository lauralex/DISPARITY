param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$AssetsPath = "Assets",
    [string]$OutputPath = "Saved/CookedAssets",
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

    $records += [pscustomobject]@{
        source = $relativePath
        kind = $kind
        hash = $hash
        bytes = $file.Length
        cooked_metadata = (Get-RelativePath -BasePath $root -Path $metadataPath).Replace("\", "/")
        dependencies = $dependencies
    }
}

$manifest = [pscustomobject]@{
    name = "DISPARITY cooked asset manifest"
    configuration = $Configuration
    created_utc = (Get-Date).ToUniversalTime().ToString("o")
    asset_count = $records.Count
    records = $records
}

$manifestPath = Join-Path $OutputPath "manifest.dcook"
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath

Write-Host "Cooked $($records.Count) asset metadata record(s) to $manifestPath"
