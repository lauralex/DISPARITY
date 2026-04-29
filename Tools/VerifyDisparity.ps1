param(
    [switch]$SkipCodeAnalysis,
    [switch]$SkipRuntime,
    [switch]$SkipPackage,
    [int]$RuntimeFrames = 90,
    [double]$RuntimeCpuFrameBudgetMs = 120.0,
    [double]$RuntimeGpuFrameBudgetMs = 50.0,
    [double]$RuntimePassBudgetMs = 60.0,
    [string]$RuntimeReplayPath = "Assets/Verification/Prototype.dreplay",
    [string]$RuntimeBaselinePath = "Assets/Verification/RuntimeBaseline.dverify",
    [string]$RuntimeSuiteFile = "Assets/Verification/RuntimeSuites.dverify",
    [string]$GoldenProfilePath = "Assets/Verification/GoldenProfiles/Default.dgoldenprofile",
    [string]$PerformanceBaselinePath = "Assets/Verification/PerformanceBaselines.dperf",
    [switch]$DisableGoldenComparison
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if (!(Test-Path -LiteralPath $msbuild)) {
    $msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if (!$msbuildCommand) {
        throw "MSBuild.exe was not found."
    }
    $msbuild = $msbuildCommand.Source
}
$solution = Join-Path $root "DISPARITY.sln"

function Invoke-Step {
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    Write-Host ""
    Write-Host "== $Name =="
    & $Action
    Write-Host "PASS: $Name"
}

function Invoke-MSBuildChecked {
    param(
        [ValidateSet("Debug", "Release")]
        [string]$Configuration,
        [switch]$CodeAnalysis
    )

    $arguments = @(
        $solution,
        "/m",
        "/p:Configuration=$Configuration",
        "/p:Platform=x64",
        "/p:PreferredToolArchitecture=x64"
    )

    if ($CodeAnalysis) {
        $arguments += "/p:RunCodeAnalysis=true"
    }

    $output = @(& $msbuild @arguments 2>&1)
    $exitCode = $LASTEXITCODE
    $output | ForEach-Object { Write-Host $_ }

    if ($exitCode -ne 0) {
        throw "MSBuild failed for $Configuration with exit code $exitCode."
    }

    if ($CodeAnalysis) {
        $ownedWarnings = New-Object System.Collections.Generic.HashSet[string]
        $thirdPartyWarnings = New-Object System.Collections.Generic.HashSet[string]
        foreach ($line in $output) {
            $text = [string]$line
            if ($text -match "warning C\d+") {
                $normalized = ($text -replace "^\s*\d+>", "") -replace "^\s+", ""
                if ($text -match "\\ThirdParty\\") {
                    [void]$thirdPartyWarnings.Add($normalized)
                }
                else {
                    [void]$ownedWarnings.Add($normalized)
                }
            }
        }

        if ($thirdPartyWarnings.Count -gt 0) {
            Write-Host "Ignored $($thirdPartyWarnings.Count) analyzer warning(s) from vendored ThirdParty code."
        }
        if ($ownedWarnings.Count -gt 0) {
            $ownedWarnings | ForEach-Object { Write-Host $_ }
            throw "MSVC static analysis produced project-owned warnings for $Configuration."
        }
    }
    else {
        foreach ($line in $output) {
            $text = [string]$line
            $match = [regex]::Match($text, "^\s*(\d+) Warning\(s\)")
            if ($match.Success -and [int]$match.Groups[1].Value -ne 0) {
                throw "MSBuild produced warnings for $Configuration."
            }
        }
    }
}

function Get-FxcPath {
    $fxc = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter fxc.exe -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -First 1 -ExpandProperty FullName
    if (!$fxc) {
        throw "fxc.exe was not found."
    }

    return $fxc
}

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $root $Path
}

function Get-RuntimeSuites {
    $suitePath = Resolve-RepoPath -Path $RuntimeSuiteFile
    if (!(Test-Path -LiteralPath $suitePath)) {
        return @([pscustomobject]@{
            Name = "Default"
            Frames = $RuntimeFrames
            ReplayPath = $RuntimeReplayPath
            BaselinePath = $RuntimeBaselinePath
        })
    }

    $suites = @()
    foreach ($line in Get-Content -LiteralPath $suitePath) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
            continue
        }

        $parts = $trimmed -split "\|"
        if ($parts.Count -ne 4) {
            throw "Invalid runtime suite line in ${suitePath}: $line"
        }

        $frames = [int]$parts[1].Trim()
        if ($frames -le 0) {
            throw "Runtime suite $($parts[0]) has invalid frame count $frames."
        }

        $suites += [pscustomobject]@{
            Name = $parts[0].Trim()
            Frames = $frames
            ReplayPath = $parts[2].Trim()
            BaselinePath = $parts[3].Trim()
        }
    }

    if ($suites.Count -eq 0) {
        throw "Runtime suite file $suitePath did not define any suites."
    }

    return $suites
}

