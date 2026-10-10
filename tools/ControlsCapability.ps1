# Rig-profile controls capability (STD-033): which runtime may advertise it. Dot-sourced by Package-WheelSettings.ps1
# (repository side only); the installer writes the game-folder receipt from the manifest fields this returns.
# tools/tests/Test-ControlsCapability.ps1 checks both.
#
# A runtime qualifies when its commit is listed, or when, in RepoRoot, its runtime sources are identical to a listed
# commit's: the packager records HEAD, and HEAD moves with any later documentation or policy commit.
$ControlsRuntimePaths = @('src', 'external', 'lib', 'cmake.toml', 'CMakeLists.txt', 'cmkr.cmake')
function Get-ControlsCapability([string]$RuntimeCommit, [string]$PolicyPath, [string]$RepoRoot) {
    $policy = Get-Content -LiteralPath $PolicyPath -Raw | ConvertFrom-Json
    if ($policy.schemaVersion -ne 1) { throw 'Unsupported controls capability policy.' }
    if (-not $RuntimeCommit) { return $null }
    $reviewed = @($policy.reviewedRuntimeCommits)
    $match = $reviewed -contains $RuntimeCommit
    if (-not $match -and $RepoRoot) {
        foreach ($c in $reviewed) {
            & git -C $RepoRoot cat-file -e "$c^{commit}" 2>$null
            if ($LASTEXITCODE) { continue }
            & git -C $RepoRoot diff --quiet $c $RuntimeCommit -- @ControlsRuntimePaths 2>$null
            if ($LASTEXITCODE -eq 0) { $match = $true; break }
        }
    }
    if (-not $match) { return $null }
    [ordered]@{ controlsProfileSchema = [int]$policy.controlsProfileSchema; controlsAdapter = [string]$policy.adapter }
}
