# Builds and runs tests/remap_inject_test.cpp (dev-only test injection for the DirectInput remap) with MSVC x86, the
# game's architecture, twice: unmuted (requests refused) and with DBCE_OUTRUN_SIGNAL_MUTE set (arming). Offline: no game,
# device or force.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$vs = & (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe') -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars32.bat'
$out = Join-Path $root 'build\remap-inject-test'
New-Item -ItemType Directory -Force $out | Out-Null
$exe = Join-Path $out 'remap_inject_test.exe'
$cmd = "`"$vcvars`" >nul && cd /d `"$root`" && cl /nologo /O2 /MT /W4 /WX /EHsc /std:c++17 /utf-8 /DREMAP_INJECT_NO_SPDLOG /DDIRECTINPUT_VERSION=0x0800 /external:anglebrackets /external:W0 /I src /I external\ini-cpp\ini tests\remap_inject_test.cpp src\remap_inject.cpp src\profile_controls.cpp /Fo`"$out\\`" /Fe`"$exe`""
$text = cmd /c $cmd 2>&1
$text | Select-String -Pattern ' warning | error ' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $text | Select-Object -Last 20; throw 'remap inject test build failed' }
& $exe 2>$null; $unmuted = $LASTEXITCODE
$env:DBCE_OUTRUN_SIGNAL_MUTE = 'legacy'
try { & $exe --muted 2>$null; $mutedExit = $LASTEXITCODE } finally { Remove-Item Env:DBCE_OUTRUN_SIGNAL_MUTE }
if ($unmuted -or $mutedExit) { throw "remap inject checks failed (unmuted $unmuted, muted $mutedExit)" }
