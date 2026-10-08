# HUD speed observation before normalization

Source-only passive addition; not packaged or installed. No force arithmetic,
telemetry output or owner configuration changes. The installed `5b19c9d` remains
the original force-capture baseline.

The existing adapter's `field_1C4 * 90` metres/second is labelled an approximate
top-speed conversion. It must not qualify cross-game speed bands. A read-only
inspection of the supported executable (SHA-256
`68CEB386829066F8455B9D027320AF962584321F3E2E8A79C72841495A6134C3`)
found the HUD path:

- At `0x4BCF10`, the game chooses a display multiplier of 1.0 or approximately
  0.6215 from its units setting. At `0x4BCF64` it multiplies that value by
  `EVWORK_CAR + 0x1F8`, converts to an integer and formats `%03d`.
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

Next bounded run: retain process mute, original force tune and known offline
route; record schema-4 discovery alongside an original force capture and a few
fresh HUD frames while moving. Compare by recorded update/time identity, not
assumed row indices. Only after agreement should a separately reviewed change
replace the telemetry estimate or add qualified speed to normalization exports.
