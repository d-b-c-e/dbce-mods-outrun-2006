# Read-only tick discovery review — 2026-10-07

Reviewed **a181519f073d7759e0809029f778a93eb06d07f9** on master. The step-1
design fits [STAGE-PLAYBACK](STAGE-PLAYBACK.md): observe named values on both
sides of the local-car simulation hook, with no gameplay writer. **Hold install
pending the four fixes below.** Claude owns runtime corrections; this review
changes documentation only. Installed f021736 and the owner configuration remain
unchanged. Native menu/race entry already passed; no repeat is requested.

## Reproduced findings

| Severity | Source at reviewed commit | Finding and required correction |
|---|---|---|
| High | `src/host_lifecycle.cpp:85`, `src/tick_discovery.hpp:302` | Discovery finalization allocates/serializes before mandatory force/input cleanup. A single allocation fault escapes the real host finalizer, skips all three cleanup callees, and leaves the lifecycle gate in Stopping; retry cannot claim it. Contain discovery exceptions at the host/game boundary and make cleanup/gate completion independent of fallible serialization or logging. Silence/drain outputs before disk work where the established lifecycle permits. |
| Medium | `src/tick_discovery.hpp:302` | If row-file commit fails, Write still emits `outcome=observed` and discards the session. A directory at `discovery.tsv` reproduces this with a WRITE FAILED log. Normal completion must require committed rows; otherwise publish explicit failed/incomplete status, preserve bounded recovery evidence where possible, and never overwrite foreign files. |
| Medium | `src/tick_discovery.hpp:244` | The due-duration path bypasses Update's pending-pre check. After 599 complete pairs and a final pre, it writes observed with unmatchedPre=true. Closure must validate the final pair before success; Stop/exit may preserve partial evidence but must remain explicitly incomplete. |
| Medium | `src/tick_discovery.hpp:125`, `:165` | Local car pointers are checked within pairs only; cross-update identity uses model/colour/transmission and the TSV omits instance identity. A new local object of the same model produces an undetected mixed stream. Retain an instance/epoch identity and end/tag replacement, preserving the changing observation. |

Add production regression cases for allocation/IO/logging failure through host
finalization (cleanup exactly once, gate completes), separate row/outcome write
failures, pending-pre duration/Stop/exit, and same-model replacement with an
unchanged-instance control. The existing happy-path cleanup-order assertion
does not prove failure containment. Do not broaden this into an unreviewed hook
or lifecycle redesign.

## Evidence and reproduction

- Original `./tools/tests/Test-TickDiscovery.ps1`: **76 checks pass**, x86 MSVC
  `/W4 /WX`. Build: `build/tick-discovery-ebe06f5f07a04309aa99808d79e0854b`.
- Independent fixture: **15 setup/reproduction assertions pass, confirming
  defects**. It compiles the frozen production header, original fixture helpers,
  lifecycle gate and verbatim extracted host finalizer. Only cleanup callees use
  fake counters. One-shot global allocation failure reproduces the shutdown
  result; no game, device, native DLL, OS input, focus, display or lease is used.
- Private sealed root:
  `%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/outrun-tick-discovery-review-20261007`.
  `run-2a3245eaf8154ba8b70a1a224e055ab9/review.log` contains all four cases.
  Run `pwsh -NoProfile -File <copy>/Build-Review.ps1` on a copy; it creates a new
  isolated run directory. Keep the frozen negative control unchanged.
- `SHA256SUMS.txt` covers **50 files**, including source snapshots and original
  fixture artifacts; seal SHA256:
  `9BBAE8BBBC9529EB23A2AA02C66B8E9EAA043D96F7051D1DC4C1C10501C97D4E`.
  Snapshots matched the reviewed checkout; the host-function extraction matched
  the source. Original test output was observed directly, not separately logged.

## Scope and remaining gaps

This discovery leaves physical outputs unchanged. Step 2 still needs independent
output admission that preserves force production. Any eventual unattended run
needs separately verified suppression of wheel/rumble/motion outputs. FFB Off
does not preserve the original force stream and must not be described as such.
Tick frequency, solved phase, matrix roles/units, camera/body ownership and actual
gameplay recording/playback remain unqualified.

The standards audit found missing STD-021..025; adoption now records them
explicitly. Pending: STD-012,021,022,025. Unchecked: STD-001,003..009,015..019,023,024.
Partial: STD-013,020. No functionality is adopted merely by adding a row.
Cross-review was delivered through the portfolio inbox at hub **6e8657b**.
