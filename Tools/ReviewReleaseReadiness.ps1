param(
    [string]$PackagePath = "dist/DISPARITY-Release",
    [string]$InstallerPath = "dist/Installer",
    [string]$ObsProfilePath = "Saved/Trailer/OBS/DISPARITY-Trailer-Scene.json",
    [string]$RuntimeReportSchemaPath = "Assets/Verification/RuntimeReportSchema.dschema",
    [string]$ProductionBatchPath = "Assets/Verification/V25ProductionBatch.dfollowups",
    [string]$DiversifiedBatchPath = "Assets/Verification/V28DiversifiedBatch.dfollowups",
    [string]$PublicDemoBatchPath = "Assets/Verification/V29PublicDemo.dfollowups",
    [string]$VerticalSliceBatchPath = "Assets/Verification/V30VerticalSlice.dfollowups",
    [string]$V31DiversifiedBatchPath = "Assets/Verification/V31DiversifiedBatch.dfollowups",
    [string]$V32RoadmapBatchPath = "Assets/Verification/V32RoadmapBatch.dfollowups",
    [string]$V33PlayableDemoBatchPath = "Assets/Verification/V33PlayableDemoBatch.dfollowups",
    [string]$V34AAAFoundationBatchPath = "Assets/Verification/V34AAAFoundationBatch.dfollowups",
    [string]$V35EngineArchitectureBatchPath = "Assets/Verification/V35EngineArchitectureBatch.dfollowups",
    [string]$V36MixedBatchPath = "Assets/Verification/V36MixedBatch.dfollowups",
    [string]$V38DiversifiedBatchPath = "Assets/Verification/V38DiversifiedBatch.dfollowups",
    [string]$V39RoadmapBatchPath = "Assets/Verification/V39RoadmapBatch.dfollowups",
    [string]$V40DiversifiedBatchPath = "Assets/Verification/V40DiversifiedBatch.dfollowups",
    [string]$V41BreadthBatchPath = "Assets/Verification/V41BreadthBatch.dfollowups",
    [string]$V42ProductionSurfacePath = "Assets/Verification/V42ProductionSurface.dfollowups",
    [string]$V43LiveValidationPath = "Assets/Verification/V43LiveProductionValidation.dfollowups",
    [string]$V44RuntimeCatalogPath = "Assets/Verification/V44RuntimeCatalogActivation.dfollowups",
    [string]$V45LiveCatalogPath = "Assets/Verification/V45LiveProductionCatalog.dfollowups",
    [string]$V46CatalogActionPreviewPath = "Assets/Verification/V46CatalogActionPreview.dfollowups",
    [string]$V47CatalogExecutionPath = "Assets/Verification/V47CatalogExecutionMode.dfollowups",
    [string]$V48ActionDirectorPath = "Assets/Verification/V48ActionDirector.dfollowups",
    [string]$V49ActionMutationPath = "Assets/Verification/V49ActionMutation.dfollowups",
    [string]$OutputPath = "Saved/Release/release_readiness_manifest.json"
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

$PackagePath = Resolve-RepoPath -Path $PackagePath
$InstallerPath = Resolve-RepoPath -Path $InstallerPath
$ObsProfilePath = Resolve-RepoPath -Path $ObsProfilePath
$RuntimeReportSchemaPath = Resolve-RepoPath -Path $RuntimeReportSchemaPath
$ProductionBatchPath = Resolve-RepoPath -Path $ProductionBatchPath
$DiversifiedBatchPath = Resolve-RepoPath -Path $DiversifiedBatchPath
$PublicDemoBatchPath = Resolve-RepoPath -Path $PublicDemoBatchPath
$VerticalSliceBatchPath = Resolve-RepoPath -Path $VerticalSliceBatchPath
$V31DiversifiedBatchPath = Resolve-RepoPath -Path $V31DiversifiedBatchPath
$V32RoadmapBatchPath = Resolve-RepoPath -Path $V32RoadmapBatchPath
$V33PlayableDemoBatchPath = Resolve-RepoPath -Path $V33PlayableDemoBatchPath
$V34AAAFoundationBatchPath = Resolve-RepoPath -Path $V34AAAFoundationBatchPath
$V35EngineArchitectureBatchPath = Resolve-RepoPath -Path $V35EngineArchitectureBatchPath
$V36MixedBatchPath = Resolve-RepoPath -Path $V36MixedBatchPath
$V38DiversifiedBatchPath = Resolve-RepoPath -Path $V38DiversifiedBatchPath
$V39RoadmapBatchPath = Resolve-RepoPath -Path $V39RoadmapBatchPath
$V40DiversifiedBatchPath = Resolve-RepoPath -Path $V40DiversifiedBatchPath
$V41BreadthBatchPath = Resolve-RepoPath -Path $V41BreadthBatchPath
$V42ProductionSurfacePath = Resolve-RepoPath -Path $V42ProductionSurfacePath
$V43LiveValidationPath = Resolve-RepoPath -Path $V43LiveValidationPath
$V44RuntimeCatalogPath = Resolve-RepoPath -Path $V44RuntimeCatalogPath
$V45LiveCatalogPath = Resolve-RepoPath -Path $V45LiveCatalogPath
$V46CatalogActionPreviewPath = Resolve-RepoPath -Path $V46CatalogActionPreviewPath
$V47CatalogExecutionPath = Resolve-RepoPath -Path $V47CatalogExecutionPath
$V48ActionDirectorPath = Resolve-RepoPath -Path $V48ActionDirectorPath
$V49ActionMutationPath = Resolve-RepoPath -Path $V49ActionMutationPath
$OutputPath = Resolve-RepoPath -Path $OutputPath

$packageManifestPath = Join-Path $PackagePath "package_manifest.json"
$installerManifestPath = Join-Path $InstallerPath "DISPARITY-SetupManifest.json"
$bootstrapperPath = Join-Path $InstallerPath "DISPARITY-InstallerBootstrapper.cmd"
$symbolManifestPath = Join-Path $PackagePath "Symbols/symbols_manifest.json"

$checks = @(
    [pscustomobject]@{ name = "package_manifest"; path = $packageManifestPath; exists = Test-Path -LiteralPath $packageManifestPath },
    [pscustomobject]@{ name = "installer_manifest"; path = $installerManifestPath; exists = Test-Path -LiteralPath $installerManifestPath },
    [pscustomobject]@{ name = "bootstrapper_command"; path = $bootstrapperPath; exists = Test-Path -LiteralPath $bootstrapperPath },
    [pscustomobject]@{ name = "symbol_manifest"; path = $symbolManifestPath; exists = Test-Path -LiteralPath $symbolManifestPath },
    [pscustomobject]@{ name = "obs_profile"; path = $ObsProfilePath; exists = Test-Path -LiteralPath $ObsProfilePath },
    [pscustomobject]@{ name = "runtime_report_schema"; path = $RuntimeReportSchemaPath; exists = Test-Path -LiteralPath $RuntimeReportSchemaPath },
    [pscustomobject]@{ name = "production_batch_manifest"; path = $ProductionBatchPath; exists = Test-Path -LiteralPath $ProductionBatchPath },
    [pscustomobject]@{ name = "diversified_batch_manifest"; path = $DiversifiedBatchPath; exists = Test-Path -LiteralPath $DiversifiedBatchPath },
    [pscustomobject]@{ name = "public_demo_batch_manifest"; path = $PublicDemoBatchPath; exists = Test-Path -LiteralPath $PublicDemoBatchPath },
    [pscustomobject]@{ name = "vertical_slice_batch_manifest"; path = $VerticalSliceBatchPath; exists = Test-Path -LiteralPath $VerticalSliceBatchPath },
    [pscustomobject]@{ name = "v31_diversified_batch_manifest"; path = $V31DiversifiedBatchPath; exists = Test-Path -LiteralPath $V31DiversifiedBatchPath },
    [pscustomobject]@{ name = "v32_roadmap_batch_manifest"; path = $V32RoadmapBatchPath; exists = Test-Path -LiteralPath $V32RoadmapBatchPath },
    [pscustomobject]@{ name = "v33_playable_demo_batch_manifest"; path = $V33PlayableDemoBatchPath; exists = Test-Path -LiteralPath $V33PlayableDemoBatchPath },
    [pscustomobject]@{ name = "v34_aaa_foundation_batch_manifest"; path = $V34AAAFoundationBatchPath; exists = Test-Path -LiteralPath $V34AAAFoundationBatchPath },
    [pscustomobject]@{ name = "v35_engine_architecture_batch_manifest"; path = $V35EngineArchitectureBatchPath; exists = Test-Path -LiteralPath $V35EngineArchitectureBatchPath },
    [pscustomobject]@{ name = "v36_mixed_batch_manifest"; path = $V36MixedBatchPath; exists = Test-Path -LiteralPath $V36MixedBatchPath },
    [pscustomobject]@{ name = "v38_diversified_batch_manifest"; path = $V38DiversifiedBatchPath; exists = Test-Path -LiteralPath $V38DiversifiedBatchPath },
    [pscustomobject]@{ name = "v39_roadmap_batch_manifest"; path = $V39RoadmapBatchPath; exists = Test-Path -LiteralPath $V39RoadmapBatchPath },
    [pscustomobject]@{ name = "v40_diversified_batch_manifest"; path = $V40DiversifiedBatchPath; exists = Test-Path -LiteralPath $V40DiversifiedBatchPath },
    [pscustomobject]@{ name = "v41_breadth_batch_manifest"; path = $V41BreadthBatchPath; exists = Test-Path -LiteralPath $V41BreadthBatchPath },
    [pscustomobject]@{ name = "v42_production_surface_manifest"; path = $V42ProductionSurfacePath; exists = Test-Path -LiteralPath $V42ProductionSurfacePath },
    [pscustomobject]@{ name = "v43_live_validation_manifest"; path = $V43LiveValidationPath; exists = Test-Path -LiteralPath $V43LiveValidationPath },
    [pscustomobject]@{ name = "v44_runtime_catalog_manifest"; path = $V44RuntimeCatalogPath; exists = Test-Path -LiteralPath $V44RuntimeCatalogPath },
    [pscustomobject]@{ name = "v45_live_catalog_manifest"; path = $V45LiveCatalogPath; exists = Test-Path -LiteralPath $V45LiveCatalogPath },
    [pscustomobject]@{ name = "v46_catalog_action_preview_manifest"; path = $V46CatalogActionPreviewPath; exists = Test-Path -LiteralPath $V46CatalogActionPreviewPath },
    [pscustomobject]@{ name = "v47_catalog_execution_manifest"; path = $V47CatalogExecutionPath; exists = Test-Path -LiteralPath $V47CatalogExecutionPath },
    [pscustomobject]@{ name = "v48_action_director_manifest"; path = $V48ActionDirectorPath; exists = Test-Path -LiteralPath $V48ActionDirectorPath },
    [pscustomobject]@{ name = "v49_action_mutation_manifest"; path = $V49ActionMutationPath; exists = Test-Path -LiteralPath $V49ActionMutationPath }
)

foreach ($check in $checks) {
    if (!$check.exists) {
        throw "Release readiness check '$($check.name)' failed; missing $($check.path)"
    }
}

$packageManifest = Get-Content -LiteralPath $packageManifestPath -Raw | ConvertFrom-Json
if (!$packageManifest.version -or [int]$packageManifest.files.Count -le 0) {
    throw "Package manifest is missing version or files."
}

$obsProfile = Get-Content -LiteralPath $ObsProfilePath -Raw | ConvertFrom-Json
if (!$obsProfile.recording.approval_required -or !$obsProfile.recording.metadata_sidecar) {
    throw "OBS profile is missing recording approval metadata."
}

$schemaMetrics = @(Get-Content -LiteralPath $RuntimeReportSchemaPath | Where-Object {
    $trimmed = $_.Trim()
    ![string]::IsNullOrWhiteSpace($trimmed) -and !$trimmed.StartsWith("#")
})
if ($schemaMetrics.Count -lt 1140 -or !($schemaMetrics -contains "v25_production_points") -or !($schemaMetrics -contains "v28_diversified_points") -or !($schemaMetrics -contains "v29_public_demo_points") -or !($schemaMetrics -contains "v30_vertical_slice_points") -or !($schemaMetrics -contains "v31_diversified_points") -or !($schemaMetrics -contains "v32_roadmap_points") -or !($schemaMetrics -contains "v33_playable_demo_points") -or !($schemaMetrics -contains "v34_aaa_foundation_points") -or !($schemaMetrics -contains "v35_engine_architecture_points") -or !($schemaMetrics -contains "v36_mixed_batch_points") -or !($schemaMetrics -contains "v38_diversified_points") -or !($schemaMetrics -contains "v39_roadmap_points") -or !($schemaMetrics -contains "v40_diversified_points") -or !($schemaMetrics -contains "v41_breadth_points") -or !($schemaMetrics -contains "v42_content_points") -or !($schemaMetrics -contains "v43_validation_points") -or !($schemaMetrics -contains "v44_catalog_points") -or !($schemaMetrics -contains "v45_live_catalog_points") -or !($schemaMetrics -contains "v46_catalog_action_preview_points") -or !($schemaMetrics -contains "v47_catalog_execution_points") -or !($schemaMetrics -contains "v48_action_director_points") -or !($schemaMetrics -contains "v49_action_mutation_points")) {
    throw "Runtime report schema does not include the v25-v49 production metrics."
}

$productionPoints = @(Get-Content -LiteralPath $ProductionBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($productionPoints.Count -ne 40) {
    throw "Production batch manifest does not define forty points."
}

$diversifiedPoints = @(Get-Content -LiteralPath $DiversifiedBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($diversifiedPoints.Count -ne 36) {
    throw "Diversified batch manifest does not define thirty-six points."
}

$publicDemoPoints = @(Get-Content -LiteralPath $PublicDemoBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($publicDemoPoints.Count -ne 30) {
    throw "Public demo batch manifest does not define thirty points."
}

$verticalSlicePoints = @(Get-Content -LiteralPath $VerticalSliceBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($verticalSlicePoints.Count -ne 36) {
    throw "Vertical slice batch manifest does not define thirty-six points."
}

$v31DiversifiedPoints = @(Get-Content -LiteralPath $V31DiversifiedBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v31DiversifiedPoints.Count -ne 30) {
    throw "v31 diversified batch manifest does not define thirty points."
}

$v32RoadmapPoints = @(Get-Content -LiteralPath $V32RoadmapBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v32RoadmapPoints.Count -ne 60) {
    throw "v32 roadmap batch manifest does not define sixty points."
}

$v33PlayableDemoPoints = @(Get-Content -LiteralPath $V33PlayableDemoBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v33PlayableDemoPoints.Count -ne 50) {
    throw "v33 playable demo batch manifest does not define fifty points."
}

$v34AAAFoundationPoints = @(Get-Content -LiteralPath $V34AAAFoundationBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v34AAAFoundationPoints.Count -ne 60) {
    throw "v34 AAA foundation batch manifest does not define sixty points."
}

$v35EngineArchitecturePoints = @(Get-Content -LiteralPath $V35EngineArchitectureBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v35EngineArchitecturePoints.Count -ne 50) {
    throw "v35 engine architecture batch manifest does not define fifty points."
}

$v36MixedPoints = @(Get-Content -LiteralPath $V36MixedBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v36MixedPoints.Count -ne 60) {
    throw "v36 mixed batch manifest does not define sixty points."
}

$v38DiversifiedPoints = @(Get-Content -LiteralPath $V38DiversifiedBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v38DiversifiedPoints.Count -ne 30) {
    throw "v38 diversified batch manifest does not define thirty points."
}

$v39RoadmapPoints = @(Get-Content -LiteralPath $V39RoadmapBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v39RoadmapPoints.Count -ne 15) {
    throw "v39 roadmap batch manifest does not define fifteen points."
}

$v40DiversifiedPoints = @(Get-Content -LiteralPath $V40DiversifiedBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v40DiversifiedPoints.Count -ne 15) {
    throw "v40 diversified batch manifest does not define fifteen points."
}

$v41BreadthPoints = @(Get-Content -LiteralPath $V41BreadthBatchPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v41BreadthPoints.Count -ne 20) {
    throw "v41 breadth batch manifest does not define twenty points."
}

$v42ProductionSurfacePoints = @(Get-Content -LiteralPath $V42ProductionSurfacePath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v42ProductionSurfacePoints.Count -ne 24) {
    throw "v42 production surface manifest does not define twenty-four points."
}

$v43LiveValidationPoints = @(Get-Content -LiteralPath $V43LiveValidationPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v43LiveValidationPoints.Count -ne 24) {
    throw "v43 live production validation manifest does not define twenty-four points."
}

$v44RuntimeCatalogPoints = @(Get-Content -LiteralPath $V44RuntimeCatalogPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v44RuntimeCatalogPoints.Count -ne 24) {
    throw "v44 runtime catalog activation manifest does not define twenty-four points."
}

$v45LiveCatalogPoints = @(Get-Content -LiteralPath $V45LiveCatalogPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v45LiveCatalogPoints.Count -ne 24) {
    throw "v45 live production catalog manifest does not define twenty-four points."
}

$v46CatalogActionPreviewPoints = @(Get-Content -LiteralPath $V46CatalogActionPreviewPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v46CatalogActionPreviewPoints.Count -ne 24) {
    throw "v46 catalog action preview manifest does not define twenty-four points."
}

$v47CatalogExecutionPoints = @(Get-Content -LiteralPath $V47CatalogExecutionPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v47CatalogExecutionPoints.Count -ne 24) {
    throw "v47 catalog execution manifest does not define twenty-four points."
}

$v48ActionDirectorPoints = @(Get-Content -LiteralPath $V48ActionDirectorPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v48ActionDirectorPoints.Count -ne 24) {
    throw "v48 Action Director manifest does not define twenty-four points."
}

$v49ActionMutationPoints = @(Get-Content -LiteralPath $V49ActionMutationPath | Where-Object {
    $_.Trim().StartsWith("point ")
})
if ($v49ActionMutationPoints.Count -ne 24) {
    throw "v49 Action Mutation manifest does not define twenty-four points."
}

$parent = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

[pscustomobject]@{
    schema = 1
    reviewed_utc = (Get-Date).ToUniversalTime().ToString("o")
    package_version = $packageManifest.version
    package_file_count = $packageManifest.files.Count
    runtime_schema_metric_count = $schemaMetrics.Count
    production_batch_point_count = $productionPoints.Count
    diversified_batch_point_count = $diversifiedPoints.Count
    public_demo_batch_point_count = $publicDemoPoints.Count
    vertical_slice_batch_point_count = $verticalSlicePoints.Count
    v31_diversified_batch_point_count = $v31DiversifiedPoints.Count
    v32_roadmap_batch_point_count = $v32RoadmapPoints.Count
    v33_playable_demo_batch_point_count = $v33PlayableDemoPoints.Count
    v34_aaa_foundation_batch_point_count = $v34AAAFoundationPoints.Count
    v35_engine_architecture_batch_point_count = $v35EngineArchitecturePoints.Count
    v36_mixed_batch_point_count = $v36MixedPoints.Count
    v38_diversified_batch_point_count = $v38DiversifiedPoints.Count
    v39_roadmap_batch_point_count = $v39RoadmapPoints.Count
    v40_diversified_batch_point_count = $v40DiversifiedPoints.Count
    v41_breadth_batch_point_count = $v41BreadthPoints.Count
    v42_production_surface_point_count = $v42ProductionSurfacePoints.Count
    v43_live_validation_point_count = $v43LiveValidationPoints.Count
    v44_runtime_catalog_point_count = $v44RuntimeCatalogPoints.Count
    v45_live_catalog_point_count = $v45LiveCatalogPoints.Count
    v46_catalog_action_preview_point_count = $v46CatalogActionPreviewPoints.Count
    v47_catalog_execution_point_count = $v47CatalogExecutionPoints.Count
    v48_action_director_point_count = $v48ActionDirectorPoints.Count
    v49_action_mutation_point_count = $v49ActionMutationPoints.Count
    checks = $checks
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath

Write-Host "Release readiness manifest written to $OutputPath"
