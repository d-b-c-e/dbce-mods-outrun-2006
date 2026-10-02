[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$ArtifactDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Ci-ArtifactPolicy.ps1')
Assert-CiPackageDirectory $PackageDirectory
Assert-CiOutputPath $ArtifactDirectory
if(Test-Path -LiteralPath $ArtifactDirectory){throw 'CI artifact outputs are immutable; choose a fresh directory.'}
New-Item -ItemType Directory -Path $ArtifactDirectory | Out-Null
$out=(Resolve-Path -LiteralPath $ArtifactDirectory).Path
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path.TrimEnd('\','/')
$zip=Join-Path $out 'outrun2006-ci-review.zip'
$archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
try {
    foreach($file in Get-PackageFiles $package | Sort-Object FullName) {
        $name=$file.FullName.Substring($package.Length+1).Replace('\','/')
        Assert-PackagePath $name
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,$name) | Out-Null
    }
}finally{$archive.Dispose()}
$files=@(Get-CiArchiveInventory $zip)
[ordered]@{schemaVersion=1;sourceCommit=(& git -C $ciRepo rev-parse HEAD).Trim();archiveName='outrun2006-ci-review.zip';archiveSha256=(Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant();files=$files} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $out 'artifact-inventory.json') -Encoding utf8
Assert-CiArtifactDirectory $out
Write-Host 'PASS: allowlisted review archive staged; no game executable, raw build tree or fixture evidence included.'
