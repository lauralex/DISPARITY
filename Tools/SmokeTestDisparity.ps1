param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$ExecutablePath = "",
    [int]$Seconds = 3
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $root "bin\x64\$Configuration\DisparityGame.exe"
}

if (!(Test-Path -LiteralPath $ExecutablePath)) {
    throw "DisparityGame.exe not found at $ExecutablePath"
}

$resolvedExecutable = (Resolve-Path -LiteralPath $ExecutablePath).Path
$workingDirectory = Split-Path -Parent $resolvedExecutable

$source = @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class DisparityWindowCloser {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

    public static bool CloseWindowByTitle(uint pid, string title) {
        IntPtr target = IntPtr.Zero;
        EnumWindows((hWnd, lParam) => {
            uint windowPid;
            GetWindowThreadProcessId(hWnd, out windowPid);
            if (windowPid == pid) {
                var sb = new StringBuilder(256);
                GetWindowText(hWnd, sb, sb.Capacity);
                if (sb.ToString() == title) {
                    target = hWnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);

        if (target == IntPtr.Zero) {
            return false;
        }

        return PostMessage(target, 0x0010, IntPtr.Zero, IntPtr.Zero);
    }
}
'@

if (-not ("DisparityWindowCloser" -as [type])) {
    Add-Type -TypeDefinition $source
}

$process = Start-Process -FilePath $resolvedExecutable -WorkingDirectory $workingDirectory -PassThru
Start-Sleep -Seconds $Seconds

if ($process.HasExited) {
    if ($process.ExitCode -ne 0) {
        throw "DISPARITY exited early with code $($process.ExitCode)"
    }

    Write-Host "DISPARITY launched and exited cleanly."
    exit 0
}

$closed = [DisparityWindowCloser]::CloseWindowByTitle([uint32]$process.Id, "DISPARITY")
if (!$closed) {
    $process.CloseMainWindow() | Out-Null
}

if (!$process.WaitForExit(5000)) {
    $process.Kill()
    $process.WaitForExit()
    throw "DISPARITY did not close after smoke test."
}

if ($process.ExitCode -ne 0) {
    throw "DISPARITY smoke test exited with code $($process.ExitCode)"
}

Write-Host "DISPARITY smoke test passed for $resolvedExecutable"
