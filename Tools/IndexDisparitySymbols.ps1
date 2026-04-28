param(
    [string]$PackagePath = "dist/DISPARITY-Release",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($PackagePath)) {
    $PackagePath = Join-Path $root $PackagePath
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $PackagePath "Symbols\symbols_manifest.json"
}
elseif (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}

function Get-DisparityFileSha256 {
    param([string]$LiteralPath)

    $fileHashCommand = Get-Command Get-FileHash -ErrorAction SilentlyContinue
    if ($fileHashCommand) {
        return (& $fileHashCommand -LiteralPath $LiteralPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }

    $sha = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::Open($LiteralPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        return (($sha.ComputeHash($stream) | ForEach-Object { $_.ToString("x2") }) -join "")
    }
    finally {
        $stream.Dispose()
        $sha.Dispose()
    }
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

if (!(Test-Path -LiteralPath $PackagePath)) {
    throw "Package path not found at $PackagePath"
}

$symbolsRoot = Join-Path $PackagePath "Symbols"
$symbols = @()
if (Test-Path -LiteralPath $symbolsRoot) {
    foreach ($file in Get-ChildItem -LiteralPath $symbolsRoot -Recurse -File -Filter *.pdb) {
        $symbols += [pscustomobject]@{
            path = (Get-RelativePath -BasePath $PackagePath -Path $file.FullName).Replace("\", "/")
            bytes = $file.Length
            sha256 = Get-DisparityFileSha256 -LiteralPath $file.FullName
            indexed_utc = (Get-Date).ToUniversalTime().ToString("o")
        }
    }
}

$parent = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

$manifest = [pscustomobject]@{
    name = "DISPARITY symbol index"
    package = (Get-RelativePath -BasePath $root -Path $PackagePath).Replace("\", "/")
    created_utc = (Get-Date).ToUniversalTime().ToString("o")
    symbol_server = [pscustomobject]@{
        protocol = "local-symsrv-plan"
        publish_command = "symstore add /r /f <pdb-root> /s <symbol-store> /t DISPARITY"
        retention_days = 180
        authenticated_publish_required = $true
    }
    symbol_count = $symbols.Count
    symbols = $symbols
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $OutputPath

Write-Host "Indexed $($symbols.Count) symbol file(s) to $OutputPath"
