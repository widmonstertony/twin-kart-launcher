param([switch]$Test)
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
$build=Join-Path $projectRoot 'build'; New-Item -ItemType Directory -Path $build -Force | Out-Null
$oldInclude=$env:INCLUDE; $oldLib=$env:LIB; $oldPath=$env:PATH
try {
    $env:INCLUDE=@((Join-Path $vcRoot 'include'),(Join-Path $sdkRoot "Include\$sdkVersion\ucrt"),(Join-Path $sdkRoot "Include\$sdkVersion\shared"),(Join-Path $sdkRoot "Include\$sdkVersion\um")) -join ';'
    $env:LIB=@((Join-Path $vcRoot 'lib\x64'),(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64"),(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64")) -join ';'
    $env:PATH=(Split-Path -Parent $compiler)+';'+$oldPath
    Push-Location $build
    try {
        & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT /LD /Fo./ /FeTwinKartDriftRaceBridge.dll (Join-Path $PSScriptRoot 'DriftRaceBridge.cpp') (Join-Path $PSScriptRoot 'GameThreadQueue.cpp') /link bcrypt.lib user32.lib
        if($LASTEXITCODE -ne 0) { throw 'Native race bridge build failed' }
        if($Test) {
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
