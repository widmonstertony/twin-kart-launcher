param(
    [string]$VcRoot,
    [string]$SdkRoot,
    [string]$SdkVersion
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
$source = Join-Path $PSScriptRoot 'InputDevice.cpp'
$programFilesX86 = ${env:ProgramFiles(x86)}
if (-not $programFilesX86) { $programFilesX86 = 'C:\Program Files (x86)' }
if (-not $SdkRoot) { $SdkRoot = Join-Path $programFilesX86 'Windows Kits\10' }

if (-not $VcRoot) {
    $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installation = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
        if ($installation) {
            $toolsDirectory = Join-Path $installation 'VC\Tools\MSVC'
            $latest = Get-ChildItem -LiteralPath $toolsDirectory -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending | Select-Object -First 1
            if ($latest) { $VcRoot = $latest.FullName }
        }
    }
}
if (-not $VcRoot) { throw 'MSVC not found; pass -VcRoot pointing to VC\Tools\MSVC\<version>.' }

if (-not $SdkVersion) {
    $includeRoot = Join-Path $SdkRoot 'Include'
    $latestSdk = Get-ChildItem -LiteralPath $includeRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            (Test-Path -LiteralPath (Join-Path $_.FullName 'um\dinput.h')) -and
            (Test-Path -LiteralPath (Join-Path $SdkRoot "Lib\$($_.Name)\um\x64\dinput8.lib"))
        } | Sort-Object Name -Descending | Select-Object -First 1
    if ($latestSdk) { $SdkVersion = $latestSdk.Name }
}
if (-not $SdkVersion) { throw 'Windows SDK not found; pass -SdkRoot and -SdkVersion.' }
$compiler = Join-Path $VcRoot 'bin\Hostx64\x64\cl.exe'

if (-not (Test-Path -LiteralPath $compiler)) { throw "MSVC compiler not found: $compiler" }
if (-not (Test-Path -LiteralPath $source)) { throw "Source not found: $source" }
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$includeDirs = @(
    (Join-Path $VcRoot 'include'),
    (Join-Path $SdkRoot "Include\$SdkVersion\ucrt"),
    (Join-Path $SdkRoot "Include\$SdkVersion\shared"),
    (Join-Path $SdkRoot "Include\$SdkVersion\um")
)
$libraryDirs = @(
    (Join-Path $VcRoot 'lib\x64'),
    (Join-Path $SdkRoot "Lib\$SdkVersion\ucrt\x64"),
    (Join-Path $SdkRoot "Lib\$SdkVersion\um\x64")
)
foreach ($path in $includeDirs + $libraryDirs) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Build dependency not found: $path" }
}

$previousInclude = $env:INCLUDE
$previousLib = $env:LIB
$previousPath = $env:PATH
$env:INCLUDE = ($includeDirs -join ';') + ';' + $previousInclude
$env:LIB = ($libraryDirs -join ';') + ';' + $previousLib
$env:PATH = (Split-Path -Parent $compiler) + ';' + $previousPath

try {
    Push-Location $buildDir
    try {
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /LD /DUNICODE /D_UNICODE `
            /FoInputDevice.obj /FeInputDevice.dll $source `
            /link dinput8.lib dxguid.lib user32.lib
        if ($LASTEXITCODE -ne 0) { throw "InputDevice.dll build failed: $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
} finally {
    $env:INCLUDE = $previousInclude
    $env:LIB = $previousLib
    $env:PATH = $previousPath
}

$dll = Join-Path $buildDir 'InputDevice.dll'
if (-not (Test-Path -LiteralPath $dll)) { throw "Build completed without DLL: $dll" }
Write-Output $dll
