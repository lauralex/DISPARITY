param(
    [string]$PackagePath = "dist/DISPARITY-Release",
    [string]$SymbolStorePath = "dist/SymbolServer",
    [string]$ManifestPath = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($PackagePath)) {
    $PackagePath = Join-Path $root $PackagePath
}
if (![System.IO.Path]::IsPathRooted($SymbolStorePath)) {
    $SymbolStorePath = Join-Path $root $SymbolStorePath
}
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $SymbolStorePath "symbol_publish_manifest.json"
}
elseif (![System.IO.Path]::IsPathRooted($ManifestPath)) {
    $ManifestPath = Join-Path $root $ManifestPath
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

$symbolsRoot = Join-Path $PackagePath "Symbols"
$pdbs = @(Get-ChildItem -LiteralPath $symbolsRoot -Recurse -Filter *.pdb -File -ErrorAction SilentlyContinue)
New-Item -ItemType Directory -Force -Path $SymbolStorePath | Out-Null

$records = @()
foreach ($pdb in $pdbs) {
    $hash = Get-DisparityFileSha256 -LiteralPath $pdb.FullName
    $destinationDirectory = Join-Path $SymbolStorePath (Join-Path $pdb.BaseName $hash.Substring(0, 16))
    New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null
    $destination = Join-Path $destinationDirectory $pdb.Name
    Copy-Item -LiteralPath $pdb.FullName -Destination $destination -Force
    $records += [pscustomobject]@{
        source = $pdb.FullName
        destination = $destination
        sha256 = $hash
        bytes = $pdb.Length
    }
}

[pscustomobject]@{
    schema = 1
    published_utc = (Get-Date).ToUniversalTime().ToString("o")
    symbol_count = $records.Count
    store = $SymbolStorePath
    symbols = $records
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ManifestPath

Write-Host "Published $($records.Count) symbol file(s) to $SymbolStorePath"
