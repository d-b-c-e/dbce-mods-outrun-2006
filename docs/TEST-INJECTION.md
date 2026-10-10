# Test injection for the DirectInput remap (development only)

`src/remap_inject.cpp` lets an automated run drive the game through the rig profile without OS input. It follows
STD-033 section 6 in the wheel-mod toolkit; the grammar and the injection table are the toolkit's, vendored in
`src/vendor/controls/`.

## Where samples enter

`PollSlot` (`hooks_inputremap.cpp`) reads each slot's `DIJOYSTATE2`. When armed, the running samples for that slot's
DirectInput instance replace their objects right after a successful read. That is before any binding, calibration,
edge, H-pattern or UI read, so the game takes an injected sample exactly as it takes the wheel:
- Axes are mapped into the range the device reports (`DIPROP_RANGE`).
- Buttons and POVs beyond the device's capabilities are never delivered.
- Lost input (`DIERR_INPUTLOST`/`NOTACQUIRED`, before the reacquire), a failed read, or a released slot (UI
  replacement, unused input, exit) drops that instance's samples.

## When it arms

Once, at startup, right after `ProfileControls::ApplyAtStartup`, and only when all of these hold:
- The process is signal-muted: `DBCE_OUTRUN_SIGNAL_MUTE` is set at its start (`signal_mute_policy.hpp`). That blocks the
  mod's FFB, the game's own effects and rumble until the process exits. No INI edit or F6 change can undo it.
- `%LOCALAPPDATA%\dbce\outrun2006\inject.on` names a session: `nonce=` (8-64 letters/digits) and `expires=` (unix
  seconds, at most an hour ahead).
- The `[WheelkitProfile]` request in `OutRun2006Tweaks.user.ini` is applied (not pending).

## Commands

They come from `%LOCALAPPDATA%\dbce\outrun2006\inject.txt`, read at most every 100 ms:
- Each version is read once, and only if it is at most 4096 bytes.
- The first line is `nonce=<the session's>`; then at most 32 commands.
- A session accepts at most 2000 commands. At expiry its samples are dropped and injection is off.

The command forms:

    inject raw <axis|button|hat> <index> [angle] dev=<instance> value=<raw> ms=<50-15000> [range=<min>..<max>]
    inject action <id> <n> ms=<50-15000>      (resolved through the applied profile request)

Use raw commands generated from the original rig profile (Wheelkit `raw-workload`) for qualification. `inject action`
resolves through the same applied store the game reads, so it cannot catch a wrong translation.

## Tests

`tools/Test-RemapInject.ps1` runs `tests/remap_inject_test.cpp` (MSVC x86, `/W4 /WX`) in two processes:
- unmuted: a request is refused even with everything else in place. This process also covers delivery, ranges, missing
  objects, actions, timing, drop on failure/release, the file caps, expiry, the `inject.on` grammar and the observer.
- `--muted`: no request, a pending profile, a session beyond an hour and a missing session are each refused; an
  applied profile arms.

## Observer

A signal-muted process publishes no telemetry: no shared memory and no Forza UDP, because SimHub can turn those into
shaker output. While injection is armed, each FFB update copies the game's own car words into
`Local\DbceOutRunInjectObserve-<nonce>`. That is a mapping only the run's observer knows; nothing else reads it. It
holds 48 bytes, `RemapInject::ObserveData` (version 2):
- `speed` and `steer`: EVWORK_CAR `field_1C4` and `field_1D0`, as OutRun2006Telemetry has them;
- `pedal`: `pedal_amount_34`, the game's throttle;
- `steerVolume`, `accelVolume`, `brakeVolume`: what the game last read through the remap's `GetVolume` hooks. That is the earliest point the game consumes the bound input, and it is where steering is observed: `field_1D0` is not the steering position (see the field probe in `hooks_dinputffb.cpp`; a 2026-10-10 run kept it within ±0.004 while steering);
- `gear`, the game mode, in-gameplay, and `packetId`.

`packetId` is a sequence: it is odd while an update is written and even when the update is whole. A reader keeps a copy only
when it saw the same even value before and after reading. The mapping is created by the game or not at all: one that
already exists (another writer, or a reader that made it first) is refused, and nothing is published for that run.

Nothing is published when not armed.