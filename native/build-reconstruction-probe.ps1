param([string]$StreamlineSdkRoot)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC x64 build tools required'}
$vcRoot=(Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkRoot=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdkVersion=(Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory | Where-Object {(Test-Path -LiteralPath (Join-Path $_.FullName 'um\Windows.h')) -and (Test-Path -LiteralPath (Join-Path $sdkRoot "Lib\$($_.Name)\um\x64\d3d12.lib"))} | Sort-Object Name -Descending | Select-Object -First 1).Name
if(-not $sdkVersion){throw 'Complete Windows SDK required'}
$compiler=Join-Path $vcRoot 'bin\Hostx64\x64\cl.exe'
$build=Join-Path $projectRoot 'build';New-Item -ItemType Directory -Path $build -Force | Out-Null
$oldInclude=$env:INCLUDE;$oldLib=$env:LIB;$oldPath=$env:PATH
try {
    $env:INCLUDE=@((Join-Path $vcRoot 'include'),(Join-Path $sdkRoot "Include\$sdkVersion\ucrt"),(Join-Path $sdkRoot "Include\$sdkVersion\shared"),(Join-Path $sdkRoot "Include\$sdkVersion\um"),(Join-Path $sdkRoot "Include\$sdkVersion\winrt")) -join ';'
    if($StreamlineSdkRoot) {
        if(-not(Test-Path -LiteralPath (Join-Path $StreamlineSdkRoot 'include/sl.h'))){throw 'External Streamline headers required'}
        $env:INCLUDE=(Join-Path $StreamlineSdkRoot 'include')+';'+$env:INCLUDE
    }
    $env:LIB=@((Join-Path $vcRoot 'lib\x64'),(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64"),(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64")) -join ';'
    $env:PATH=(Split-Path -Parent $compiler)+';'+$oldPath
    Push-Location $build
    try {
        $sources=@('RenderPassTrace.cpp','NativeTemporalPass.cpp','NativeFrameMarkers.cpp','SharedTextureBridge.cpp') | ForEach-Object {Join-Path $PSScriptRoot $_}
        $features=@()
        if($StreamlineSdkRoot){$sources+=Join-Path $PSScriptRoot 'FrameGenerationD3D12.cpp';$features+='/DTWIN_KART_RECONSTRUCTION_FG=1';$features+='/DTWIN_KART_SYNTHETIC_MARKER_INPUT=1'}
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT $features /Fo./ /FeProbeNativeReconstruction.exe (Join-Path $projectRoot 'tools\ProbeNativeReconstruction.cpp') $sources /link bcrypt.lib user32.lib d3d11.lib d3d12.lib dxgi.lib d3dcompiler.lib wintrust.lib crypt32.lib
        if($LASTEXITCODE -ne 0){throw 'Production reconstruction probe build failed'}
    } finally {Pop-Location}
} finally {$env:INCLUDE=$oldInclude;$env:LIB=$oldLib;$env:PATH=$oldPath}
Get-FileHash -LiteralPath (Join-Path $build 'ProbeNativeReconstruction.exe') -Algorithm SHA256
