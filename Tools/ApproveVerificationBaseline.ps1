param(
    [string]$VerificationPath = "Assets/Verification",
    [string]$OutputPath = "Saved/Verification/baseline_approval.json",
    [switch]$RequireSignedHead,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($VerificationPath)) {
    $VerificationPath = Join-Path $root $VerificationPath
}
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

$patterns = @("*.dverify", "*.dperf", "*.dgoldenprofile", "*.ppm")
$baselineFiles = @()
foreach ($pattern in $patterns) {
    $baselineFiles += @(Get-ChildItem -LiteralPath $VerificationPath -Recurse -Filter $pattern -File)
}
$baselineFiles = @($baselineFiles | Sort-Object FullName -Unique)

if ($baselineFiles.Count -eq 0) {
    throw "No verification baseline files found under $VerificationPath."
}

$gitHead = "unknown"
$gitSignatureStatus = "N"
$gitSignatureKey = ""
$gitSignatureSigner = ""
$gitSignatureVerified = $false
try {
    $gitHead = (git -C $root rev-parse HEAD 2>$null).Trim()
    $signatureLine = (git -C $root log -1 --format="%G?|%GK|%GS" 2>$null).Trim()
    $signatureParts = $signatureLine -split "\|", 3
    if ($signatureParts.Count -ge 1 -and ![string]::IsNullOrWhiteSpace($signatureParts[0])) {
        $gitSignatureStatus = $signatureParts[0]
    }
    if ($signatureParts.Count -ge 2) {
        $gitSignatureKey = $signatureParts[1]
    }
    if ($signatureParts.Count -ge 3) {
        $gitSignatureSigner = $signatureParts[2]
    }
    $gitSignatureVerified = @("G", "U") -contains $gitSignatureStatus
}
catch {
    $gitHead = "unknown"
}

if ($RequireSignedHead -and !$gitSignatureVerified) {
    throw "Baseline approval requires a signed HEAD commit. Git signature status: $gitSignatureStatus."
}

$rootFullPath = (Resolve-Path -LiteralPath $root).Path.TrimEnd([char[]]@("\", "/"))
$rootPrefix = $rootFullPath + "\"

$records = foreach ($file in $baselineFiles) {
    $fileFullPath = $file.FullName
    if ($fileFullPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        $relativePath = $fileFullPath.Substring($rootPrefix.Length).Replace("\", "/")
    }
    else {
        $relativePath = $fileFullPath.Replace("\", "/")
    }
    $hash = Get-DisparityFileSha256 -LiteralPath $file.FullName
    [ordered]@{
        path = $relativePath
        bytes = $file.Length
        sha256 = $hash
    }
}

$payload = [ordered]@{
    schema = 1
    dry_run = [bool]$DryRun
    timestamp_utc = (Get-Date).ToUniversalTime().ToString("o")
    approver = $env:USERNAME
    git_head = $gitHead
    git_signature_status = $gitSignatureStatus
    git_signature_key = $gitSignatureKey
    git_signature_signer = $gitSignatureSigner
    git_signature_verified = $gitSignatureVerified
    signature_policy = "git-head-gpg"
    require_signed_head = [bool]$RequireSignedHead
    file_count = $records.Count
    files = @($records)
}

$parent = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

$payload | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
$manifestHash = Get-DisparityFileSha256 -LiteralPath $OutputPath
$payload.manifest_sha256 = $manifestHash
$payload | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
Write-Host "Baseline approval manifest written: $OutputPath"
