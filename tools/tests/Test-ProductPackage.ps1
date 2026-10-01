[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $root ('build/product-package-fixture-' + [guid]::NewGuid().ToString('N'))
$runtime = Join-Path $fixture 'synthetic-runtime'
$out = Join-Path $fixture 'package'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
foreach ($name in 'dinput8.dll','WheelFfb.dll','force-profiles.ini') { [IO.File]::WriteAllText((Join-Path $runtime $name), "synthetic $name") }
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
if (-not $failed -or (Test-Path -LiteralPath $blockedOutput)) { throw 'Unresolved provenance must prevent normal package creation' }
Write-Host "PASS: descriptor, licensing and setup payload hashes; missing-feature flags; runtime identity; immutable package. Synthetic DLL text only. Evidence: $fixture"