Invoke-Step "Git whitespace check" {
    Push-Location $root
    try {
        git diff --check
        if ($LASTEXITCODE -ne 0) {
            throw "git diff --check failed."
        }
    }
    finally {
        Pop-Location
    }
}

Invoke-Step "Dear ImGui ID conflict check" {
    & (Join-Path $PSScriptRoot "TestImGuiIds.ps1")
}

Invoke-Step "Game source split check" {
    & (Join-Path $PSScriptRoot "VerifyGameSourceSplit.ps1")
}

Invoke-Step "Debug build, warning-free" {
    Invoke-MSBuildChecked -Configuration Debug
}

Invoke-Step "Release build, warning-free" {
    Invoke-MSBuildChecked -Configuration Release
}

if (!$SkipCodeAnalysis) {
    Invoke-Step "MSVC static analysis" {
        Invoke-MSBuildChecked -Configuration Debug -CodeAnalysis
    }
}

Invoke-Step "HLSL shader compilation" {
    $fxc = Get-FxcPath
    & $fxc /nologo /T vs_5_0 /E VSMain /Fo NUL (Join-Path $root "Assets\Shaders\Basic.hlsl")
    if ($LASTEXITCODE -ne 0) { throw "Basic.hlsl VSMain failed." }
    & $fxc /nologo /T ps_5_0 /E PSMain /Fo NUL (Join-Path $root "Assets\Shaders\Basic.hlsl")
    if ($LASTEXITCODE -ne 0) { throw "Basic.hlsl PSMain failed." }
    & $fxc /nologo /T vs_5_0 /E VSMain /Fo NUL (Join-Path $root "Assets\Shaders\PostProcess.hlsl")
    if ($LASTEXITCODE -ne 0) { throw "PostProcess.hlsl VSMain failed." }
    & $fxc /nologo /T ps_5_0 /E PSMain /Fo NUL (Join-Path $root "Assets\Shaders\PostProcess.hlsl")
    if ($LASTEXITCODE -ne 0) { throw "PostProcess.hlsl PSMain failed." }
}

Invoke-Step "Asset cook manifest" {
    & (Join-Path $PSScriptRoot "CookDisparityAssets.ps1") -Configuration Debug -BinaryPackages
}

Invoke-Step "Optimized glTF package review" {
    & (Join-Path $PSScriptRoot "ReviewCookedPackages.ps1")
}

Invoke-Step "Crash upload manifest dry run" {
    & (Join-Path $PSScriptRoot "CollectCrashReports.ps1") -DryRun
    & (Join-Path $PSScriptRoot "UploadCrashReports.ps1") -DryRun
}

Invoke-Step "Verification baseline review" {
    & (Join-Path $PSScriptRoot "ReviewVerificationBaselines.ps1") -HistoryPath (Join-Path $root "Saved\Verification\performance_history.csv") -PerformanceBaselinePath $PerformanceBaselinePath -ListGoldenProfiles
}

