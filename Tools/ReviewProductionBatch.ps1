param(
    [string]$ManifestPath = "Assets/Verification/V25ProductionBatch.dfollowups",
    [int]$ExpectedCount = 40,
    [string]$OutputPath = "Saved/Verification/v25_production_batch_review.json"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
function Resolve-RepoPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $root $Path
}

$ManifestPath = Resolve-RepoPath -Path $ManifestPath
$OutputPath = Resolve-RepoPath -Path $OutputPath

if (!(Test-Path -LiteralPath $ManifestPath)) {
    throw "Production batch manifest is missing: $ManifestPath"
}

$points = New-Object System.Collections.Generic.List[object]
foreach ($line in Get-Content -LiteralPath $ManifestPath) {
    $trimmed = $line.Trim()
    if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
        continue
    }

    if ($trimmed -notmatch "^point\s+([^|]+)\|([^|]+)\|(.+)$") {
        throw "Malformed production batch line: $trimmed"
    }

    $key = $Matches[1].Trim()
    $domain = $Matches[2].Trim()
    $description = $Matches[3].Trim()
    if ($key -notmatch "^v25_point_\d{2}_[a-z0-9_]+$") {
        throw "Production batch point key '$key' does not match the v25 naming convention."
    }
    if ([string]::IsNullOrWhiteSpace($domain) -or [string]::IsNullOrWhiteSpace($description)) {
        throw "Production batch point '$key' is missing a domain or description."
    }

    $points.Add([pscustomobject]@{
        key = $key
        domain = $domain
        description = $description
    })
}

if ($points.Count -ne $ExpectedCount) {
    throw "Production batch manifest expected $ExpectedCount point(s), found $($points.Count)."
}

$duplicateKeys = @($points | Group-Object key | Where-Object { $_.Count -gt 1 })
if ($duplicateKeys.Count -gt 0) {
    throw "Production batch manifest has duplicate key(s): $(@($duplicateKeys | ForEach-Object { $_.Name }) -join ', ')"
}

$expectedKeys = 1..$ExpectedCount | ForEach-Object { "v25_point_{0:D2}_" -f $_ }
foreach ($prefix in $expectedKeys) {
    $matchingPrefix = @($points | Where-Object { $_.key.StartsWith($prefix) })
    if ($matchingPrefix.Count -eq 0) {
        throw "Production batch manifest is missing point prefix $prefix."
    }
}

$domainCounts = @{}
foreach ($point in $points) {
    if (!$domainCounts.ContainsKey($point.domain)) {
        $domainCounts[$point.domain] = 0
    }
    ++$domainCounts[$point.domain]
}

$parent = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

[pscustomobject]@{
    schema = 1
    reviewed_utc = (Get-Date).ToUniversalTime().ToString("o")
    manifest = $ManifestPath
    expected_count = $ExpectedCount
    point_count = $points.Count
    domain_counts = $domainCounts
    points = $points
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath

Write-Host "Production batch manifest OK: $($points.Count) point(s)"
Write-Host "Production batch review written to $OutputPath"
