param(
    [switch]$Clean,
    [string]$Out = "",
    [switch]$Debug
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found at $vcvars"
}

$buildDir = Join-Path $root "build"
if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$sources = @(Get-ChildItem -Path (Join-Path $root "src") -Filter *.cpp | ForEach-Object { $_.FullName })
$sources += @(
    (Join-Path $root "vendor\imgui\imgui.cpp"),
    (Join-Path $root "vendor\imgui\imgui_draw.cpp"),
    (Join-Path $root "vendor\imgui\imgui_tables.cpp"),
    (Join-Path $root "vendor\imgui\imgui_widgets.cpp"),
    (Join-Path $root "vendor\imgui\backends\imgui_impl_win32.cpp"),
    (Join-Path $root "vendor\imgui\backends\imgui_impl_dx11.cpp")
)

$flags = @("/nologo", "/std:c++20", "/EHsc", "/utf-8", "/W3", "/WX-", "/MT", "/DUNICODE", "/D_UNICODE", "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX")
$flags += "/I" + (Join-Path $root "vendor\imgui")
$flags += "/I" + (Join-Path $root "vendor\imgui\backends")
$flags += "/I" + (Join-Path $root "vendor")
if ($Debug) {
    $flags += @("/Od", "/Zi")
} else {
    $flags += @("/O2", "/DNDEBUG")
}

$libs = @("d3d11.lib", "dxgi.lib", "gdi32.lib", "user32.lib", "shell32.lib", "ole32.lib", "advapi32.lib",
          "comctl32.lib", "shlwapi.lib", "dwmapi.lib", "comdlg32.lib")

$exeName = if ($Out -ne "") { $Out } else { "registryexplorerpp.exe" }

$rsp = @()
$rsp += $sources
$rsp += $flags
$buildFwd = $buildDir -replace '\\', '/'
$rsp += "/Fo$buildFwd/"
$rsp += "/Fe$buildFwd/$exeName"
$rsp += $libs

$kitsRoot = "C:\Program Files (x86)\Windows Kits\10"
$sdkVersion = Get-ChildItem (Join-Path $kitsRoot "Include") -ErrorAction SilentlyContinue |
    Where-Object { $_.PSIsContainer } | Sort-Object Name -Descending | Select-Object -First 1
if ($null -eq $sdkVersion) {
    throw "no Windows SDK include folder found under $kitsRoot"
}
$sdkInc = Join-Path $kitsRoot "Include\$($sdkVersion.Name)"
$sdkLib = Join-Path $kitsRoot "Lib\$($sdkVersion.Name)"

$bat = Join-Path $buildDir "compile.bat"
$lines = @(
    "@echo off",
    "call `"$vcvars`" >nul",
    ('set "INCLUDE=%INCLUDE%;' + $sdkInc + '\ucrt;' + $sdkInc + '\um;' + $sdkInc + '\shared;' + $sdkInc + '\winrt"'),
    ('set "LIB=%LIB%;' + $sdkLib + '\ucrt\x64;' + $sdkLib + '\um\x64"'),
    "cd /d `"$root`"",
    "cl @$buildDir\args.rsp",
    "exit /b %ERRORLEVEL%"
)
Set-Content -Path $bat -Value $lines -Encoding ASCII

$rspPath = Join-Path $buildDir "args.rsp"
Set-Content -Path $rspPath -Value $rsp -Encoding ASCII

Write-Host "compiling $($sources.Count) source files -> $exeName"

$ErrorActionPreference = "Continue"
& cmd.exe /c "`"$bat`"" 2>&1 | Write-Host
$ErrorActionPreference = "Stop"
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "compilation failed with exit code $exitCode"
}
Write-Host "built $buildDir\$exeName"
