# OutRun 2006 offline calculation first slice

The production `FFB::Update` still owns admission, gameplay/focus/UI gates,
telemetry and deferred device initialization. Its existing arithmetic is extracted
into `src/ffb_calculation.inl`: an explicit sink receives constant and periodic
requests, an explicit clock supplies diagnostic timestamps, and production surface
sampling stays at the original point after signal reads and before conditioning.
Production passes the existing native wrappers, clock and surface LUT sampler.
No INI/default, toolkit pin, force tune/cap, input backend or package change occurs.

`OUTRUN_OFFLINE_SIGNALS` enables reset only in the standalone test executable.
It is absent from runtime build flags; there is no live recording switch, recorder
thread or game-facing playback API. The runner compiles actual production code,
calls the extracted calculation directly and passes memory-only sinks and numeric
surface results. It calls no production Update, loader, telemetry emitter, game
lookup, device initialization or effect creation. Native ABI pointers remain null.
The internal ffbLoaded calculation-availability flag is set synthetically; it does
not represent a loaded DLL. Unexpected surface or input sampling aborts the test.

Reset clears warmup, envelope counters, speed/lateral histories, gear/collision
edges, smoothing, crash/shift/splash timers, synthesis phase and prior output;
shared model/shaper reset hooks exist, but this first runner accepts only compiled
legacy-default settings. It is single-process/single-calculation, with existing
global model state; concurrent independent sessions are not supported. Diagnostics
are disabled for offline runs, so their private logging timer is not part of the
reset contract. Physical device ownership and consumer lifecycle are not reset.

## Experimental numeric source schema v1

UTF-8 ASCII subset, LF, final newline, at most 64 KiB, 512 bytes per line and
4096 samples. Header must be exactly:

```
dbce.outrun2006.calculation-input,1,legacy-defaults,60
```

Each sample has exactly twelve numeric columns: sequence, tickMs, normalized
speed, steering field1D0, steering-rate field1D4, lateral264, lateral268, gear,
collision flags, throttle byte, evaluated surface roughness, water flag.
Sequence starts at zero and is contiguous; unsigned 32-bit ticks are nondecreasing
and may repeat. Wrap/decreasing time is rejected. Ticks label observations;
calculation remains one fixed 60 Hz step per sample, preserving current behavior.
This does not implement variable-dt resampling or imply a measured real frame rate.
Final line is `complete,N`. Unsupported headers, columns, nonfinite/range-invalid
numbers, truncation, count mismatches and trailing data fail before calculation.
No strings, GUIDs, paths, addresses, user identifiers or arbitrary extra channels
are accepted. Numeric validation cannot establish that an externally supplied
file was legitimately captured, complete or anonymized; only synthetic cases are
used here.

Output is experimental `dbce.outrun2006.calculation-observation,1`, observe-only,
`physicalOutput=false`, with frame/tick identity, emitted native-range scaled
constant requests and periodic slot/magnitude/millihertz requests, then a count
footer. These are requests, not measured torque. It is not the portable toolkit
force-observation schema; a later adapter must bind source/config/profile hashes
and normalize units to the shared v1 contract. No observed driver acceptance is
modeled; named profiles and full input/telemetry/lifecycle replay remain follow-ups.

## Verification

Run `tools/tests/Test-SignalCalculation.ps1`. It builds an x86 executable and
generates a privacy-safe 48-frame case with warmup, shifting, held collision edge,
speed-loss history and water roughness. A retained golden observation detects
legacy request changes; repeated constant and periodic runs verify reset behavior.
Invalid version, empty/incomplete input, count, extra/private/nonfinite channels,
sequence/time ordering and overflow fail with exit 2. Exact observation mismatch
exits 1, valid/equal exits 0. Equal ticks and maximum unsigned clock are tested.
No files are overwritten outside a new synthetic fixture directory.

This establishes a testable calculation/clock/reset seam, not a completed
capture system or deterministic game-driving replay. The bounded recorder,
effective settings identity, input availability, gate/reset events and shared
case adapter described in `SIGNAL-REPLAY-PREREQUISITES.md` remain prerequisites
for real session reprocessing. Independent source review is required before
publication; physical acceptance, frozen packages and owner installations remain
unchanged.
