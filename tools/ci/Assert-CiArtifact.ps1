[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ArtifactDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Ci-ArtifactPolicy.ps1')
Assert-CiArtifactDirectory $ArtifactDirectory
Write-Host 'PASS: final two-file artifact inventory, embedded allowlist, hashes and public defaults verified.'
