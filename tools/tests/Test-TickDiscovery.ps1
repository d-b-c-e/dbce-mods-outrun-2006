[CmdletBinding()]
param()
# x86 build of the production src/tick_discovery.hpp with tools/tests/tick_discovery_fixture.cpp: request parsing and
# refusals, window ordering/bounds, capacity, stop/exit outcomes and the exact files written under a temporary root.
# No game, hook, device or owner file.
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $repo ('build\tick-discovery-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'x86 C++ build tools required'}
$exe=Join-Path $out 'tick-discovery.exe'
$source=Join-Path $repo 'tools\tests\tick_discovery_fixture.cpp'
$runner=Join-Path $out 'build.cmd'
@('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),'if errorlevel 1 exit /b %errorlevel%',
  ('cl /nologo /std:c++latest /EHsc /MD /W4 /WX /DNOMINMAX "'+$source+'" /Fe"'+$exe+'" /Fo"'+$out+'\\"'),'exit /b %errorlevel%')|Set-Content $runner -Encoding ascii
Push-Location $out
try {
  & $runner
  if($LASTEXITCODE -ne 0){throw 'compile failed'}
  $root=Join-Path $out 'root'; New-Item -ItemType Directory -Path $root | Out-Null
  & $exe $root
  if($LASTEXITCODE -ne 0){throw 'tick discovery fixture failed'}
  Write-Host "Evidence: $out"
} finally { Pop-Location }
