# No game input, runtime loading or upload APIs. Shared by staging and final guard.
$ciRepo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $ciRepo 'tools/Validate-Package.ps1')
function Assert-CiObjectKeys($Object,[string[]]$Expected) {
    $actual=@($Object.PSObject.Properties.Name)
    if($actual.Count -ne $Expected.Count -or (Compare-Object $actual $Expected)){throw 'Unexpected CI metadata fields.'}
}
function Assert-CiOutputPath([string]$Directory) {
    $path=[IO.Path]::GetFullPath($Directory)
    $prefix=[IO.Path]::GetFullPath((Join-Path $ciRepo 'build')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(-not $path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'CI artifact output must stay inside this checkout build directory.'}
    $parent=$path
    while($parent -and $parent.Length -ge $ciRepo.Length){
        if((Test-Path -LiteralPath $parent) -and ((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Linked CI artifact output ancestry refused.'}
        $parent=Split-Path -Parent $parent
    }
}
function Assert-CiManifest($Manifest) {
    Assert-CiObjectKeys $Manifest @('schemaVersion','architecture','sourceCommit','runtimeSourceCommit','installerSourceCommit','packagingSourceCommit','sourceDirty','builtUtc','baselineToolkit','nativeToolkitOverride','nativeComponent','distributionReady','reviewOnly','files')
    $head=(& git -C $ciRepo rev-parse HEAD).Trim()
    if($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$'){throw 'Cannot verify CI source identity.'}
    if($Manifest.schemaVersion -ne 2 -or $Manifest.architecture -cne 'x86' -or
       $Manifest.sourceDirty -isnot [bool] -or $Manifest.sourceDirty -or
       $Manifest.reviewOnly -isnot [bool] -or -not $Manifest.reviewOnly){throw 'CI requires a clean-source, review-only x86 package.'}
    foreach($field in 'sourceCommit','runtimeSourceCommit','installerSourceCommit','packagingSourceCommit') {
        if($Manifest.$field -cne $head){throw "CI source identity mismatch: $field"}
    }
    $builtUtc=if($Manifest.builtUtc -is [datetime]){$Manifest.builtUtc.ToUniversalTime().ToString('o')}else{$Manifest.builtUtc}
    if($builtUtc -notmatch '^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(\.\d+)?Z$' -or
       $Manifest.baselineToolkit -cne 'v0.8.0' -or $Manifest.nativeToolkitOverride -cne 'v0.13.0' -or $Manifest.nativeComponent -cne '0.6.0'){throw 'CI package provenance mismatch.'}
    $index=Assert-ThirdPartyNotices $ciRepo
    if($Manifest.distributionReady -isnot [bool] -or $Manifest.distributionReady -ne (@($index.distributionBlockers).Count -eq 0)){throw 'CI distribution status mismatch.'}
    $seen=@{}
    foreach($entry in $Manifest.files) {
        Assert-CiObjectKeys $entry @('name','sha256')
        Assert-PackagePath $entry.name
        if($entry.name -ceq 'package-manifest.json' -or $seen.ContainsKey($entry.name) -or $entry.sha256 -cnotmatch '^[0-9a-f]{64}$'){throw 'Invalid CI package inventory entry.'}
        $seen[$entry.name]=$entry.sha256
    }
    if($seen.Count -eq 0){throw 'Empty CI package.'}
    return $seen
}
function Get-CiPublicSource([string]$Name) {
    Assert-PackagePath $Name
    if($Name -ceq 'dinput8.dll' -or $Name -ceq 'package-manifest.json'){return $null}
    $map=@{
        'WheelFfb.dll'='lib/toolkit/native/x86/WheelFfb.dll'; 'force-profiles.ini'='lib/toolkit/profiles/force-profiles.ini';
        'Install.ps1'='tools/Install-WheelSettings.ps1'; 'Install.bat'='tools/Install-WheelSettings.bat';
        'Validate-Package.ps1'='tools/Validate-Package.ps1'; 'README.md'='docs/INSTALL-WHEEL-SETTINGS.md';
        'UNIFIED-PRODUCT.md'='docs/UNIFIED-PRODUCT.md'; 'provenance/NATIVE-PIN-2026-09-17.md'='docs/NATIVE-PIN-2026-09-17.md'
    }
    $source=if($map.ContainsKey($Name)){$map[$Name]}elseif($Name.StartsWith('provenance/')){'lib/toolkit/'+$Name.Substring(11)}else{$Name}
    return Get-SafePackageFile $ciRepo $source
}
function Assert-CiDllBytes([byte[]]$Bytes) {
    if($Bytes.Length -lt 128 -or $Bytes.Length -gt 16777216 -or $Bytes[0] -ne 0x4d -or $Bytes[1] -ne 0x5a){throw 'CI runtime must be a bounded x86 DLL.'}
    $pe=[BitConverter]::ToUInt32($Bytes,0x3c)
    if($pe -gt $Bytes.Length-24 -or $Bytes[$pe] -ne 0x50 -or $Bytes[$pe+1] -ne 0x45 -or
       $Bytes[$pe+2] -ne 0 -or $Bytes[$pe+3] -ne 0 -or
       [BitConverter]::ToUInt16($Bytes,$pe+4) -ne 0x14c -or
       ([BitConverter]::ToUInt16($Bytes,$pe+22) -band 0x2000) -eq 0){throw 'Executable/private game input is not a CI mod DLL.'}
}
function Assert-CiPackageDirectory([string]$Directory) {
    Assert-PackageInventory $Directory
    $manifest=Get-Content -LiteralPath (Get-SafePackageFile $Directory 'package-manifest.json') -Raw | ConvertFrom-Json
    $seen=Assert-CiManifest $manifest
    foreach($name in $seen.Keys) {
        $source=Get-CiPublicSource $name
        if($source -and (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant() -cne $seen[$name]){throw "CI payload differs from public source: $name"}
    }
    Assert-CiDllBytes ([IO.File]::ReadAllBytes((Get-SafePackageFile $Directory 'dinput8.dll')))
}
function Get-CiArchiveInventory([string]$ArchivePath) {
    $archive=[IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        if($archive.Entries.Count -eq 0 -or $archive.Entries.Count -gt 128){throw 'CI archive entry bound.'}
        $files=@{}; $total=0L; $manifest=$null; $dll=$null
        foreach($entry in $archive.Entries) {
            Assert-PackagePath $entry.FullName
            if($files.ContainsKey($entry.FullName) -or $entry.Length -gt 16777216 -or ($entry.ExternalAttributes -band 0x400) -ne 0 -or
               (($entry.ExternalAttributes -shr 16) -band 0xf000) -eq 0xa000){throw 'Duplicate, linked or oversized CI archive entry.'}
            $total+=$entry.Length; if($total -gt 67108864){throw 'CI archive total bound.'}
            $stream=$entry.Open()
            try {
                $memory=[IO.MemoryStream]::new()
                try {
                    $buffer=[byte[]]::new(8192)
                    while(($count=$stream.Read($buffer,0,$buffer.Length)) -gt 0){
                        if($memory.Length+$count -gt 16777216){throw 'CI archive decoded-entry bound.'}
                        $memory.Write($buffer,0,$count)
                    }
                    $bytes=$memory.ToArray()
                    if($bytes.Length -ne $entry.Length){throw 'CI archive decoded length mismatch.'}
                }finally{$memory.Dispose()}
            }finally{$stream.Dispose()}
            $sha=[Security.Cryptography.SHA256]::Create()
            try {$files[$entry.FullName]=[Convert]::ToHexString($sha.ComputeHash($bytes)).ToLowerInvariant()}finally{$sha.Dispose()}
            if($entry.FullName -ceq 'package-manifest.json'){$manifest=[Text.Encoding]::UTF8.GetString($bytes).TrimStart([char]0xfeff) | ConvertFrom-Json}
            elseif($entry.FullName -ceq 'dinput8.dll'){$dll=$bytes}
        }
        if(-not $manifest -or -not $dll){throw 'CI archive missing manifest/runtime.'}
        $expected=Assert-CiManifest $manifest
        if($files.Count -ne $expected.Count+1){throw 'Unlisted CI archive payload.'}
        foreach($name in $expected.Keys) {
            if(-not $files.ContainsKey($name) -or $files[$name] -cne $expected[$name]){throw "CI archive inventory mismatch: $name"}
            $source=Get-CiPublicSource $name
            if($source -and (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant() -cne $files[$name]){throw "CI archive differs from public source: $name"}
        }
        Assert-CiDllBytes $dll
        # Required public files/notices cannot disappear from a forged manifest.
        $required=@('dinput8.dll','WheelFfb.dll','force-profiles.ini','OutRun2006Tweaks.ini','OutRun2006Tweaks.lods.ini','product.json','Install.ps1','Install.bat','Validate-Package.ps1','LICENSE.md','README.md','UNIFIED-PRODUCT.md','third-party/index.json','third-party/README.md','provenance/VERSION','provenance/NATIVE-VERSION','provenance/NATIVE-PROVENANCE.json','provenance/MANIFEST.txt','provenance/NATIVE-PIN-2026-09-17.md')
        $index=Assert-ThirdPartyNotices $ciRepo
        $required+=@($index.components | ForEach-Object {$_.notices} | ForEach-Object {$_.path})
        foreach($name in $required){if(-not $files.ContainsKey($name)){throw "Missing CI archive public payload: $name"}}
        return @($files.Keys | Sort-Object | ForEach-Object {[pscustomobject]@{name=$_;sha256=$files[$_]}})
    }finally{$archive.Dispose()}
}
function Assert-CiArtifactDirectory([string]$Directory) {
    Assert-CiOutputPath $Directory
    $root=(Resolve-Path -LiteralPath $Directory).Path
    if((Get-Item -LiteralPath $root -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked CI artifact root refused.'}
    $items=@(Get-ChildItem -LiteralPath $root -Force)
    if($items.Count -ne 2){throw 'CI artifact directory must contain exactly two allowlisted files.'}
    foreach($item in $items){if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Name -cnotin @('outrun2006-ci-review.zip','artifact-inventory.json')){throw 'Unknown/linked CI artifact path.'}}
    $zip=Get-SafePackageFile $root 'outrun2006-ci-review.zip'
    $actual=@(Get-CiArchiveInventory $zip)
    $report=Get-Content -LiteralPath (Get-SafePackageFile $root 'artifact-inventory.json') -Raw | ConvertFrom-Json
    Assert-CiObjectKeys $report @('schemaVersion','sourceCommit','archiveName','archiveSha256','files')
    if($report.schemaVersion -ne 1 -or $report.sourceCommit -cne (& git -C $ciRepo rev-parse HEAD).Trim() -or
       $report.archiveName -cne 'outrun2006-ci-review.zip' -or $report.archiveSha256 -cne (Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant() -or
       @($report.files).Count -ne $actual.Count){throw 'CI artifact report mismatch.'}
    for($i=0;$i -lt $actual.Count;$i++){
        Assert-CiObjectKeys $report.files[$i] @('name','sha256')
        if($report.files[$i].name -cne $actual[$i].name -or $report.files[$i].sha256 -cne $actual[$i].sha256){throw 'CI artifact report inventory mismatch.'}
    }
}
