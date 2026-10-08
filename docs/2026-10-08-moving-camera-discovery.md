# Moving car and camera discovery — October 8

The observed car and camera updates have different boundaries. All car groups
change inside `GamePlCar_Ctrl`; all observed camera groups change between those
calls. A car-postfix writer alone cannot own the complete rendered scene.
This is discovery evidence, **not a qualified replay or triple-screen renderer**.

## Exact run

Claude installed the reviewed camera-discovery package from runtime **6ccd7dd**
with its own installer. ZIP SHA-256:
`2276ED98976E799ECD4FADA1FADC0206D1F971C9200CDCAF7D90BCC2014A5321`.
It retains the native FFB pin. The private candidate remains installed.

Request **f409819cfe544ab59680ea64c9ccbf9e** finished `observed`, duration ended.
The native offline route was checked from fresh frames, then a pinned virtual
pad requested gentle straight acceleration for 20 seconds. Wheel force,
telemetry and rumble were disabled. This run does not retain original FFB inputs
and must not be used as a normalization capture. Claude reports normal exit 0,
exact owner-file restoration and lease release.

Original evidence: `%LOCALAPPDATA%/Dbce/StagePlayback/outrun-discovery/` followed
by the request ID. Private archive, owner snapshots, frames and log:
`E:/Source/_archive/2026-10-08/outrun-camera-20261008-032603`.
TSV SHA-256: `71863405C8C88405051F6E42935FA29647862FD59AFC889043D564C15B7CFFF9`.

Independent readback with `tools/analyze_tick_discovery.py` validates schema 2,
200 columns, **6,620 rows / 3,310 ordered pairs**, complete outcome and stable
car/stage identity. All camera rows are observed and finite. The car-row span is
55.138054 seconds; median hook duration 62 us, maximum 1,090 us. No unmatched
pre or missing in-game hook pair was reported by the producer. Catch-up tick
values up to 8 remain part of the time contract; a pair is not necessarily one
fixed 1/60-second physics step.

Frames `i11-throttle-5s` and `i13-end` independently show the Dino moving on Palm
Beach, HUD 18 then 17 km/h, readable camera and picture. The later frame shows
contact with the left barrier. These are single-view frames, not triple-screen
evidence. Recorded position path is 218.15 raw units; units and complete road/
render coordinate relationships are not yet calibrated.

## Update boundaries

| Observed group | Changes inside 3,310 pairs | Changes between 3,309 pairs |
|---|---:|---:|
| Position and speed vector | 3,071 each | 0 |
| Car matrix 0x70 | 3,071 | 0 |
| Car matrix 0xB0 | 3,272 | 0 |
| Car matrix 0xF0 | 2,625 | 0 |
| Camera 0x140 / 0x180 / 0x1C0 | 0 each | 3,141 / 3,138 / 3,141 |
| Camera 0x200 / 0x240 / 0x280 / 0x2C0 | 0 each | 3,071 each |
| Camera position / look target / angle | 0 each | 3,097 / 3,142 / 1,366 |

Equality between observations cannot exclude intervening writes which restore
the same value. These observations establish neither exclusive writer ownership
nor the point at which D3D consumes a matrix.

## Algebraic clues, with limits

Across all 3,310 pre rows, `camera140 * camera1C0` differs from identity by at
most **1.92e-5** per element (median maximum error 4.07e-7). The translation of
1C0 differs from the recorded camera position by at most **4.6e-5** raw units.
This supports a view/inverse-view interpretation. Camera 180 is similar but
not identical: its inverse-product residual reaches 3.22e-4. Do not alias them.

Camera 200 and 240 are identical on every row. Camera 2C0 equals 200 throughout
mode 16, but differs during the intro. Camera 280 commonly matches pre-call car
B0, not post-call B0: mode-16 median post-call difference is 0.132827 raw units.
There are real intro/transition exceptions (pre-match error up to 1.51 in mode
16), so it is not a universal equality or permission to replace either matrix.

Existing source also consumes car B0 in `RestoreCarBaseShadow::CalcPeraShadow`
through `Game::mxPushLoadMatrix`. The existing `FixZBufferPrecision` hook calls
**CalcCameraMatrix RVA 0x84BD0**, including extra calls around scene effects at
**0xBE70**, on the camera object at **0x39FE10**. This is a concrete next
observation point, but extra screen-effect calls mean it cannot be assumed to
run once per physics tick or once per rendered frame.

Private analysis outputs: `build/discovery-f409819c-analysis-20261008.json` and
`build/discovery-f409819c-matrix-relations.json`, both pinned to the TSV hash.

## Next work

1. Preserve force calculation under a separate output mute before any FFB
   capture. `DirectInputFFB=false` currently skips the producer itself.
2. Observe camera calculation versus the outer update/render boundary, retaining
   scene-effect call distinctions. Inspect the native call sites before adding
   a writer. A projection matrix change alone is not three rendered views.
3. Resolve road/car/render coordinates and scenario reset, then implement a
   bounded pose/camera prototype with following-tick verification. Keep this
   discovery format distinct from a replayable session.

No additional stationary title/start-line repeat is needed for these findings.
