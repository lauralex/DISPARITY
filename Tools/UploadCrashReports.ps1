param(
    [string]$ManifestPath = "Saved/CrashUploads/crash_upload_manifest.json",
    [string]$Endpoint = "",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($ManifestPath)) {
    $ManifestPath = Join-Path $root $ManifestPath
}

if (!(Test-Path -LiteralPath $ManifestPath)) {
    throw "Crash upload manifest not found at $ManifestPath"
}

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$effectiveDryRun = $DryRun -or [string]::IsNullOrWhiteSpace($Endpoint)
if ($effectiveDryRun) {
    Write-Host "Crash upload dry run: $($manifest.report_count) report(s), endpoint '$Endpoint'"
    return
}

$response = Invoke-RestMethod -Method Post -Uri $Endpoint -ContentType "application/json" -Body ($manifest | ConvertTo-Json -Depth 6)
Write-Host "Crash upload completed: $response"
