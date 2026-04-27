param(
    [string]$PackagePath = "dist/DISPARITY-Release",
    [string]$OutputPath = "dist/Installer/DISPARITY-InstallerBootstrapper.cmd"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($PackagePath)) {
    $PackagePath = Join-Path $root $PackagePath
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}
if (!(Test-Path -LiteralPath (Join-Path $PackagePath "DisparityGame.exe"))) {
    throw "Package executable not found under $PackagePath"
}

$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $parent | Out-Null

$script = @"
@echo off
setlocal
set "DISPARITY_PACKAGE=%~dp0..\DISPARITY-Release"
if not exist "%DISPARITY_PACKAGE%\DisparityGame.exe" (
  echo DISPARITY package not found: %DISPARITY_PACKAGE%
  exit /b 2
)
echo Installing DISPARITY runtime payload from %DISPARITY_PACKAGE%
echo This v22 bootstrapper validates the payload and starts the public vertical slice.
pushd "%DISPARITY_PACKAGE%"
start "" "DisparityGame.exe"
popd
endlocal
"@

$script | Set-Content -LiteralPath $OutputPath -Encoding ASCII
Write-Host "Created installer bootstrapper $OutputPath"
