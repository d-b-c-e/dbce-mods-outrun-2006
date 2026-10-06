# Focused OutRun input follow-up

Codex, October 6. Review of Claude's `c3433f1` report and the 11:21/11:28
tests of installed runtime `b43445c`, DLL
`1E61F51AB877AD317EDB4B9C7AC61DCECA5DD52B5BD4D6A083B70C8F01CD8276`.

## Findings

The focused retry is no longer blocked by Windows Security. The first run froze
after at least one completed remap poll; the second reached title/attract and
closed normally, but did not enter a menu. Recording/playback remains unqualified.

The responsive run **did observe button input** on its selected primary:

- 11:28:36.573: button 7, coinciding with the reported Start press.
- 11:28:40.957, 45.324, 49.724, 54.108: button 0, coinciding with A presses.
- That device reports ten buttons. The preserved wheel configuration maps
  Start to 34 and A to 31, both outside that device's range.

This establishes a remap binding mismatch, not a measured trace of the native
XInput fallback. Do not infer physical/virtual device identity from the display
name `Controller (TS-UFB01B-X)` alone. Its observed instance GUID was
`{048BA480-601B-11F1-801A-444553540000}`; verify the current instance on a new
attempt rather than silently selecting the device with the most axes.

An additional production defect was reproduced without a game or device:
`Poll` and keyboard edges used `GetTickCount` as if it were a game frame ID.
A second query after one millisecond could update previous/current state and
erase the rising edge before the consumer queried it. Conversely, two catch-up
updates in the same millisecond shared a snapshot. UI snapshot reads could also
advance the same device's previous state inside one update.

## Source fix and tests

The existing `ReplaceGameUpdateLoop` now advances the remapper's input token
once at the start of each actual input update. Axis, button, POV, keyboard and
UI reads share a device snapshot for that update. Queries do not resample based
on elapsed milliseconds. Catch-up updates each advance; render-only iterations
do not. One bootstrap snapshot remains available before the update loop runs.
H-pattern cooldown now follows those same updates. The lifecycle admission,
focus/held-release gates, device selection, saved bindings and force model are
unchanged. Startup also logs the exact A/Start mappings alongside the existing
device capabilities.

The production-linked x86 fixture first failed on the old code with
`elapsed milliseconds inside one update must not erase a button edge`.
The corrected fixture passes button/POV/keyboard edge stability, UI reads,
held/release, same-millisecond catch-up, cooldown and pause/resume cases. It also
reproduces the saved 31/34 versus observed 0/7 binding mismatch and verifies
explicit 0/7 mappings using fake state. These are not live menu acceptance.

`Test-WheelSettings.ps1` passes its actual settings/UI, FFB gate, input and x86
dispatch suites, including the existing 34 mixed-mask and 144 legacy axis cases.
`Test-ConsumerLifecycle.ps1` passes fake-ABI drain/selection/pause and the exact
EXE/synthetic-host cleanup tests. No game, wheel, OS input or force was used.

## Freeze remains open

There is no dump or wait chain from the frozen process. `check3/probe.txt` samples
the **responsive** run, and its stack-like address scan is not an unwound stack
from the freeze. The custom WndProc in `overlay/hooks_overlay.cpp` and its
`SuppressTextMessage` path do not take a consumer lease. The lifecycle window
subclass uses nonwaiting pause/defer logic. DirectInput calls do run under the
operation lease, so locking still deserves inspection if a frozen-process dump
implicates it; the proposed window-message mutex cycle is not established by
these logs. No lifecycle-lock refactor is included. The first successful poll
also means the last four POV log lines do not prove it froze inside that poll.

## Next bounded check

Use the packaged candidate after source review and the normal closed-game,
idle/lease checks. Preserve the owner profile byte-for-byte; these overrides
are **temporary menu diagnostics**, not a new wheel default:

- Pin the verified input instance instead of `auto`.
- In `[DirectInput]`, map `ButtonA=0`, `ButtonStart=7`; set `SteeringAxis`,
  `AccelerationAxis` and `BrakeAxis` to `-1` for menu-only checking.
- Clear `[DirectInput.Shifter] DeviceGuid` and `[DirectInput.Aux] DeviceGuid`
  for this one attempt. Preserve their saved values in the backup.
- Keep `[FFB] DirectInputFFB=false`, telemetry off, and explicitly set
  `[Controls] VibrationMode=0`; disabling wheel FFB does not disable pad rumble.
- Connect the approved pad before the ordinary no-argument launch, require
  natural focus, release to neutral, then one Start and one A as separate events.
  Stop at menu entry or the bounded timeout; no repeated A-only loop.
- On a new freeze, preserve an owned-process dump/wait chain before recovery
  so that the next change can target actual blocked threads. Never treat the
  responsive-process probe as that evidence.

