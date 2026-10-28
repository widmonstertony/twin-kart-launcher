$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $csc)) { throw '.NET Framework compiler is required.' }
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ "/out:$buildDir\RoomStateTests.exe" /r:System.Web.Extensions.dll (Join-Path $PSScriptRoot 'PacketContracts.cs') (Join-Path $PSScriptRoot 'RoomState.cs') (Join-Path $projectRoot 'tests\RoomStateTests.cs') (Join-Path $projectRoot 'tests\PacketContractTests.cs')
if ($LASTEXITCODE -ne 0) { throw 'Room state test compilation failed.' }
& (Join-Path $buildDir 'RoomStateTests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Room state tests failed.' }
