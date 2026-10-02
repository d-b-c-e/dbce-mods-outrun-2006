[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$DependencyRoot)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$DependencyRoot=(Resolve-Path -LiteralPath $DependencyRoot).Path
$out=Join-Path $repo ('build\signal-snapshots-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$overlay=Join-Path $out 'toolkit-fixture-overlay'
New-Item -ItemType Directory -Path $overlay | Out-Null
$model=[IO.File]::ReadAllText((Join-Path $repo 'lib\toolkit\include\force_model.h')).Replace("`r`n","`n")
$sha=[Security.Cryptography.SHA256]::Create()
$modelHash=[Convert]::ToHexString($sha.ComputeHash([Text.UTF8Encoding]::new($false).GetBytes($model))).ToLowerInvariant()
if($modelHash -ne '5a0596a0bce15cfb38f35c16540196cd8529b888d49662467a73c8bd01f155db'){throw 'Toolkit model source differs from pinned proposal'}
$friendLines=@(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'toolkit-snapshot-proposal\force_model.friend.patch') | Where-Object { $_.StartsWith('+') -and !$_.StartsWith('+++') } | ForEach-Object { $_.Substring(1) })
if($friendLines.Count -ne 6 -or ($friendLines[0..2] -join "`n") -ne ($friendLines[3..5] -join "`n")){throw 'Invalid two-class friend patch'}
$friend=($friendLines[0..2] -join "`n")+"`n"
foreach($anchor in @("private:`n    float shift_t_, shift_emitted_;","private:`n    float smoothed_, last_, ramp_t_;")) {
    if($model.Split(@($anchor),[StringSplitOptions]::None).Count -ne 2){throw 'Fixture patch anchor is not unique'}
    $model=$model.Replace($anchor,$anchor.Replace("private:`n","private:`n"+$friend))
}
[IO.File]::WriteAllText((Join-Path $overlay 'force_model.h'),$model,[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $repo 'lib\toolkit\include\force_profile.h') -Destination $overlay
$profileText=[IO.File]::ReadAllText((Join-Path $overlay 'force_profile.h')).Replace("`r`n","`n")
$profileHash=[Convert]::ToHexString($sha.ComputeHash([Text.UTF8Encoding]::new($false).GetBytes($profileText))).ToLowerInvariant()
if($profileHash -ne '4dd03ea92405c9bb157755b1b506036e469afac335253a2a40e0d4b32fa13999'){throw 'Toolkit profile parser differs from pinned proposal'}
$profiles=Join-Path $out 'synthetic-profiles'
New-Item -ItemType Directory -Path $profiles | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'lib\toolkit\profiles\force-profiles.ini') -Destination $profiles
@('[synthetic-owner@1]','inherits = arcade-outrun@3','model.spring.strength = 0.73','shaper.invert = true','shaper.strength = 37') | Set-Content -LiteralPath (Join-Path $profiles 'force-profiles.user.ini') -Encoding ascii
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'x86 C++ build tools required'}
function Build-SnapshotFixture($fixture,$name) {
    $argsList=@('/nologo','/std:c++latest','/EHsc','/MD','/DDBCE_FORCE_OFFLINE_SNAPSHOT_V1','/DOUTRUN_PROFILE_RECORDING_GUARDS','/D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING','/D_CRT_SECURE_NO_WARNINGS','/DDIRECTINPUT_VERSION=0x0800','/DZYDIS_STATIC_BUILD','/DZYCORE_STATIC_BUILD',('/I"'+$overlay+'"'))
    foreach($inc in @('src','lib\toolkit\include')){$argsList+='/I"'+(Join-Path $repo $inc)+'"'}
    foreach($inc in @('external\imgui','external\spdlog\include','external\ModUtils','build\_deps\safetyhook-src\include','build\_deps\zydis-src\include','build\_deps\zydis-build','build\_deps\zydis-src\dependencies\zycore\include','build\_deps\zydis-build\zycore')){$argsList+='/I"'+(Join-Path $DependencyRoot $inc)+'"'}
    $exe=Join-Path $out ($name+'.exe')
    $argsList+='"'+(Join-Path $PSScriptRoot $fixture)+'"'; $argsList+='/Fe"'+$exe+'"'
    foreach($lib in @('build\_deps\safetyhook-build\Release\safetyhook.lib','build\_deps\zydis-build\Release\Zydis.lib','build\_deps\zydis-build\zycore\Release\Zycore.lib')){$argsList+='"'+(Join-Path $DependencyRoot $lib)+'"'}
    $argsList+='user32.lib shell32.lib ole32.lib'
    $runner=Join-Path $out ($name+'.cmd')
    @('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),'if errorlevel 1 exit /b %errorlevel%',('cl '+($argsList -join ' ')),'exit /b %errorlevel%') | Set-Content -LiteralPath $runner -Encoding ascii
    & $runner | Out-Host
    if($LASTEXITCODE -ne 0){throw "$name compile failed"}
    return $exe
}
Push-Location $out
try {
    $goldenExe=Build-SnapshotFixture 'signal_profile_fixture.cpp' 'profile-overlay'
    $observed=@(& $goldenExe $profiles)
    if($LASTEXITCODE -ne 0){throw 'Overlay profile regressions failed'}
    $golden=Join-Path $out 'profile-overlay.observation'
    [IO.File]::WriteAllText($golden,($observed -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    $goldenHash=(Get-FileHash -LiteralPath $golden -Algorithm SHA256).Hash.ToLowerInvariant()
    if($goldenHash -ne '237e8a94e677a831e4c3736647b3a3b36afdae72272040148397dc148bba4a92'){throw 'Fixture friendship changed published named-profile golden'}
    $snapshotExe=Build-SnapshotFixture 'signal_snapshot_fixture.cpp' 'snapshot'
    & $snapshotExe $profiles
    if($LASTEXITCODE -ne 0){throw 'Snapshot tests failed'}
    $legacyInput=Join-Path $out 'legacy-synthetic.csv'
    $rows=@('dbce.outrun2006.calculation-input,1,legacy-defaults,60')
    for($i=0;$i -lt 48;$i++) {
        $gear=if($i -lt 20){2}else{3}; $flags=if($i -ge 32 -and $i -le 36){4096}else{0}
        $speed=if($i -lt 32){'0.8'}else{'0.5'}
        $rows+="$i,$($i*17),$speed,0.2,0.01,15,2,$gear,$flags,180,0.8,1"
    }
    $rows+='complete,48'
    [IO.File]::WriteAllText($legacyInput,($rows -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    $legacyExe=Build-SnapshotFixture 'signal_calculation_fixture.cpp' 'legacy'
    & $legacyExe $legacyInput (Join-Path $repo 'tools\tests\data\signal-legacy-v1.expected') | Out-Null
    if($LASTEXITCODE -ne 0){throw 'Published legacy calculation golden changed'}
    $recordExe=Build-SnapshotFixture 'signal_recording_fixture.cpp' 'legacy-recording'
    $recordFile=Join-Path $out 'legacy.osig'
    & $recordExe selftest $legacyInput $recordFile
    if($LASTEXITCODE -ne 0){throw 'Published legacy recorder roundtrip tests failed'}
    & $recordExe replay $recordFile
    if($LASTEXITCODE -ne 0){throw 'Published legacy v2 replay failed'}
    [ordered]@{schemaVersion=1;modelSourceLfSha256=$modelHash;profileParserLfSha256=$profileHash;patchedModelSha256=(Get-FileHash -LiteralPath (Join-Path $overlay 'force_model.h')).Hash;profileOverlayGoldenSha256=$goldenHash;snapshotVersion=1;replayCases=303;invalidCases=856;legacyGoldenAndV2Roundtrip='pass';runtimeHeaderChanges=$false;nativeAbiChanges=$false;scope='Build-local fixture-only toolkit friend proposal; no v2 named recording or live acceptance'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'receipt.json')
    Write-Host "PASS: pinned build-local snapshot proposal; published profile golden unchanged. Evidence: $out"
} finally { Pop-Location }
