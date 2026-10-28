$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$sources = @('WireCodec.cs', 'PreAuthSession.cs', 'LocalAccountSession.cs', 'DynamicDataCatalog.cs', 'LoopbackWebSocket.cs', 'OfflineProfileStore.cs', 'NativeKartParameters.cs', 'InventoryUnlocks.cs', 'OfflineTuning.cs', 'OfflineGarage.cs', 'TrackCatalog.cs') |
    ForEach-Object { (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot $_)).Path }
$sources += (Resolve-Path -LiteralPath (Join-Path $projectRoot 'tests\GatewayTests.cs')).Path
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ /r:System.Web.Extensions.dll "/out:$buildDir\GatewayTests.exe" $sources
if ($LASTEXITCODE -ne 0) { throw 'Gateway test build failed' }
& (Join-Path $buildDir 'GatewayTests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Gateway tests failed' }
