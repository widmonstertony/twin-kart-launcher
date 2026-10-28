param([Parameter(Mandatory=$true)][string]$StreamlineSdkRoot)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation) { throw 'MSVC x64 build tools required' }
$vcRoot=(Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkRoot=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdkVersion=(Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory | Where-Object { (Test-Path -LiteralPath (Join-Path $_.FullName 'um\Windows.h')) -and (Test-Path -LiteralPath (Join-Path $sdkRoot "Lib\$($_.Name)\um\x64\d3d12.lib")) } | Sort-Object Name -Descending | Select-Object -First 1).Name
if(-not $sdkVersion) { throw 'Complete Windows SDK required' }
$compiler=Join-Path $vcRoot 'bin\Hostx64\x64\cl.exe'
$build=Join-Path $projectRoot 'build'; New-Item -ItemType Directory -Path $build -Force | Out-Null
$oldInclude=$env:INCLUDE; $oldLib=$env:LIB; $oldPath=$env:PATH
try {
    $env:INCLUDE=@((Join-Path $StreamlineSdkRoot 'include'),(Join-Path $vcRoot 'include'),(Join-Path $sdkRoot "Include\$sdkVersion\ucrt"),(Join-Path $sdkRoot "Include\$sdkVersion\shared"),(Join-Path $sdkRoot "Include\$sdkVersion\um"),(Join-Path $sdkRoot "Include\$sdkVersion\winrt")) -join ';'
    $env:LIB=@((Join-Path $vcRoot 'lib\x64'),(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64"),(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64")) -join ';'
    $env:PATH=(Split-Path -Parent $compiler)+';'+$oldPath
    Push-Location $build
    try {
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MD /Fo./ /FeProbeFrameGeneration.exe (Join-Path $projectRoot 'tools\ProbeFrameGeneration.cpp') (Join-Path $PSScriptRoot 'FrameGenerationD3D12.cpp') /link user32.lib d3d12.lib dxgi.lib wintrust.lib crypt32.lib
        if($LASTEXITCODE -ne 0) { throw 'Original frame generation adapter build failed' }
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MD /Fo./ /FeProbeSharedTexture.exe (Join-Path $projectRoot 'tools\ProbeSharedTexture.cpp') (Join-Path $PSScriptRoot 'SharedTextureBridge.cpp') /link user32.lib d3d11.lib d3d12.lib dxgi.lib
        if($LASTEXITCODE -ne 0) { throw 'Original D3D11/D3D12 texture bridge build failed' }
    } finally { Pop-Location }
} finally { $env:INCLUDE=$oldInclude; $env:LIB=$oldLib; $env:PATH=$oldPath }
