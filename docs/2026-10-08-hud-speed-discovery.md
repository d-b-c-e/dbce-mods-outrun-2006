# HUD speed observation before normalization

Passive schema-4 candidate `7852534` was packaged, installed and observed in
offline Palm Beach on October 8 at 15:26-15:34 CT. No force arithmetic, telemetry
output or owner configuration changes. The separate `5b19c9d` original capture
remains preserved as the earlier baseline.

The existing adapter's `field_1C4 * 90` metres/second is labelled an approximate
top-speed conversion. It must not qualify cross-game speed bands. A read-only
inspection of the supported executable (SHA-256
`68CEB386829066F8455B9D027320AF962584321F3E2E8A79C72841495A6134C3`)
found the HUD path:

- At `0x4BCF10`, the game chooses a display multiplier of 1.0 or approximately
  0.6215 from its units setting. At `0x4BCF64` it multiplies that value by
  the float at `EVWORK_CAR + 0x1F8`, converts to an integer and formats `%03d`.
- At `0x4A140F`, the car update stores its computed speed in `+0x1F8`: the raw
  `+0x1C4` input times a car-parameter factor at `+0x2438` times 216.72. A
  conditional low-speed correction/clamp can update that result afterward.

This makes `+0x1F8` a substantially better candidate for matching the HUD than
an assumed universal top speed. It still needs a live displayed-value check;
the static path alone does not qualify physical motion units or vehicle speed.
Private disassembly notes are under `_archive/2026-10-08/outrun-speed-research`.

Discovery schema 4 appends only `hud_speed_observed`, `hud_speed_finite` and
`field_1f8`. A compile-time offset check protects the struct read. It is read
only for the offline local car at the existing pre/post boundary, and never
used as an input to force or telemetry. Non-finite or unobserved values retain
their quality flag and do not become a measured zero or invalidate unrelated
car/camera observations. The analyzer still accepts schemas 1–3 and reports
no HUD speed for those captures. V3 binary force recordings are unchanged.

Validation: 410 production discovery checks, 14 analyzer tests and the x86
Release build pass. Tests cover exact serialization, old-capture absence,
non-finite exclusion, contradictory quality flags and retained car/camera data.

## Original muted moving check

Runtime source `78525341fc356f492a7291d9245f7e05f2440d00`, proxy SHA-256
`4CE061539382908CC5CF895913292CB75E5FBB273A67AFF6E2AE18BB78EA5D1D`, private
ZIP `9014FDD69BD82CDCAD18BE00B43F3EE94E7FF3BB4E3D6083949382F066BB750A`.
The 41-export native pin is unchanged. PS 5.1 actual batch installer checks passed.

Frame-guided navigation reached offline Palm Beach in the Dino 246 GTS with
Automatic transmission. One bounded 20-second 70% virtual-pad throttle request
returned to neutral; no steering or brake input was sent. Physical force,
controller rumble and telemetry delivery were blocked for the whole process.
The owner's 2560x1440 presentation was retained.

Discovery `2620f9e8e9f24475bcd2412e3ceaf732` completed 3,600 pairs / 7,200 rows
over 59.959027 seconds, with no unpaired updates. All camera and HUD observations
were valid. Hook time was 72 microseconds median / 2,044 maximum. TSV SHA-256:
`44A007897F28414911BC37A663D32E6A9609FBEB1FD7AAAD8BDBCFE0C65A7CA7`.

| Captured HUD km/h | Observed +0x1F8 range during the capture call |
|---|---|
| 51 | 45.6429-55.0283 |
| 103 | 96.1861-115.5121 |
| 18 | 18.0701-18.2347 |

The comparison uses the log's arm wall time plus recorded monotonic microseconds,
the screenshot call-to-file-write interval and a 100 ms tolerance on each end.
It is not a tick-synchronous screen readback. The stable 18 km/h frame and the
two changing frames support the executable-derived HUD interpretation for this
car and km/h setting. Maximum observed HUD base was 124.1652, while raw +0x1C4
reached 0.91287: the existing universal raw*324 km/h estimate would reach 295.77.
This evidence qualifies a HUD-derived game speed, not physical position units,
all vehicles, or the mph branch. Any telemetry replacement needs separate review.

Companion force case `e66712a8e687450fa15013f9b48b8a7b` completed 3,600 original
rows; strict standalone producer replay matched every request and state. Its
data SHA-256 is `72F97177B0DFF64DDF7B9CB0EC1A564308AFD5CCC4112B856747BA192DCE2826`.
Both windows cover updates 22,140-25,739; join using those identities, never a
presumed row index. The force file still explicitly labels its own raw speed
unqualified. This simple automated drive is not an Art normalization workload.

The game and pad closed normally. Nine owner files, complete root file inventory
and both installed payloads were verified exact before the lease was released.
Evidence, sealed captures, replay, source hashes and `hud-frame-comparison.json`
are under `_archive/2026-10-08/outrun-hud-20261008-152635`.
