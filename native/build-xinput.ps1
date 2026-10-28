param([string]$VcRoot, [string]$SdkVersion, [ValidateSet('x64','x86')][string]$Architecture = 'x64')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
if ($Architecture -eq 'x86') { $buildDir = Join-Path $buildDir 'x86' }
if (-not $VcRoot) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installation) { throw 'Install MSVC C++ Build Tools or pass -VcRoot.' }
    $VcRoot = (Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).FullName
}
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
if (-not $SdkVersion) { $SdkVersion = (Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory | Where-Object { $_.Name -match '^10\.0\.' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).Name }
$compiler = Join-Path $VcRoot "bin\Hostx64\$Architecture\cl.exe"
$includeDirs = @((Join-Path $VcRoot 'include'), (Join-Path $sdkRoot "Include\$SdkVersion\ucrt"), (Join-Path $sdkRoot "Include\$SdkVersion\shared"), (Join-Path $sdkRoot "Include\$SdkVersion\um"))
$libraryDirs = @((Join-Path $VcRoot "lib\$Architecture"), (Join-Path $sdkRoot "Lib\$SdkVersion\ucrt\$Architecture"), (Join-Path $sdkRoot "Lib\$SdkVersion\um\$Architecture"))
$oldInclude = $env:INCLUDE; $oldLib = $env:LIB; $oldPath = $env:PATH
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
try {
    $env:INCLUDE = $includeDirs -join ';'; $env:LIB = $libraryDirs -join ';'; $env:PATH = (Split-Path -Parent $compiler) + ';' + $oldPath
    Push-Location $buildDir
    try {
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /LD /FoVirtualXInput.obj /FeKartWheelXInput.dll (Join-Path $PSScriptRoot 'VirtualXInput.cpp') /link "/DEF:$PSScriptRoot\VirtualXInput.def"
        if ($LASTEXITCODE -ne 0) { throw 'XInput adapter compilation failed.' }
    } finally { Pop-Location }
} finally { $env:INCLUDE = $oldInclude; $env:LIB = $oldLib; $env:PATH = $oldPath }
