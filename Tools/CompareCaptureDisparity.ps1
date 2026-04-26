param(
    [Parameter(Mandatory = $true)]
    [string]$CapturePath,
    [Parameter(Mandatory = $true)]
    [string]$GoldenPath,
    [string]$ThumbnailPath = "",
    [string]$DiffPath = "",
    [int]$ThumbnailWidth = 64,
    [int]$ThumbnailHeight = 36,
    [double]$MeanTolerance = 18.0,
    [double]$BadPixelDelta = 42.0,
    [double]$BadPixelRatio = 0.35,
    [switch]$UpdateGolden
)

$ErrorActionPreference = "Stop"

function Read-PpmToken {
    param(
        [byte[]]$Bytes,
        [ref]$Index
    )

    while ($Index.Value -lt $Bytes.Length) {
        $value = $Bytes[$Index.Value]
        if ($value -eq 35) {
            while ($Index.Value -lt $Bytes.Length -and $Bytes[$Index.Value] -ne 10 -and $Bytes[$Index.Value] -ne 13) {
                $Index.Value++
            }
            continue
        }

        if ($value -eq 9 -or $value -eq 10 -or $value -eq 13 -or $value -eq 32) {
            $Index.Value++
            continue
        }

        break
    }

    $start = $Index.Value
    while ($Index.Value -lt $Bytes.Length) {
        $value = $Bytes[$Index.Value]
        if ($value -eq 9 -or $value -eq 10 -or $value -eq 13 -or $value -eq 32 -or $value -eq 35) {
            break
        }
        $Index.Value++
    }

    if ($Index.Value -eq $start) {
        throw "Could not read PPM token."
    }

    return [System.Text.Encoding]::ASCII.GetString($Bytes, $start, $Index.Value - $start)
}

function Read-PpmImage {
    param([string]$Path)

    if (!(Test-Path -LiteralPath $Path)) {
        throw "PPM file not found at $Path"
    }

    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)
    $index = 0
    $magic = Read-PpmToken -Bytes $bytes -Index ([ref]$index)
    if ($magic -ne "P6") {
        throw "$Path is not a binary P6 PPM image."
    }

    $width = [int](Read-PpmToken -Bytes $bytes -Index ([ref]$index))
    $height = [int](Read-PpmToken -Bytes $bytes -Index ([ref]$index))
    $maxValue = [int](Read-PpmToken -Bytes $bytes -Index ([ref]$index))
    if ($maxValue -ne 255) {
        throw "$Path uses unsupported PPM max value $maxValue."
    }

    if ($index -ge $bytes.Length) {
        throw "$Path ended before pixel data."
    }

    $separator = $bytes[$index]
    if ($separator -ne 9 -and $separator -ne 10 -and $separator -ne 13 -and $separator -ne 32) {
        throw "$Path has an invalid PPM header terminator."
    }
    $index++

    $expectedLength = $width * $height * 3
    if (($bytes.Length - $index) -lt $expectedLength) {
        throw "$Path contains less pixel data than expected."
    }

    $pixels = New-Object byte[] $expectedLength
    [System.Array]::Copy($bytes, $index, $pixels, 0, $expectedLength)

    return [pscustomobject]@{
        Width = $width
        Height = $height
        Pixels = $pixels
    }
}

