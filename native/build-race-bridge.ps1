param([switch]$Test,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation) { throw 'MSVC x64 build tools required' }
$vcRoot=(Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkRoot=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdkVersion=(Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory | Where-Object { (Test-Path -LiteralPath (Join-Path $_.FullName 'um\Windows.h')) -and (Test-Path -LiteralPath (Join-Path $sdkRoot "Lib\$($_.Name)\um\x64\bcrypt.lib")) } | Sort-Object Name -Descending | Select-Object -First 1).Name
if(-not $sdkVersion) { throw 'Complete Windows SDK required' }
$compiler=Join-Path $vcRoot 'bin\Hostx64\x64\cl.exe'
$build=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $projectRoot 'build'}
New-Item -ItemType Directory -Path $build -Force | Out-Null
$oldInclude=$env:INCLUDE; $oldLib=$env:LIB; $oldPath=$env:PATH
try {
    $env:INCLUDE=@((Join-Path $vcRoot 'include'),(Join-Path $sdkRoot "Include\$sdkVersion\ucrt"),(Join-Path $sdkRoot "Include\$sdkVersion\shared"),(Join-Path $sdkRoot "Include\$sdkVersion\um")) -join ';'
    $env:LIB=@((Join-Path $vcRoot 'lib\x64'),(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64"),(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64")) -join ';'
    $env:PATH=(Split-Path -Parent $compiler)+';'+$oldPath
    Push-Location $build
    try {
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /LD /Fo./ /FeTwinKartDriftRaceBridge.dll (Join-Path $PSScriptRoot 'DriftRaceBridge.cpp') (Join-Path $PSScriptRoot 'GameThreadQueue.cpp') /link bcrypt.lib user32.lib gdi32.lib
        if($LASTEXITCODE -ne 0) { throw 'Native race bridge build failed' }
        if($Test) {
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoHideRepairedSceneTests.obj /FeHideRepairedSceneTests.exe (Join-Path $projectRoot 'tests\HideRepairedSceneTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Hide arena layout test build failed' }
            & (Join-Path $build 'HideRepairedSceneTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Hide arena layout checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoLoadingPreviewTests.obj /FeLoadingPreviewTests.exe (Join-Path $projectRoot 'tests\LoadingPreviewTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Loading preview test build failed' }
            & (Join-Path $build 'LoadingPreviewTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Loading preview checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoKartTranslationEdgeTests.obj /FeKartTranslationEdgeTests.exe (Join-Path $projectRoot 'tests\KartTranslationEdgeTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Kart translation edge tests build failed' }
            & (Join-Path $build 'KartTranslationEdgeTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Kart translation edge tests failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoNativeHitObjectsTests.obj /FeNativeHitObjectsTests.exe (Join-Path $projectRoot 'tests\NativeHitObjectsTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Native hit-object test build failed' }
            & (Join-Path $build 'NativeHitObjectsTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native hit-object checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoKartRoadPoseTests.obj /FeKartRoadPoseTests.exe (Join-Path $projectRoot 'tests\KartRoadPoseTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Kart road/body pose test build failed' }
            & (Join-Path $build 'KartRoadPoseTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Kart road/body pose checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoNativeKartContactsTests.obj /FeNativeKartContactsTests.exe (Join-Path $projectRoot 'tests\NativeKartContactsTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Native kart contact test build failed' }
            & (Join-Path $build 'NativeKartContactsTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native kart contact checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoRoadEdgeQueriesTests.obj /FeRoadEdgeQueriesTests.exe (Join-Path $projectRoot 'tests\RoadEdgeQueriesTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Road edge query budget test build failed' }
            & (Join-Path $build 'RoadEdgeQueriesTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Road edge query budget checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoNativeBodyGeometryTests.obj /FeNativeBodyGeometryTests.exe (Join-Path $projectRoot 'tests\NativeBodyGeometryTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Native body geometry snapshot test build failed' }
            & (Join-Path $build 'NativeBodyGeometryTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native body geometry snapshot checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoNativeComponentSweepTests.obj /FeNativeComponentSweepTests.exe (Join-Path $projectRoot 'tests\NativeComponentSweepTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Native component sweep boundary test build failed' }
            & (Join-Path $build 'NativeComponentSweepTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native component sweep boundary checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoNativeVehicleSweepTests.obj /FeNativeVehicleSweepTests.exe (Join-Path $projectRoot 'tests\NativeVehicleSweepTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Native vehicle sweep boundary test build failed' }
            & (Join-Path $build 'NativeVehicleSweepTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native vehicle sweep boundary checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoChaserItemInputTests.obj /FeChaserItemInputTests.exe (Join-Path $projectRoot 'tests\ChaserItemInputTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Chaser item input test build failed' }
            & (Join-Path $build 'ChaserItemInputTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Chaser item input checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoChaserObjectiveDriverTests.obj /FeChaserObjectiveDriverTests.exe (Join-Path $projectRoot 'tests\ChaserObjectiveDriverTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Chaser objective policy test build failed' }
            & (Join-Path $build 'ChaserObjectiveDriverTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Chaser objective policy checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoScopedMaterialRepairTests.obj /FeScopedMaterialRepairTests.exe (Join-Path $projectRoot 'tests\ScopedMaterialRepairTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Scoped material repair test build failed' }
            & (Join-Path $build 'ScopedMaterialRepairTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Scoped material repair checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoRoadSegmentSupportTests.obj /FeRoadSegmentSupportTests.exe (Join-Path $projectRoot 'tests\RoadSegmentSupportTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Road support test build failed' }
            & (Join-Path $build 'RoadSegmentSupportTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Road support checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoLayeredRoadNavigationTests.obj /FeLayeredRoadNavigationTests.exe (Join-Path $projectRoot 'tests\LayeredRoadNavigationTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Layered road navigation test build failed' }
            & (Join-Path $build 'LayeredRoadNavigationTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Layered road navigation checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoChaserArenaDriverTests.obj /FeChaserArenaDriverTests.exe (Join-Path $projectRoot 'tests\ChaserArenaDriverTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Chaser arena navigation contract test build failed' }
            & (Join-Path $build 'ChaserArenaDriverTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Chaser arena navigation contract checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /FoNativeChaserModeTests.obj /FeNativeChaserModeTests.exe (Join-Path $projectRoot 'tests\NativeChaserModeTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Retained Chaser mode boundary test build failed' }
            & (Join-Path $build 'NativeChaserModeTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Retained Chaser mode boundary checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeNativeAiRosterTests.exe (Join-Path $projectRoot 'tests\NativeAiRosterTests.cpp') /link bcrypt.lib
            if($LASTEXITCODE -ne 0) { throw 'AI roster test build failed' }
            & (Join-Path $build 'NativeAiRosterTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'AI roster contract checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeNativeEquipmentPresetTests.exe (Join-Path $projectRoot 'tests\NativeEquipmentPresetTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Native equipment preset test build failed' }
            & (Join-Path $build 'NativeEquipmentPresetTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native equipment preset checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeNativeTrackCatalogTests.exe (Join-Path $projectRoot 'tests\NativeTrackCatalogTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Native track catalog test build failed' }
            & (Join-Path $build 'NativeTrackCatalogTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Native track catalog checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeLocalAiLifecycleTests.exe (Join-Path $projectRoot 'tests\LocalAiLifecycleTests.cpp') /link bcrypt.lib
            if($LASTEXITCODE -ne 0) { throw 'Local AI lifecycle test build failed' }
            & (Join-Path $build 'LocalAiLifecycleTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Local AI lifecycle checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeOwnedCodePatchTests.exe (Join-Path $projectRoot 'tests\OwnedCodePatchTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Owned call adapter test build failed' }
            & (Join-Path $build 'OwnedCodePatchTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Owned call adapter checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeRaceControlTests.exe (Join-Path $projectRoot 'tests\RaceControlTests.cpp')
            if($LASTEXITCODE -ne 0) { throw 'Race request test build failed' }
            & (Join-Path $build 'RaceControlTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Race request checks failed' }
            $csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
            & $csc /nologo /langversion:5 /platform:x64 /target:exe /r:System.Web.Extensions.dll "/out:$build\LocalRaceProfileTests.exe" (Join-Path $projectRoot 'src\Profiles.cs') (Join-Path $projectRoot 'tests\LocalRaceProfileTests.cs')
            if($LASTEXITCODE -ne 0) { throw 'Managed race request test build failed' }
            & (Join-Path $build 'LocalRaceProfileTests.exe') (Join-Path $build 'RaceControlTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Managed/native race request checks failed' }
            & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /Fo./ /FeGameThreadQueueTests.exe (Join-Path $projectRoot 'tests\GameThreadQueueTests.cpp') (Join-Path $PSScriptRoot 'GameThreadQueue.cpp') /link user32.lib
            if($LASTEXITCODE -ne 0) { throw 'Game-thread queue test build failed' }
            & (Join-Path $build 'GameThreadQueueTests.exe')
            if($LASTEXITCODE -ne 0) { throw 'Game-thread queue checks failed' }
        }
    } finally { Pop-Location }
} finally { $env:INCLUDE=$oldInclude; $env:LIB=$oldLib; $env:PATH=$oldPath }
Get-FileHash -LiteralPath (Join-Path $build 'TwinKartDriftRaceBridge.dll') -Algorithm SHA256
