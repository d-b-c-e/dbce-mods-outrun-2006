[CmdletBinding()]
param([string]$PackagedDirectory)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $root 'tools/Validate-Package.ps1')
if (-not $PackagedDirectory) { throw 'Supply a package produced by Package-WheelSettings.ps1.' }
$source = (Resolve-Path -LiteralPath $PackagedDirectory).Path
Assert-PackageInventory $source
$fixture = Join-Path $root ('build/inventory-fixture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$script:checks = 1
function Reject([string]$Label, [scriptblock]$Mutation, [string]$Expected) {
    $package = Join-Path $fixture $Label
    Copy-Item -LiteralPath $source -Destination $package -Recurse
    $manifestPath = Join-Path $package 'package-manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    & $Mutation $package $manifest
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath
    $failed = $false
    try { Assert-PackageInventory $package }
    catch { $failed = $_.Exception.Message -like $Expected }
    if (-not $failed) { throw "Refusal failed: $Label" }
    # Production installer must refuse before creating a backup or changing state.
    $game = Join-Path $fixture ($Label + '-game')
    New-Item -ItemType Directory -Path $game | Out-Null
    $exe = [byte[]]::new(128)
    $exe[0]=0x4d; $exe[1]=0x5a; $exe[0x3c]=0x60
    $exe[0x60]=0x50; $exe[0x61]=0x45; $exe[0x64]=0x4c; $exe[0x65]=0x01
    [IO.File]::WriteAllBytes((Join-Path $game 'OR2006C2C.EXE'), $exe)
    $failed = $false
    try { & (Join-Path $root 'tools/Install-WheelSettings.ps1') -GameDirectory $game -PackageDirectory $package | Out-Null }
    catch { $failed = $_.Exception.Message -like $Expected }
    if (-not $failed -or (Test-Path -LiteralPath (Join-Path $game '.wheel-settings-backups')) -or (Test-Path -LiteralPath (Join-Path $game 'dinput8.dll'))) { throw "Installer refusal changed state: $Label" }
    $script:checks += 2
    return $package
}
Reject 'nested-tamper' { param($p,$m) [IO.File]::AppendAllText((Join-Path $p 'provenance/VERSION'),'tamper') } 'Package hash mismatch: provenance/VERSION'
Reject 'missing-notice' { param($p,$m) Remove-Item -LiteralPath (Join-Path $p 'third-party/notices/ogg-BSD.txt') } 'Missing required package file: third-party/notices/ogg-BSD.txt'
Reject 'unlisted-file' { param($p,$m) [IO.File]::WriteAllText((Join-Path $p 'provenance/unlisted.txt'),'unexpected') } 'Unlisted package file: provenance/unlisted.txt'
Reject 'duplicate-entry' { param($p,$m) $m.files = @($m.files) + @($m.files[0]) } 'Duplicate package entry:*'
Reject 'unsafe-path' { param($p,$m) $m.files[0].name='../outside.txt' } 'Unsafe package path:*'
Reject 'removed-component' {
    param($p,$m)
    $path = Join-Path $p 'third-party/index.json'
    $index = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    $index.components = @($index.components | Where-Object id -ne 'toolkit')
    $index | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path
    ($m.files | Where-Object name -eq 'third-party/index.json').sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()
} 'Missing required notice component: toolkit'
Reject 'rehash-notice-tamper' {
    param($p,$m)
    $path = Join-Path $p 'third-party/notices/ogg-BSD.txt'
    [IO.File]::AppendAllText($path,'tamper')
    ($m.files | Where-Object name -eq 'third-party/notices/ogg-BSD.txt').sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()
} 'Notice hash mismatch: third-party/notices/ogg-BSD.txt'
# Rehashed/cataloged private paths must fail regardless of reviewOnly. Also
# exercise the real packaging entry point: it must leave no output directory.
foreach ($review in $true,$false) {
    foreach ($privateName in 'captures/private-session.jsonl','roms/game.rom','owner-config.json','logs/runtime.log','third-party/notices/unknown.txt') {
        $script:injectedName=$privateName
        $script:injectedReview=$review
        $label='private-'+$review+'-'+$privateName.Replace('/','-')
        $p = Reject $label {
            param($p,$m)
            $file=Join-Path $p $script:injectedName
            New-Item -ItemType Directory -Path (Split-Path -Parent $file) -Force | Out-Null
            [IO.File]::WriteAllText($file,'private fixture; never real user data')
            $m.reviewOnly=$script:injectedReview
            # Model an attacker recataloging all metadata, including cleanliness.
            $m.sourceDirty=$false
            $m.files=@($m.files)+@([pscustomobject]@{name=$script:injectedName;sha256=(Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant()})
        } ('Disallowed private or unknown package path: '+$privateName)
        foreach ($reviewSwitch in $true,$false) {
            $output=Join-Path $fixture ($label+'-output-'+$reviewSwitch)
            $failed=$false
            try { & (Join-Path $root 'tools/Package-WheelSettings.ps1') -RuntimePackageDirectory $p -OutputDirectory $output -ReviewOnly:$reviewSwitch | Out-Null }
            catch { $failed=$_.Exception.Message -like 'Disallowed private or unknown package path:*' }
            if (-not $failed -or (Test-Path -LiteralPath $output)) { throw "Packaging privacy refusal failed: $label" }
            $script:checks++
        }
    }
}
$sourceFixture=Join-Path $fixture 'source-with-private-notice'
New-Item -ItemType Directory -Path (Join-Path $sourceFixture 'tools') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'third-party') -Destination $sourceFixture -Recurse
foreach($file in 'Package-WheelSettings.ps1','Validate-Package.ps1') { Copy-Item -LiteralPath (Join-Path $root ('tools/'+$file)) -Destination (Join-Path $sourceFixture 'tools') }
[IO.File]::WriteAllText((Join-Path $sourceFixture 'third-party/notices/user.ini'),'private fixture')
foreach($reviewSwitch in $true,$false) {
    $output=Join-Path $fixture ('source-private-output-'+$reviewSwitch)
    $failed=$false
    try { & (Join-Path $sourceFixture 'tools/Package-WheelSettings.ps1') -RuntimePackageDirectory $source -OutputDirectory $output -ReviewOnly:$reviewSwitch | Out-Null }
    catch { $failed=$_.Exception.Message -eq 'Disallowed private or unknown package path: third-party/notices/user.ini' }
    if(-not $failed -or (Test-Path -LiteralPath $output)){throw 'Private source notice must fail before packaging output'}
    $script:checks++
}
$index = Assert-ThirdPartyNotices $source
if (@($index.distributionBlockers).Count -gt 0) {
    $failed=$false
    try { Assert-PackageInventory $source -ForDistribution }
    catch { $failed=$_.Exception.Message -like 'Distribution blocked:*' }
    if (-not $failed) { throw 'Unresolved identity must block distribution' }
    $script:checks++
}
Write-Host "PASS: $script:checks checks; recursive inventory and installer pre-write refusal, required notices, independent notice hashes, distribution blocker. Actual package payload not executed. Evidence: $fixture"
