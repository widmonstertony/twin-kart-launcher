param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = if($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $projectRoot 'build' }
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$sources = @('WireCodec.cs', 'PreAuthSession.cs', 'LocalAccountSession.cs', 'DynamicDataCatalog.cs', 'LoopbackWebSocket.cs', 'GatewayHost.cs', 'LocalPingServer.cs', 'OfflineProfileStore.cs', 'NativeKartParameters.cs', 'InventoryUnlocks.cs', 'OfflineTuning.cs', 'OfflineGarage.cs', 'TrackCatalog.cs', 'PacketContracts.cs', 'RoomState.cs', 'OfflineRoomService.cs') |
    ForEach-Object { (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot $_)).Path }
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ /r:System.Web.Extensions.dll "/out:$buildDir\TwinKartGateway.exe" $sources
if ($LASTEXITCODE -ne 0) { throw 'Gateway build failed' }
