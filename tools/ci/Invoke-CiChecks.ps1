[CmdletBinding()]
param([string]$DependencyRoot)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if(-not $DependencyRoot){$DependencyRoot=$repo}
function Run-Check([string]$script,[hashtable]$parameters=@{}) {
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -NonInteractive -File (Join-Path $PSScriptRoot 'Invoke-CiTest.ps1') -ScriptPath (Join-Path $repo $script) -ParametersJson ($parameters | ConvertTo-Json -Compress)
    if($LASTEXITCODE -ne 0){throw "CI check failed: $script (exit $LASTEXITCODE)"}
}
Run-Check 'tools/tests/Test-CiArtifactSafety.ps1'
Run-Check 'tools/Check-IniCoverage.ps1'
Run-Check 'tools/tests/Test-SignalCalculation.ps1' @{DependencyRoot=$DependencyRoot}
Run-Check 'tools/tests/Test-ProductPackage.ps1'
Run-Check 'tools/tests/Test-WheelInstall.ps1'
Run-Check 'tools/tests/Test-WheelInstallPlayer.ps1'
$package=Join-Path $repo 'build/ci-review-package'
Run-Check 'tools/Package-WheelSettings.ps1' @{OutputDirectory=$package;ReviewOnly=$true}
Run-Check 'tools/tests/Test-PackageInventory.ps1' @{PackagedDirectory=$package}
Run-Check 'tools/tests/Test-WheelInstallPlayer.ps1' @{PackagedDirectory=$package}
Write-Host 'PASS: selected device-free CI checks; package is review-only, not a release.'