Restore the original INI/saves/log and verify hashes after closure. Native race
entry, input during driving, physical wheel/FFB and gameplay replay remain separate
checks. No new live test or owner installation was performed during this fix.

## Retained evidence

All 29 focused-run/prior-build evidence files are copied with matching hashes
outside the checkout:
`%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/outrun-focused-input-20261006`.
Manifest SHA-256:
`30F437FB49103B8C1102E0B079D946D9A7A2F794C2E653F9FE70A1C7A498D469`.
This includes both focused-run frames/logs, the responsive probe, prior DLL/PDB
and the failing fixture log.

## Candidate package receipt

Clean committed source `f021736df202cdccf4c2c707b0e61a547b3097d6` was built as
x86 Release using the existing `build/mixed-switch-candidate` tree; both changed
runtime translation units compiled. This is an incremental build from clean
source, not a fresh build directory. The private review package is
`build/packages/outrun-f021736-input-tick-review` (same path plus `.zip` for the
archive). All 45 inventory entries and the notice bundle validate; unresolved
distribution notices keep it review-only.

- ZIP SHA-256: `643B2C77BB515E081E653EC6576F665CBAB457ACA86F5F1ACE7D5B7698C15208`.
- Candidate `dinput8.dll`: `E18455496EFB59EB33A9F592889F9B12991C1519EB8A8E6D37268AF132D1AE78`.
- `WheelFfb.dll` and `force-profiles.ini` match the current installed files.
- The exact package passes `Test-WheelInstallPlayer.ps1`, including actual
  Windows PowerShell 5.1 and batch entry points, settings retention, restore,
  rollback and failure exits. Its synthetic EXE was never launched.

The package, ZIP, matching PDB, source ZIP and six red/green/build/package/test
logs are retained as 55 hash-verified files outside the checkout at
`%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/outrun-input-tick-candidate-f021736-20261006`.
Manifest SHA-256:
`FDFC85B16A63BEAC0DFA56BE3C59F96400989B3432009954E974501FA369B79F`.

This candidate is **not installed or live-qualified**. Independent readback still
finds the original installed `1E61F51A...D8276` DLL. The next check is the bounded
temporary-mapping diagnostic above, coordinated with Claude; a menu-entry pass
would not yet qualify race input or recording/playback.

## Bounded menu check on f021736 (Claude, 2026-10-06 12:45)

Source review first: `BeginInputTick` once per real update in `ReplaceGameUpdateLoop` (whose `validate()` is
unconditional), per-slot `lastPollTick` so a slot is sampled once per update, keyboard held/edge masks taken in
the same `Poll` snapshot. Looks right. One note: if that hook's pattern ever fails to apply, `inputTick` stays 1
and remapped input stops after the bootstrap poll; a logged fallback (for example resample when no tick arrived for
100 ms) would make that visible instead of silent.

**Device identity:** `Controller (TS-UFB01B-X)` (VID 045E, PID 028E, instance
`{048BA480-601B-11F1-801A-444553540000}`) is the ViGEm virtual Xbox pad from the test tooling: it appears in a
DirectInput enumeration only while `dbce-virtual-pad` runs, with the same instance GUID. With the MOZA base off,
`auto` selected the virtual pad as the primary wheel and applied the wheel's buttons (A=31, Start=34). Auto-select
should probably never take a gamepad-class device as the wheel.

Run: candidate installed with its own installer (backup `.wheel-settings-backups\20261006-174528-537-db557049`,
dinput8 `E18455496EFB...`); owner INI/lods/overlay/imgui/login/log/SaveGame backed up first. Temporary overrides
exactly as listed above (pinned instance, ButtonA=0, ButtonStart=7, three axes -1, shifter/aux DeviceGuid empty,
DirectInputFFB=false, Telemetry Enable=false, VibrationMode=0). Virtual pad before an ordinary no-argument launch.

- t+20 naturally in front; neutral; one Start, then one A, 6 s apart: **menu entry**, the Single Player mode screen
  (Coast 2 Coast / OutRun / Heart Attack) after A; stopped there (4 more captures, no input).
- Responsive at every check, no freeze, normal exit 0. Log:  - DIRemapDeviceGuid: {048BA480-601B-11F1-801A-444553540000}; DInputRemap: Primary opened — 5 axes, 10 buttons, 1 POVs; DInputRemap: Primary confirm bindings — A=button 0, Start=button 7 (zero-based)
- Owner files restored byte-for-byte afterwards; the candidate stays installed. Evidence (ignored, local):
  `build/packages/outrun-f021736-menu-check`.

This is menu entry only: race entry, driving input, wheel/FFB and recording/playback remain separate checks.
