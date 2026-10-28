param([switch]$SkipNative, [ValidateSet('x64','x86')][string]$Architecture = 'x64')
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
if ($Architecture -eq 'x86') { $buildDir = Join-Path $buildDir 'x86' }
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
if (-not $SkipNative) {
    & (Join-Path $projectRoot 'native\build-xinput.ps1') -Architecture $Architecture
    if ($LASTEXITCODE -ne 0) { throw 'XInput adapter compilation failed.' }
}
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $csc)) { throw '.NET Framework compiler is required.' }
$sources = @(
    (Join-Path $projectRoot 'src\Profiles.cs'),
    (Join-Path $projectRoot 'src\SharedPad.cs'),
    (Join-Path $projectRoot 'tests\InputBridgeTests.cs')
)
& $csc /nologo /langversion:5 "/platform:$Architecture" /target:exe /optimize+ "/out:$buildDir\InputBridgeTests.exe" /r:System.Web.Extensions.dll $sources
if ($LASTEXITCODE -ne 0) { throw 'Input bridge test compilation failed.' }
& (Join-Path $buildDir 'InputBridgeTests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Input bridge tests failed.' }
