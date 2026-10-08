# Original legacy force capture with output muted — October 8

Privately installed and qualified for one original software-only capture at
14:15–14:24 CT, runtime `5b19c9d`. This extends the existing
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

Reciprocal source review, clean Release build/package, exact packaged installer
checks and the bounded live capture below passed. Physical force and matched
normalization remain separate attended work.

Example after a reviewed installation and caller-owned muted launch:

```powershell
./tools/Arm-ForceCapture.ps1 -GameDir <game> -RuntimePackageDirectory <immutable-package> -LeaseToken $lease.token -Seconds 60
# Once outcome=complete exists, use the signal-recording.exe built by the suite:
& <test-output>/signal-recording.exe replay <result>/signals.osig
```

## First original capture — 14:23 CT

Clean runtime/package source `5b19c9d11175fdb8a439634d08644da97bd78990`:

- Proxy SHA-256: `B517AC92FFAC26CFFB2B1516B725F2AF49E63A0A1CA4C5884F744BB28D1F3EFE`.
- Private review ZIP SHA-256: `AE084777759DA3BAD7B2C54AB427037BC2EBB4B65FF70AF5F5F6E0C68357C32B`.
- Native remained the reviewed 41-export x86 `50ba139`, SHA-256
  `432727681B1A8A6F62D68F2E1ED865FF9F229AECCC628B82FA97147626451968`.

The supervised child logged the process mute before navigation; its module list
contained no WheelFfb DLL. Frame-guided virtual-pad navigation reached offline
Palm Beach, Dino 246 GTS / Automatic / Splash Wave. A 20-second 35% throttle
request returned automatically to neutral. No steering or brake was injected.
Frames show movement and 018 km/h; the owner's 2560×1440 presentation was retained.
The discovery rows confirm pedal values 0–89, returning to zero. This deliberately
gentle drive is a recorder qualification, not a representative force workload.

Force case `75b8655112ac485b97a854f1b3344815` completed 3,601 rows. The standalone
x86 fixture strictly decoded the V3 envelope and replayed the actual producer:
all ordered requests and before/after states matched exactly. Data SHA-256:
`D70FE9977B9458112F3EAC8C0DC21B11722CDB9B4D45FD8F2EE96A37A972BB20`.
The result explicitly records virtual admission, nativeLoaded=false,
physicalOutput=false and telemetryDelivery=false. No physical test occurred.

Companion discovery `b280895d03874d97b984de93ab3e39e3` contains 7,200 rows /
3,600 complete pairs over 59.964163 seconds, no unpaired game updates, all
camera rows valid. Hook time: 70 µs median, 2,265 µs maximum. TSV SHA-256:
`0B5F83824EDCBC5813634CAB9055DBF6C863D2CE9ADFB408DE5B128AF7DA7E68`.
These two captures have separately armed windows; join by their recorded game
update identity rather than assuming equal row indices.

The game closed normally with exit 0, the virtual pad exited, and the runner
verified all nine original owner-file hashes, the complete non-Data file
inventory and both installed payload hashes before releasing its lease.
The new private runtime remains installed; settings and saves are unchanged.
Evidence, copied recordings, frames, exact request, module list, installer log,
replay result and restoration receipt are under
`_archive/2026-10-08/outrun-original-20261008-141510`.

The earlier intermittent menu hang remains open. This successful normal exit
does not diagnose it. Gameplay pose ownership/writing, speed units, original
periodic-route capture and physical force normalization remain unqualified.
