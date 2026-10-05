# PC named-profile calculation audit

The published recorder at `6904140` remains default-off, fixed at 128 legacy
calculation frames, with caller-declared provenance and experimental schema v2.
This candidate adds tests and documentation only; it does not expand recording
support or change runtime calculation, toolkit headers, profiles or packages.

Named/shared profiles already pass through the production `CalculateSignals`
seam. A synthetic fixture loads all six pinned shipped profiles plus one wholly
synthetic inherited user profile through the actual `load_profile_dir` parser,
then constructs the same vendored `Model` and `Shaper` classes as production.
It calls the actual PC calculation with memory-only sinks. It never calls
production Update, native loading/init/effect creation, device polling, telemetry,
game lookup or the game surface sampler. The fake constant sink explicitly
controls admission feedback; no successful torque or driver behavior is inferred.

`tools/tests/Test-SignalProfiles.ps1 -DependencyRoot <existing verified build>
-BaselineRoot <clean checkout of 73747cc>` builds the same fixture twice, once
against the old pre-recorder production seam and once against this candidate.
Every profile covers constant fallback and periodic calculation requests, fresh
and mid-run populated-history resets, steering sign changes, idle/water/crash/gear
transitions and rejected constant admission. Each 96-frame synthetic case repeats
with fresh model/shaper objects and equal/jumped clock values, requiring exact
repeatability under the existing fixed 60 Hz semantics. These fixture calls are
not a recorder size increase. The two executables must produce identical LF
observation bytes, including hexadecimal float periodic values and public model
structural/event traces. The generated receipt records the observation SHA256.

Executed on 2026-10-02: both executables passed all 28 cases (84 production runs
each). Exact old/new observation SHA256:
`237e8a94e677a831e4c3736647b3a3b36afdae72272040148397dc148bba4a92`.

The candidate fixture also verifies that schema v2 refuses each named setting,
an active shared-model flag even with a legacy setting, and a profile selection
change between recorded frames. That change produces an incomplete capture
before an extra clock sample. It deliberately preserves conservative refusal
when a named setting could have fallen back to legacy after load failure.

## Bounded prerequisite before named-profile recording

Schema v2's 46-value checkpoint contains PC history, counters, event timers,
availability and slots. Its 13-value configuration contains legacy tuning and
the user master controls. Neither includes the effective shared profile's model
and shaper settings. Recording a name/version alone would not identify user
overrides, inherited values or mutable public settings.

The pinned `force_model.h` also keeps necessary evolving state private:
`Model` owns shift time/emitted pulse, impact time/sign/magnitude and texture
phase; `Shaper` owns smoothed force, last force, ramp time and started state.
Public `last_was_event` and `last_structural` are insufficient to reconstruct
that state. `ResetCalculationState` resets both objects in the offline fixture,
but an arbitrary mid-session checkpoint is not a full reset. Copying object
padding/private bytes into a file would violate the numeric format and would
not provide stable versioned semantics. No calculation logic is duplicated here.

Before admitting named profiles, the toolkit needs a reviewed, versioned,
numeric model/shaper state export and restoration contract, including finite
range validation and exact reset/ramp/event semantics. The PC codec then needs
an explicit new schema/model discriminator, immutable effective settings plus
source/toolkit provenance, full first-frame and discontinuity checkpoints,
configuration mutation detection, and bounded reject tests for every new field.
The legacy v2 reader must continue rejecting incompatible captures. Test
restoration from populated history, in-flight pulses, texture phase and active
smoothing/slew/ramp, rather than replaying only fresh objects.

Synthetic old/new equality proves the inspected calculation seam preserves these
cases. It does not prove private-state checkpoint completeness, authentic profile
identity, variable-dt replay, full force-event/session capture, live recording
controls, real-game driving replay, correct steering scale or physical output.
No real/private capture is acquired and no installation changes are made.
