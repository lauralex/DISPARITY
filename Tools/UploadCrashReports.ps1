param(
    [string]$ManifestPath = "Saved/CrashUploads/crash_upload_manifest.json",
    [string]$Endpoint = "",
    [int]$Retries = 3,
    [int]$RetryDelaySeconds = 2,
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
    Write-Host "Crash upload dry run: $($manifest.report_count) report(s), endpoint '$Endpoint', retries $Retries"
    return
}

$attempt = 0
$lastError = $null
while ($attempt -lt [Math]::Max(1, $Retries)) {
    ++$attempt
    try {
        $response = Invoke-RestMethod -Method Post -Uri $Endpoint -ContentType "application/json" -Body ($manifest | ConvertTo-Json -Depth 6)
        Write-Host "Crash upload completed on attempt ${attempt}: $response"
        return
    }
    catch {
        $lastError = $_
        Write-Warning "Crash upload attempt $attempt failed: $($_.Exception.Message)"
        if ($attempt -lt $Retries) {
            Start-Sleep -Seconds ([Math]::Max(0, $RetryDelaySeconds) * $attempt)
        }
    }
}

throw "Crash upload failed after $Retries attempt(s): $($lastError.Exception.Message)"
