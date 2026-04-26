param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$IncludeSymbols,
    [switch]$CreateArchive,
    [string]$ArtifactVersion = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if (!(Test-Path -LiteralPath $msbuild)) {
    $msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if (!$msbuildCommand) {
        throw "MSBuild.exe was not found."
    }
    $msbuild = $msbuildCommand.Source
}
$dist = Join-Path $root "dist\DISPARITY-$Configuration"

& $msbuild (Join-Path $root "DISPARITY.sln") /m /p:Configuration=$Configuration /p:Platform=x64 /p:PreferredToolArchitecture=x64

if (Test-Path $dist) {
    Remove-Item -LiteralPath $dist -Recurse -Force
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

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath (Join-Path $root "bin\x64\$Configuration\DisparityGame.exe") -Destination $dist
Copy-Item -LiteralPath (Join-Path $root "Assets") -Destination $dist -Recurse
Copy-Item -LiteralPath (Join-Path $root "Docs") -Destination $dist -Recurse
Copy-Item -LiteralPath (Join-Path $root "README.md") -Destination $dist

if ($IncludeSymbols) {
    $symbols = Join-Path $dist "Symbols"
    New-Item -ItemType Directory -Force -Path $symbols | Out-Null
    $pdb = Join-Path $root "bin\x64\$Configuration\DisparityGame.pdb"
    if (Test-Path -LiteralPath $pdb) {
        Copy-Item -LiteralPath $pdb -Destination $symbols
    }
}

if ([string]::IsNullOrWhiteSpace($ArtifactVersion)) {
    $versionHeader = Join-Path $root "DisparityEngine\Source\Disparity\Core\Version.h"
    $versionText = Get-Content -LiteralPath $versionHeader -Raw
    $major = [regex]::Match($versionText, "Major\s*=\s*(\d+)").Groups[1].Value
    $minor = [regex]::Match($versionText, "Minor\s*=\s*(\d+)").Groups[1].Value
    $patch = [regex]::Match($versionText, "Patch\s*=\s*(\d+)").Groups[1].Value
    $ArtifactVersion = "$major.$minor.$patch"
}

$commit = "unknown"
Push-Location $root
try {
    $commit = (git rev-parse --short HEAD 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commit)) {
        $commit = "unknown"
    }
}
finally {
    Pop-Location
}

$files = @()
foreach ($file in Get-ChildItem -LiteralPath $dist -Recurse -File) {
    if ($file.Name -eq "package_manifest.json") {
        continue
    }

    $relativePath = Get-RelativePath -BasePath $dist -Path $file.FullName
    $files += [pscustomobject]@{
        path = $relativePath.Replace("\", "/")
        bytes = $file.Length
        sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$manifest = [pscustomobject]@{
    name = "DISPARITY"
    version = $ArtifactVersion
    configuration = $Configuration
    commit = $commit
    created_utc = (Get-Date).ToUniversalTime().ToString("o")
    includes_symbols = [bool]$IncludeSymbols
    files = $files
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $dist "package_manifest.json")

if ($CreateArchive) {
    $archivePath = Join-Path $root "dist\DISPARITY-$Configuration-$ArtifactVersion.zip"
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $archivePath -Force
    Write-Host "Created archive $archivePath"
}

Write-Host "Packaged DISPARITY to $dist"
