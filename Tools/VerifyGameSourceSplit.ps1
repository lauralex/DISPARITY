param(
    [int]$MaxRootGameLines = 13800,
    [int]$MinSplitFiles = 15
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$gameSource = Join-Path $root "DisparityGame\Source\DisparityGame.cpp"
$splitDirectory = Join-Path $root "DisparityGame\Source\DisparityGame"
$projectPath = Join-Path $root "DisparityGame\DisparityGame.vcxproj"
$outputPath = Join-Path $root "Saved\Verification\game_source_split_review.json"

if (!(Test-Path -LiteralPath $gameSource)) {
    throw "Missing game source file: $gameSource"
}
if (!(Test-Path -LiteralPath $splitDirectory)) {
    throw "Missing split game source directory: $splitDirectory"
}

$rootLineCount = (Get-Content -LiteralPath $gameSource).Count
$splitFiles = Get-ChildItem -LiteralPath $splitDirectory -File -Include *.cpp,*.h
$projectText = Get-Content -LiteralPath $projectPath -Raw

$requiredProjectEntries = @(
    "Source\DisparityGame\GameFollowupCatalog.cpp",
    "Source\DisparityGame\GameFollowupCatalog.h",
    "Source\DisparityGame\GameEventRouteCatalog.cpp",
    "Source\DisparityGame\GameEventRouteCatalog.h",
    "Source\DisparityGame\GameModuleRegistry.cpp",
    "Source\DisparityGame\GameModuleRegistry.h",
    "Source\DisparityGame\GameProductionCatalog.cpp",
    "Source\DisparityGame\GameProductionCatalog.h",
    "Source\DisparityGame\GameProductionRuntimeCatalog.cpp",
    "Source\DisparityGame\GameProductionRuntimeCatalog.h",
    "Source\DisparityGame\GameRuntimeHelpers.cpp",
    "Source\DisparityGame\GameRuntimeHelpers.h",
    "Source\DisparityGame\GameRuntimeTypes.h",
    "Source\DisparityGame\GameRoadmapBatch.cpp",
    "Source\DisparityGame\GameRoadmapBatch.h"
)

$missingProjectEntries = @()
foreach ($entry in $requiredProjectEntries) {
    if ($projectText -notlike "*$entry*") {
        $missingProjectEntries += $entry
    }
}

$rootText = Get-Content -LiteralPath $gameSource -Raw
$forbiddenInlineCatalogs = @(
    "static const std::array<ProductionFollowupPoint, V25ProductionPointCount>& GetV25ProductionPoints",
    "static const std::array<ProductionFollowupPoint, V35EngineArchitecturePointCount>& GetV35EngineArchitecturePoints",
    "struct RuntimeBaseline",
    "struct PublicDemoDiagnostics"
)

$inlineCatalogHits = @()
foreach ($pattern in $forbiddenInlineCatalogs) {
    if ($rootText.Contains($pattern)) {
        $inlineCatalogHits += $pattern
    }
}

$review = [pscustomobject]@{
    root_game_lines = $rootLineCount
    max_root_game_lines = $MaxRootGameLines
    split_file_count = $splitFiles.Count
    min_split_file_count = $MinSplitFiles
    split_files = @($splitFiles | Sort-Object Name | ForEach-Object { $_.Name })
    missing_project_entries = $missingProjectEntries
    forbidden_inline_catalog_hits = $inlineCatalogHits
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputPath) | Out-Null
$review | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outputPath

if ($rootLineCount -gt $MaxRootGameLines) {
    throw "DisparityGame.cpp has $rootLineCount lines; expected <= $MaxRootGameLines."
}
if ($splitFiles.Count -lt $MinSplitFiles) {
    throw "Expected at least $MinSplitFiles split game files; found $($splitFiles.Count)."
}
if ($missingProjectEntries.Count -gt 0) {
    throw "DisparityGame.vcxproj is missing split source entries: $($missingProjectEntries -join ', ')"
}
if ($inlineCatalogHits.Count -gt 0) {
    throw "DisparityGame.cpp still contains extracted catalog/type blocks: $($inlineCatalogHits -join ', ')"
}

Write-Host "Game source split verified: root_lines=$rootLineCount split_files=$($splitFiles.Count)"
