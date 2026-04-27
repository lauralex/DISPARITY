param(
    [string[]]$SearchRoots = @("DisparityGame", "DisparityEngine\Source\Disparity")
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$widgetNames = @(
    "Button",
    "Checkbox",
    "Combo",
    "DragFloat",
    "DragFloat2",
    "DragFloat3",
    "DragInt",
    "InputFloat",
    "InputInt",
    "InputText",
    "RadioButton",
    "Selectable",
    "SliderFloat",
    "SliderFloat2",
    "SliderFloat3",
    "SliderInt",
    "SmallButton"
)

$widgetPattern = "ImGui::(?<widget>$($widgetNames -join '|'))\s*\(\s*`"(?<label>(?:[^`"\\]|\\.)*)`""
$beginPattern = "ImGui::Begin\s*\(\s*`"(?<label>(?:[^`"\\]|\\.)*)`""
$endPattern = "ImGui::End\s*\("
$violations = New-Object System.Collections.Generic.List[string]

foreach ($searchRoot in $SearchRoots) {
    $resolvedRoot = if ([System.IO.Path]::IsPathRooted($searchRoot)) {
        $searchRoot
    }
    else {
        Join-Path $root $searchRoot
    }

    if (!(Test-Path -LiteralPath $resolvedRoot)) {
        throw "ImGui ID search root not found: $resolvedRoot"
    }

    $files = Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File -Include *.cpp,*.h,*.hpp |
        Where-Object {
            $_.FullName -notmatch "\\ThirdParty\\" -and
            $_.FullName -notmatch "\\dist\\" -and
            $_.FullName -notmatch "\\bin\\" -and
            $_.FullName -notmatch "\\obj\\"
        }

    foreach ($file in $files) {
        $windowStack = New-Object System.Collections.Generic.List[string]
        $labelsByWindow = @{}
        $lines = Get-Content -LiteralPath $file.FullName

        for ($index = 0; $index -lt $lines.Count; ++$index) {
            $line = $lines[$index]
            $lineNumber = $index + 1

            $beginMatch = [regex]::Match($line, $beginPattern)
            if ($beginMatch.Success) {
                [void]$windowStack.Add($beginMatch.Groups["label"].Value)
            }

            $currentWindow = if ($windowStack.Count -gt 0) {
                $windowStack[$windowStack.Count - 1]
            }
            else {
                "<global>"
            }

            $widgetMatch = [regex]::Match($line, $widgetPattern)
            if ($widgetMatch.Success -and $line -notmatch "DISPARITY_IMGUI_DUPLICATE_OK") {
                $label = $widgetMatch.Groups["label"].Value
                $widget = $widgetMatch.Groups["widget"].Value
                $key = "$($file.FullName)|$currentWindow|$label"

                if ($label.Length -eq 0) {
                    [void]$violations.Add("$($file.FullName):$lineNumber $widget uses an empty ImGui label. Use a visible label or a unique hidden ID such as ##Name.")
                }

                if (!$labelsByWindow.ContainsKey($key)) {
                    $labelsByWindow[$key] = [pscustomobject]@{
                        Widget = $widget
                        Line = $lineNumber
                    }
                }
                else {
                    $previous = $labelsByWindow[$key]
                    [void]$violations.Add("$($file.FullName):$lineNumber $widget label `"$label`" duplicates $($previous.Widget) at line $($previous.Line) in ImGui window `"$currentWindow`". Append ##UniqueId or rename one control.")
                }
            }

            if ($line -match $endPattern -and $windowStack.Count -gt 0) {
                $nextCodeLine = ""
                for ($lookahead = $index + 1; $lookahead -lt $lines.Count; ++$lookahead) {
                    $candidate = $lines[$lookahead].Trim()
                    if ($candidate.Length -gt 0) {
                        $nextCodeLine = $candidate
                        break
                    }
                }

                if ($nextCodeLine -ne "return;") {
                    $windowStack.RemoveAt($windowStack.Count - 1)
                }
            }
        }
    }
}

if ($violations.Count -gt 0) {
    $violations | ForEach-Object { Write-Host $_ }
    throw "Found $($violations.Count) potential Dear ImGui ID conflict(s)."
}

Write-Host "Dear ImGui literal ID check passed."
