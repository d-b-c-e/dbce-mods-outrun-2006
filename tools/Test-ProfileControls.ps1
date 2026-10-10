# Builds and runs tests/profile_controls_test.cpp (STD-033 rig-profile controls -> the DirectInput remap's user INI
# keys) with MSVC x86, the game's architecture. Offline: no game, device or force.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$vs = & (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe') -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars32.bat'
$out = Join-Path $root 'build\profile-controls-test'
New-Item -ItemType Directory -Force $out | Out-Null
$exe = Join-Path $out 'profile_controls_test.exe'
$cmd = "`"$vcvars`" >nul && cd /d `"$root`" && cl /nologo /O2 /MT /W4 /WX /EHsc /std:c++17 /utf-8 /I src tests\profile_controls_test.cpp src\profile_controls.cpp /Fo`"$out\\`" /Fe`"$exe`" && `"$exe`""
$text = cmd /c $cmd 2>&1
$text | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $text | Select-Object -Last 20; throw 'profile controls checks failed' }
