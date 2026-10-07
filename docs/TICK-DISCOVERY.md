# Read-only local-car tick discovery

STAGE-PLAYBACK step 1. Claude, 2026-10-07. Source and fixtures only: not installed or live-tested yet, and the
design fits the agreed scope. Independent review of a181519 passes the original
76 checks but reproduces four defects; **hold install** pending corrections.
See [the review and repeatable evidence](2026-10-07-tick-discovery-review.md).
The implementation description below is the reviewed candidate, not qualification.

## What it records

The recorder observes both sides of `GamePlCar_Ctrl` (RVA `0xA8330`) inside the existing `DirectInputFFBHook`:
- **pre**: before `FFB::Update`
- **post**: after the original call

It adds no new inline hook. The `Vibration` hook on the same address is reported only as enabled or not;
`chainOrder:unobserved`.

Each row holds named values:
- **Our counters and timing:**
  - phase
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

Rows are kept in every case. `outcome.txt` is written last, with counts and `replayable=false`.

| Outcome | When |
|---|---|
| `observed` | `seconds × 60` updates passed |
| `ended` | car identity or `stg_stage_num` changed (the changing row is kept) |
| `failed` | online/LAN driver or lobby (relocation-aware vtable check), a car that is not event 8, two pre, post without pre or on another car, pre unmatched across updates, a skipped update, non-finite value, capacity |
| `stopped` | `<id>/stop.txt` (`Arm-TickDiscovery.ps1 -StopId <id>`) |
| `exit` | normal exit at the outer-loop boundary before the window ended (finalized before FFB/input) |

**Write cost:** both files are written once, from the game thread, temp file then rename. A 120 s window produces
about 10 MB of text. Expect one short hitch when the window closes; there is no background worker.

## Tests

`tools/tests/Test-TickDiscovery.ps1` builds the production header for x86 (`/W4 /WX`) and runs 76 checks:
- request parsing and every refusal
- pair ordering and per-update accounting, including the arming update's own car tick
- capacity
- car and stage ends
- the exact files written, no re-arm, stop, network, exit
- finalize-once

The production FFB, settings and lifecycle fixtures link inert stubs. The host fixture asserts that discovery
finalizes before FFB and input, and before the game's cleanup.

## Not claimed

- No replay, writer or ownership.
- The force mute (step 2) is not done.
- No live run yet. The x86 Release DLL builds in `build/mixed-switch-candidate` (`dinput8.dll` `25705CCA8DE3...`); it is neither packaged nor installed.
- Whether `GamePlCar_Ctrl` runs once per update, which side holds the solved pose, and what the matrices mean are
  exactly what a first offline window should show.
