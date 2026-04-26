param(
    [switch]$SkipCodeAnalysis,
    [switch]$SkipRuntime,
    [switch]$SkipPackage,
    [int]$RuntimeFrames = 90,
    [double]$RuntimeCpuFrameBudgetMs = 120.0,
    [double]$RuntimeGpuFrameBudgetMs = 50.0,
    [double]$RuntimePassBudgetMs = 60.0
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

if (!$SkipRuntime) {
    Invoke-Step "Debug runtime window smoke" {
        & (Join-Path $PSScriptRoot "SmokeTestDisparity.ps1") -Configuration Debug -Seconds 3 -ExerciseWindow
    }

    Invoke-Step "Debug runtime self-verification" {
        & (Join-Path $PSScriptRoot "RuntimeVerifyDisparity.ps1") -Configuration Debug -Frames $RuntimeFrames -CpuFrameBudgetMs $RuntimeCpuFrameBudgetMs -GpuFrameBudgetMs $RuntimeGpuFrameBudgetMs -PassBudgetMs $RuntimePassBudgetMs
    }
}

if (!$SkipPackage) {
    Invoke-Step "Release package" {
        & (Join-Path $PSScriptRoot "PackageDisparity.ps1") -Configuration Release
    }

    if (!$SkipRuntime) {
        $packagedExecutable = Join-Path $root "dist\DISPARITY-Release\DisparityGame.exe"
        Invoke-Step "Packaged runtime window smoke" {
            & (Join-Path $PSScriptRoot "SmokeTestDisparity.ps1") -ExecutablePath $packagedExecutable -Seconds 3 -ExerciseWindow
        }

        Invoke-Step "Packaged runtime self-verification" {
            & (Join-Path $PSScriptRoot "RuntimeVerifyDisparity.ps1") -ExecutablePath $packagedExecutable -Frames $RuntimeFrames -CpuFrameBudgetMs $RuntimeCpuFrameBudgetMs -GpuFrameBudgetMs $RuntimeGpuFrameBudgetMs -PassBudgetMs $RuntimePassBudgetMs
        }
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
