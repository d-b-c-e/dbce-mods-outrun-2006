# Shared source/package validation; compatible with Windows PowerShell 5.1.
function Assert-PackagePath([string]$Name) {
    if ($Name -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*(/[A-Za-z0-9][A-Za-z0-9._-]*)*$') { throw "Unsafe package path: $Name" }
    # Product-owned policy: neither the manifest, notice index nor review flags
    # may extend this list. New payload paths require a reviewed source change.
    $allowed = @('dinput8.dll','WheelFfb.dll','force-profiles.ini','OutRun2006Tweaks.ini','OutRun2006Tweaks.lods.ini','product.json','Install.ps1','Install.bat','Validate-Package.ps1','LICENSE.md','README.md','UNIFIED-PRODUCT.md','package-manifest.json','third-party/index.json','third-party/README.md','provenance/VERSION','provenance/NATIVE-VERSION','provenance/NATIVE-PROVENANCE.json','provenance/MANIFEST.txt','provenance/NATIVE-PIN-2026-09-17.md')
    $noticeNames = @('directx-MIT.txt','flac-BSD.txt','fmt-MIT.txt','hidapi-BSD.txt','hidapi-choice.txt','imgui-MIT.txt','ini-license.txt','ixbase64-MIT.txt','ixwebsocket-BSD.txt','jsoncpp-license.txt','miniupnpc-BSD.txt','miniz-MIT.txt','modutils-MIT.txt','ogg-BSD.txt','proggy-license.txt','safetyhook-Boost.txt','sdl-zlib.txt','spdlog-MIT.txt','stb-rectpack.txt','stb-textedit.txt','stb-truetype.txt','toolkit-MIT.txt','xxhash-BSD.txt','yuv2rgb-BSD.txt','zycore-license.txt','zydis-MIT.txt')
    $allowed += $noticeNames | ForEach-Object { 'third-party/notices/' + $_ }
    if ($Name -cnotin $allowed) { throw "Disallowed private or unknown package path: $Name" }
}
function Assert-NoticeTreePrivacy([string]$Root) {
    $prefix = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    foreach ($item in Get-PackageFiles (Join-Path $Root 'third-party')) {
        Assert-PackagePath ($item.FullName.Substring($prefix.Length).Replace('\','/'))
    }
}
function Get-SafePackageFile([string]$Root, [string]$Name) {
    if ($Name -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*(/[A-Za-z0-9][A-Za-z0-9._-]*)*$') { throw "Unsafe package path: $Name" }
    $path = (Resolve-Path -LiteralPath $Root).Path
    if ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked package root refused.' }
    foreach ($part in $Name.Split('/')) {
        $path = Join-Path $path $part
        if (-not (Test-Path -LiteralPath $path)) { throw "Missing required package file: $Name" }
        if ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked package path refused: $Name" }
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Not a package file: $Name" }
    return $path
}
function Get-PackageFiles([string]$Root) {
    foreach ($item in Get-ChildItem -LiteralPath $Root -Force) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Package contains a linked path.' }
        if ($item.PSIsContainer) { Get-PackageFiles $item.FullName }
        else { $item }
    }
}
function Assert-ThirdPartyNotices([string]$Root, [switch]$ForDistribution) {
    Assert-NoticeTreePrivacy $Root
    $index = Get-Content -LiteralPath (Get-SafePackageFile $Root 'third-party/index.json') -Raw | ConvertFrom-Json
    if ($index.schemaVersion -ne 1 -or $index.productId -ne 'outrun2006-c2c-pc') { throw 'Unsupported third-party index.' }
    $required = @('toolkit','ini-cpp','proggyclean','imgui','stb','spdlog','fmt','modutils','xxhash','ixwebsocket','ixbase64','miniz','safetyhook','zydis','zycore','ogg','flac','miniupnpc','jsoncpp','sdl','hidapi','yuv2rgb','directx-headers')
    foreach ($id in $required) {
        $component = @($index.components | Where-Object id -eq $id)
        if ($component.Count -ne 1 -or -not $component[0].revision -or @($component[0].notices).Count -eq 0) { throw "Missing required notice component: $id" }
        foreach ($notice in $component[0].notices) {
            Assert-PackagePath $notice.path
            if ($notice.path -notlike 'third-party/notices/*') { throw 'Notice must be in the notice bundle.' }
            $actual = (Get-FileHash -LiteralPath (Get-SafePackageFile $Root $notice.path) -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($actual -ne $notice.sha256) { throw "Notice hash mismatch: $($notice.path)" }
        }
    }
    Get-SafePackageFile $Root 'third-party/README.md' | Out-Null
    if ($ForDistribution -and @($index.distributionBlockers).Count -gt 0) { throw ('Distribution blocked: ' + ($index.distributionBlockers -join '; ')) }
    return $index
}
function Assert-PackageInventory([string]$Root, [switch]$ForDistribution) {
    $manifest = Get-Content -LiteralPath (Join-Path $Root 'package-manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.schemaVersion -ne 2 -or $manifest.architecture -ne 'x86') { throw 'Recursive inventory requires schema 2 x86 package.' }
    $seen = @{}
    foreach ($entry in $manifest.files) {
        Assert-PackagePath $entry.name
        if ($seen.ContainsKey($entry.name)) { throw "Duplicate package entry: $($entry.name)" }
        $seen[$entry.name] = $true
        $actual = (Get-FileHash -LiteralPath (Get-SafePackageFile $Root $entry.name) -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $entry.sha256) { throw "Package hash mismatch: $($entry.name)" }
    }
    $prefix = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    foreach ($item in Get-PackageFiles $Root) {
        $name = $item.FullName.Substring($prefix.Length).Replace('\','/')
        if ($name -ne 'package-manifest.json' -and -not $seen.ContainsKey($name)) { throw "Unlisted package file: $name" }
    }
    foreach ($name in 'dinput8.dll','WheelFfb.dll','force-profiles.ini','OutRun2006Tweaks.ini','OutRun2006Tweaks.lods.ini','product.json','Install.ps1','Install.bat','Validate-Package.ps1','LICENSE.md','third-party/index.json','third-party/README.md','provenance/VERSION','provenance/NATIVE-VERSION','provenance/NATIVE-PROVENANCE.json','provenance/MANIFEST.txt') {
        if (-not $seen.ContainsKey($name)) { throw "Missing required inventory entry: $name" }
    }
    $index = Assert-ThirdPartyNotices $Root -ForDistribution:$ForDistribution
    if ($manifest.distributionReady -ne (@($index.distributionBlockers).Count -eq 0)) { throw 'Distribution status differs from notice index.' }
}
