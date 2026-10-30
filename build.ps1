param([switch]$SkipNative)
$ErrorActionPreference = 'Stop'
$buildDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
if (-not $SkipNative) {
    & (Join-Path $PSScriptRoot 'native\build-input.ps1')
    & (Join-Path $PSScriptRoot 'native\build-xinput.ps1')
    & (Join-Path $PSScriptRoot 'native\build-xinput.ps1') -Architecture x86
}
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $csc)) { throw '.NET Framework 4.8 compiler is required.' }
$sources = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'src') -Filter '*.cs' | ForEach-Object { $_.FullName })
$sources += @('OfflineTuning.cs','OfflineGarage.cs','TrackCatalog.cs','TrackSupportAudit.cs') | ForEach-Object { Join-Path $PSScriptRoot ('gateway\'+$_) }
& $csc /nologo /langversion:5 /platform:x64 /target:winexe /optimize+ "/out:$buildDir\TwinKartLauncher.exe" /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll $sources
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
$auditSources = @((Join-Path $PSScriptRoot 'src\Profiles.cs'), (Join-Path $PSScriptRoot 'src\ClientDiscovery.cs'), (Join-Path $PSScriptRoot 'tools\InspectClient.cs'))
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ "/out:$buildDir\TwinKartAudit.exe" /r:System.Web.Extensions.dll $auditSources
if ($LASTEXITCODE -ne 0) { throw 'Client audit compilation failed.' }
$sessionSources = @('Profiles.cs','OwnedGameModule.cs','OwnedGameStartup.cs','OfflineGraphicsOptions.cs','OfflineKeyboardOptions.cs','OfflineLanguageOptions.cs','NativeTrackSessionCatalog.cs','AiRosterCatalog.cs','OfflineLaunchSession.cs') | ForEach-Object { Join-Path $PSScriptRoot ('src\'+$_) }
$sessionSources += @('TrackCatalog.cs','TrackSupportAudit.cs') | ForEach-Object {Join-Path $PSScriptRoot ('gateway\'+$_)}
$sessionSources += Join-Path $PSScriptRoot 'tools\RunSession.cs'
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ "/out:$buildDir\TwinKartSession.exe" /r:System.Web.Extensions.dll $sessionSources
if ($LASTEXITCODE -ne 0) { throw 'Original session runner compilation failed.' }
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ "/out:$buildDir\TwinKartSdkCopy.exe" (Join-Path $PSScriptRoot 'tools\PrepareSdkCopy.cs')
if ($LASTEXITCODE -ne 0) { throw 'Original SDK copy tool compilation failed.' }
Write-Output (Join-Path $buildDir 'TwinKartLauncher.exe')
$trackSources=@('TrackCatalog.cs','TrackSupportAudit.cs') | ForEach-Object { Join-Path $PSScriptRoot ('gateway\'+$_) }
$trackSources+=Join-Path $PSScriptRoot 'tools\AuditTracks.cs'
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ /r:System.Web.Extensions.dll "/out:$buildDir\TwinKartTrackAudit.exe" $trackSources
if($LASTEXITCODE -ne 0) { throw 'Native track audit compilation failed' }
