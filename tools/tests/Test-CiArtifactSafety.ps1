[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $repo 'tools/ci/Ci-ArtifactPolicy.ps1')
$out=Join-Path $repo ('build/ci-safety-fixture-'+[guid]::NewGuid().ToString('N'))
$runtime=Join-Path $out 'synthetic-runtime'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
$head=(& git -C $repo rev-parse HEAD).Trim()
$stub=[byte[]]::new(128)
$stub[0]=0x4d;$stub[1]=0x5a;$stub[0x3c]=0x60;$stub[0x60]=0x50;$stub[0x61]=0x45;$stub[0x64]=0x4c;$stub[0x65]=1;$stub[0x77]=0x20
[IO.File]::WriteAllBytes((Join-Path $runtime 'dinput8.dll'),$stub)
Copy-Item -LiteralPath (Join-Path $repo 'lib/toolkit/native/x86/WheelFfb.dll') -Destination $runtime
Copy-Item -LiteralPath (Join-Path $repo 'lib/toolkit/profiles/force-profiles.ini') -Destination $runtime
$files=@(Get-ChildItem -LiteralPath $runtime -File | ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
@{schemaVersion=1;architecture='x86';sourceDirty=$false;sourceCommit=$head;runtimeSourceCommit=$head;baselineToolkit='v0.8.0';nativeToolkitOverride='v0.13.0';nativeComponent='0.6.0';files=$files} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runtime 'package-manifest.json')
$source=Join-Path $out 'synthetic-package'
& (Join-Path $repo 'tools/Package-WheelSettings.ps1') -RuntimePackageDirectory $runtime -OutputDirectory $source -ReviewOnly | Out-Null
# Synthetic fixture only: tests may run with an uncommitted candidate. No actual
# built runtime is relabeled or uploaded; the 128-byte stub is never executed.
$manifest=Get-Content -LiteralPath (Join-Path $source 'package-manifest.json') -Raw | ConvertFrom-Json
$manifest.sourceDirty=$false
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $source 'package-manifest.json')
$script:checks=0
function Check($condition,$reason){if(-not $condition){throw $reason};$script:checks++}
$artifact=Join-Path $out 'good-artifact'
& (Join-Path $repo 'tools/ci/Prepare-CiArtifact.ps1') -PackageDirectory $source -ArtifactDirectory $artifact
Assert-CiArtifactDirectory $artifact; Check $true 'valid artifact failed'
function Reject-Package([string]$Name,[scriptblock]$Mutation) {
    $package=Join-Path $out $Name; Copy-Item -LiteralPath $source -Destination $package -Recurse
    $m=Get-Content -LiteralPath (Join-Path $package 'package-manifest.json') -Raw | ConvertFrom-Json
    & $Mutation $package $m
    $m | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $package 'package-manifest.json')
    $output=Join-Path $out ($Name+'-artifact');$refused=$false
    try {& (Join-Path $repo 'tools/ci/Prepare-CiArtifact.ps1') -PackageDirectory $package -ArtifactDirectory $output | Out-Null}catch{$refused=$true}
    Check ($refused -and -not (Test-Path -LiteralPath $output)) "Unsafe package not refused before staging: $Name"
}
foreach($name in 'OR2006C2C.EXE','captures/private-session.jsonl','logs/runtime.log','owner-config.json','third-party/notices/unknown.txt') {
    $script:injected=$name
    Reject-Package ($name.Replace('/','-')) {
        param($p,$m)
        $file=Join-Path $p $script:injected; New-Item -ItemType Directory -Path (Split-Path -Parent $file) -Force | Out-Null
        [IO.File]::WriteAllText($file,'synthetic private marker')
        $m.files=@($m.files)+@([pscustomobject]@{name=$script:injected;sha256=(Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant()})
    }
}
Reject-Package 'exe-renamed-as-dll' {
    param($p,$m)
    $file=Join-Path $p 'dinput8.dll';$bytes=[IO.File]::ReadAllBytes($file);$bytes[0x77]=0;[IO.File]::WriteAllBytes($file,$bytes)
    ($m.files | Where-Object name -eq 'dinput8.dll').sha256=(Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant()
}
Reject-Package 'owner-ini-as-default' {
    param($p,$m)
    $file=Join-Path $p 'OutRun2006Tweaks.ini';[IO.File]::WriteAllText($file,'synthetic owner GUID marker')
    ($m.files | Where-Object name -eq 'OutRun2006Tweaks.ini').sha256=(Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant()
}
Reject-Package 'dirty-source' {param($p,$m) $m.sourceDirty=$true}
Reject-Package 'nonreview' {param($p,$m) $m.reviewOnly=$false}
Reject-Package 'wrong-source' {param($p,$m) $m.runtimeSourceCommit='0000000000000000000000000000000000000000'}
Reject-Package 'private-metadata' {param($p,$m) $m | Add-Member NoteProperty ownerPath 'synthetic private marker'}
function Reject-Artifact([string]$Name,[scriptblock]$Mutation) {
    $directory=Join-Path $out $Name;Copy-Item -LiteralPath $artifact -Destination $directory -Recurse
    & $Mutation $directory
    $refused=$false;try{Assert-CiArtifactDirectory $directory}catch{$refused=$true}
    Check $refused "Unsafe final artifact admitted: $Name"
}
Reject-Artifact 'extra-file' {param($d) [IO.File]::WriteAllText((Join-Path $d 'OR2006C2C.EXE'),'synthetic marker')}
Reject-Artifact 'missing-upload' {param($d) Remove-Item -LiteralPath (Join-Path $d 'outrun2006-ci-review.zip')}
Reject-Artifact 'rehash-private-zip-entry' {
    param($d)
    $zip=Join-Path $d 'outrun2006-ci-review.zip';$archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Update)
    try {$entry=$archive.CreateEntry('OR2006C2C.EXE');$writer=[IO.StreamWriter]::new($entry.Open());try{$writer.Write('synthetic marker')}finally{$writer.Dispose()}}finally{$archive.Dispose()}
    $report=Get-Content -LiteralPath (Join-Path $d 'artifact-inventory.json') -Raw | ConvertFrom-Json
    $report.archiveSha256=(Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant()
    $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $d 'artifact-inventory.json')
}
Reject-Artifact 'private-report-metadata' {
    param($d)
    $report=Get-Content -LiteralPath (Join-Path $d 'artifact-inventory.json') -Raw | ConvertFrom-Json
    $report | Add-Member NoteProperty ownerPath 'synthetic private marker'
    $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $d 'artifact-inventory.json')
}
Reject-Artifact 'archive-hash-mismatch' {
    param($d)
    $report=Get-Content -LiteralPath (Join-Path $d 'artifact-inventory.json') -Raw | ConvertFrom-Json
    $report.archiveSha256='0'*64;$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $d 'artifact-inventory.json')
}
$workflow=[IO.File]::ReadAllText((Join-Path $repo '.github/workflows/build.yml'))
Check ($workflow -match '(?m)^on:\s*\r?\n  workflow_dispatch:\s*\r?\n' -and $workflow -notmatch '(?m)^  (push|pull_request|pull_request_target):') 'Workflow must stay manual-only'
Check ($workflow -notmatch 'OR2006C2C|Invoke-WebRequest|build/bin/' -and $workflow -notmatch '(?m)^\s+path:.*\*') 'Workflow reacquires game input or broadly uploads build tree'
Check ($workflow -match '(?s)path: \|\s+build/ci-artifact/outrun2006-ci-review\.zip\s+build/ci-artifact/artifact-inventory\.json\s+if-no-files-found: error') 'Upload must use two exact paths and missing-file failure'
Check ($workflow -match 'contents: read' -and $workflow -match 'persist-credentials: false' -and $workflow -match 'if: \$\{\{ success\(\) \}\}') 'Least permissions/success-only upload missing'
$hostPath=Join-Path $PSHOME 'pwsh.exe'
foreach($case in @(@{name='throw';body="throw 'synthetic CI failure'";exit=1},@{name='exit';body='exit 17';exit=17},@{name='expected-native-refusal';body='cmd /c exit 2; if($LASTEXITCODE -ne 2){throw "fixture refused incorrectly"}';exit=0})) {
    $script=Join-Path $out ($case.name+'.ps1');Set-Content -LiteralPath $script -Value $case.body -Encoding ascii
    & $hostPath -NoProfile -NonInteractive -File (Join-Path $repo 'tools/ci/Invoke-CiTest.ps1') -ScriptPath $script *> (Join-Path $out ($case.name+'.log'))
    Check ($LASTEXITCODE -eq $case.exit) "CI child failure exit was masked: $($case.name)"
}
Write-Host "PASS: $script:checks CI safety checks; rehashed private paths, renamed EXE, owner defaults, metadata, ZIP/final-path refusal and failure propagation. Synthetic stub only; no executable or DLL loaded. Evidence: $out"
