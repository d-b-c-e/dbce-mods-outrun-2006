# PC named-profile snapshot proposal, fixture only

This isolated candidate starts at published PC source `721eb07`. It adds tests,
documentation and a pinned toolkit dependency proposal. No production `src/` or
`lib/toolkit/` file changes. The published default-off, 128-frame legacy recorder,
experimental schema v2, native DLL/ABI, model arithmetic, defaults and profiles
remain unchanged. No named-profile recording admission, live controls, runtime
capture, installation, game or physical-device work is introduced.

## Toolkit owner coordination boundary

The PC working instructions assign shared force model changes to the toolkit
owner. The proposal is therefore retained under
`tools/tests/toolkit-snapshot-proposal/`, not applied to live or vendored headers.
`force_model.friend.patch` proposes exactly two macro-gated friend declarations
on the actual `Model` and `Shaper` classes. It adds no data fields, arithmetic or
native exports. `offline_snapshot_v1.hpp` implements the fixture API through that
friendship. `Test-SignalSnapshots.ps1` checks exact source pins and applies the
friend declarations only to a generated build-local header overlay; the profile
parser is copied unchanged. No second implementation of compute/shape is made.

Owner action remains pending: review/adopt the bounded snapshot API in toolkit
`native/forcemodel/` against the inspected **v0.8.0 consumer model**; establish a
published toolkit source/component identity before any PC production repin or new
recording schema. This does not request or imply a native DLL upgrade. The parent
coordinator owns routing this exact proposal to the existing toolkit owner; this
PC task has not claimed ownership of that checkout or opened a second toolkit
implementation task. The live toolkit's Git ownership check refused read-only
HEAD inspection; its trust configuration was left unchanged. No current live
toolkit commit or compatibility with later models is asserted.

Exact dependency pins, with LF-normalized UTF-8 SHA256:

| File | SHA256 | Git blob |
|---|---|---|
| `force_model.h` | `5a0596a0bce15cfb38f35c16540196cd8529b888d49662467a73c8bd01f155db` | `76b14d53d1f5878c83fd12ec27a93be9d814d08d` |
| `force_profile.h` | `4dd03ea92405c9bb157755b1b506036e469afac335253a2a40e0d4b32fa13999` | `ff27dcaab28cd1912847f2f1bd5e53a0cbe2a567` |

## Snapshot and configuration contract

Fixture version 1 captures all 19 numeric ModelSettings and 12 ShaperSettings
values in a read-only effective configuration object, plus the caller's profile
ID/version. It captures evaluated inherited/user values, not an INI filename,
mutable parser object, description or ignored unknown key. Model state includes
shift time/emitted pulse, impact time/sign/magnitude, texture phase and public
event/structural results. Shaper state includes smoothing, previous output, ramp
time and started state. Scalars are explicit numeric arrays, never raw object
bytes, padding or pointers. This is an in-memory fixture API, not a file codec.

Restore validates version, pinned source identity, profile ID, every configuration
and state scalar, exact binary32 representation, boolean/integer channels, timer
sentinels, bounded ranges, peak output and restart-ramp consistency. It also
requires both destination objects' actual settings to equal the immutable
configuration. All validation precedes either object's mutation. A rejected
snapshot leaves both dynamic states unchanged. Capture refuses configuration
mutation and state outside the bounded fixture domain. Profile IDs/source hashes
remain caller-declared provenance, not authentication.

The configuration domain deliberately accepts the inspected shipped profiles and
synthetic fixture tunes, not every value the permissive production parser could
load. Numeric state/configuration channels are finite and bounded to one million;
strength/gain, flags, events and timer/ramp values have narrower stated checks in
the API. Long-running texture phase or unsupported custom settings may be refused.
No production parser policy is changed. This prototype validates eligibility;
it does not establish that arbitrary externally supplied checkpoints are genuine
reachable production states.

## Executed evidence

`tools/tests/Test-SignalSnapshots.ps1 -DependencyRoot <verified existing build>`
compiles actual PC calculation code with actual toolkit classes and memory-only
sinks. It does not invoke production Update, native loading/init/effect creation,
telemetry, device polling or game surface sampling.

303 replay cases restore a populated PC checkpoint plus the named model/shaper
checkpoint into fresh objects and require exact subsequent constant/periodic
requests, PC final state and complete private model/shaper final state. Seven
shipped/inherited profiles cover seven checkpoints, two periodic modes and three
reset/restart modes. An eighth synthetic tune explicitly exercises texture, slew
and a long active ramp. Aggregate assertions establish populated history, active
shift/impact pulses, texture phase, ramp and smoothing. Rejected constant sink
feedback remains explicit, without inferring driver admission or torque.

856 rejection tests cover version/source/profile mismatches, identical-settings
cross-profile refusal, effective-configuration mutation, nonfinite/out-of-range
fields, timer/sign/flag/ramp errors and non-binary32-exact numeric values. Rejected
restore atomicity is checked. The patched fixture model preserves the published
seven-profile golden SHA256
`237e8a94e677a831e4c3736647b3a3b36afdae72272040148397dc148bba4a92`.
The published legacy golden and synthetic legacy v2 recorder/read/recalculation
roundtrips also pass with the overlay. Existing golden files are unchanged.

## Remaining capture limits

This closes a bounded in-memory fixture prerequisite, pending independent review
and toolkit-owner adoption. It does not extend schema v2. A later PC schema needs
an explicit model discriminator, immutable configuration/provenance encoding,
whole PC/shared state checkpoints, reset/selection/restart events, configuration
change rules and bounded malformed-file tests. No variable-dt, full-session force
event coverage, real-game capture/driving replay, live recording controls, verified
steering scale or physical acceptance is claimed. Owner WIP and frozen packages
remain untouched.
