$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\PackedFieldLayoutTests.exe'
New-Item -ItemType Directory -Path (Split-Path -Parent $output) -Force | Out-Null
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:exe ("/out:" + $output) (Join-Path $PSScriptRoot 'PackedFieldLayout.cs') (Join-Path $root 'tests\PackedFieldLayoutTests.cs')
if ($LASTEXITCODE -ne 0) { throw 'Packed field tests failed to build' }
& $output
if ($LASTEXITCODE -ne 0) { throw 'Packed field tests failed' }
