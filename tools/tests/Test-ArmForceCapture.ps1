#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ExactExecutable)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$arm=Join-Path $repo 'tools/Arm-ForceCapture.ps1'
$out=Join-Path $repo ('build/arm-force-fixture-'+[guid]::NewGuid().ToString('N'))
foreach($name in @('appdata/dbce','game','package')){[IO.Directory]::CreateDirectory((Join-Path $out $name))|Out-Null}
$game=Join-Path $out 'game';$package=Join-Path $out 'package';$slot=Join-Path $out 'appdata/dbce/test-slot.txt'
# Only copies and dummy proxy bytes. No launch, DLL load, live lease or device.
Copy-Item -LiteralPath $ExactExecutable -Destination (Join-Path $game 'OR2006C2C.EXE')
$proxyText='fixture bytes SignalCapture: armed; not a DLL'
[IO.File]::WriteAllText((Join-Path $game 'dinput8.dll'),$proxyText)
Copy-Item -LiteralPath (Join-Path $game 'dinput8.dll') -Destination $package
$manifest=[ordered]@{schemaVersion=2;architecture='x86';sourceDirty=$false;runtimeSourceCommit=('a'*40);
 files=@(@{name='dinput8.dll';sha256=(Get-FileHash (Join-Path $game 'dinput8.dll')).Hash.ToLowerInvariant()})}
function Save-Manifest {$manifest|ConvertTo-Json -Depth 5|Set-Content (Join-Path $package 'package-manifest.json')}
Save-Manifest
$lease='fixture owner lease=00112233445566778899aabbccddeeff'
[IO.File]::WriteAllText($slot,$lease+"`r`n")
$savedLocal=$env:LOCALAPPDATA;$env:LOCALAPPDATA=Join-Path $out 'appdata';$script:checks=0
function Check($okay,$why) {if(-not $okay){throw $why};$script:checks++}
function Refuses([scriptblock]$action,$why) {$threw=$false;try{& $action|Out-Null}catch{$threw=$true};Check $threw $why}
function Arm {& $arm -GameDir $game -RuntimePackageDirectory $package -LeaseToken $lease -Seconds 10}
try {
 Refuses {& $arm -GameDir $game -RuntimePackageDirectory $package -LeaseToken 'other'} 'wrong lease accepted'
 [IO.File]::SetLastWriteTimeUtc($slot,[DateTime]::UtcNow.AddHours(-3));Refuses {Arm} 'stale lease accepted'
 [IO.File]::SetLastWriteTimeUtc($slot,[DateTime]::UtcNow.AddMinutes(3));Refuses {Arm} 'future lease accepted'
 [IO.File]::SetLastWriteTimeUtc($slot,[DateTime]::UtcNow)
 $manifest.sourceDirty=$true;Save-Manifest;Refuses {Arm} 'dirty source accepted'
 $manifest.Remove('sourceDirty');Save-Manifest;Refuses {Arm} 'missing dirty flag accepted'
 $manifest.sourceDirty=$false;$manifest.architecture='x64';Save-Manifest;Refuses {Arm} 'wrong architecture accepted'
 $manifest.architecture='x86';$manifest.runtimeSourceCommit='A'*40;Save-Manifest;Refuses {Arm} 'bad source identity accepted'
 $manifest.runtimeSourceCommit='a'*40;Save-Manifest
 [IO.File]::WriteAllText((Join-Path $game 'dinput8.dll'),'changed');Refuses {Arm} 'installed mismatch accepted'
 [IO.File]::WriteAllText((Join-Path $game 'dinput8.dll'),$proxyText)
 $exe=Join-Path $game 'OR2006C2C.EXE';$bytes=[IO.File]::ReadAllBytes($exe);$bytes[0]=$bytes[0] -bxor 1;[IO.File]::WriteAllBytes($exe,$bytes)
 Refuses {Arm} 'wrong executable accepted';Copy-Item -LiteralPath $ExactExecutable -Destination $exe -Force
 $accepted=Arm
 Check ($accepted.launchesGame -eq $false -and $accepted.seconds -eq 10) 'arm result'
 $root=Split-Path $accepted.result -Parent;$request=Join-Path $root 'request.txt'
 $hash=(Get-FileHash -LiteralPath $request).Hash
 $lines=[IO.File]::ReadAllLines($request)
 Check ($lines.Count -eq 8 -and $lines -ccontains "leaseToken=$lease") 'request identity/shape'
 Refuses {Arm} 'pending request overwritten'
 Check ((Get-FileHash -LiteralPath $request).Hash -ceq $hash) 'pending request changed'
 $receipt=Get-Content (Join-Path $root ('armed-'+$accepted.id+'.json')) -Raw|ConvertFrom-Json
 Check ($receipt.sourceCommit -ceq ('a'*40) -and $receipt.physicalOutput -eq $false) 'provenance receipt'
 [IO.Directory]::CreateDirectory($accepted.result)|Out-Null
 Move-Item -LiteralPath $request -Destination (Join-Path $accepted.result 'request.txt')
 $stop=& $arm -StopId $accepted.id -LeaseToken $lease
 Check ($stop.stopRequested -and (Test-Path (Join-Path $accepted.result 'stop.txt'))) 'stop request'
 [IO.File]::WriteAllText((Join-Path $accepted.result 'outcome.txt'),"outcome=incomplete`n")
 Check ((& $arm -StopId $accepted.id -LeaseToken $lease).alreadyFinished) 'finished stop'
 Refuses {& $arm -StopId ('f'*32) -LeaseToken $lease} 'missing accepted capture stopped'
 [IO.File]::WriteAllText((Join-Path $accepted.result 'request.txt'),"leaseToken=other`n")
 Refuses {& $arm -StopId $accepted.id -LeaseToken $lease} 'another lease capture stopped'
 "PASS $script:checks arm/stop boundary checks; dummy files only: $out"
} finally {$env:LOCALAPPDATA=$savedLocal}
