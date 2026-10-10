# Rig-profile controls capability (STD-033): which runtime commit may advertise it. Dot-sourced by
# Package-WheelSettings.ps1 (repository side only); the installer writes the game-folder receipt from the manifest fields
# this returns. tools/tests/Test-ControlsCapability.ps1 checks both.
function Get-ControlsCapability([string]$RuntimeCommit, [string]$PolicyPath) {
    $policy = Get-Content -LiteralPath $PolicyPath -Raw | ConvertFrom-Json
    if ($policy.schemaVersion -ne 1) { throw 'Unsupported controls capability policy.' }
    if (-not $RuntimeCommit -or @($policy.reviewedRuntimeCommits) -notcontains $RuntimeCommit) { return $null }
    [ordered]@{ controlsProfileSchema = [int]$policy.controlsProfileSchema; controlsAdapter = [string]$policy.adapter }
}
