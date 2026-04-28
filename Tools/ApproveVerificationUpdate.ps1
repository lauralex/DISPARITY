param(
    [string]$Reason = "local v22 verification update approval",
    [string]$OutputPath = "Saved/Verification/baseline_update_approval.json",
    [switch]$RequireSignedHead
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
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

Push-Location $root
try {
    $head = (git rev-parse HEAD).Trim()
    $signature = git log -1 --format="%G?`n%GS`n%GK"
}
finally {
    Pop-Location
}

$signatureLines = @($signature -split "`r?`n")
$signatureStatus = if ($signatureLines.Count -gt 0) { $signatureLines[0] } else { "N" }
if ($RequireSignedHead -and $signatureStatus -ne "G") {
    throw "HEAD is not signed with a good signature; status=$signatureStatus"
}

$tracked = @(
    "Assets/Verification/*Baseline.dverify",
    "Assets/Verification/PerformanceBaselines.dperf",
    "Assets/Verification/GoldenProfiles/*.dgoldenprofile",
    "Assets/Verification/Goldens/*.ppm"
)

$records = @()
foreach ($pattern in $tracked) {
    foreach ($file in Get-ChildItem -Path (Join-Path $root $pattern) -File -ErrorAction SilentlyContinue) {
        $records += [pscustomobject]@{
            path = Resolve-Path -LiteralPath $file.FullName -Relative
            sha256 = Get-DisparityFileSha256 -LiteralPath $file.FullName
            bytes = $file.Length
        }
    }
}

$parent = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

[pscustomobject]@{
    schema = 1
    approved_utc = (Get-Date).ToUniversalTime().ToString("o")
    reason = $Reason
    git_head = $head
    git_signature_status = $signatureStatus
    git_signer = if ($signatureLines.Count -gt 1) { $signatureLines[1] } else { "" }
    git_signing_key = if ($signatureLines.Count -gt 2) { $signatureLines[2] } else { "" }
    require_signed_head = [bool]$RequireSignedHead
    file_count = $records.Count
    files = $records
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath

Write-Host "Wrote signed baseline update approval intent to $OutputPath"
