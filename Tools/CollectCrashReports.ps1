param(
    [string]$CrashPath = "Saved/CrashLogs",
    [string]$OutputPath = "Saved/CrashUploads",
    [switch]$CreateArchive,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($CrashPath)) {
    $CrashPath = Join-Path $root $CrashPath
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null

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

$reports = @()
if (Test-Path -LiteralPath $CrashPath) {
    foreach ($file in Get-ChildItem -LiteralPath $CrashPath -Recurse -File) {
        $reports += [pscustomobject]@{
            source = (Get-RelativePath -BasePath $root -Path $file.FullName).Replace("\", "/")
            bytes = $file.Length
            sha256 = Get-DisparityFileSha256 -LiteralPath $file.FullName
            last_write_utc = $file.LastWriteTimeUtc.ToString("o")
        }

        if (!$DryRun) {
            Copy-Item -LiteralPath $file.FullName -Destination $OutputPath -Force
        }
    }
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

$manifest = [pscustomobject]@{
    name = "DISPARITY crash upload manifest"
    dry_run = [bool]$DryRun
    commit = $commit
    created_utc = (Get-Date).ToUniversalTime().ToString("o")
    report_count = $reports.Count
    reports = $reports
}
$manifestPath = Join-Path $OutputPath "crash_upload_manifest.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath

if ($CreateArchive -and !$DryRun) {
    $archivePath = Join-Path $OutputPath "crash_upload_bundle.zip"
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    Compress-Archive -Path (Join-Path $OutputPath "*") -DestinationPath $archivePath -Force
    Write-Host "Created crash upload bundle $archivePath"
}

Write-Host "Collected $($reports.Count) crash report(s); manifest written to $manifestPath"
