#Requires -Version 7.0
<#
.SYNOPSIS
Arms a bounded software-only legacy-force capture; never launches a game.
.DESCRIPTION
Launch the exact installed review package as a child with DBCE_OUTRUN_SIGNAL_MUTE=legacy.
Before input, verify that child's PID/start time, proxy bytes and fresh PROCESS MUTE log.
This request cannot mute an ordinarily launched game. A complete .osig stores original
software calculations, virtual constant-sink admission and checkpoints, not wheel output.
Use only offline play; the caller holds the shared rig lease and restores owner files.
The package manifest is required to bind the source claim to the installed DLL bytes.
#>
[CmdletBinding(DefaultParameterSetName='Arm')]
param(
 [Parameter(Mandatory,ParameterSetName='Arm')][string]$GameDir,
 [Parameter(Mandatory,ParameterSetName='Arm')][string]$RuntimePackageDirectory,
 [Parameter(Mandatory)][string]$LeaseToken,
 [Parameter(ParameterSetName='Arm')][ValidateRange(10,120)][int]$Seconds=60,
 [Parameter(Mandatory,ParameterSetName='Stop')][ValidatePattern('^[a-f0-9]{32}$')][string]$StopId
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$slot=Join-Path $env:LOCALAPPDATA 'dbce/test-slot.txt'
if (-not (Test-Path -LiteralPath $slot) -or (Get-Item -LiteralPath $slot).LastWriteTimeUtc -le [DateTime]::UtcNow.AddHours(-2) -or
 (Get-Item -LiteralPath $slot).LastWriteTimeUtc -gt [DateTime]::UtcNow -or
 (Get-Content -LiteralPath $slot -Raw).TrimEnd("`r","`n") -cne $LeaseToken) {throw 'An exact current rig lease is required'}
if($LeaseToken -cnotmatch '^[\x20-\x7e]{1,512}$'){throw 'Lease must fit the capture request'}
$root=Join-Path $env:LOCALAPPDATA 'Dbce/StagePlayback/outrun-force'
if($PSCmdlet.ParameterSetName -eq 'Stop') {
 $directory=Join-Path $root $StopId
 if(-not (Test-Path -LiteralPath (Join-Path $directory 'request.txt'))){throw 'No accepted capture with this ID'}
 $accepted=[IO.File]::ReadAllLines((Join-Path $directory 'request.txt'))
 if(-not ($accepted -ccontains "leaseToken=$LeaseToken")){throw 'This capture belongs to another lease'}
 if(Test-Path -LiteralPath (Join-Path $directory 'outcome.txt')){return [pscustomobject]@{id=$StopId;alreadyFinished=$true;result=$directory}}
 [IO.File]::WriteAllText((Join-Path $directory 'stop.txt'),"stop`n")
 return [pscustomobject]@{id=$StopId;stopRequested=$true;result=$directory}
}
$game=(Resolve-Path -LiteralPath $GameDir).Path
$package=(Resolve-Path -LiteralPath $RuntimePackageDirectory).Path
$manifest=Get-Content -LiteralPath (Join-Path $package 'package-manifest.json') -Raw|ConvertFrom-Json
if($manifest.schemaVersion -ne 2 -or $manifest.architecture -ne 'x86' -or $manifest.sourceDirty -isnot [bool] -or $manifest.sourceDirty){throw 'Use a clean x86 runtime package manifest'}
$source=$manifest.runtimeSourceCommit
if($source -cnotmatch '^[a-f0-9]{40}$'){throw 'Missing exact runtime source'}
$entry=@($manifest.files|Where-Object name -ceq 'dinput8.dll')
if($entry.Count -ne 1 -or $entry[0].sha256 -cnotmatch '^[a-f0-9]{64}$'){throw 'Missing exact proxy inventory'}
$proxy=Join-Path $game 'dinput8.dll'
$proxySha=(Get-FileHash -LiteralPath $proxy).Hash.ToLowerInvariant()
if($proxySha -cne $entry[0].sha256 -or (Get-FileHash -LiteralPath (Join-Path $package 'dinput8.dll')).Hash.ToLowerInvariant() -cne $proxySha){throw 'Package and installed proxy differ'}
$policy=Get-Content -LiteralPath (Join-Path $repo 'src/host_lifecycle_policy.hpp') -Raw
if($policy -notmatch 'ExactDiskSha256\[\] = "([0-9a-f]{64})"'){throw 'Exact game policy unavailable'}
$gameSha=$Matches[1]
if((Get-FileHash -LiteralPath (Join-Path $game 'OR2006C2C.EXE')).Hash.ToLowerInvariant() -cne $gameSha){throw 'Unsupported game executable'}
if(-not (Select-String -LiteralPath $proxy -SimpleMatch -Pattern 'SignalCapture: armed' -Quiet)){throw 'This proxy has no software-force capture'}
[IO.Directory]::CreateDirectory($root)|Out-Null
$request=Join-Path $root 'request.txt'
if(Test-Path -LiteralPath $request){throw 'Inspect the existing request; it will not be overwritten'}
$id=[guid]::NewGuid().ToString('N');$expires=[DateTimeOffset]::UtcNow.AddMinutes(5).ToUnixTimeSeconds()
$provenance=[ordered]@{schema='outrun2006.software-force-arm@1';id=$id;seconds=$Seconds;sourceCommit=$source;proxySha256=$proxySha;
 gameSha256=$gameSha;runtimePackage=$package;packageManifestSha256=(Get-FileHash -LiteralPath (Join-Path $package 'package-manifest.json')).Hash;
 physicalOutput=$false;route='constant-fallback-software';expiresUnix=$expires}
$provenance|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root "armed-$id.json") -Encoding utf8
$temp=Join-Path $root "$id.request.tmp"
$lines=@('action=record-legacy-muted',"id=$id","seconds=$Seconds","expiresUnix=$expires","gameSha256=$gameSha","proxySha256=$proxySha","sourceCommit=$source","leaseToken=$LeaseToken")
[IO.File]::WriteAllText($temp,($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
[IO.File]::Move($temp,$request,$false)
[pscustomobject]@{id=$id;result=(Join-Path $root $id);seconds=$Seconds;launchesGame=$false;
 next='Only a verified child launched with DBCE_OUTRUN_SIGNAL_MUTE=legacy can claim this. Verify its fresh PROCESS MUTE log before navigating offline.'}
