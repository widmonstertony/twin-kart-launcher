$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $csc)) { throw '.NET Framework compiler is required.' }
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ "/out:$buildDir\WireCodecTests.exe" /r:System.Web.Extensions.dll (Join-Path $PSScriptRoot 'WireCodec.cs') (Join-Path $projectRoot 'tests\WireCodecTests.cs')
if ($LASTEXITCODE -ne 0) { throw 'Wire codec test compilation failed.' }
& (Join-Path $buildDir 'WireCodecTests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Wire codec tests failed.' }
