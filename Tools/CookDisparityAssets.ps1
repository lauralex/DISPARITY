param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$AssetsPath = "Assets",
    [string]$OutputPath = "Saved/CookedAssets",
    [switch]$BinaryPackages,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($AssetsPath)) {
    $AssetsPath = Join-Path $root $AssetsPath
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $root $OutputPath
}

if (!(Test-Path -LiteralPath $AssetsPath)) {
    throw "Assets folder not found at $AssetsPath"
}
if ($Clean -and (Test-Path -LiteralPath $OutputPath)) {
    Remove-Item -LiteralPath $OutputPath -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
if ($BinaryPackages) {
    New-Item -ItemType Directory -Force -Path (Join-Path $OutputPath "Binary") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $OutputPath "Optimized") | Out-Null
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

function Get-AssetKind {
    param([string]$Extension)

    switch ($Extension.ToLowerInvariant()) {
        ".gltf" { return "gltf_scene" }
        ".glb" { return "glb_scene" }
        ".dmat" { return "material" }
        ".dprefab" { return "prefab" }
        ".dscript" { return "script" }
        ".dscene" { return "scene" }
        ".hlsl" { return "shader" }
        ".dverify" { return "verification_baseline" }
        ".dreplay" { return "verification_replay" }
        ".dgoldenprofile" { return "golden_profile" }
        ".dperf" { return "performance_profile" }
        default { return "source" }
    }
}

function Add-Dependency {
    param(
        [string[]]$Dependencies,
        [string]$Dependency
    )

    if ([string]::IsNullOrWhiteSpace($Dependency)) {
        return $Dependencies
    }
    if ($Dependency.StartsWith("data:", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $Dependencies
    }
    if ($Dependencies -contains $Dependency) {
        return $Dependencies
    }
    return @($Dependencies + $Dependency)
}

function Resolve-DeclaredDependency {
    param(
        [System.IO.FileInfo]$Owner,
        [string]$Dependency
    )

    if ([string]::IsNullOrWhiteSpace($Dependency) -or $Dependency.StartsWith("data:", [System.StringComparison]::OrdinalIgnoreCase)) {
        return ""
    }

    $resolved = $Dependency
    if (![System.IO.Path]::IsPathRooted($resolved)) {
        $rootRelative = Join-Path $root $resolved
        if (Test-Path -LiteralPath $rootRelative) {
            $resolved = $rootRelative
        }
        else {
            $resolved = Join-Path $Owner.DirectoryName $resolved
        }
    }
    if (!(Test-Path -LiteralPath $resolved)) {
        return (Get-RelativePath -BasePath $root -Path $resolved).Replace("\", "/")
    }
    return (Get-RelativePath -BasePath $root -Path (Resolve-Path -LiteralPath $resolved).Path).Replace("\", "/")
}

function Get-DeclaredDependencies {
    param(
        [System.IO.FileInfo]$File,
        [string]$Kind
    )

    $dependencies = @()
    if ($Kind -eq "gltf_scene") {
        try {
            $json = Get-Content -LiteralPath $File.FullName -Raw | ConvertFrom-Json
            foreach ($buffer in @($json.buffers)) {
                if ($buffer.uri) {
                    $dependencies = Add-Dependency -Dependencies $dependencies -Dependency (Resolve-DeclaredDependency -Owner $File -Dependency $buffer.uri)
                }
            }
            foreach ($image in @($json.images)) {
                if ($image.uri) {
                    $dependencies = Add-Dependency -Dependencies $dependencies -Dependency (Resolve-DeclaredDependency -Owner $File -Dependency $image.uri)
                }
            }
        }
        catch {
            Write-Warning "Failed to inspect glTF dependencies for $($File.FullName): $($_.Exception.Message)"
        }
    }
    elseif ($Kind -eq "material") {
        foreach ($line in Get-Content -LiteralPath $File.FullName) {
            if ($line -match "^(texture|normal_texture|metallic_roughness_texture|emissive_texture|occlusion_texture)=(.+)$") {
                $dependencies = Add-Dependency -Dependencies $dependencies -Dependency (Resolve-DeclaredDependency -Owner $File -Dependency $Matches[2].Trim())
            }
        }
    }
    elseif ($Kind -eq "prefab") {
        foreach ($line in Get-Content -LiteralPath $File.FullName) {
            if ($line -match "^(parent|child_prefab)=(.+)$") {
                $dependencies = Add-Dependency -Dependencies $dependencies -Dependency (Resolve-DeclaredDependency -Owner $File -Dependency $Matches[2].Trim())
            }
        }
    }
    elseif ($Kind -eq "script") {
        foreach ($line in Get-Content -LiteralPath $File.FullName) {
            $trimmed = $line.Trim()
            if ($trimmed.StartsWith("#") -or $trimmed.Length -eq 0) {
                continue
            }
            $parts = $trimmed -split "\s+"
            if ($parts.Count -ge 2 -and $parts[0] -eq "prefab") {
                $dependencies = Add-Dependency -Dependencies $dependencies -Dependency (Resolve-DeclaredDependency -Owner $File -Dependency $parts[1])
            }
        }
    }

    return $dependencies
}

$records = @()
foreach ($file in Get-ChildItem -LiteralPath $AssetsPath -Recurse -File) {
    $relativePath = (Get-RelativePath -BasePath $root -Path $file.FullName).Replace("\", "/")
    $assetRelativePath = (Get-RelativePath -BasePath $AssetsPath -Path $file.FullName).Replace("\", "/")
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $kind = Get-AssetKind -Extension $file.Extension
    $metadataName = ($assetRelativePath -replace "[\\/:\*\?`"<>|]", "_") + ".dcooked"
    $metadataPath = Join-Path $OutputPath $metadataName
    $binaryName = ($assetRelativePath -replace "[\\/:\*\?`"<>|]", "_") + ".dassetbin"
    $binaryPath = Join-Path (Join-Path $OutputPath "Binary") $binaryName
    $optimizedName = ($assetRelativePath -replace "[\\/:\*\?`"<>|]", "_") + ".dglbpack"
    $optimizedPath = Join-Path (Join-Path $OutputPath "Optimized") $optimizedName
    $importSettings = Join-Path $root ("Assets/ImportSettings/" + $relativePath + ".dimport")
    $dependencies = @()
    if (Test-Path -LiteralPath $importSettings) {
        $dependencies = Add-Dependency -Dependencies $dependencies -Dependency ((Get-RelativePath -BasePath $root -Path $importSettings).Replace("\", "/"))
    }
    foreach ($dependency in Get-DeclaredDependencies -File $file -Kind $kind) {
        $dependencies = Add-Dependency -Dependencies $dependencies -Dependency $dependency
    }

    $cookPayload = "metadata_only"
    if ($kind -eq "glb_scene" -or $kind -eq "gltf_scene") {
        $cookPayload = "optimized_glb_package_placeholder"
    }
    elseif ($BinaryPackages) {
        $cookPayload = "source_bundle"
    }

    $metadata = [pscustomobject]@{
        source = $relativePath
        kind = $kind
        configuration = $Configuration
        source_sha256 = $hash
        source_bytes = $file.Length
        cooked_utc = (Get-Date).ToUniversalTime().ToString("o")
        dependencies = $dependencies
        dependency_count = $dependencies.Count
        cook_payload = $cookPayload
    }
    $metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $metadataPath

    $binaryRelativePath = ""
    $binaryBytes = 0
    $binaryHash = ""
    $optimizedRelativePath = ""
    $optimizedBytes = 0
    $optimizedHash = ""
    if ($BinaryPackages) {
        $sourceBytes = [System.IO.File]::ReadAllBytes($file.FullName)
        $metadataJson = $metadata | ConvertTo-Json -Depth 4 -Compress
        $header = [System.Text.Encoding]::UTF8.GetBytes("DSPK1`n$metadataJson`n---SOURCE---`n")
        $payload = New-Object byte[] ($header.Length + $sourceBytes.Length)
        [System.Buffer]::BlockCopy($header, 0, $payload, 0, $header.Length)
        [System.Buffer]::BlockCopy($sourceBytes, 0, $payload, $header.Length, $sourceBytes.Length)
        [System.IO.File]::WriteAllBytes($binaryPath, $payload)
        $binaryRelativePath = (Get-RelativePath -BasePath $root -Path $binaryPath).Replace("\", "/")
        $binaryBytes = (Get-Item -LiteralPath $binaryPath).Length
        $binaryHash = (Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash.ToLowerInvariant()

        if ($kind -eq "glb_scene" -or $kind -eq "gltf_scene") {
            $optimizedHeader = [System.Text.Encoding]::UTF8.GetBytes("DSGLBPK1`n$metadataJson`n---OPTIMIZED-SOURCE---`n")
            $optimizedPayload = New-Object byte[] ($optimizedHeader.Length + $sourceBytes.Length)
            [System.Buffer]::BlockCopy($optimizedHeader, 0, $optimizedPayload, 0, $optimizedHeader.Length)
            [System.Buffer]::BlockCopy($sourceBytes, 0, $optimizedPayload, $optimizedHeader.Length, $sourceBytes.Length)
            [System.IO.File]::WriteAllBytes($optimizedPath, $optimizedPayload)
            $optimizedRelativePath = (Get-RelativePath -BasePath $root -Path $optimizedPath).Replace("\", "/")
            $optimizedBytes = (Get-Item -LiteralPath $optimizedPath).Length
            $optimizedHash = (Get-FileHash -LiteralPath $optimizedPath -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }

    $records += [pscustomobject]@{
        source = $relativePath
        kind = $kind
        hash = $hash
        bytes = $file.Length
        cooked_metadata = (Get-RelativePath -BasePath $root -Path $metadataPath).Replace("\", "/")
        cooked_binary = $binaryRelativePath
        binary_bytes = $binaryBytes
        binary_sha256 = $binaryHash
        optimized_package = $optimizedRelativePath
        optimized_package_bytes = $optimizedBytes
        optimized_package_sha256 = $optimizedHash
        dependencies = $dependencies
    }
}

$manifest = [pscustomobject]@{
    name = "DISPARITY cooked asset manifest"
    configuration = $Configuration
    created_utc = (Get-Date).ToUniversalTime().ToString("o")
    asset_count = $records.Count
    binary_packages = [bool]$BinaryPackages
    records = $records
}

$manifestPath = Join-Path $OutputPath "manifest.dcook"
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath

if ($BinaryPackages) {
    Write-Host "Cooked $($records.Count) asset metadata and binary package record(s) to $manifestPath"
}
else {
    Write-Host "Cooked $($records.Count) asset metadata record(s) to $manifestPath"
}
