param(
    [string]$Out = "$env:TEMP\opencode\registryexplorerpp.png",
    [int]$Wait = 3,
    [string]$ExeArgs = "",
    [switch]$Fresh
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$code = @'
using System;
using System.Runtime.InteropServices;
public class Cap {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
'@
if (-not ("Cap" -as [type])) {
    Add-Type -TypeDefinition $code
}

$exe = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "build\registryexplorerpp.exe"
if ($Fresh) {
    Get-Process registryexplorerpp -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}
$proc = Start-Process -FilePath $exe -ArgumentList $ExeArgs -PassThru
Start-Sleep -Seconds $Wait
$hwnd = $proc.MainWindowHandle
if ($hwnd -ne [IntPtr]::Zero) {
    $client = New-Object Cap+RECT
    [void][Cap]::GetClientRect($hwnd, [ref]$client)
    $w = $client.Right - $client.Left
    $h = $client.Bottom - $client.Top
    $bitmap = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bitmap)
    $dc = $g.GetHdc()
    [void][Cap]::PrintWindow($hwnd, $dc, 2)
    $g.ReleaseHdc($dc)
    $g.Dispose()
    $bitmap.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    Write-Host "$Out ${w}x${h}"
} else {
    Write-Host "no main window yet"
}
