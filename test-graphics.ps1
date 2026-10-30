$ErrorActionPreference = 'Stop'
$buildDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $csc)) { throw '.NET Framework compiler is required.' }
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ "/out:$buildDir\GraphicsProfileTests.exe" /r:System.Web.Extensions.dll (Join-Path $PSScriptRoot 'src\Profiles.cs') (Join-Path $PSScriptRoot 'src\GraphicsProfile.cs') (Join-Path $PSScriptRoot 'tests\GraphicsProfileTests.cs')
if ($LASTEXITCODE -ne 0) { throw 'Graphics profile test compilation failed.' }
& (Join-Path $buildDir 'GraphicsProfileTests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Graphics profile tests failed.' }
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ "/out:$buildDir\OfflineGraphicsOptionsTests.exe" (Join-Path $PSScriptRoot 'src\OfflineGraphicsOptions.cs') (Join-Path $PSScriptRoot 'tests\OfflineGraphicsOptionsTests.cs')
if($LASTEXITCODE -ne 0) { throw 'Offline graphics policy test compilation failed' }
& (Join-Path $buildDir 'OfflineGraphicsOptionsTests.exe')
if($LASTEXITCODE -ne 0) { throw 'Offline graphics policy checks failed' }
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ "/out:$buildDir\OfflineGraphicsPreferenceStoreTests.exe" /r:System.Web.Extensions.dll (Join-Path $PSScriptRoot 'src\OfflineGraphicsPreferenceStore.cs') (Join-Path $PSScriptRoot 'tests\OfflineGraphicsPreferenceStoreTests.cs')
if($LASTEXITCODE -ne 0) { throw 'Offline graphics preference test compilation failed' }
& (Join-Path $buildDir 'OfflineGraphicsPreferenceStoreTests.exe')
if($LASTEXITCODE -ne 0) { throw 'Offline graphics preference checks failed' }
& $csc /nologo /langversion:5 /platform:x64 /target:exe /optimize+ "/out:$buildDir\OfflineKeyboardOptionsTests.exe" (Join-Path $PSScriptRoot 'src\OfflineKeyboardOptions.cs') (Join-Path $PSScriptRoot 'tests\OfflineKeyboardOptionsTests.cs')
if($LASTEXITCODE -ne 0) { throw 'Offline keyboard policy test compilation failed' }
& (Join-Path $buildDir 'OfflineKeyboardOptionsTests.exe')
if($LASTEXITCODE -ne 0) { throw 'Offline keyboard policy checks failed' }
