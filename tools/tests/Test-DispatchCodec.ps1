$ErrorActionPreference='Stop'
$repo=(Resolve-Path "$PSScriptRoot/../..").Path
$out=Join-Path $repo ('build/dispatch-codec-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory $out | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'x86 C++ tools required'}
$runner=Join-Path $out 'build.cmd';$exe=Join-Path $out 'codec-fixture.exe'
@('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),'if errorlevel 1 exit /b %errorlevel%',('cl /nologo /std:c++17 /EHsc /W4 /WX "'+$PSScriptRoot+'\dispatch_codec_fixture.cpp" /Fe"'+$exe+'" /link /MANIFEST:EMBED /MANIFESTUAC:"level=''asInvoker'' uiAccess=''false''"'),'exit /b %errorlevel%') | Set-Content $runner -Encoding ascii
Push-Location $out
try{& $runner;if($LASTEXITCODE -ne 0){throw 'Compile failed'};& $exe "$out/synthetic.odiq" | Out-Host;if($LASTEXITCODE -ne 0){throw 'Codec fixture failed'};@{syntheticOnly=$true;file='synthetic.odiq';sha256=(Get-FileHash "$out/synthetic.odiq").Hash.ToLowerInvariant();bytes=(Get-Item "$out/synthetic.odiq").Length;source=(git -C $repo rev-parse HEAD).Trim();sourceDirty=[bool](git -C $repo status --porcelain);runtimeWriter='absent; fixture output only'}|ConvertTo-Json|Set-Content "$out/catalog.json";Write-Host "PASS: dispatch codec runner. Evidence: $out"}finally{Pop-Location}
