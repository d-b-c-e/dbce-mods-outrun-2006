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
- No live run yet. The x86 Release DLL builds in `build/mixed-switch-candidate` (`dinput8.dll` `73432838D92B...`); it is neither packaged nor installed.
- Whether `GamePlCar_Ctrl` runs once per update, which side holds the solved pose, and what the matrices mean are
  exactly what a first offline window should show.

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
