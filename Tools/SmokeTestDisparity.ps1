param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$ExecutablePath = "",
    [int]$Seconds = 3,
    [string]$Arguments = "",
    [switch]$ExerciseWindow,
    [int]$StartupTimeoutSeconds = 10
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
using System.Diagnostics;
using System.Text;
using System.Runtime.InteropServices;

public static class DisparityWindowProbe {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int x, int y, int cx, int cy, uint flags);

    public static IntPtr FindWindowByTitle(uint pid, string title) {
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

        return target;
    }

    public static IntPtr WaitForWindowByTitle(uint pid, string title, int timeoutMilliseconds) {
        var stopwatch = Stopwatch.StartNew();
        while (stopwatch.ElapsedMilliseconds < timeoutMilliseconds) {
            var window = FindWindowByTitle(pid, title);
            if (window != IntPtr.Zero) {
                return window;
            }
            System.Threading.Thread.Sleep(100);
        }

        return IntPtr.Zero;
    }

    public static bool CloseWindowByTitle(uint pid, string title) {
        var target = FindWindowByTitle(pid, title);
        if (target == IntPtr.Zero) {
            return false;
        }

        return PostMessage(target, 0x0010, IntPtr.Zero, IntPtr.Zero);
    }

    public static void ExerciseWindow(IntPtr hWnd) {
        const uint SWP_NOZORDER = 0x0004;
        const uint SWP_NOACTIVATE = 0x0010;
        SetWindowPos(hWnd, IntPtr.Zero, 60, 60, 960, 540, SWP_NOZORDER | SWP_NOACTIVATE);
        SendKey(hWnd, 0x70); // F1
        SendKey(hWnd, 0x72); // F3
        SendKey(hWnd, 0x74); // F5
        SetWindowPos(hWnd, IntPtr.Zero, 80, 80, 1280, 720, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    private static void SendKey(IntPtr hWnd, int virtualKey) {
        PostMessage(hWnd, 0x0100, new IntPtr(virtualKey), IntPtr.Zero);
        PostMessage(hWnd, 0x0101, new IntPtr(virtualKey), IntPtr.Zero);
    }
}
'@

if (-not ("DisparityWindowProbe" -as [type])) {
    Add-Type -TypeDefinition $source
}

$startArgs = @{
    FilePath = $resolvedExecutable
    WorkingDirectory = $workingDirectory
    PassThru = $true
}
if (![string]::IsNullOrWhiteSpace($Arguments)) {
    $startArgs.ArgumentList = $Arguments
}

$process = Start-Process @startArgs
$window = [DisparityWindowProbe]::WaitForWindowByTitle([uint32]$process.Id, "DISPARITY", $StartupTimeoutSeconds * 1000)
if ($window -eq [IntPtr]::Zero) {
    if (!$process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
    }
    throw "DISPARITY window was not found within $StartupTimeoutSeconds second(s)."
}

if ($ExerciseWindow) {
    [DisparityWindowProbe]::ExerciseWindow($window)
}

Start-Sleep -Seconds $Seconds

if ($process.HasExited) {
    if ($process.ExitCode -ne 0) {
        throw "DISPARITY exited early with code $($process.ExitCode)"
    }

    Write-Host "DISPARITY launched and exited cleanly."
    exit 0
}

$closed = [DisparityWindowProbe]::CloseWindowByTitle([uint32]$process.Id, "DISPARITY")
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
