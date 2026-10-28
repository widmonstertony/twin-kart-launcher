$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root 'build'; New-Item -ItemType Directory -Path $build -Force | Out-Null
$csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $csc /nologo /langversion:5 /platform:x64 /target:exe /r:System.Web.Extensions.dll "/out:$build\TrackSupportAuditTests.exe" (Join-Path $PSScriptRoot 'TrackCatalog.cs') (Join-Path $PSScriptRoot 'TrackSupportAudit.cs') (Join-Path $root 'tests\TrackSupportAuditTests.cs')
if($LASTEXITCODE -ne 0) { throw 'Track audit test build failed' }
& (Join-Path $build 'TrackSupportAuditTests.exe')
if($LASTEXITCODE -ne 0) { throw 'Track audit checks failed' }
