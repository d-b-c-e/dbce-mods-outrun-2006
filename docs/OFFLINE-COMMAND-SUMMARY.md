# Bounded synthetic command evidence

This review candidate adds an offline adapter over the existing legacy calculation
fixture, schema-v2 reader and exact production replay. It does not change runtime
source, toolkit code, profiles, defaults, output encoding or CI. The recorder
fixture gains only an optional main-function guard so its existing replay can be
reused. No second recording format or force model is introduced.

Run from the repository with PowerShell 7 and the existing verified x86 dependency
checkout (read-only):

```powershell
./tools/tests/Test-CommandSummary.ps1 -DependencyRoot <verified-dependency-checkout>
```

The runner first executes the existing calculation golden and recorder tests,
then summarizes their existing 48-frame synthetic recording after strict decode
and exact production recalculation. It emits `command-summary.json`, copied
synthetic input and recording, `metric-definitions.json`, and `hash-catalog.json`
under a new `build/command-summary-*` directory. The catalog binds file hashes,
source blobs and working-file hashes, actual adapter checkout identity, compiler
and test executable. It is a local evidence receipt, not a signed attestation.

The recording's caller-declared revision remains
`73747cc3585f48c64465340c8c21ee5c06093cf8`, as in the existing synthetic generator;
it is explicitly unauthenticated. Production baseline for this milestone is
`86599699ab3926bef796413d266a9090158383c9`. These identities must not be conflated.
The generated summary hashes the canonical encoded recording and independently
checks that hash against the exact input file. The deterministic summary is
compared with `tools/tests/data/command-summary-v1.expected`.

## Metric boundary

Versioned definitions are in `tools/tests/data/command-metric-definitions-v1.json`.
Constant magnitude statistics and cap fraction describe recorded calculation
requests in signed native-range units, before master strength and native encoding.
Cap fraction is request-count weighted. Nonzero request and frame fractions
describe activity, not held-effect duty. Last-minus-first sample ticks and the
fixed-60-Hz calculation-step sum describe two different bounded sample timings,
not real driving or effect duration.

Request deltas and sample-tick average rates exclude checkpoint/configuration
boundaries and equal-tick rate division. They include recorded sink feedback
conditions and cannot establish instantaneous actuator slew or structural limiter
occupancy. Periodic amplitude parameters and Hz remain separate from constant
units; amplitudes are pre-clamp and are not torque percentages.

Post-strength output envelope, time-weighted output cap occupancy, held-effect
duty/duration, physical torque, instantaneous actuator slew, shutdown zero/release
success and cross-game feel equivalence remain explicitly unsupported (`null`).
Recorder completion is not game shutdown. The stream omits native admission,
external zero/watchdog events and full effect lifecycle. This evidence is not the
shared toolkit case/observation v1 format and cannot safely compare OutRun with
Woden merely because numerical summaries resemble each other.

## Validation and review gate

The runner checks existing legacy golden/recorder regressions; eight arithmetic
and boundary cases; repeated summary and recording/golden SHA256 equality; and
refusal of wrong declared revision, changed expected calculation, checksum
corruption and truncation. Arithmetic-only cases validate metric math; exported
synthetic evidence additionally passes actual production replay. No game,
private capture, real replay or physical device is used. This is bounded offline
evidence, not tuning or live-force acceptance. Publication and CI remain gated
on independent review and the pending allowance.
