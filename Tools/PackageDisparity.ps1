param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
$dist = Join-Path $root "dist\DISPARITY-$Configuration"

& $msbuild (Join-Path $root "DISPARITY.sln") /m /p:Configuration=$Configuration /p:Platform=x64

if (Test-Path $dist) {
    Remove-Item -LiteralPath $dist -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath (Join-Path $root "bin\x64\$Configuration\DisparityGame.exe") -Destination $dist
Copy-Item -LiteralPath (Join-Path $root "Assets") -Destination $dist -Recurse
Copy-Item -LiteralPath (Join-Path $root "Docs") -Destination $dist -Recurse
Copy-Item -LiteralPath (Join-Path $root "README.md") -Destination $dist

Write-Host "Packaged DISPARITY to $dist"
