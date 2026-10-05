[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$SDLSourceDirectory)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$index = Get-Content (Join-Path $root 'third-party/index.json') -Raw | ConvertFrom-Json
$receipt = @($index.components | Where-Object id -eq 'directx-headers')[0].provenance
function Get-BlobHash([string]$Text) {
    $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $prefix = [Text.Encoding]::UTF8.GetBytes("blob $($bytes.Length)`0")
    ([BitConverter]::ToString([Security.Cryptography.SHA1]::Create().ComputeHash([byte[]]($prefix + $bytes)))).Replace('-','').ToLowerInvariant()
}
foreach ($header in $receipt.headers) {
    $path = Join-Path $SDLSourceDirectory $header.path
    $text = [IO.File]::ReadAllText($path).Replace("`r`n","`n")
    if ((Get-BlobHash $text) -ne $header.sdlFinalBlob) { throw "Pinned SDL input differs: $($header.path)" }
    if ($header.afterCompatibilityEditBlob) {
        $text = $text.Replace('pGPUTimestamp','pGpuTimestamp').Replace('GPUBasedValidationFlags','GpuBasedValidationFlags')
        if ((Get-BlobHash $text) -ne $header.afterCompatibilityEditBlob) { throw 'GPU token-edit reversal differs' }
        $text = $text.Replace("#ifndef _In_opt_count_`n#define _In_opt_count_(x)`n#endif`n#ifndef _In_count_`n#define _In_count_(x)`n#endif`n`n",'')
    }
    $text = $text.Replace("#ifdef _MSC_VER`n#pragma region App Family`n#endif","#pragma region App Family").Replace("#ifdef _MSC_VER`n#pragma endregion`n#endif","#pragma endregion")
    if ((Get-BlobHash $text) -ne $header.sdlImportBlob) { throw 'Full downstream-edit reversal differs from SDL import' }
    if ((Get-BlobHash ($text.Replace("`n","`r`n"))) -ne $header.microsoftBlob) { throw 'Full reconstructed Microsoft blob differs' }
}
$notice = Join-Path $root 'third-party/notices/directx-MIT.txt'
if ((Get-Item $notice).Length -ne 1074) { throw 'Retained license byte length differs' }
$normalized = [IO.File]::ReadAllText($notice).Replace("`r`n","`n").TrimEnd("`n")
$hash = ([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($normalized)))).Replace('-','').ToLowerInvariant()
if ($hash -ne 'fd532481d828e13a0b13ccb598e02338a3617740675a862ee6bdc1541b68e93d') { throw 'Normalized license text differs' }
if ((Get-BlobHash ($normalized.Replace("`n","`r`n"))) -ne $receipt.licenseReceipt.officialBlob) { throw 'Reconstructed official license blob differs' }
Write-Host 'PASS: complete final, intermediate, import and reconstructed Microsoft header blobs; normalized license text and reconstructed official license blob. No source or binary executed.'