function Write-PpmImage {
    param(
        [string]$Path,
        [int]$Width,
        [int]$Height,
        [byte[]]$Pixels
    )

    $parent = Split-Path -Parent $Path
    if (![string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    $header = [System.Text.Encoding]::ASCII.GetBytes("P6`n$Width $Height`n255`n")
    $output = New-Object byte[] ($header.Length + $Pixels.Length)
    [System.Array]::Copy($header, 0, $output, 0, $header.Length)
    [System.Array]::Copy($Pixels, 0, $output, $header.Length, $Pixels.Length)
    [System.IO.File]::WriteAllBytes($Path, $output)
}

function New-PpmThumbnail {
    param(
        [pscustomobject]$Image,
        [int]$Width,
        [int]$Height
    )

    if ($Width -le 0 -or $Height -le 0) {
        throw "Thumbnail dimensions must be positive."
    }

    $pixels = New-Object byte[] ($Width * $Height * 3)
    for ($y = 0; $y -lt $Height; $y++) {
        $sourceY = [Math]::Min($Image.Height - 1, [int]((($y + 0.5) * $Image.Height) / $Height))
        for ($x = 0; $x -lt $Width; $x++) {
            $sourceX = [Math]::Min($Image.Width - 1, [int]((($x + 0.5) * $Image.Width) / $Width))
            $sourceIndex = (($sourceY * $Image.Width) + $sourceX) * 3
            $targetIndex = (($y * $Width) + $x) * 3
            $pixels[$targetIndex + 0] = $Image.Pixels[$sourceIndex + 0]
            $pixels[$targetIndex + 1] = $Image.Pixels[$sourceIndex + 1]
            $pixels[$targetIndex + 2] = $Image.Pixels[$sourceIndex + 2]
        }
    }

    return [pscustomobject]@{
        Width = $Width
        Height = $Height
        Pixels = $pixels
    }
}

function Compare-PpmImages {
    param(
        [pscustomobject]$Actual,
        [pscustomobject]$Expected,
        [double]$AllowedMeanDelta,
        [double]$AllowedBadPixelDelta,
        [double]$AllowedBadPixelRatio,
        [string]$DiffPath
    )

    if ($Actual.Width -ne $Expected.Width -or $Actual.Height -ne $Expected.Height) {
        throw "Golden dimensions $($Expected.Width)x$($Expected.Height) do not match actual $($Actual.Width)x$($Actual.Height)."
    }

    $channelCount = $Actual.Width * $Actual.Height * 3
    $pixelCount = $Actual.Width * $Actual.Height
    $totalDelta = 0.0
    $badPixels = 0
    $maxDelta = 0.0
    $diffPixels = if (![string]::IsNullOrWhiteSpace($DiffPath)) { New-Object byte[] $channelCount } else { $null }

    for ($pixel = 0; $pixel -lt $pixelCount; $pixel++) {
        $base = $pixel * 3
        $redDelta = [Math]::Abs([int]$Actual.Pixels[$base + 0] - [int]$Expected.Pixels[$base + 0])
        $greenDelta = [Math]::Abs([int]$Actual.Pixels[$base + 1] - [int]$Expected.Pixels[$base + 1])
        $blueDelta = [Math]::Abs([int]$Actual.Pixels[$base + 2] - [int]$Expected.Pixels[$base + 2])
        $totalDelta += $redDelta + $greenDelta + $blueDelta

        $pixelDelta = ($redDelta + $greenDelta + $blueDelta) / 3.0
        $maxDelta = [Math]::Max($maxDelta, $pixelDelta)
        if ($null -ne $diffPixels) {
            $diffPixels[$base + 0] = [byte][Math]::Min(255, $redDelta * 4)
            $diffPixels[$base + 1] = [byte][Math]::Min(255, $greenDelta * 4)
            $diffPixels[$base + 2] = [byte][Math]::Min(255, $blueDelta * 4)
        }
        if ($pixelDelta -gt $AllowedBadPixelDelta) {
            $badPixels++
        }
    }

    $meanDelta = $totalDelta / $channelCount
    $badRatio = $badPixels / [double]$pixelCount
    if ($null -ne $diffPixels) {
        Write-PpmImage -Path $DiffPath -Width $Actual.Width -Height $Actual.Height -Pixels $diffPixels
    }
    if ($meanDelta -gt $AllowedMeanDelta -or $badRatio -gt $AllowedBadPixelRatio) {
        throw ("Golden comparison failed: mean_delta={0:N2} allowed={1:N2}, bad_pixel_ratio={2:N4} allowed={3:N4}, max_delta={4:N2}, bad_pixels={5}, diff={6}" -f $meanDelta, $AllowedMeanDelta, $badRatio, $AllowedBadPixelRatio, $maxDelta, $badPixels, $DiffPath)
    }

    return [pscustomobject]@{
        MeanDelta = $meanDelta
        BadPixelRatio = $badRatio
        MaxDelta = $maxDelta
        BadPixels = $badPixels
    }
}

if ([string]::IsNullOrWhiteSpace($ThumbnailPath)) {
    $thumbnailDirectory = Split-Path -Parent $CapturePath
    $ThumbnailPath = Join-Path $thumbnailDirectory "runtime_capture_${ThumbnailWidth}x${ThumbnailHeight}.ppm"
}

$capture = Read-PpmImage -Path $CapturePath
$thumbnail = New-PpmThumbnail -Image $capture -Width $ThumbnailWidth -Height $ThumbnailHeight
Write-PpmImage -Path $ThumbnailPath -Width $thumbnail.Width -Height $thumbnail.Height -Pixels $thumbnail.Pixels

if ($UpdateGolden) {
    Write-PpmImage -Path $GoldenPath -Width $thumbnail.Width -Height $thumbnail.Height -Pixels $thumbnail.Pixels
    Write-Host "Updated golden capture at $GoldenPath"
    return
}

$golden = Read-PpmImage -Path $GoldenPath
$comparison = Compare-PpmImages -Actual $thumbnail -Expected $golden -AllowedMeanDelta $MeanTolerance -AllowedBadPixelDelta $BadPixelDelta -AllowedBadPixelRatio $BadPixelRatio -DiffPath $DiffPath
Write-Host ("Golden comparison passed: mean_delta={0:N2}, bad_pixel_ratio={1:N4}, max_delta={2:N2}, bad_pixels={3}, thumbnail={4}" -f $comparison.MeanDelta, $comparison.BadPixelRatio, $comparison.MaxDelta, $comparison.BadPixels, $ThumbnailPath)
if (![string]::IsNullOrWhiteSpace($DiffPath)) {
    Write-Host "Golden diff thumbnail written to $DiffPath"
}
