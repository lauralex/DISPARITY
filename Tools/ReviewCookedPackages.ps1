param(
    [string]$OptimizedPath = "Saved/CookedAssets/Optimized",
    [string]$OutputPath = "Saved/CookedAssets/package_review.json"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($OptimizedPath)) {
    $OptimizedPath = Join-Path $root $OptimizedPath
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}

$packages = @(Get-ChildItem -LiteralPath $OptimizedPath -Filter "*.dglbpack" -File -ErrorAction SilentlyContinue)
if ($packages.Count -eq 0) {
    throw "No optimized .dglbpack packages were produced."
}

$records = @()
foreach ($package in $packages) {
    $payload = Get-Content -LiteralPath $package.FullName -Raw | ConvertFrom-Json
    if ($payload.magic -ne "DSGLBPK2" -or [int]$payload.schema -lt 2) {
        throw "Optimized package $($package.Name) has an invalid schema."
    }
    if ([int]$payload.primitive_count -le 0 -or [int]$payload.mesh_count -le 0) {
        throw "Optimized package $($package.Name) did not record mesh primitives."
    }
    if (!$payload.buffers -or @($payload.buffers).Count -eq 0) {
        throw "Optimized package $($package.Name) did not record buffer payload metadata."
    }
    if ([int]$payload.dependency_count -le 0) {
        throw "Optimized package $($package.Name) did not record dependencies."
    }

    $records += [pscustomobject]@{
        package = $package.Name
        magic = $payload.magic
        schema = [int]$payload.schema
        source = $payload.source
        meshes = [int]$payload.mesh_count
        primitives = [int]$payload.primitive_count
        materials = [int]$payload.material_count
        nodes = [int]$payload.node_count
        skins = [int]$payload.skin_count
        animations = [int]$payload.animation_count
        dependencies = [int]$payload.dependency_count
        runtime_loadable = $true
    }
}

$parent = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

[pscustomobject]@{
    schema = 1
    reviewed_utc = (Get-Date).ToUniversalTime().ToString("o")
    package_count = $records.Count
    packages = $records
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath

Write-Host "Reviewed $($records.Count) optimized package(s) to $OutputPath"
