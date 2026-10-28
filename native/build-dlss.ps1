param([Parameter(Mandatory=$true)][string]$NgxSdkRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC x64 build tools required' }
$vcRoot = (Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdkVersion = (Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory | Where-Object { (Test-Path -LiteralPath (Join-Path $_.FullName 'um\Windows.h')) -and (Test-Path -LiteralPath (Join-Path $sdkRoot "Lib\$($_.Name)\um\x64\bcrypt.lib")) } | Sort-Object Name -Descending | Select-Object -First 1).Name
if (-not $sdkVersion) { throw 'Complete Windows SDK required' }
$compiler = Join-Path $vcRoot 'bin\Hostx64\x64\cl.exe'
$includeDirs = @((Join-Path $NgxSdkRoot 'include'),(Join-Path $vcRoot 'include'),(Join-Path $sdkRoot "Include\$sdkVersion\ucrt"),(Join-Path $sdkRoot "Include\$sdkVersion\shared"),(Join-Path $sdkRoot "Include\$sdkVersion\um"))
$libraryDirs = @((Join-Path $NgxSdkRoot 'lib\Windows_x86_64'),(Join-Path $vcRoot 'lib\x64'),(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64"),(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64"))
$build = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$oldInclude = $env:INCLUDE; $oldLib = $env:LIB; $oldPath = $env:PATH
try {
    $env:INCLUDE = $includeDirs -join ';'; $env:LIB = $libraryDirs -join ';'; $env:PATH = (Split-Path -Parent $compiler) + ';' + $oldPath
    Push-Location $build
    try {
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MD /LD /FoDlssD3D11.obj /FeTwinKartDlssD3D11.dll (Join-Path $PSScriptRoot 'DlssD3D11.cpp') /link nvsdk_ngx_d.lib user32.lib d3d11.lib dxgi.lib shlwapi.lib version.lib wintrust.lib crypt32.lib advapi32.lib ole32.lib shell32.lib
        if ($LASTEXITCODE -ne 0) { throw 'DLSS D3D11 consumer build failed' }
    } finally { Pop-Location }
} finally { $env:INCLUDE = $oldInclude; $env:LIB = $oldLib; $env:PATH = $oldPath }
Get-FileHash -LiteralPath (Join-Path $build 'TwinKartDlssD3D11.dll') -Algorithm SHA256



