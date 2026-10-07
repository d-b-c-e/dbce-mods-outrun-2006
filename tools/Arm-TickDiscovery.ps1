#Requires -Version 7.0
<#
.SYNOPSIS
    Arms one read-only local-car tick discovery window (src/tick_discovery.hpp), or stops an armed one.

.DESCRIPTION
    Writes %LOCALAPPDATA%\Dbce\StagePlayback\outrun-discovery\request.txt atomically. The installed plugin polls it
    once a second (every 60 game updates). It refuses expired, malformed or mismatched requests into
    request.refused.txt + refused.txt. An accepted request is moved into <id>\request.txt, so it cannot arm twice.
    Arming works before launch or while the game runs, for example at an offline start line; the request expires
    after five minutes if never claimed.
    Results: <id>\discovery.tsv, then <id>\outcome.txt (written last). Outcomes are observed, ended (car or stage
    changed), failed, stopped or exit.
    Discovery evidence only: no replay, no writer, no force, input, telemetry or display change. The caller holds
    the rig lease and keeps the session offline.

.EXAMPLE
    .\tools\Arm-TickDiscovery.ps1 -Seconds 60 -LeaseToken $lease.token
    .\tools\Arm-TickDiscovery.ps1 -StopId <id>
#>
[CmdletBinding()]
param(
    [ValidateRange(10, 120)][int]$Seconds = 60,
    [string]$LeaseToken,
    [string]$GameDir = 'E:\Source\_archive\2026-10-04\outrun2006\outrun2006-redux\game',
    [string]$StopId
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$root = Join-Path $env:LOCALAPPDATA 'Dbce\StagePlayback\outrun-discovery'
if ($StopId) {
    if ($StopId -cnotmatch '^[a-f0-9]{32}$') { throw 'Use the exact discovery ID printed when armed' }
    $dir = Join-Path $root $StopId
    if (!(Test-Path -LiteralPath (Join-Path $dir 'request.txt'))) { throw 'That window has not been claimed by the game' }
    if (Test-Path -LiteralPath (Join-Path $dir 'outcome.txt')) { return [pscustomobject]@{ id = $StopId; result = $dir; alreadyFinished = $true } }
    [IO.File]::WriteAllText((Join-Path $dir 'stop.txt'), "stop`n")
    return [pscustomobject]@{ id = $StopId; result = $dir; stopRequested = $true }
}
$policy = Get-Content (Join-Path $repo 'src\host_lifecycle_policy.hpp') -Raw
if ($policy -notmatch 'ExactDiskSha256\[\] = "([0-9a-f]{64})"') { throw 'Cannot read the exact game hash from host_lifecycle_policy.hpp' }
$gameSha = $Matches[1]
$exe = Join-Path $GameDir 'OR2006C2C.EXE'
if ((Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant() -ne $gameSha) { throw "Unsupported OutRun executable: $exe" }
$proxy = Join-Path $GameDir 'dinput8.dll'
if (!(Select-String -LiteralPath $proxy -Pattern 'TickDiscovery: armed' -SimpleMatch -Quiet)) { throw 'The installed dinput8.dll has no tick discovery; install a build that has it' }
$slot = Join-Path $env:LOCALAPPDATA 'dbce\test-slot.txt'
if ((Test-Path $slot) -and (Get-Item $slot).LastWriteTimeUtc -gt [DateTime]::UtcNow.AddHours(-2)) {
    if (!$LeaseToken -or (Get-Content $slot -Raw).TrimEnd("`r", "`n") -cne $LeaseToken) { throw "Coordinate the rig slot first: $(Get-Content $slot -Raw)" }
} elseif ($LeaseToken) { throw 'The supplied runner lease is no longer current' }
[IO.Directory]::CreateDirectory($root) | Out-Null
$request = Join-Path $root 'request.txt'
if (Test-Path -LiteralPath $request) { throw 'A request already exists; inspect it rather than overwriting' }
$id = [guid]::NewGuid().ToString('N')
$expires = [DateTimeOffset]::UtcNow.AddMinutes(5).ToUnixTimeSeconds()
$provenance = [ordered]@{ schema = 'outrun2006.tick-discovery-arm@1'; id = $id; seconds = $Seconds; expiresUnix = $expires
    gameSha256 = $gameSha; proxySha256 = (Get-FileHash -LiteralPath $proxy).Hash.ToLowerInvariant(); gameDir = $GameDir
    source = (git -C $repo rev-parse HEAD); sourceDirty = [bool](git -C $repo status --porcelain) }
$provenance | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root "armed-$id.json") -Encoding utf8
$tmp = Join-Path $root "$id.request.tmp"
[IO.File]::WriteAllLines($tmp, @('action=discover', "id=$id", "seconds=$Seconds", "expiresUnix=$expires", "gameSha256=$gameSha"))
[IO.File]::Move($tmp, $request, $false)
[pscustomobject]@{ id = $id; result = (Join-Path $root $id); seconds = $Seconds; expiresInSeconds = 300; launchesGame = $false
    next = 'Within five minutes the running game claims it into the result folder; outcome.txt appears when the window closes.' }
