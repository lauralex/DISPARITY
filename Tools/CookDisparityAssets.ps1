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

function Get-BytesSha256 {
    param([byte[]]$Bytes)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return (($sha.ComputeHash($Bytes) | ForEach-Object { $_.ToString("x2") }) -join "")
    }
    finally {
        $sha.Dispose()
    }
}

function Get-AssetFileSha256 {
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

function New-OptimizedGltfPackage {
    param(
        [System.IO.FileInfo]$File,
        [string]$Kind,
        [string]$RelativePath,
        [string]$Hash,
        [string[]]$Dependencies,
        [string]$ImportSettings
    )

    $package = [ordered]@{
        magic = "DSGLBPK2"
        schema = 2
        payload_format = "optimized_static_scene"
        source = $RelativePath
        source_sha256 = $Hash
        source_bytes = $File.Length
        kind = $Kind
        dependencies = $Dependencies
        dependency_count = $Dependencies.Count
        import_settings = ""
        mesh_count = 0
        primitive_count = 0
        material_count = 0
        node_count = 0
        animation_count = 0
        skin_count = 0
        buffers = @()
        buffer_views = @()
        accessors = @()
        primitives = @()
        materials = @()
        nodes = @()
        animations = @()
    }

    if (Test-Path -LiteralPath $ImportSettings) {
        $package.import_settings = (Get-RelativePath -BasePath $root -Path $ImportSettings).Replace("\", "/")
    }

    if ($Kind -eq "glb_scene") {
        $package.payload_format = "optimized_glb_container_manifest"
        return [pscustomobject]$package
    }

    try {
        $json = Get-Content -LiteralPath $File.FullName -Raw | ConvertFrom-Json
    }
    catch {
        $package.payload_format = "optimized_static_scene_parse_failed"
        $package.parse_error = $_.Exception.Message
        return [pscustomobject]$package
    }

    $package.mesh_count = @($json.meshes).Count
    $package.material_count = @($json.materials).Count
    $package.node_count = @($json.nodes).Count
    $package.animation_count = @($json.animations).Count
    $package.skin_count = @($json.skins).Count

    $bufferIndex = 0
    foreach ($buffer in @($json.buffers)) {
        $bufferRecord = [ordered]@{
            index = $bufferIndex
            declared_byte_length = [int]($buffer.byteLength)
            uri_kind = "none"
            resolved_source = ""
            payload_sha256 = ""
            payload_bytes = 0
        }
        if ($buffer.uri) {
            $uri = [string]$buffer.uri
            if ($uri.StartsWith("data:", [System.StringComparison]::OrdinalIgnoreCase)) {
                $bufferRecord.uri_kind = "embedded_base64"
                if ($uri -match "^data:[^;]+;base64,(.+)$") {
                    $bytes = [System.Convert]::FromBase64String($Matches[1])
                    $bufferRecord.payload_bytes = $bytes.Length
                    $bufferRecord.payload_sha256 = Get-BytesSha256 -Bytes $bytes
                }
            }
            else {
                $bufferRecord.uri_kind = "external_file"
                $resolved = Resolve-DeclaredDependency -Owner $File -Dependency $uri
                $bufferRecord.resolved_source = $resolved
                $absolute = Join-Path $root $resolved
                if (Test-Path -LiteralPath $absolute) {
                    $bytes = [System.IO.File]::ReadAllBytes($absolute)
                    $bufferRecord.payload_bytes = $bytes.Length
                    $bufferRecord.payload_sha256 = Get-BytesSha256 -Bytes $bytes
                }
            }
        }
        $package.buffers += [pscustomobject]$bufferRecord
        ++$bufferIndex
    }

    $viewIndex = 0
    foreach ($view in @($json.bufferViews)) {
        $package.buffer_views += [pscustomobject]@{
            index = $viewIndex
            buffer = [int]($view.buffer)
            byte_offset = [int]($view.byteOffset)
            byte_length = [int]($view.byteLength)
            byte_stride = [int]($view.byteStride)
            target = [int]($view.target)
        }
        ++$viewIndex
    }

    $accessorIndex = 0
    foreach ($accessor in @($json.accessors)) {
        $package.accessors += [pscustomobject]@{
            index = $accessorIndex
            buffer_view = [int]($accessor.bufferView)
            component_type = [int]($accessor.componentType)
            count = [int]($accessor.count)
            type = [string]$accessor.type
            normalized = [bool]$accessor.normalized
        }
        ++$accessorIndex
    }

    $meshIndex = 0
    foreach ($mesh in @($json.meshes)) {
        $primitiveIndex = 0
        foreach ($primitive in @($mesh.primitives)) {
            $attributes = [ordered]@{}
            if ($primitive.attributes) {
                foreach ($attribute in $primitive.attributes.PSObject.Properties) {
                    $attributes[$attribute.Name] = $attribute.Value
                }
            }
            $package.primitives += [pscustomobject]@{
                mesh_index = $meshIndex
                primitive_index = $primitiveIndex
                mode = [int]($primitive.mode)
                material = [int]($primitive.material)
                indices = [int]($primitive.indices)
                attributes = [pscustomobject]$attributes
            }
            ++$primitiveIndex
            ++$package.primitive_count
        }
        ++$meshIndex
    }

    $materialIndex = 0
    foreach ($material in @($json.materials)) {
        $package.materials += [pscustomobject]@{
            index = $materialIndex
            name = [string]$material.name
            double_sided = [bool]$material.doubleSided
            alpha_mode = [string]$material.alphaMode
        }
        ++$materialIndex
    }

    $nodeIndex = 0
    foreach ($node in @($json.nodes)) {
        $package.nodes += [pscustomobject]@{
            index = $nodeIndex
            name = [string]$node.name
            mesh = [int]($node.mesh)
            skin = [int]($node.skin)
            child_count = @($node.children).Count
        }
        ++$nodeIndex
    }

    $animationIndex = 0
    foreach ($animation in @($json.animations)) {
        $package.animations += [pscustomobject]@{
            index = $animationIndex
            name = [string]$animation.name
            sampler_count = @($animation.samplers).Count
            channel_count = @($animation.channels).Count
        }
        ++$animationIndex
    }

    return [pscustomobject]$package
}

$records = @()
foreach ($file in Get-ChildItem -LiteralPath $AssetsPath -Recurse -File) {
    $relativePath = (Get-RelativePath -BasePath $root -Path $file.FullName).Replace("\", "/")
    $assetRelativePath = (Get-RelativePath -BasePath $AssetsPath -Path $file.FullName).Replace("\", "/")
    $hash = Get-AssetFileSha256 -LiteralPath $file.FullName
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
        $cookPayload = "optimized_dglbpack_v2"
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
        $binaryHash = Get-AssetFileSha256 -LiteralPath $binaryPath

        if ($kind -eq "glb_scene" -or $kind -eq "gltf_scene") {
            $optimizedPackage = New-OptimizedGltfPackage `
                -File $file `
                -Kind $kind `
                -RelativePath $relativePath `
                -Hash $hash `
                -Dependencies $dependencies `
                -ImportSettings $importSettings
            $optimizedPackage | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $optimizedPath
            $optimizedRelativePath = (Get-RelativePath -BasePath $root -Path $optimizedPath).Replace("\", "/")
            $optimizedBytes = (Get-Item -LiteralPath $optimizedPath).Length
            $optimizedHash = Get-AssetFileSha256 -LiteralPath $optimizedPath
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
