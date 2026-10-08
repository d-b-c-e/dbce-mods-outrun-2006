[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$DependencyRoot,[switch]$CreateGolden)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$DependencyRoot=(Resolve-Path -LiteralPath $DependencyRoot).Path
$out=Join-Path $repo ('build/command-summary-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
# Existing recorder fixture is the sole generator: no new capture/model logic.
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -NonInteractive -File (Join-Path $repo 'tools/ci/Invoke-CiTest.ps1') -ScriptPath (Join-Path $repo 'tools/tests/Test-SignalCalculation.ps1') -ParametersJson (@{DependencyRoot=$DependencyRoot}|ConvertTo-Json -Compress) *> (Join-Path $out 'existing-recorder-tests.log')
if($LASTEXITCODE -ne 0){throw 'Existing golden/recorder tests failed'}
$line=(Select-String -LiteralPath (Join-Path $out 'existing-recorder-tests.log') -Pattern 'Evidence: (.+)$' | Select-Object -Last 1).Matches[0].Groups[1].Value
$sourceRun=(Resolve-Path -LiteralPath $line).Path
Copy-Item -LiteralPath (Join-Path $sourceRun 'synthetic.csv') -Destination (Join-Path $out 'synthetic-input.csv')
Copy-Item -LiteralPath (Join-Path $sourceRun 'synthetic.osig') -Destination (Join-Path $out 'synthetic-recording.osig')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'data/command-metric-definitions-v1.json') -Destination (Join-Path $out 'metric-definitions.json')
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'x86 C++ build tools required'}
$argsList=@('/nologo','/std:c++latest','/EHsc','/MD','/D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING','/D_CRT_SECURE_NO_WARNINGS','/DDIRECTINPUT_VERSION=0x0800','/DZYDIS_STATIC_BUILD','/DZYCORE_STATIC_BUILD')
foreach($inc in @('src','lib/toolkit/include')){$argsList+='/I"'+(Join-Path $repo $inc)+'"'}
foreach($inc in @('external/imgui','external/spdlog/include','external/ModUtils','build/_deps/safetyhook-src/include','build/_deps/zydis-src/include','build/_deps/zydis-build','build/_deps/zydis-src/dependencies/zycore/include','build/_deps/zydis-build/zycore')){$argsList+='/I"'+(Join-Path $DependencyRoot $inc)+'"'}
$exe=Join-Path $out 'command-summary.exe'
$argsList+='"'+(Join-Path $PSScriptRoot 'signal_command_summary.cpp')+'"';$argsList+='/Fe"'+$exe+'"'
foreach($lib in @('build/_deps/safetyhook-build/Release/safetyhook.lib','build/_deps/zydis-build/Release/Zydis.lib','build/_deps/zydis-build/zycore/Release/Zycore.lib')){$argsList+='"'+(Join-Path $DependencyRoot $lib)+'"'}
$argsList+='user32.lib shell32.lib ole32.lib bcrypt.lib'
$runner=Join-Path $out 'build.cmd'
@('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),'if errorlevel 1 exit /b %errorlevel%',('cl '+($argsList -join ' ')),'exit /b %errorlevel%') | Set-Content -LiteralPath $runner -Encoding ascii
Push-Location $out
try {
    & $runner;if($LASTEXITCODE -ne 0){throw 'Summary fixture compile failed'}
    & $exe selftest;if($LASTEXITCODE -ne 0){throw 'Metric arithmetic tests failed'}
    $recording=Join-Path $out 'synthetic-recording.osig';$declared='73747cc3585f48c64465340c8c21ee5c06093cf8'
    $lines=@(& $exe summary $recording $declared);if($LASTEXITCODE -ne 0){throw 'Valid production-linked recording summary failed'}
    $summary=Join-Path $out 'command-summary.json'
    [IO.File]::WriteAllText($summary,($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    $again=@(& $exe summary $recording $declared);if($LASTEXITCODE -ne 0 -or ($again -join "`n") -cne ($lines -join "`n")){throw 'Repeated summary differs'}
    $result=Get-Content -LiteralPath $summary -Raw | ConvertFrom-Json
    if($result.recording_sha256 -cne (Get-FileHash -LiteralPath $recording).Hash.ToLowerInvariant() -or
       $result.frame_count -ne 48 -or $result.sample_tick_span_ms -ne 799 -or $result.fixed60_calculation_step_sum_seconds -ne 0.8 -or
       -not $result.production_recalculation_verified -or $result.physical_output -or $result.declared_revision_authenticated){throw 'Summary provenance/frame/time/domain invariants failed'}
    if(@($result.unsupported.PSObject.Properties | Where-Object {$null -ne $_.Value}).Count){throw 'Unsupported metrics must remain null'}
    $golden=Join-Path $PSScriptRoot 'data/command-summary-v1.expected'
    if($CreateGolden) {
        if(Test-Path -LiteralPath $golden){throw 'Existing summary golden is immutable'}
        [IO.File]::WriteAllBytes($golden,[IO.File]::ReadAllBytes($summary))
    } elseif(-not (Test-Path -LiteralPath $golden) -or (Get-FileHash -LiteralPath $golden).Hash -ne (Get-FileHash -LiteralPath $summary).Hash){throw 'Summary golden mismatch'}
    & $exe summary $recording ('0'*40) 2>&1 | Out-Null;if($LASTEXITCODE -ne 2){throw 'Declared revision mismatch must refuse'}
    & $exe summary (Join-Path $sourceRun 'synthetic.osig.mismatch') $declared 2>&1 | Out-Null;if($LASTEXITCODE -ne 1){throw 'Expected calculation mismatch must refuse summary'}
    & $exe summary (Join-Path $sourceRun 'corrupt.osig') $declared 2>&1 | Out-Null;if($LASTEXITCODE -ne 2){throw 'Corrupt recording must refuse summary'}
    $truncated=Join-Path $out 'truncated.osig';$bytes=[IO.File]::ReadAllBytes($recording);[IO.File]::WriteAllBytes($truncated,$bytes[0..($bytes.Length-2)])
    & $exe summary $truncated $declared 2>&1 | Out-Null;if($LASTEXITCODE -ne 2){throw 'Truncation must refuse summary'}
    # Reuse the actual muted Update fixture's 360-row V3 capture. V2 golden bytes
    # stay unchanged; V3 must not be labelled V2 or mistaken for native admission.
    $muted=Join-Path $sourceRun ('muted-legacy/Dbce/StagePlayback/outrun-force/'+('b'*32)+'/signals.osig')
    $mutedLines=@(& $exe summary $muted ('a'*40));if($LASTEXITCODE -ne 0){throw 'V3 production-linked summary failed'}
    $mutedJson=Join-Path $out 'software-command-summary.json'
    [IO.File]::WriteAllText($mutedJson,($mutedLines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    $mutedResult=Get-Content $mutedJson -Raw|ConvertFrom-Json
    if($mutedResult.recording_format -cne 'experimental-DBCEORS3-v3' -or $mutedResult.frame_count -ne 360 -or
       $mutedResult.recording_sha256 -cne (Get-FileHash $muted).Hash.ToLowerInvariant() -or
       $mutedResult.software_capture.admission -cne 'virtual' -or $mutedResult.software_capture.native_loaded -or
       $mutedResult.software_capture.speed_units -cne 'raw-field-1c4-unqualified' -or
       $mutedResult.software_capture.last_game_update -le $mutedResult.software_capture.first_game_update -or
       $mutedResult.constant.nonzero_requests -le 0 -or $mutedResult.periodic.request_count -ne 0){throw 'V3 format, identity, context or request contract lost'}
    if($result.PSObject.Properties.Name -contains 'software_capture'){throw 'V2 falsely labelled as software capture'}
    Copy-Item -LiteralPath $muted -Destination (Join-Path $out 'software-recording.osig')
    $catalogFiles=@('synthetic-input.csv','synthetic-recording.osig','metric-definitions.json','command-summary.json','software-recording.osig','software-command-summary.json') | ForEach-Object {
        $path=Join-Path $out $_;[ordered]@{name=$_;bytes=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}
    }
    $sourceFiles=@('src/hooks_dinputffb.cpp','src/ffb_calculation.inl','src/signal_recording.inl','src/signal_recording_codec.hpp','tools/tests/signal_calculation_fixture.cpp','tools/tests/signal_recording_fixture.cpp','tools/tests/signal_command_summary.cpp','tools/tests/Test-CommandSummary.ps1','tools/tests/data/command-metric-definitions-v1.json') | ForEach-Object {
        [ordered]@{path=$_;gitBlob=(& git -C $repo hash-object -- (Join-Path $repo $_)).Trim();workingFileSha256=(Get-FileHash -LiteralPath (Join-Path $repo $_)).Hash.ToLowerInvariant()}
    }
    [ordered]@{schema='dbce.outrun2006.synthetic-command-evidence';version=1;syntheticOnly=$true;physicalOutput=$false;baseProductionCommit='86599699ab3926bef796413d266a9090158383c9';adapterCheckoutCommit=(& git -C $repo rev-parse HEAD).Trim();sourceDirty=[bool](& git -C $repo status --porcelain);declaredCalculationRevision=$declared;declaredRevisionAuthenticated=$false;generator='Unmodified existing SelfTest calculation/record/read/replay scenario; fixture-only main opt-out for adapter reuse';compiler=@{visualStudio=$vs;toolset=(Get-Content -LiteralPath (Join-Path $vs 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt')).Trim();executableSha256=(Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant();nativeOutput='none; memory-only sinks'};files=@($catalogFiles);sourceFiles=@($sourceFiles);validated=@('Existing legacy golden and recorder regressions','Exact production recalculation before summary','Repeated summary and SHA256/golden equality','Eight metric arithmetic/boundary cases','Provenance, changed expected requests, checksum and truncation refusal');limits='Request-domain summary only; not full-session or private capture, post-strength envelope, effect duty/duration, physical torque, shutdown proof, shared v1 case schema or cross-game feel equivalence'} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $out 'hash-catalog.json')
    Write-Host "PASS: bounded production-linked synthetic command summary, catalog and metric definitions; existing recording reused. Evidence: $out"
}finally{Pop-Location}
