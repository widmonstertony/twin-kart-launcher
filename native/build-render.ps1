param([switch]$Test,[string]$StreamlineSdkRoot)
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
$includeDirs = @((Join-Path $vcRoot 'include'),(Join-Path $sdkRoot "Include\$sdkVersion\ucrt"),(Join-Path $sdkRoot "Include\$sdkVersion\shared"),(Join-Path $sdkRoot "Include\$sdkVersion\um"),(Join-Path $sdkRoot "Include\$sdkVersion\winrt"))
if($StreamlineSdkRoot) {
    if(-not (Test-Path -LiteralPath (Join-Path $StreamlineSdkRoot 'include\sl.h'))) { throw 'External Streamline SDK headers required for FG presentation' }
    $includeDirs += Join-Path $StreamlineSdkRoot 'include'
}
$libraryDirs = @((Join-Path $vcRoot 'lib\x64'),(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64"),(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64"))
$build = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$oldInclude = $env:INCLUDE; $oldLib = $env:LIB; $oldPath = $env:PATH
try {
    $env:INCLUDE = $includeDirs -join ';'; $env:LIB = $libraryDirs -join ';'; $env:PATH = (Split-Path -Parent $compiler) + ';' + $oldPath
    Push-Location $build
    try {
        $renderSources=@('RenderPassTrace.cpp','TemporalCapture.cpp','NativeNeuralPass.cpp','NativeTemporalPass.cpp','SharedTextureBridge.cpp','NativePresentation.cpp') | ForEach-Object { Join-Path $PSScriptRoot $_ }
        $features=@()
        if($StreamlineSdkRoot) { $renderSources+=Join-Path $PSScriptRoot 'FrameGenerationD3D12.cpp';$features+='/DTWIN_KART_FRAME_PRESENTATION=1' }
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT $features /LD /Fo./ /FeTwinKartDriftRenderProbe.dll (Join-Path $PSScriptRoot 'DriftRenderProbe.cpp') $renderSources /link bcrypt.lib user32.lib d3d11.lib d3d12.lib dxgi.lib d3dcompiler.lib wintrust.lib crypt32.lib
        if ($LASTEXITCODE -ne 0) { throw 'Render observer build failed' }
        if ($Test) {
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT $features /Fo./ /FeRenderPassTraceTests.exe (Join-Path $projectRoot 'tests\RenderPassTraceTests.cpp') $renderSources /link bcrypt.lib user32.lib dxgi.lib d3d11.lib d3d12.lib d3dcompiler.lib wintrust.lib crypt32.lib
            if ($LASTEXITCODE -ne 0) { throw 'Render pass test build failed' }
            & (Join-Path $build 'RenderPassTraceTests.exe')
            if ($LASTEXITCODE -ne 0) { throw 'Render pass checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeNativeViewMathTests.exe (Join-Path $projectRoot 'tests\NativeViewMathTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Native camera test build failed' }
            & (Join-Path $build 'NativeViewMathTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native camera checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeNativeTemporalContractTests.exe (Join-Path $projectRoot 'tests\NativeTemporalContractTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Native temporal contract test build failed' }
            & (Join-Path $build 'NativeTemporalContractTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native temporal contract checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeNativeTemporalPassTests.exe (Join-Path $projectRoot 'tests\NativeTemporalPassTests.cpp') (Join-Path $PSScriptRoot 'RenderPassTrace.cpp') /link bcrypt.lib d3d11.lib d3dcompiler.lib
            if($LASTEXITCODE -ne 0) { throw 'Native temporal boundary test build failed' }
            & (Join-Path $build 'NativeTemporalPassTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native temporal boundary checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeNativeGuideTests.exe (Join-Path $projectRoot 'tests\NativeGuideTests.cpp') /link d3d11.lib d3dcompiler.lib
            if($LASTEXITCODE -ne 0) { throw 'Native guide shader test build failed' }
            & (Join-Path $build 'NativeGuideTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native guide shader checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoStartupFixtureExe.obj /FeStartupFixture.exe (Join-Path $projectRoot 'tests\StartupFixture.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Owned startup EXE fixture build failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /DTWIN_KART_STARTUP_DLL=1 /LD /FoStartupFixtureDll.obj /FeStartupFixture.dll (Join-Path $projectRoot 'tests\StartupFixture.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Owned startup DLL fixture build failed' }
            $csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
            & $csc /nologo /langversion:5 /platform:x64 /target:exe /r:System.Web.Extensions.dll "/out:$build\OwnedGameStartupTests.exe" (Join-Path $projectRoot 'src\Profiles.cs') (Join-Path $projectRoot 'src\OwnedGameStartup.cs') (Join-Path $projectRoot 'tests\OwnedGameStartupTests.cs')
            if($LASTEXITCODE -ne 0) { throw 'Owned startup test build failed' }
            & (Join-Path $build 'OwnedGameStartupTests.exe') (Join-Path $build 'StartupFixture.exe') (Join-Path $build 'StartupFixture.dll')
            if($LASTEXITCODE -ne 0) { throw 'Owned startup checks failed' }
            if($StreamlineSdkRoot) {
                $interopSources=$renderSources | Where-Object { (Split-Path -Leaf $_) -ne 'NativePresentation.cpp' }
                & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT $features /Fo./ /FePresentationInteropTests.exe (Join-Path $projectRoot 'tests\PresentationInteropTests.cpp') $interopSources /link bcrypt.lib user32.lib dxgi.lib d3d11.lib d3d12.lib d3dcompiler.lib wintrust.lib crypt32.lib
                if($LASTEXITCODE -ne 0) { throw 'Native presentation boundary test build failed' }
                & (Join-Path $build 'PresentationInteropTests.exe')
                if($LASTEXITCODE -ne 0) { throw 'Native presentation boundary checks failed' }
            }
        }
    } finally { Pop-Location }
} finally { $env:INCLUDE = $oldInclude; $env:LIB = $oldLib; $env:PATH = $oldPath }
Get-FileHash -LiteralPath (Join-Path $build 'TwinKartDriftRenderProbe.dll') -Algorithm SHA256
