param(
    [string]$PresetPath = "Saved/Trailer/trailer_launch_preset.json",
    [string]$OutputPath = "Saved/Trailer/OBS/DISPARITY-Trailer-Scene.json",
    [switch]$VerticalSlice
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($PresetPath)) {
    $PresetPath = Join-Path $root $PresetPath
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}

$preset = $null
if (Test-Path -LiteralPath $PresetPath) {
    $preset = Get-Content -LiteralPath $PresetPath -Raw | ConvertFrom-Json
}

$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $parent | Out-Null

[pscustomobject]@{
    schema = 1
    profile = "DISPARITY-Trailer"
    created_utc = (Get-Date).ToUniversalTime().ToString("o")
    executable = if ($preset) { $preset.executable } else { "DisparityGame.exe" }
    working_directory = if ($preset) { $preset.working_directory } else { "." }
    canvas = if ($VerticalSlice) { "1080x1920" } else { "1920x1080" }
    fps = 60
    scenes = @(
        [pscustomobject]@{
            name = "DISPARITY Gameplay"
            sources = @("game_capture_disparity", "audio_output_capture", "watermark_toggle")
            hotkeys = @("F7 showcase", "F8 authored trailer", "F9 high-res capture")
        }
    )
    recording = [pscustomobject]@{
        format = "mkv"
        encoder = "hardware_preferred"
        approval_required = $true
        metadata_sidecar = "Saved/Trailer/capture_metadata.json"
    }
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath

Write-Host "Generated OBS scene/profile metadata $OutputPath"
