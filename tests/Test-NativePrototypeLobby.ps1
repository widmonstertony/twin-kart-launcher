param([switch]$CompileOnly)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=(Get-ChildItem (Join-Path $installation 'VC/Tools/MSVC') -Directory|Sort-Object Name -Descending|Select-Object -First 1).FullName
$sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'
$version=(Get-ChildItem (Join-Path $sdk 'Include') -Directory|Where-Object {Test-Path (Join-Path $_.FullName 'um/Windows.h')}|Sort-Object Name -Descending|Select-Object -First 1).Name
$savedInclude=$env:INCLUDE;$savedLib=$env:LIB;$savedPath=$env:PATH
try {
    $env:INCLUDE=@((Join-Path $vc 'include'),(Join-Path $sdk "Include/$version/ucrt"),(Join-Path $sdk "Include/$version/shared"),(Join-Path $sdk "Include/$version/um"))-join ';'
    $env:LIB=@((Join-Path $vc 'lib/x64'),(Join-Path $sdk "Lib/$version/ucrt/x64"),(Join-Path $sdk "Lib/$version/um/x64"))-join ';'
    $compiler=Join-Path $vc 'bin/Hostx64/x64/cl.exe'
    $env:PATH=(Split-Path -Parent $compiler)+';'+$savedPath
    $exe=Join-Path $root 'build/NativePrototypeLobbyTests.exe';$obj=Join-Path $root 'build/NativePrototypeLobbyTests.obj'
    & $compiler /nologo /std:c++17 /EHsc /W4 /O2 /MT "/Fo$obj" "/Fe$exe" (Join-Path $PSScriptRoot 'NativePrototypeLobbyTests.cpp') /link user32.lib gdi32.lib
    if($LASTEXITCODE -ne 0){throw 'Native prototype lobby check compilation failed'}
    if(-not $CompileOnly) {
        & $exe
        if($LASTEXITCODE -ne 0){throw 'Native prototype lobby checks failed'}
    }
} finally {$env:INCLUDE=$savedInclude;$env:LIB=$savedLib;$env:PATH=$savedPath}
