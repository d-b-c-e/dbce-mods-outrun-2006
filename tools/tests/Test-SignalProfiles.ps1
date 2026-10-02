[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$DependencyRoot,
    [Parameter(Mandatory=$true)][string]$BaselineRoot
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$DependencyRoot=(Resolve-Path -LiteralPath $DependencyRoot).Path
$BaselineRoot=(Resolve-Path -LiteralPath $BaselineRoot).Path
$baselineHead=git -C $BaselineRoot rev-parse HEAD
if($LASTEXITCODE -ne 0 -or $baselineHead -ne '73747cc3585f48c64465340c8c21ee5c06093cf8'){throw 'Expected exact pre-recorder production seam baseline 73747cc'}
git -C $BaselineRoot diff --quiet HEAD --
if($LASTEXITCODE -ne 0){throw 'Baseline tracked files differ from exact commit'}
$out=Join-Path $repo ('build\signal-profiles-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$profiles=Join-Path $out 'synthetic-profiles'
New-Item -ItemType Directory -Path $profiles | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'lib\toolkit\profiles\force-profiles.ini') -Destination $profiles
@('[synthetic-owner@1]','inherits = arcade-outrun@3','model.spring.strength = 0.73','shaper.invert = true','shaper.strength = 37') |
    Set-Content -LiteralPath (Join-Path $profiles 'force-profiles.user.ini') -Encoding ascii
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'x86 C++ build tools required'}
function Build-ProfileFixture($sourceRoot,$name,$guards) {
    $argsList=@('/nologo','/std:c++latest','/EHsc','/MD','/D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING','/D_CRT_SECURE_NO_WARNINGS','/DDIRECTINPUT_VERSION=0x0800','/DZYDIS_STATIC_BUILD','/DZYCORE_STATIC_BUILD')
    if($guards){$argsList+='/DOUTRUN_PROFILE_RECORDING_GUARDS'}
    foreach($inc in @('src','lib\toolkit\include')){$argsList+='/I"'+(Join-Path $sourceRoot $inc)+'"'}
    foreach($inc in @('external\imgui','external\spdlog\include','external\ModUtils','build\_deps\safetyhook-src\include','build\_deps\zydis-src\include','build\_deps\zydis-build','build\_deps\zydis-src\dependencies\zycore\include','build\_deps\zydis-build\zycore')){$argsList+='/I"'+(Join-Path $DependencyRoot $inc)+'"'}
    $wrapper=Join-Path $out ($name+'.cpp')
    @(('#define OUTRUN_PROFILE_HOOKS_SOURCE "'+(Join-Path $sourceRoot 'src\hooks_dinputffb.cpp').Replace('\','/')+'"'),
      ('#include "'+(Join-Path $repo 'tools\tests\signal_profile_fixture.cpp').Replace('\','/')+'"')) |
        Set-Content -LiteralPath $wrapper -Encoding ascii
    $exe=Join-Path $out ($name+'.exe')
    $argsList+='"'+$wrapper+'"'; $argsList+='/Fe"'+$exe+'"'
    foreach($lib in @('build\_deps\safetyhook-build\Release\safetyhook.lib','build\_deps\zydis-build\Release\Zydis.lib','build\_deps\zydis-build\zycore\Release\Zycore.lib')){$argsList+='"'+(Join-Path $DependencyRoot $lib)+'"'}
    $argsList+='user32.lib shell32.lib ole32.lib'
    $runner=Join-Path $out ($name+'.cmd')
    @('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),'if errorlevel 1 exit /b %errorlevel%',('cl '+($argsList -join ' ')),'exit /b %errorlevel%') |
        Set-Content -LiteralPath $runner -Encoding ascii
    & $runner | Out-Host
    if($LASTEXITCODE -ne 0){throw "$name compile failed"}
    $result=@(& $exe $profiles)
    if($LASTEXITCODE -ne 0){throw "$name profile regressions failed"}
    $observation=Join-Path $out ($name+'.observation')
    [IO.File]::WriteAllText($observation,($result -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    return $observation
}
Push-Location $out
try {
    $old=Build-ProfileFixture $BaselineRoot 'pre-recorder' $false
    $new=Build-ProfileFixture $repo 'candidate' $true
    $oldHash=(Get-FileHash -LiteralPath $old -Algorithm SHA256).Hash
    $newHash=(Get-FileHash -LiteralPath $new -Algorithm SHA256).Hash
    if($oldHash -ne $newHash){throw 'Exact named-profile observations differ from old production seam'}
    [ordered]@{schemaVersion=1;baselineCommit=$baselineHead;candidateHead=(git -C $repo rev-parse HEAD);profiles=7;casesPerExecutable=28;productionRunsPerExecutable=84;exactObservationSha256=$newHash;recordingProfiles='legacy only; named/shared refusal unchanged';output='memory-only raw calculation requests, public model event/structural traces; no native actuator encoding';limits='Private model/shaper state is not captured or proven complete; no live capture or physical acceptance';dependencyRoot=$DependencyRoot} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'receipt.json')
    Write-Host "PASS: exact old-production golden comparison; seven profiles, constant/periodic, populated-history resets, rejected sink feedback, fixed60Hz equal/jumped ticks and recorder refusal. SHA256 $newHash. Evidence: $out"
} finally { Pop-Location }
