$ErrorActionPreference='Stop'
$repo=(Resolve-Path "$PSScriptRoot/../..").Path
$out=Join-Path $repo ('build/dispatch-fixture-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory $out | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'x86 C++ toolchain required'}
$runner=Join-Path $out 'build.cmd';$exe=Join-Path $out 'dispatch-fixture.exe'
@('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),'if errorlevel 1 exit /b %errorlevel%',('cl /nologo /std:c++17 /EHsc /W4 /WX "'+$PSScriptRoot+'\dispatch_trace_fixture.cpp" /Fe"'+$exe+'"'),'exit /b %errorlevel%') | Set-Content $runner -Encoding ascii
Push-Location $out
try{& $runner;if($LASTEXITCODE -ne 0){throw 'Compile failed'};& $exe | Out-Host;if($LASTEXITCODE -ne 0){throw 'Fixture failed'};Write-Host "Evidence: $out"}finally{Pop-Location}
