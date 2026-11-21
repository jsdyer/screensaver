# Run from Windows PowerShell on a Windows desktop with the .scr files in build.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class SaverWindows {
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern IntPtr GetWindow(IntPtr window, uint relation);
    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr window, out Rect area);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr window, uint message, IntPtr w, IntPtr l);

    public static bool SameSize(IntPtr parent, IntPtr child) {
        Rect a, b;
        return GetClientRect(parent, out a) && GetClientRect(child, out b)
            && a.Right == b.Right && a.Bottom == b.Bottom;
    }
}
'@

function Wait-Until($description, [scriptblock]$condition) {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    do {
        # A preview attaches to a window owned by this thread. Keep it responsive.
        [Windows.Forms.Application]::DoEvents()
        if (& $condition) { return }
        Start-Sleep -Milliseconds 40
    } while ($clock.Elapsed.TotalSeconds -lt 8)
    throw "Timed out waiting for $description"
}

foreach ($name in 'aquarium', 'space') {
    $path = (Resolve-Path "$PSScriptRoot/../build/$name.scr").Path
    foreach ($mode in '/p', '/s', '/c') {
        $preview = $null
        $process = $null
        try {
            # Execute the PE file directly, bypassing the shell's .scr association.
            $start = New-Object Diagnostics.ProcessStartInfo
            $start.FileName = $path
            $start.UseShellExecute = $false
            $start.Arguments = $mode
            if ($mode -eq '/p') {
                $preview = New-Object Windows.Forms.Form
                $preview.Show()
                $start.Arguments = "/p $($preview.Handle.ToInt64())"
            }
            $process = [Diagnostics.Process]::Start($start)

            if ($mode -eq '/p') {
                # An empty form has no child windows until the saver attaches.
                Wait-Until "$name preview window" {
                    [SaverWindows]::GetWindow($preview.Handle, 5) -ne [IntPtr]::Zero # GW_CHILD
                }
                $child = [SaverWindows]::GetWindow($preview.Handle, 5)
                $preview.ClientSize = New-Object Drawing.Size(419, 153)
                Wait-Until "$name preview resize" { [SaverWindows]::SameSize($preview.Handle, $child) }
                $preview.Close()
            } else {
                Wait-Until "$name $mode window" {
                    $process.Refresh()
                    $process.MainWindowHandle -ne [IntPtr]::Zero
                }
                if ($mode -eq '/s') {
                    $message = 0x0100 # WM_KEYDOWN
                    $value = 27       # Escape
                } else {
                    $message = 0x0111 # WM_COMMAND
                    $value = 2        # Cancel in the settings dialog
                }
                if (!([SaverWindows]::PostMessage($process.MainWindowHandle, $message, [IntPtr]$value, [IntPtr]::Zero))) {
                    throw "Could not send input to $name $mode"
                }
            }
            Wait-Until "$name $mode to exit" { $process.HasExited }
            if ($process.ExitCode -ne 0) { throw "$name $mode exited with code $($process.ExitCode)" }
            Write-Host "$name $mode passed"
        } finally {
            if ($process) {
                if (!$process.HasExited) { $process.Kill() }
                $process.Dispose()
            }
            if ($preview) { $preview.Dispose() }
        }
    }
}
