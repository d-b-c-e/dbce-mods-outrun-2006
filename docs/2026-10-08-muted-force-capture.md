# Original legacy force capture with output muted — October 8

Source candidate; not installed or live-qualified. This extends the existing
legacy producer and recorder. It does not implement gameplay playback, change
the owner's tune, select a different force model, or calibrate the unqualified
speed/steering fields against Art of Rally.

## Process and data contract

An explicitly launched child can set `DBCE_OUTRUN_SIGNAL_MUTE=legacy`. The value
is read once, before hooks initialize, and remains in force until process exit.
Any present unrecognized value also blocks output but refuses calculation.
Ordinary launches retain their previous behavior. In diagnostic mode the mod
refuses native loading/initialization/sends, controller rumble and motion
telemetry delivery. Wheel input remains a separate reader. A fresh startup log
must contain `SignalCapture: PROCESS MUTE legacy` before a runner sends input;
if a launcher relaunch drops the environment, the test must stop before driving.

The existing legacy arithmetic continues on eligible offline local-car updates,
including while the window is unfocused. Settings/menus suspend calculation and
reset its neutral/warmup state. A new local car/stage resets its producer history;
changing it during a capture ends that capture as incomplete. No game physics or
input is written. The profile must already be legacy; another model is refused.
The constant-force fallback is an explicit software route. No hardware periodic
capability is invented or negotiated. A virtual sink updates only the model's
duplicate-suppression state; it does not claim an accepted native command.

`tools/Arm-ForceCapture.ps1` binds an external request to the exact game hash,
installed/package proxy hash, package runtime source claim, and current rig lease.
The package provenance remains the caller's source assertion, clearly identified
in the result; the game itself verifies its proxy bytes, not Git history. Requests
expire and claim a new ID directory under
`%LOCALAPPDATA%/Dbce/StagePlayback/outrun-force`. They never arm an ordinary game
process. Nothing is written into the game folder by the recorder.

The 10–120 second window reserves at most 8,192 rows before recording. Per row:
original ten force inputs, all 13 legacy tuning values, exact before/after model
state, ordered requested constants, virtual admission, monotonic-clock tick,
and game-update/stage/car context. No per-row disk writes. An explicit V3
`DBCEORS3` envelope identifies software-only data; V2 `DBCEORR2` decoding and its
128-row bounds remain intact. V3 refuses native-loaded state, hardware slots,
periodic requests, false virtual admission, invalid context and non-monotonic
game-update order. Numeric state/checkpoints, checksum and completion remain
mandatory. Original calculation replay uses the same producer, not a copied law.

Completion detaches the recorder before fallible serialization. Data is flushed
to a new temporary file and renamed without replacing an existing destination;
the outcome comes last. Missing, incomplete or failed outcomes are not success.
Early stop, exit, lost lease, invalid input, model changes and overflow do not
produce a complete capture. Physical output stays blocked after capture ends.
On normal exit, force/input cleanup and lifecycle drain precede file writing.
Only one accepted request can record per process; another capture needs a new
supervised launch. A fault during an active row marks the buffer incomplete and
defers writing until that row drains, including normal exit. No outcome is never
a pass. The lease argument is the entire one-line `$lease.token` returned by
`Enter-StageRigLease`, with its two-hour expiry; it is not just the UUID suffix.

The nominal 60 Hz field describes the existing arithmetic, not a measured
wall-clock sampling rate. `field_1c4` remains a raw speed input with unqualified
units. Do not send it through the shared speed-band normalization analyzer as
metres per second. An accompanying tick-discovery recording can retain raw
pose/camera/force inputs; it is not a gameplay trajectory writer.

## Offline evidence and next check

`tools/tests/Test-SignalCalculation.ps1 -DependencyRoot <main-checkout>` compiles
the production producer, both codecs and runtime controller into x86 fixtures.
The legacy golden, constant/periodic replay, refused delivery and malformed-file
cases still pass. The new fixture drives the actual `FFB::Update` against
synthetic local-car memory with FFB Off, catches every loader attempt, and
checks 360 original nonzero rows, pause checkpoints, V3 exact replay, cached
mute, invalid-mode refusal, hash/lease/expiry/request bounds and false native
state. Loader, output and motion-delivery calls remain absent. The real
consumer/host lifecycle fixtures also pass, including cleanup-before-recording.

Claude's source review passed on `4cb26b4`. Follow-up fixtures exercise empty
captures, early stop, replaced/stale lease, changed model/car, non-finite input,
exit, mid-frame fault, overflow and an existing destination file: all remain
incomplete and output-muted. A second request cannot arm after completion.
`tools/tests/Test-ArmForceCapture.ps1 -ExactExecutable <supported-exe>` checks
the arm/stop tool using copied game bytes and dummy proxy files; it never loads
a DLL, starts a game or touches the live rig lease.

Required before runtime: reciprocal review, clean Release build/package, exact
installer/owner-file preservation, one bounded offline moving capture and exact
recalculation of those original rows. Keep the installed span candidate and all
owner settings until this candidate is reviewed. Physical force and matched
normalization remain separate attended work.

Example after a reviewed installation and caller-owned muted launch:

```powershell
./tools/Arm-ForceCapture.ps1 -GameDir <game> -RuntimePackageDirectory <immutable-package> -LeaseToken $lease.token -Seconds 60
# Once outcome=complete exists, use the signal-recording.exe built by the suite:
& <test-output>/signal-recording.exe replay <result>/signals.osig
```
