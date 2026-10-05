# OutRun 2006 bounded calculation recording candidate

This adds a default-off in-memory recorder around the reviewed calculation seam.
There is no INI switch, automatic enable, installation or live launch command.
The internal `SignalRecording::Begin` / `Take` API is an explicit source seam for
a later reviewed owner workflow. No attended record control is implemented here.
When disabled, `CalculateSignals` checks null recorder pointers: no recorder
allocation, additional clock read, settings/state copy or file I/O occurs.
Existing sinks and arithmetic still execute in their original order.

Begin allocates one fixed 128-frame buffer outside the calculation callback and
requires the existing consumer lifecycle lease. It refuses a second active
session, invalid declared revision or named/shared profile. Production Update
already serializes calculation through that lease. The offline fixture is single
threaded; independently concurrent calculations are not supported. Capture takes
one extra clock sample only while enabled, then writes numeric values into fixed
arrays. Capacity exhaustion, backwards/wrapped time, nonfinite data, reentrancy,
exceptions or configuration change during a frame mark the buffer incomplete.
These failures never enable FFB or suppress the existing runtime sink.

Each selected calculation frame records:

- The declared 20-byte calculation-source revision, contiguous sequence and
  unsigned monotonic tick in milliseconds; fixed 60 Hz step semantics remain.
- Every legacy tuning value read by calculation plus master strength and inversion.
  Changes between frames are represented by each frame's numeric configuration.
- Speed, flags, lateral components, gear, steering fields, throttle and the exact
  evaluated surface roughness/water result; no raw game-memory block is copied.
  Signals are copied from actual calculation locals. Idle throttle selection and
  normalization are checked against the captured pedal; inconsistent reads make
  the capture incomplete rather than silently becoming a reproducible sample.
- Initial and final calculation state: history arrays, counters, smoothing,
  event timers/phases, warmup, gear/collision edges, prior force, periodic slots
  and calculation-availability flags.
- An explicit state checkpoint when prior state differs from the preceding final
  state. This retains partial resets/selection/gate changes without pretending
  they were full zero resets. The first frame always has a checkpoint.
- At most three calculation requests per frame: raw constant magnitude or
  periodic slot/amplitude/frequency, and prior-force state after the existing sink.
  Reader simulation uses that recorded admission feedback; it does not call a
  driver or infer successful torque. This is calculation output, before actuator
  encoding/clamping, not a complete application force-event stream.

Take detaches the buffer under the lease. Completion requires an explicit true
close, a nonempty buffer and no failure. A false close is incomplete. Encoding and
`SaveNew` are offline/caller-side operations after detachment; never call them
from game callbacks or DllMain. Files are exclusively created, never overwritten.
No background writer thread, path setting, auto-flush or exit hook is introduced.
Process loss before explicit save produces no completed file. Owner-facing save
and recording controls remain a follow-up; this source milestone does not make
real capture operationally ready.

## Binary schema v2

`DBCEORR2`, version 2, fixed rate 60, 20 raw revision bytes and frame count precede
the ordered frame records. Each row stores sequence, tick and checkpoint marker;
46-double initial/final state, 13-double configuration, ten-double input, request
count and three fixed five-double request slots. Empty request slots must be zero.
State scalar indices 0–21 are sharedPrevGear, prevGear, prevCollisionFlags,
prevSpeed, smoothedLateral, speedHistoryIdx, latHistoryIdx, crashImpulseTimer,
crashImpulseForce, prevConstantLevel, prevStructLevel, gearShiftTimer, splashTimer,
splashAmp, warmupFrames, updateCounter, roadPhase, slipPhase, ffbLoaded,
periodicsActive, slotRoadTexture and slotTireSlip. Indices 22–29 hold eight speed
history values; 30–45 hold sixteen lateral history values. Configuration order is
lateralDeadzone, gripLoss, wallImpact, roadTexture, tireSlip, engineIdle,
springStrength, damperStrength, steeringWeight, weightTransfer, gearShift,
invertForce and globalStrength, using existing FFB setting units. Input order is
speed, flags, lateral264, lateral268, gear, steer1D0, rate1D4, rawThrottle,
roughness and water flag. Request order is kind, slot, raw magnitude, frequency
and prior-force-after-sink. Kind 1 is constant (integer native range, zero
slot/frequency); kind 2 is periodic (float amplitude and Hz). Constant feedback
must be the prior value or requested value; periodic requests cannot change it.
Integers are little endian; numeric fields are IEEE-754 binary64 values, with
exact binary32 round-trip validation for originally float fields. Struct padding,
pointers, device identities, names, strings, paths and user data are never encoded.
The declared revision must be a lowercase 40-hex value at Begin; it is provenance
supplied by the caller, not an authenticated source checkout claim.

An END marker, explicit complete/incomplete bit, retained count and FNV-1a checksum
terminate the file. The checksum detects byte corruption, not malicious tampering
or authenticity. Reader bounds are 128 frames and 256 KiB. Unsupported versions,
invalid markers/counts, truncation, trailing bytes, incomplete footer, nonfinite or
precision/range-invalid channels, backwards time and unmarked state discontinuity
are rejected. Numeric-only format does not authenticate external captures or
guarantee their anonymity; no real/private captures are used in this milestone.

This is an experimental PC source format, not the shared toolkit case/force
observation schema. Portable case identity and normalized observation adapter
remain follow-ups. It records calculation calls, not every game frame, input
event, telemetry packet or external watchdog/zero request. Those omissions are
part of the scope, not reported as zero/dropped channels. A complete footer means
this bounded calculation sample set closed without a recorder failure; it does
not establish a complete real driving session, deterministic game input replay,
variable-dt timing, named-profile coverage or physical force acceptance.

## Tests

`tools/tests/Test-SignalCalculation.ps1` retains the disabled-mode legacy golden
and schema tests, then builds a production-linked recorder/reader fixture.
Synthetic record → encode/save → bounded read → actual production recalculate
roundtrips cover reset checkpoints, between-frame tuning changes and constant/
periodic output, rejected sink admission and capture starting from populated
history. Repeated replay compares requests and final state exactly.
Tests reject truncation, corruption/checksum/version/magic changes, nonfinite
input/config, invalid reset/state/time, extra bytes, incomplete close, overflow,
named profiles and existing-output overwrite. A checksum-valid changed expected
request reports mismatch. CLI exits: 0 equal, 1 calculation mismatch, 2 invalid.
Clock wrap and interrupted sink execution cannot produce a complete capture.

The fixture never calls production Update, native load/init/effect APIs, input
polling, telemetry, game launch or the game surface sampler. It uses copied numeric
synthetic values and memory-only sinks. Independent source review is required
before publication. Existing frozen packages and owner/Redux installation remain
unchanged; no capture or force setting is auto-enabled.
