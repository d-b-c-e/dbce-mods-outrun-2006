# OutRun 2006 PC signal replay intake

Inspected base: `feccbc5c36be3e7e7ba110552145dd0793bee7fc`, tree
`fb1210fcf9dd1ef883bef5d114f72a30a265812a`. This is an evidence-based
prerequisite report, not a recording or playback implementation. No runtime or
package change is made. `product.json` correctly keeps session recording and
driving-input playback unimplemented.

## Existing evidence and limits

| Existing path | What exists | Replay limitation |
|---|---|---|
| `src/hooks_dinputffb.cpp`, `Telemetry::Write` | Per-update shared memory and Forza UDP, including pedal availability | Live outputs, no bounded session file, complete/drop footer or immutable configuration identity; UDP/shared memory can lose or overwrite samples |
| Same file, `FFB::Update` | Production force computation, native output wrappers and lifecycle gate | Reads game mode, car fields, focus/UI gates, clock, surface LUT, settings and retained model history; no captured-frame reader or offline clock/reset adapter |
| Same file, `FFBDiagnosticLog` block | Two-second summaries and steering min/max | Aggregated ranges cannot reconstruct per-frame inputs, event order or output requests |
| `src/overlay/wheel_settings.cpp`, Capture | Binding/calibration transactions | Configuration capture, not a driving session |
| `tools/tests/consumer_lifecycle_fixture.cpp` | Compiles production FFB source with fake ABI; producer/selection/teardown tests | Useful production test seam, but no recorded signal codec, complete frame capture or full model replay/reset contract |
| Other settings, calibration and dispatch fixtures | Synthetic production-linked input and UI regressions | No captured driving-input stream or game initial-state restoration |

Inventory of `src` and `tools` finds no `.jsonl`, `.csv`, `.fzpt` or `.inp`
capture assets and no session recorder/replay command. Searches for replay and
playback find product limitations and queued-character suppression tests, not
driving playback. This conclusion covers the inspected public PC checkout; it
does not claim that no private recording exists elsewhere. No private logs or
owner game assets were imported.

## Shared conventions to retain

The toolkit's `docs/RECORDED-PLAYBACK.md` separates `signal-reprocess` from
`game-input-replay`. Its source-only v1 case manifest hashes source artifacts,
configuration and profile identities. Force-observation output is
`output="observe"`, `physicalOutput=false`, with ordered sequences/ticks and a
complete count footer. Invalid/unsupported/incomplete data must fail rather than
become zero; comparison exits are 0 equal, 1 mismatch and 2 invalid input.
Those portable conventions do not supply a PC frame-capture codec automatically.
No common Recording extension or native toolkit version is repinned here.

## Minimum prerequisite for a useful production replay runner

1. Define the PC numeric frame contract at `FFB::Update` before state mutation:
   monotonic sequence/time, field values and validity, gameplay/focus/UI/lifecycle
   gates, input/pedal availability, actual settings/profile identity, surface-LUT
   result, and reset/selection transitions. Retain unavailable values explicitly.
2. Separate complete sample acquisition and effective clock from stateful force
   evaluation; expose a deterministic reset for warmup, counters, prior gear,
   smoothing, speed/lateral history and impulse timers. Reuse production code;
   a duplicate force model would not establish a regression.
3. Observe post-gate constant/periodic/zero requests in a memory-only sink with
   explicit native-range normalization and slot/event identity. Offline execution
   must never load WheelFfb.dll, initialize/enumerate devices, create effects,
   send UDP, open shared memory, inject game input or launch a game.
4. Only then add a versioned strict reader and synthetic case with fixed expected
   observations; test duplicate/unknown keys, unsupported versions, nonfinite
   values, ordering, truncation, missing channels, drops, and mismatch/error exits.
   Compare repeated runs after a full reset using the shared case/observation
   identity conventions. A later real capture remains an attended workflow.

Existing telemetry and diagnostic output cannot satisfy step 1. Therefore the
requested captured-signal replay harness is blocked by the missing production
capture/clock/reset seam. A file that merely repeats synthetic ABI requests would
test a reader or gate, not reproduce this production force computation. The next
reviewable implementation should establish that narrow seam and synthetic-only
verification before adding an opt-in bounded recorder; no attended driving time
is requested for this intake.

Signal replay would verify adapter/model requests, not physical force, receiver
acceptance, simultaneous triples or deterministic game driving. Game-input replay
additionally needs a PC input codec and verified initial-state restoration, neither
of which is established here. Frozen ZIPs, Redux/owner installation and F-Zero's
reservation remain unchanged. No game, screen, device or force operation occurred.
