param([string]$ExternalData)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$sources=@('OfflineProfileStore','NativeKartParameters','InventoryUnlocks','OfflineTuning','OfflineGarage','TrackCatalog','DynamicDataCatalog','PacketContracts','RoomState','OfflineRoomService') | ForEach-Object { Join-Path $PSScriptRoot ($_+'.cs') }
$sources+=Join-Path $root 'tests\OfflineBackendTests.cs'
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$target=Join-Path $build 'OfflineBackendTests.exe'
& $compiler /nologo /langversion:5 /target:exe /platform:x64 /r:System.Web.Extensions.dll "/out:$target" $sources
if($LASTEXITCODE -ne 0) { throw 'Original offline backend test build failed' }
if($ExternalData) { & $target $ExternalData } else { & $target }
if($LASTEXITCODE -ne 0) { throw 'Original offline backend checks failed' }
