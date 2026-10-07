# Read-only tick discovery review — 2026-10-07

**Current result:** all four findings below are closed in **069fe41**, with
independent source/device-free checks passing. Packaging is clear from this
review; no live discovery, output-mute or gameplay replay qualification is added.
See the correction section below. The original findings/evidence remain historical.

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

## Correction review — 069fe41

Reviewed **069fe415eef21990a7f7bdf6f34b4cafc82ce75a**. All four findings are
resolved without a new pose writer or force behavior:

- Mandatory force/input cleanup and lifecycle completion now precede discovery
  saving; the host boundary also contains discovery exceptions. Actual update,
  observation and exit entry points contain allocation/logging faults.
- Data must commit before the requested outcome. A single non-overwriting retry
  uses `discovery.retry.tsv`; the outcome names it in `dataFile`. Both blocked
  targets produce failed/dataFile=none. Outcome failure leaves committed data
  and a log diagnostic. Serialization failure attempts a fixed-buffer failed
  marker; if storage also fails, missing completion means incomplete evidence.
- A pending pre at duration end fails. Explicit Stop/exit still preserve and
  label partial evidence; neither means a completed observation window.
- Same-model object replacement retains its first changing row with
  `car_instance=1` and ends; unchanged-instance rows remain epoch 0.

Independent validation: **100 original discovery checks**, production consumer
and host lifecycle suites, and quiet-detach checks pass. An additional **33 x86
source-linked checks** pass on frozen source. They exercise foreign data/outcome
targets, full retry data, pending final pre, serialized instance transition,
throwing producer/log boundaries and actual host finalization under allocation
failure, with cleanup exactly once and a stopped gate. The full production
TickDiscovery namespace and host finalizer are verbatim extractions; only game
field holders/log/device cleanup boundaries are fake. Native layouts and tick
phase remain unqualified. No new game/native DLL/device or rig operation.

Private evidence:
`%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/outrun-tick-fixes-069fe41-20261007`.
`Build-Review.ps1` repeats the independent fixture on a copy. Source hashes and
extraction equalities are checked; the 51-file SHA256SUMS seal is
`C10F4CDDD0376EDE0717BBED11DB72360397D922F8E3CB430071CCAB75071ABF`.
The copied lifecycle runner changes source/output-root assignments only and
reads the exact game EXE for hashing, without running it. Original a181519
negative control stays untouched. Installed f021736 proxy hash remains
`E18455496EFB59EB33A9F592889F9B12991C1519EB8A8E6D37268AF132D1AE78`.

Before interpreting a live discovery, use the recorded `dataFile` and require
its successful outcome; filenames alone cannot establish completion. Keep the
coordinated output-disabled discovery and producer-preserving step-2 capture
distinct. The documentation's prior claims that every failure retains rows and
that discovery finalizes before force/input are corrected in TICK-DISCOVERY.
Runtime corrections remain Claude's; review sent through hub **24d2445**.
