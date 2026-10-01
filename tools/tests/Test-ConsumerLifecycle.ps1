[CmdletBinding()]
param([Parameter(Mandatory)][string]$ExactExecutable)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $repo 'build\consumer-lifecycle-fixture'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual C++ x86 build tools required' }
$environment=Join-Path $vs 'VC\Auxiliary\Build\vcvars32.bat'
$includes=@('src','external\imgui','external\spdlog\include','external\ModUtils','build\_deps\safetyhook-src\include','build\_deps\zydis-src\include','build\_deps\zydis-build','build\_deps\zydis-src\dependencies\zycore\include','build\_deps\zydis-build\zycore','lib\toolkit\include')
$libraries=@('build\_deps\safetyhook-build\Release\safetyhook.lib','build\_deps\zydis-build\Release\Zydis.lib','build\_deps\zydis-build\zycore\Release\Zycore.lib')
Push-Location $out
try {
    foreach($fixture in @('consumer_lifecycle','host_lifecycle')) {
        $arguments=@('/nologo','/std:c++latest','/EHsc','/MD','/D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING','/D_CRT_SECURE_NO_WARNINGS','/DDIRECTINPUT_VERSION=0x0800','/DZYDIS_STATIC_BUILD','/DZYCORE_STATIC_BUILD')
        $arguments += $includes | ForEach-Object { '/I"'+(Join-Path $repo $_)+'"' }
        $arguments += '"'+(Join-Path $repo ('tools\tests\'+$fixture+'_fixture.cpp'))+'"'
        $executable=Join-Path $out ($fixture+'-fixture.exe')
        $arguments += '/Fe"'+$executable+'"'
        $arguments += $libraries | ForEach-Object { '"'+(Join-Path $repo $_)+'"' }
        $arguments += 'user32.lib shell32.lib ole32.lib'
        $arguments += '/link /MANIFEST:EMBED /MANIFESTUAC:"level=''asInvoker'' uiAccess=''false''"'
        $runner=Join-Path $out 'build-fixture.cmd'
        @('@echo off',('call "'+$environment+'" >nul'),'if errorlevel 1 exit /b %errorlevel%',('cl '+($arguments -join ' ')),'exit /b %errorlevel%') | Set-Content -LiteralPath $runner -Encoding ascii
        & $runner
        if ($LASTEXITCODE -ne 0) { throw "$fixture compilation failed" }
        if($fixture -eq 'host_lifecycle') { & $executable $ExactExecutable } else { & $executable }
        if ($LASTEXITCODE -ne 0) { throw "$fixture failed" }
    }
    # Both detach forms use the same quiet production entry: no foreign ABI,
    # logging, wait or FreeLibrary is reachable from its detach branch.
    $dll=[IO.File]::ReadAllText((Join-Path $repo 'src\dllmain.cpp'))
    $entry=$dll.Substring($dll.IndexOf('BOOL APIENTRY DllMain'))
    $entry=[regex]::Replace($entry,'(?m)//[^\r\n]*','')
    if($entry -match 'FFB::|proxy::on_detach|FreeLibrary|WaitFor|spdlog::') { throw 'Detach has cleanup calls' }
    $manager=[IO.File]::ReadAllText((Join-Path $repo 'src\input_manager.cpp'))
    if($manager -match '~InputManager\s*\(' -or $manager -notmatch 'InputManager& InputManager::instance = \*new InputManager') { throw 'InputManager has loader-lock destruction' }
    Write-Host 'PASS: quiet production DllMain detach source contract; no game/device/native toolkit executed.'
} finally { Pop-Location }
