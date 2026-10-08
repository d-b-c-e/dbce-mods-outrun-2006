[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $root ('build/product-package-fixture-' + [guid]::NewGuid().ToString('N'))
$runtime = Join-Path $fixture 'synthetic-runtime'
$out = Join-Path $fixture 'package'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
foreach ($name in 'dinput8.dll','WheelFfb.dll','force-profiles.ini') { [IO.File]::WriteAllText((Join-Path $runtime $name), "synthetic $name") }
Copy-Item -LiteralPath (Join-Path $root 'lib/toolkit/native/x86/WheelFfb.dll') -Destination (Join-Path $runtime 'WheelFfb.dll')
$files = @(Get-ChildItem -LiteralPath $runtime -File | ForEach-Object { @{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()} })
@{schemaVersion=1;architecture='x86';sourceDirty=$false;sourceCommit='synthetic-runtime';runtimeSourceCommit='synthetic-runtime';baselineToolkit='v0.8.0';nativeToolkitOverride='v0.13.0';nativeComponent='0.6.0';files=$files} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $runtime 'package-manifest.json')
& (Join-Path $root 'tools/Package-WheelSettings.ps1') -RuntimePackageDirectory $runtime -OutputDirectory $out -ReviewOnly | Out-Null
$manifest = Get-Content -LiteralPath (Join-Path $out 'package-manifest.json') -Raw | ConvertFrom-Json
if ($manifest.runtimeSourceCommit -ne 'synthetic-runtime') { throw 'Repack lost runtime identity' }
foreach ($name in 'product.json','UNIFIED-PRODUCT.md','LICENSE.md','Install.ps1','Install.bat') {
    $entry = @($manifest.files | Where-Object name -eq $name)
    if ($entry.Count -ne 1 -or $entry[0].sha256 -ne (Get-FileHash -LiteralPath (Join-Path $out $name)).Hash.ToLowerInvariant()) { throw "Missing or unhashed package file: $name" }
}
$product = Get-Content -LiteralPath (Join-Path $out 'product.json') -Raw | ConvertFrom-Json
if ($product.canonicalRepository -ne 'd-b-c-e/dbce-mods-outrun-2006' -or $product.repositoryUrl -ne 'https://github.com/d-b-c-e/dbce-mods-outrun-2006' -or $product.repositoryId -ne 1163595970 -or $product.legacyRepositoryAliases -notcontains 'd-b-c-e/OutRun2006Tweaks-FFB') { throw 'Canonical repository identity or legacy alias lost' }
if ($product.productId -ne 'outrun2006-c2c-pc' -or $product.runtime -ne 'OutRun2006Tweaks-FFB' -or $product.executable -ne 'OR2006C2C.EXE' -or $product.settings -ne 'OutRun2006Tweaks.ini') { throw 'Stable product/runtime compatibility changed' }
if ($product.repositoryRenameStatus -ne 'completed; existing repository renamed in place on 2026-10-01') { throw 'Verified completed in-place rename status lost' }
foreach ($name in 'tripleScreen','sessionRecording','drivingInputPlayback') {
    if ($product.features.$name.implemented -or $product.features.$name.accepted) { throw "Unsupported feature claimed: $name" }
}
$failed = $false
try { & (Join-Path $root 'tools/Package-WheelSettings.ps1') -RuntimePackageDirectory $runtime -OutputDirectory $out -ReviewOnly | Out-Null }
catch { $failed = $_.Exception.Message -like 'Choose a new package directory*' }
if (-not $failed) { throw 'Existing package must remain immutable' }
$blockedOutput = Join-Path $fixture 'distribution-blocked'
$failed = $false
try { & (Join-Path $root 'tools/Package-WheelSettings.ps1') -RuntimePackageDirectory $runtime -OutputDirectory $blockedOutput | Out-Null }
catch { $failed = $_.Exception.Message -like 'Distribution blocked:*' }
if (-not $failed -or (Test-Path -LiteralPath $blockedOutput)) { throw 'Outstanding acceptance/shutdown gates must prevent normal package creation' }
$index = Get-Content -LiteralPath (Join-Path $out 'third-party/index.json') -Raw | ConvertFrom-Json
$directx = @($index.components | Where-Object id -eq 'directx-headers')[0]
if ($index.legalClearance -or $manifest.distributionReady -or @($index.distributionBlockers).Count -ne 2) { throw 'Remaining review gates or clearance status changed' }
if (($index.distributionBlockers -join ' ') -match 'upstream.*unresolved|DirectX.*needs review') { throw 'Resolved factual DirectX gate retained' }
if (($index.distributionBlockers -join ' ') -notmatch 'independent consumer integration review' -or ($index.distributionBlockers -join ' ') -notmatch 'Physical driving') { throw 'Acceptance or shutdown integration gate lost' }
$native = Get-Content -LiteralPath (Join-Path $out 'provenance/NATIVE-PROVENANCE.json') -Raw | ConvertFrom-Json
if ($native.sourceCommit -ne '50ba139bcaee14aee080abe438d6beec4f6a2b47' -or $native.exportCount -ne 41 -or $native.nativeVersion -ne '0.6.0' -or $native.buildEvidence.generalBitReproducibilityProven) { throw 'Matched native source/ABI provenance lost or overstated' }
if ($native.files[1].sha256 -ne (Get-FileHash -LiteralPath (Join-Path $root 'lib/toolkit/native/x86/WheelFfb.dll')).Hash.ToLowerInvariant()) { throw 'Native provenance does not match vendored bytes' }
if ($directx.provenance.status -ne 'verified content match; original importer checkout not claimed' -or $directx.provenance.licenseReceipt.byteIdentical -or $directx.provenance.licenseReceipt.retainedBytes -ne 1074) { throw 'DirectX content-match or license-normalization receipt lost' }
$defaultResult = @(& (Join-Path $root 'tools/Package-WheelSettings.ps1') -RuntimePackageDirectory $runtime -ReviewOnly)
$defaultZip = $defaultResult[-1].Path
if ((Split-Path -Leaf $defaultZip) -notmatch '^dbce-mods-outrun-2006-[0-9]{8}-[0-9]{6}\.zip$') { throw 'Canonical default prefix or existing timestamp semantics changed' }
$defaultManifest = Get-Content -LiteralPath (Join-Path ([IO.Path]::ChangeExtension($defaultZip,$null)) 'package-manifest.json') -Raw | ConvertFrom-Json
if ($defaultManifest.runtimeSourceCommit -ne 'synthetic-runtime') { throw 'Default name lost source-qualified runtime identity' }
$legacyOutput = Join-Path $fixture 'wheel-settings-explicit historical path'
& (Join-Path $root 'tools/Package-WheelSettings.ps1') -RuntimePackageDirectory $runtime -OutputDirectory $legacyOutput -ReviewOnly | Out-Null
if (-not (Test-Path -LiteralPath ($legacyOutput+'.zip'))) { throw 'Explicit historical output path was renamed' }
# A self-consistent frozen inventory must still agree with the native provenance
# which packaging is about to ship. Never relabel stale native bytes as current.
[IO.File]::WriteAllText((Join-Path $runtime 'WheelFfb.dll'), 'stale native fixture')
$frozen = Get-Content (Join-Path $runtime 'package-manifest.json') -Raw | ConvertFrom-Json
($frozen.files | Where-Object name -eq 'WheelFfb.dll').sha256 = (Get-FileHash (Join-Path $runtime 'WheelFfb.dll')).Hash.ToLowerInvariant()
$frozen | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $runtime 'package-manifest.json')
foreach ($mode in 'frozen','build') {
    $refusedOut = Join-Path $fixture ('stale-native-' + $mode)
    $failed = $false
    try {
        if ($mode -eq 'frozen') { & (Join-Path $root 'tools/Package-WheelSettings.ps1') -RuntimePackageDirectory $runtime -OutputDirectory $refusedOut -ReviewOnly | Out-Null }
        else { & (Join-Path $root 'tools/Package-WheelSettings.ps1') -RuntimeBuildDirectory $runtime -OutputDirectory $refusedOut -ReviewOnly | Out-Null }
    } catch { $failed = $_.Exception.Message -like 'Runtime WheelFfb.dll differs from native source provenance*' }
    if (-not $failed -or (Test-Path -LiteralPath $refusedOut)) { throw "Stale native $mode was packaged or changed the target" }
}
Write-Host "PASS: descriptor, licensing/setup hashes, missing-feature flags, runtime identity, immutable package, stale native build/frozen refusal. Fake proxy; real native copied but never loaded. Evidence: $fixture"
