# Read-only local-car tick discovery

STAGE-PLAYBACK step 1. Claude, 2026-10-07. Source and fixtures only: not installed
or live-tested. Independent review closes all four original findings in 069fe41:
100 discovery checks, production lifecycle suites and 33 additional fault checks
pass. Packaging is clear from source review; live tick/output-mute qualification
is still separate. See [the review and evidence](2026-10-07-tick-discovery-review.md).

## What it records

The recorder observes both sides of `GamePlCar_Ctrl` (RVA `0xA8330`) inside the existing `DirectInputFFBHook`:
- **pre**: before `FFB::Update`
- **post**: after the original call

It adds no new inline hook. The `Vibration` hook on the same address is reported only as enabled or not;
`chainOrder:unobserved`.

Each row holds named values:
- **Our counters and timing:**
  - phase
  - `car_instance`: 0 for the first local car object, 1 for a replacement row
  - our update index (one per real `ReplaceGameUpdateLoop` update, never per render)
  - microseconds since arming
- **Game state:**
  - `sprani_num_ticks`, `app_time`, `power_on_timer`
  - `current_mode`, `game_mode`, `stg_stage_num`
- **Car (EVWORK_CAR):**
  - identity: car id, kind, colour, manual transmission
  - `flags_4`, `cur_gear_208`, `pedal_amount_34`, `field_1C4`
  - `position_14` and `spd_mb_20`
  - matrices `0x70`, `0xB0` and `0xF0`

There is no raw memory blob. Matrix roles, units and interpolation stay unqualified.

## Arming and bounds

Arm with `tools/Arm-TickDiscovery.ps1 -Seconds 10..120`.
- **The request:** `%LOCALAPPDATA%/Dbce/StagePlayback/outrun-discovery/request.txt`, containing exactly `action`, `id`,
  `seconds`, `expiresUnix` (at most 10 minutes ahead) and `gameSha256` (must equal `ExactDiskSha256`).
- **Polling:** once a second from the update loop, never from DllMain. A window can be armed before launch or
  while the game is running.
- **Refusal:** the request is refused unless the exact-build host check passed and the car hook is active.
  - Refusals go to `request.refused.txt` plus `refused.txt`.
  - An accepted request moves into `<id>/request.txt`.
  - An existing result folder is refused.
- **Memory:** reserved at arming: `seconds × 60 × 2 + 1024` rows. Hitting the bound fails the window; it never wraps.

## When a window stops

Rows are retained on ordinary stops and observation failures when serialization
and storage succeed. `outcome.txt` is written last, with counts and
`replayable=false`; read its `dataFile` because a retry may use
`discovery.retry.tsv`. A missing/failed outcome is incomplete evidence. Memory or
storage failure can prevent row recovery; a best-effort failed marker does not
mean the rows were saved.

| Outcome | When |
|---|---|
| `observed` | `seconds × 60` updates passed, the last pair closed, and the data file committed |
| `ended` | a different local car object (same model included), car identity or `stg_stage_num` changed (the changing row is kept) |
| `failed` | online/LAN driver or lobby (relocation-aware vtable check), a car that is not event 8, two pre, post without pre or on another car, pre unmatched across updates, a skipped update, non-finite value, capacity |
| `stopped` | `<id>/stop.txt` (`Arm-TickDiscovery.ps1 -StopId <id>`) |
| `exit` | normal exit at the outer-loop boundary before the window ended (an open pre says `unmatchedPre=true`) |

**Write cost:** files are written when the window closes, from the game thread,
temp file then rename, with one alternate-name data retry. A 120 s window produces
about 10 MB of text. This can hitch; duration is not runtime-qualified. There is
no background worker.

## Tests

`tools/tests/Test-TickDiscovery.ps1` builds the production header for x86 (`/W4 /WX`) and runs 100 checks:
- request parsing and every refusal
- pair ordering and per-update accounting, including the arming update's own car tick
- capacity
- car and stage ends
- the exact files written, no re-arm, stop, network, exit
- finalize-once

The production FFB, settings and lifecycle fixtures link inert stubs. The host
fixture asserts discovery finalizes after FFB/input and gate completion, before
the game's cleanup, and injects a discovery exception without interrupting exit.

## Not claimed

- No replay, writer or ownership.
- The force mute (step 2) is not done.
- The first stationary run below used the packaged/installed private candidate
  `dinput8.dll` `73432838D92B...`. This is not a public release or playback qualification.
- The stationary window observes one hook pair per recorded update and changes
  inside the call. Moving-car behavior, units, complete pose ownership and matrix
  roles are not established.

## Closing safely (after Astra's review, 2026-10-07)

