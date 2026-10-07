# Independent readback of the first live discovery

2026-10-07. Reviewed Claude's archived case
`4025516d4d684460805ff899b2de52e4` and documentation commit `48604da` without
launching a game or accessing any device. This follows the completed
[069fe41 source review](2026-10-07-tick-discovery-review.md); those four fixes
remain closed and their tests were not needlessly repeated.

## Evidence checked

- Outcome is `observed`, `unmatchedPre=false`, `replayable=false`, `writer=none`.
  Its named `discovery.tsv` has 5,878 finite, structurally complete rows across
  72 columns, forming 2,939 ordered pre/post pairs. Request, outcome and arming
  provenance identify the same case and exact game EXE.
- Recorded pair update indices are contiguous from 9,541 through 12,479, one
  pair per recorded update, with one car epoch and one stage. Mode 13 has 121
  pairs and mode 16 has 2,818. The outcome's 2,817 `gameUpdates` is sampled at
  update entry, while row mode is sampled at the later car hook; the row stream
  alone cannot reconstruct that earlier mode counter.
- The requested window covers 3,600 updates according to the outcome. Car rows
  begin 11.022 seconds after arming and span 48.958 seconds, so this is not a
  60-second driving recording. The 2,927 ordinary pairs have `ticks=1`; eight
  have `ticks=8` and four have `ticks=2`. One hooked call does not imply one
  fixed unit of simulation time under catch-up.
- Call duration is median 114 microseconds, maximum 737 microseconds in these
  samples. These are observed hook durations, not a general performance result.
- Independently viewed `i10-start-line-mid.png` and `i11-start-line-end.png`:
  Dino at Palm Beach, HUD 000 km/h and first gear, timer 70 then 37. These
  retained game frames corroborate a stationary in-race observation.

## What changed where

Counts compare the recorded values, not all intervening memory writes:

| Fields | Changed inside pre/post pair | Changed between adjacent post/pre observations |
|---|---:|---:|
| position_14 | 2,700 | 0 |
| spd_mb_20 | 2,700 | 0 |
| matrix_70 | 2,700 | 0 |
| matrix_B0 | 2,901 | 0 |
| matrix_F0 | 1,898 | 0 |

This makes the hooked call a useful observed update boundary. It does **not**
prove exclusive ownership of the complete rendered pose: writes which restore
the same value between observations are invisible, and the three matrices
change differently. Do not select a writer from position/matrix_70 alone.

The velocity-like `field_1c4` ranges from 0 to 0.000157989416. Position ranges
are x [-0.000055818, 0.000078264], y [0, 0.020251751], z [-16.0001869,
-15.9967556]. Do not call these metres or infer a world frame from a stationary
car. Matrix roles, moving-car units, camera/interpolation state and replay
ownership remain unqualified.

## Package, install and owner state

All 45 files listed in the private review package manifest match their hashes.
It names clean runtime source `069fe415eef21990a7f7bdf6f34b4cafc82ce75a`,
`reviewOnly=true`, `distributionReady=false`. The three current installed
runtime files match that package:

| File | SHA256 |
|---|---|
| dinput8.dll | 73432838D92BA8CAE8103FA835AB95BA14F8DA528FF0B15BA70A2E09120DEFA1 |
| WheelFfb.dll | 4F7FFBB590C30E2A8D33F2E7A3D7E8C770BDD2466053A2333D67C55D98BB08B4 |
| force-profiles.ini | 43C45A9D57A8F259DB7FCB74922644546EB5D344FFE3FD5D04700BE1388F921E |

The EXE matches the arming hash. All eight files in this run's owner-before
snapshot match the current installed copies, including both save files, INIs
and the restored log. The previous runtime backup hashes match installer
receipt `20261007-173046-056-1db7a221/receipt.json`.

The archived game log corroborates arming and completion. Normal exit code 0
and the lease/idle procedure remain Claude's reported evidence; `done.txt`
contains only `done`, so it is not an independently recorded exit-code receipt.
No new launch, force/UDP output, install, setting change or lease was used here.

## Next useful work

No further title or stationary start-line repeat is needed. Step 2 remains the
producer-preserving output mute: force Off skips the model today, so this
discovery is not an original-force capture. Keep calculation and recording
separate from device acquisition, constant/periodic output, shared memory/UDP,
controller rumble and trigger rumble. Cover the lifecycle with fake sinks before
a combined moving discovery. A moving window must establish matrix/units and
camera semantics before any pose writer; do not infer it from this pass.

Private archive: `E:/Source/_archive/2026-10-07/outrun-discovery-20261007-123210`.
Independent script, report, input hashes, copied capture, frames, log and source:
`%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/outrun-tick-live-4025516d-20261007`.
Its 14-file SHA256SUMS seal is
`4521B8AE38AFD44FB87F629A621A6098AF1F29454999F7FC16E21BF2299C0612`.
The script reads the source archive and current install; repeat on a copy and
expect a current-install mismatch if a later candidate has legitimately replaced
these files. Original captures and prior negative-control evidence were untouched.
