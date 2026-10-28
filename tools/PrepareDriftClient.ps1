param([Parameter(Mandatory=$true)][string]$InstalledRoot,[Parameter(Mandatory=$true)][string]$DestinationRoot)
$ErrorActionPreference='Stop'
$installed=(Resolve-Path -LiteralPath $InstalledRoot).Path
$target=[System.IO.Path]::GetFullPath($DestinationRoot)
if(Test-Path -LiteralPath $target) { throw 'Choose a new isolated destination directory' }
$binarySource=Join-Path $installed 'KartDrift\Binaries\Win64'
$expectedGame='5041d9f5d45adb4b1c932d8056a89f70db7fa98f93614ac30685cd1adeb9d9c2'
if((Get-FileHash -LiteralPath (Join-Path $binarySource 'KartDrift-Win64-Shipping.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedGame) { throw 'Unsupported game build' }
$binaryTarget=Join-Path $target 'KartDrift\Binaries\Win64'
New-Item -ItemType Directory -Path $binaryTarget -Force | Out-Null
foreach($file in Get-ChildItem -LiteralPath $binarySource -Recurse -File) {
    if($file.Extension -in @('.log','.dmp') -or $file.FullName -eq (Join-Path $binarySource 'nexon_api_x64.dll')) { continue }
    $relative=$file.FullName.Substring($binarySource.Length).TrimStart('\')
    $destination=Join-Path $binaryTarget $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination
}
foreach($folder in @('Engine','KartDrift\Content','KartDrift\Plugins')) {
    $original=Join-Path $installed $folder
    if(-not(Test-Path -LiteralPath $original -PathType Container)) { throw ('Game resource directory missing: '+$folder) }
    New-Item -ItemType Junction -Path (Join-Path $target $folder) -Target $original | Out-Null
}
$sdkTool=Join-Path (Split-Path -Parent $PSScriptRoot) 'build\TwinKartSdkCopy.exe'
& $sdkTool (Join-Path $binarySource 'nexon_api_x64.dll') (Join-Path $binaryTarget 'nexon_api_x64.dll')
if($LASTEXITCODE -ne 0) { throw 'SDK preparation failed; incomplete isolated directory retained for diagnosis' }
Write-Output (Join-Path $binaryTarget 'KartDrift-Win64-Shipping.exe')