Invoke-Step "Production followup manifest review" {
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1")
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V28DiversifiedBatch.dfollowups" `
        -ExpectedCount 36 `
        -OutputPath "Saved/Verification/v28_diversified_batch_review.json" `
        -KeyPrefix "v28"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V29PublicDemo.dfollowups" `
        -ExpectedCount 30 `
        -OutputPath "Saved/Verification/v29_public_demo_review.json" `
        -KeyPrefix "v29"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V30VerticalSlice.dfollowups" `
        -ExpectedCount 36 `
        -OutputPath "Saved/Verification/v30_vertical_slice_review.json" `
        -KeyPrefix "v30"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V31DiversifiedBatch.dfollowups" `
        -ExpectedCount 30 `
        -OutputPath "Saved/Verification/v31_diversified_batch_review.json" `
        -KeyPrefix "v31"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V32RoadmapBatch.dfollowups" `
        -ExpectedCount 60 `
        -OutputPath "Saved/Verification/v32_roadmap_batch_review.json" `
        -KeyPrefix "v32"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V33PlayableDemoBatch.dfollowups" `
        -ExpectedCount 50 `
        -OutputPath "Saved/Verification/v33_playable_demo_batch_review.json" `
        -KeyPrefix "v33"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V34AAAFoundationBatch.dfollowups" `
        -ExpectedCount 60 `
        -OutputPath "Saved/Verification/v34_aaa_foundation_batch_review.json" `
        -KeyPrefix "v34"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V35EngineArchitectureBatch.dfollowups" `
        -ExpectedCount 50 `
        -OutputPath "Saved/Verification/v35_engine_architecture_batch_review.json" `
        -KeyPrefix "v35"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V36MixedBatch.dfollowups" `
        -ExpectedCount 60 `
        -OutputPath "Saved/Verification/v36_mixed_batch_review.json" `
        -KeyPrefix "v36"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V38DiversifiedBatch.dfollowups" `
        -ExpectedCount 30 `
        -OutputPath "Saved/Verification/v38_diversified_batch_review.json" `
        -KeyPrefix "v38"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V39RoadmapBatch.dfollowups" `
        -ExpectedCount 15 `
        -OutputPath "Saved/Verification/v39_roadmap_batch_review.json" `
        -KeyPrefix "v39"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V40DiversifiedBatch.dfollowups" `
        -ExpectedCount 15 `
        -OutputPath "Saved/Verification/v40_diversified_batch_review.json" `
        -KeyPrefix "v40"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V41BreadthBatch.dfollowups" `
        -ExpectedCount 20 `
        -OutputPath "Saved/Verification/v41_breadth_batch_review.json" `
        -KeyPrefix "v41"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V42ProductionSurface.dfollowups" `
        -ExpectedCount 24 `
        -OutputPath "Saved/Verification/v42_production_surface_review.json" `
        -KeyPrefix "v42"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V43LiveProductionValidation.dfollowups" `
        -ExpectedCount 24 `
        -OutputPath "Saved/Verification/v43_live_validation_review.json" `
        -KeyPrefix "v43"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V44RuntimeCatalogActivation.dfollowups" `
        -ExpectedCount 24 `
        -OutputPath "Saved/Verification/v44_runtime_catalog_review.json" `
        -KeyPrefix "v44"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V45LiveProductionCatalog.dfollowups" `
        -ExpectedCount 24 `
        -OutputPath "Saved/Verification/v45_live_catalog_review.json" `
        -KeyPrefix "v45"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V46CatalogActionPreview.dfollowups" `
        -ExpectedCount 24 `
        -OutputPath "Saved/Verification/v46_catalog_action_preview_review.json" `
        -KeyPrefix "v46"
    & (Join-Path $PSScriptRoot "ReviewProductionBatch.ps1") `
        -ManifestPath "Assets/Verification/V47CatalogExecutionMode.dfollowups" `
        -ExpectedCount 24 `
        -OutputPath "Saved/Verification/v47_catalog_execution_review.json" `
        -KeyPrefix "v47"
}

Invoke-Step "Baseline approval manifest" {
    & (Join-Path $PSScriptRoot "ApproveVerificationBaseline.ps1") -DryRun
    $approvalPath = Join-Path $root "Saved\Verification\baseline_approval.json"
    $approval = Get-Content -LiteralPath $approvalPath -Raw | ConvertFrom-Json
    if (!$approval.signature_policy -or !$approval.git_signature_status -or !$approval.manifest_sha256) {
        throw "Baseline approval manifest is missing signature metadata."
    }
}

Invoke-Step "Signed baseline update approval" {
    & (Join-Path $PSScriptRoot "ApproveVerificationUpdate.ps1") -Reason "v24 verification dry-run"
    $approvalPath = Join-Path $root "Saved\Verification\baseline_update_approval.json"
    $approval = Get-Content -LiteralPath $approvalPath -Raw | ConvertFrom-Json
    if (!$approval.git_head -or !$approval.git_signature_status -or [int]$approval.file_count -le 0) {
        throw "Baseline update approval manifest is missing git or file metadata."
    }
}

Invoke-Step "Interactive CI plan" {
    & (Join-Path $PSScriptRoot "GenerateInteractiveCiPlan.ps1") -RuntimeSuiteFile $RuntimeSuiteFile
    & (Join-Path $PSScriptRoot "LaunchTrailerCapture.ps1") -Configuration Debug -NoLaunch
    & (Join-Path $PSScriptRoot "GenerateObsSceneProfile.ps1")
}

