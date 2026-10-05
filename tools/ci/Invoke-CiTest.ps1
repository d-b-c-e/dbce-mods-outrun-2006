[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ScriptPath,[string]$ParametersJson='{}')
$ErrorActionPreference='Stop'
# Existing suites intentionally run invalid-input native cases and assert exits.
$PSNativeCommandUseErrorActionPreference=$false
try {
    $parameters=@{}
    (ConvertFrom-Json -InputObject $ParametersJson).PSObject.Properties | ForEach-Object { $parameters[$_.Name]=$_.Value }
    $global:LASTEXITCODE=0
    & $ScriptPath @parameters
    if(-not $?){if($LASTEXITCODE -ne 0){exit $LASTEXITCODE};exit 1}
    # Each selected suite throws for unexpected native exits; expected refusal
    # cases may leave LASTEXITCODE nonzero after a successful test.
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