- **Exit order:** at exit, discovery runs **after** FFB, DInputRemap and InputManager finalization and after the
  lifecycle gate completes, inside `catch (...)`. A failing save can no longer skip mandatory cleanup or strand the
  gate.
- **Containment:** every entry point the game calls (update, car tick, exit) contains exceptions. A fault stops
  discovery for the session.
- **Commit rule:** the requested outcome is published only after the data file commits. A blocked `discovery.tsv`
  is retried once as `discovery.retry.tsv`, never over an existing file; otherwise the outcome is `failed`
  ("data file could not be written ..."). `outcome.txt` names the file in `dataFile=`.
- **Memory:** the paths a close needs are reserved at arming. A close that runs out of memory writes a fixed-buffer
  `failed` outcome without allocating.
- **Window end:** a pre still waiting for its post fails the window ("pre without post at the window end"). Stop and
  exit keep their own outcome and report `unmatchedPre=true`.
- **Tests:** 100 checks, including injected allocation failure while closing, blocked data, retry and outcome
  files, the window-end and stop boundaries, and same-model car replacement against a same-instance control. The
  host-lifecycle fixture's discovery stub throws `bad_alloc` after cleanup and the gate, proving the exit path stays
  intact. Astra's frozen negative control in her review evidence is left as is.

## First live window — 2026-10-07, 12:32-12:36 CT (Claude)

Reviewed package `build/packages/outrun-069fe41-tick-discovery` (clean 069fe41, review-only, 45 files, `dinput8.dll`
73432838...) was installed to the Stream Deck copy with its own installer and backup, so the f021736 proxy E1845549
was replaced.

**One slot.**
- **Gates:** Rig-Lease and idle ≥ 310 s. The 8 owner files were backed up on disk at run start.
- **Temporary overrides:** pinned virtual pad, FFB/telemetry/rumble off, driving axes -1.
- **Launch:** a plain EXE launch; the game came to the front on its own at t+20.
- **Route, each step checked from a fresh game-window frame:** Start (title), A Single Player, Right to OutRun, A,
  A (course OutRun2), A (Dino 246 GTS, Novice), A (Automatic). At music select I armed
  `Arm-TickDiscovery.ps1 -Seconds 60` with the lease token, then pressed A (Splash Wave). Countdown and start line
  followed, with no driving input.

**Outcome** `4025516d4d684460805ff899b2de52e4`: `observed`, duration ended.
- **Timing:** armed 12:34:47.764 and observed 12:35:47.890 in the game log.
- **Counts:** 5,878 rows, 2,939 pairs, 3,600 updates, 2,817 game updates, `gameUpdatesWithoutPair=0`,
  `maxPairsPerUpdate=1`, `unmatchedPre=false`, `dataFile=discovery.tsv`, `directInputFfbCarHook:1`.
- **Close:** normal exit code 0, owner files restored exactly, lease released. The reviewed proxy stays installed.

**What the window shows (stationary kart at the Palm Beach start line):**
- **One car tick per real update.** Every in-game update had exactly one pre/post pair (121 pairs in mode 13,
  2,818 in mode 16). `sprani_num_ticks` was 1 except for 12 catch-up updates (8 and 2).
- **Observed update boundary.** `position_14` and `matrix_70` changed inside the call in 2,700 of
  2,939 pairs. Each recorded post equals the next recorded pre for these fields.
  That comparison cannot exclude intervening writes which restore the same value,
  or establish ownership of the complete rendered pose. Matrix_B0 and matrix_F0
  changed inside 2,901 and 1,898 pairs respectively; their roles still need research.
- **Small stationary variation.** `field_1c4` stayed at or below 0.00016; position
  x ranged from -5.58e-5 to 7.83e-5, y from 0 to 0.02025 and z near -16.
  Units and coordinate frames still need a moving window.
- The median call took 114 µs (max 737 µs).

**Limits:** stationary, one stage, one car, no driving input. The matrices' roles, units and the moving-car
behaviour are not established, and nothing here qualifies a writer or replay. Evidence (local, private):
`E:\Source\_archive\2026-10-07\outrun-discovery-20261007-123210` (11 frames, game log, owner-before copies, and
`discovery-4025516d` with the request, arming provenance, data and outcome).

**Independent readback:** [rows, frames, package and restoration checks](2026-10-07-live-discovery-readback.md)
confirm 2,939 ordered pairs, 45 package files, three installed runtime files and
eight restored owner files. The car rows span 48.958 seconds within the 60-second
observation window; 12 catch-up pairs have ticks greater than one. Normal exit
code 0 remains Claude-reported. No new runtime test was needed for this review.
