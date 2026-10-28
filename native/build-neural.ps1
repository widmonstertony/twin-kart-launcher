param([Parameter(Mandatory=$true)][string]$NgxSdkRoot,[ValidatePattern('^TwinKart[A-Za-z0-9_]*_nvngx$')][string]$OutputName='TwinKartNeural_nvngx')
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
    $env:INCLUDE=@((Join-Path $NgxSdkRoot 'include'),(Join-Path $vcRoot 'include'),(Join-Path $sdkRoot "Include\$sdkVersion\ucrt"),(Join-Path $sdkRoot "Include\$sdkVersion\shared"),(Join-Path $sdkRoot "Include\$sdkVersion\um"),(Join-Path $sdkRoot "Include\$sdkVersion\winrt")) -join ';'
    $env:LIB=@((Join-Path $NgxSdkRoot 'lib\Windows_x86_64'),(Join-Path $vcRoot 'lib\x64'),(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64"),(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64"),$build) -join ';'
    $env:PATH=(Split-Path -Parent $compiler)+';'+$oldPath
    Push-Location $build
    try {
        # This original adapter's name identifies it separately from the
        # vendor core. The private snippet ABI checks the nvngx component name;
        # no NVIDIA system DLL is renamed, replaced or patched.
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MD /LD /FoNeuralD3D12.obj "/Fe$OutputName.dll" (Join-Path $PSScriptRoot 'NeuralD3D12.cpp') /link nvsdk_ngx_d.lib bcrypt.lib user32.lib d3d12.lib dxgi.lib shlwapi.lib version.lib wintrust.lib crypt32.lib advapi32.lib ole32.lib shell32.lib
        if($LASTEXITCODE -ne 0) { throw 'Original neural D3D12 consumer build failed' }
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MD /FoProbeNeuralRuntime.obj /FeProbeNeuralRuntime.exe (Join-Path $projectRoot 'tools\ProbeNeuralRuntime.cpp') /link "$OutputName.lib" d3d12.lib dxgi.lib user32.lib
        if($LASTEXITCODE -ne 0) { throw 'Standalone neural probe build failed' }
    } finally { Pop-Location }
} finally { $env:INCLUDE=$oldInclude; $env:LIB=$oldLib; $env:PATH=$oldPath }
Get-FileHash -LiteralPath (Join-Path $build "$OutputName.dll") -Algorithm SHA256