if (!$SkipRuntime) {
    $runtimeSuites = Get-RuntimeSuites
    Invoke-Step "Debug runtime window smoke" {
        & (Join-Path $PSScriptRoot "SmokeTestDisparity.ps1") -Configuration Debug -Seconds 3 -ExerciseWindow
    }

    foreach ($suite in $runtimeSuites) {
        Invoke-Step "Debug runtime self-verification ($($suite.Name))" {
            $runtimeArguments = @{
                Configuration = "Debug"
                Frames = $suite.Frames
                CpuFrameBudgetMs = $RuntimeCpuFrameBudgetMs
                GpuFrameBudgetMs = $RuntimeGpuFrameBudgetMs
                PassBudgetMs = $RuntimePassBudgetMs
                SuiteName = $suite.Name
                ReplayPath = $suite.ReplayPath
                BaselinePath = $suite.BaselinePath
                GoldenProfilePath = $GoldenProfilePath
                HistoryPath = (Join-Path $root "Saved\Verification\performance_history.csv")
            }
            if ($DisableGoldenComparison) {
                $runtimeArguments["DisableGoldenComparison"] = $true
            }

            & (Join-Path $PSScriptRoot "RuntimeVerifyDisparity.ps1") @runtimeArguments
        }
    }
}

if (!$SkipPackage) {
    Invoke-Step "Release package" {
        & (Join-Path $PSScriptRoot "PackageDisparity.ps1") -Configuration Release -IncludeSymbols -CreateArchive
    }

    Invoke-Step "Release symbol index" {
        & (Join-Path $PSScriptRoot "IndexDisparitySymbols.ps1") -PackagePath (Join-Path $root "dist\DISPARITY-Release")
        & (Join-Path $PSScriptRoot "PublishDisparitySymbols.ps1") -PackagePath (Join-Path $root "dist\DISPARITY-Release")
    }

    Invoke-Step "Installer payload manifest" {
        & (Join-Path $PSScriptRoot "CreateDisparityInstaller.ps1") -PackagePath (Join-Path $root "dist\DISPARITY-Release") -CreateArchive
        & (Join-Path $PSScriptRoot "CreateDisparityBootstrapper.ps1") -PackagePath (Join-Path $root "dist\DISPARITY-Release")
    }

    Invoke-Step "Release readiness manifest" {
        & (Join-Path $PSScriptRoot "ReviewReleaseReadiness.ps1") `
            -PackagePath (Join-Path $root "dist\DISPARITY-Release") `
            -InstallerPath (Join-Path $root "dist\Installer") `
            -ObsProfilePath (Join-Path $root "Saved\Trailer\OBS\DISPARITY-Trailer-Scene.json") `
            -RuntimeReportSchemaPath (Join-Path $root "Assets\Verification\RuntimeReportSchema.dschema")
    }

    if (!$SkipRuntime) {
        $packagedExecutable = Join-Path $root "dist\DISPARITY-Release\DisparityGame.exe"
        Invoke-Step "Packaged runtime window smoke" {
            & (Join-Path $PSScriptRoot "SmokeTestDisparity.ps1") -ExecutablePath $packagedExecutable -Seconds 3 -ExerciseWindow
        }

        if (!$SkipRuntime) {
            $runtimeSuites = Get-RuntimeSuites
        }

        foreach ($suite in $runtimeSuites) {
            Invoke-Step "Packaged runtime self-verification ($($suite.Name))" {
                $runtimeArguments = @{
                    ExecutablePath = $packagedExecutable
                    Frames = $suite.Frames
                    CpuFrameBudgetMs = $RuntimeCpuFrameBudgetMs
                    GpuFrameBudgetMs = $RuntimeGpuFrameBudgetMs
                    PassBudgetMs = $RuntimePassBudgetMs
                    SuiteName = $suite.Name
                    ReplayPath = $suite.ReplayPath
                    BaselinePath = $suite.BaselinePath
                    GoldenProfilePath = $GoldenProfilePath
                    HistoryPath = (Join-Path $root "Saved\Verification\performance_history.csv")
                }
                if ($DisableGoldenComparison) {
                    $runtimeArguments["DisableGoldenComparison"] = $true
                }

                & (Join-Path $PSScriptRoot "RuntimeVerifyDisparity.ps1") @runtimeArguments
            }
        }
    }
}

if (!$SkipRuntime) {
    Invoke-Step "Performance history summary" {
        & (Join-Path $PSScriptRoot "SummarizePerformanceHistory.ps1") -HistoryPath (Join-Path $root "Saved\Verification\performance_history.csv") -BaselinePath $PerformanceBaselinePath
    }
}

Invoke-Step "Git status review" {
    Push-Location $root
    try {
        git status --short
    }
    finally {
        Pop-Location
    }
}

Write-Host ""
Write-Host "DISPARITY verification completed successfully."
