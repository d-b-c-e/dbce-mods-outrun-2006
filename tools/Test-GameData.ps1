#Requires -Version 7.0
# Read-only prerequisite for unattended startup. Never repairs or launches.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$GameDir)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($GameDir).TrimEnd('\')
$manifest=Get-Content (Join-Path $PSScriptRoot '../docs/game-data-required.json') -Raw | ConvertFrom-Json
if($manifest.schema -ne 'outrun.required-game-data@1'){throw 'Unknown game-data contract'}
$exe=Join-Path $root 'OR2006C2C.EXE'
if(!(Test-Path -LiteralPath $exe) -or (Get-FileHash -LiteralPath $exe).Hash -ne $manifest.gameSha256){
    throw 'Unsupported or missing OutRun executable'
}
$problems=[Collections.Generic.List[string]]::new()
foreach($file in $manifest.files){
    $path=[IO.Path]::GetFullPath((Join-Path $root $file.path))
    if(!$path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid asset path'}
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){$problems.Add('missing: '+$file.path);continue}
    $item=Get-Item -LiteralPath $path
    if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){$problems.Add('linked: '+$file.path);continue}
    if($item.Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path).Hash -ne $file.sha256){
        $problems.Add('different: '+$file.path)
    }
}
if($problems.Count){throw ("Game data preflight failed; no launch. Preserve custom assets and review differences:`n"+($problems -join "`n"))}
[pscustomobject]@{schema='outrun.game-data-check@1';gameSha256=$manifest.gameSha256;matchedAssets=@($manifest.files).Count;scope=$manifest.scope;launchesGame=$false}
