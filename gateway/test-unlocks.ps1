$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$build = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$target = Join-Path $build 'InventoryUnlockTests.exe'
& $compiler /nologo /target:exe /r:System.Web.Extensions.dll "/out:$target" (Join-Path $PSScriptRoot 'InventoryUnlocks.cs') (Join-Path $PSScriptRoot 'OfflineTuning.cs') (Join-Path $PSScriptRoot 'OfflineGarage.cs') (Join-Path $PSScriptRoot 'TrackCatalog.cs') (Join-Path $projectRoot 'tests\InventoryUnlockTests.cs')
if ($LASTEXITCODE -ne 0) { throw 'Unlock migration build failed' }
& $target
if ($LASTEXITCODE -ne 0) { throw 'Unlock migration checks failed' }
