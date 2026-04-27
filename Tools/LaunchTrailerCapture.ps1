param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Packaged,
    [switch]$NoLaunch,
    [string]$OutputPath = "Saved/Trailer"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}

if ($Packaged) {
    $exe = Join-Path $root "dist/DISPARITY-$Configuration/DisparityGame.exe"
}
else {
    $exe = Join-Path $root "bin/x64/$Configuration/DisparityGame.exe"
}

if (!(Test-Path -LiteralPath $exe)) {
    throw "DisparityGame.exe not found at $exe. Build or package $Configuration first."
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null

$preset = [pscustomobject]@{
    name = "DISPARITY trailer capture preset"
    executable = (Resolve-Path -LiteralPath $exe).Path
    working_directory = $root
    configuration = $Configuration
    packaged = [bool]$Packaged
    recommended_hotkeys = [ordered]@{
        toggle_editor = "F1"
        trailer_mode = "F8"
        high_res_capture = "F9"
        release_mouse = "Tab"
    }
    capture_outputs = [ordered]@{
        source_ppm = "Saved/Captures/DISPARITY_photo_source.ppm"
        source_png = "Saved/Captures/DISPARITY_photo_source.png"
        upscaled_ppm = "Saved/Captures/DISPARITY_photo_2x.ppm"
    }
    obs_notes = "Use game/window capture, 60 FPS, and let DISPARITY own the audio focus before starting trailer mode."
    created_utc = (Get-Date).ToUniversalTime().ToString("o")
}

$presetPath = Join-Path $OutputPath "trailer_launch_preset.json"
$preset | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $presetPath
Write-Host "Wrote trailer launch preset to $presetPath"

if (!$NoLaunch) {
    Start-Process -FilePath $exe -WorkingDirectory $root
    Write-Host "Launched DISPARITY. Press F8 for trailer/photo mode and F9 for capture."
}
