# OutRun recording/playback intake — 2026-10-05

Gameplay recording/playback is **not implemented** in this PC adapter. The
existing bounded recorder and replay fixture operate on force calculations,
not a drive through the game. No owner installation, force setting, display
setting or native hook changed during this intake. DRIVE's completed diagnostic
allowed this follow-up to begin; its Unity adapter cannot qualify these native
x86 hooks.

**Later startup recovery:** Claude's installed-copy test crashed with missing
script data. The 110 absent script/BK assets are restored and hash-verified;
native hooks, settings and saves remain unchanged. Run `tools/Test-GameData.ps1`
before a new launch. Claude's 15:38 CT test reached title and exited normally;
native title confirmation and an offline driving route still need qualification. See
[the repair evidence](STARTUP-ASSETS-2026-10-05.md).

## What exists and was checked

Source `cfc6a6b` includes a production-linked numeric calculation seam:

- `src/ffb_calculation.inl` separates `CalculateSignals` and its constant/periodic
  sink. `src/signal_recording.inl` captures original numeric inputs, complete
  legacy model state/configuration and emitted requests into preallocated memory.
- Capacity is **128 calculation frames**, roughly two seconds at 60 Hz. There is
  no external arm, background writer, runtime save or normal-exit capture control.
  An internal Begin/Take API is not a usable owner recording workflow.
- `src/signal_recording_codec.hpp` is an offline-only `DBCEORR2` reader/writer,
  bounded to 256 KiB, with explicit completion, continuity and corruption checks.
  It rejects named/shared profiles; never silently switch an owner's profile to
  make a capture fit. It records requests before device encoding, not measured
  force or every application-level stop/zero event.
- Actual x86 `tools/tests/Test-SignalCalculation.ps1` passed again on October 5:
  48-frame warmup/shift/crash/water calculation, recorded state/request roundtrip,
  rejected admission, reset/tuning changes, corrupt/truncated/invalid captures
  and mismatch exits. No native device, game or UDP output. Private evidence:
  `build/signal-calculation-6b315b4c2a0c422aac4f6d1ea2570962`.

See [the existing format](BOUNDED-SIGNAL-RECORDING.md). The older
[prerequisite report](SIGNAL-REPLAY-PREREQUISITES.md) predates the implemented
calculation seam; its no-seam statement is superseded by that format and tests.

## Verified source boundaries for the next implementation

| Boundary | Finding and consequence |
|---|---|
| Simulation hook | `DirectInputFFBHook::GamePlCar_Ctrl_Hook`, RVA `0xA8330`, calls FFB Update **before** the original function. This is not evidence of post-solve pose authority. Vibration independently hooks the same function; chaining/order must be observed. |
| Frame clock | `Game::app_time`, `sprani_num_ticks` and `CalcNumUpdatesToRun` exist. The number of ticks per render can be zero or multiple. Verify the actual simulation phase; never record one pose per rendered frame and call it 60 Hz. |
| Car data | `EVWORK_CAR` is size `0x10F0`; position at `0x14`, velocity-like vector at `0x20`, matrices at `0x70/0xB0/0xF0`. Record individually named observed values for discovery. Matrix roles, units, interpolation, recovery and camera writers are not yet established; no raw memory replay. |
| Scenario | `game_mode`, `current_mode`, `stg_stage_num`, car ID/kind/colour/transmission and online driver/lobby pointers exist. `STATE_GAME` alone does not establish offline practice eligibility, durable starting state or a safe native restart. |
| FFB calculation | `FFB::Update` returns on `DirectInputFFB=false` before `CalculateSignals`. Disabling the producer would destroy the intended force stream. Separate capture/calculation from output admission; do not report an absent stream as zero. |
| Wheel delivery | `SetConstantForce`, `UpdatePeriodic`, DeferredInit/LoadApi, selection and watchdog paths share consumer lifecycle/device ownership. A recording mute must prevent device acquisition as well as nonzero delivery and remain latched after Stop. Preserve actual requested data and delivery status independently. |
| Other output | `Telemetry::Write` emits both shared memory and Forza UDP. `SetVibration` calls SDL through `InputManager_SetVibration` and possibly XInput; SDL also has trigger rumble. Capture before all relevant delivery gates. Suppressing only WheelFfb is insufficient. |
| Startup/shutdown | Plugin_Init runs from DllMain under the loader lock. Do not create/join a recording worker there. Use an established outside-loader-lock game boundary; join after producer leases drain at normal outer-loop finalization. No hard-kill recovery with a connected wheel. |

The historical native title confirmation failure reproduced on both candidate
and baseline. Without a working native startup route, simulated Return presses
would not prove unattended launch. See `docs/NATIVE-INPUT-DIAGNOSIS-2026-09-19.md`.

## Next concrete implementation and qualification

1. Add an externally armed, expiring, bounded **read-only discovery** recording
   around both sides of the actual local-car tick. Record phase, game tick,
   mode/track/car identity and named numeric pose/camera fields. Reject online,
   ambiguous player, changed car/stage, reordered ticks and incomplete output.
   Keep discovery distinct from a replayable trajectory.
2. Refactor the existing calculation seam so muted capture retains normal
   configured model requests without creating a device. Preserve original
   state/config/profile identity and label unavailable channels. Exercise all
   stop/selection/watchdog/network/rumble paths with fake sinks before installing.
3. Add bounded queued disk output, an exclusive new capture directory, correlated
   external Stop/status, complete seals and normal-exit failure handling. Reuse
   toolkit request/seal/supervision contracts, with a native x86 implementation;
   do not embed a second CLR or copy game-specific Unity hooks.
4. Qualify native offline menu/startup and the body/camera writers. Only then add
   pose ownership and following-tick verification including the final sample.
   Preserve original forces; a reproduced pose cannot regenerate tire/contact
   data. Rendered HUD/camera/wheel state needs its own checks, as DRIVE showed.
5. Package/backup/install a clean candidate under the shared rig lease, then
   collect a clearly labelled owner drive and replay it in a separate process.
   Preserve settings and the complete sealed reference outside the worktree.

There is intentionally no launch command claiming that these missing runtime
controls already exist. Offline force regression can be run now with:

```powershell
./tools/tests/Test-SignalCalculation.ps1
```

Toolkit handoff: `E:/Source/toolkits/dbce-wheel-mod-toolkit/docs/RECORDING-PLAYBACK-RUNBOOK.md`
and `knowledge/RECORDING-PLAYBACK-LESSONS.md`, especially transient scenario IDs,
presentation ownership and the original-signal/physical-output distinction.
