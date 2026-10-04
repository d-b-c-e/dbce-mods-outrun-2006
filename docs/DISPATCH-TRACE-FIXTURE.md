# Ordered dispatch fixture, not gameplay playback

Test-only candidate over the existing integer return interfaces in
`src/wheel_input_gate.cpp`: Current/Previous analog channel 0 steering, 1
accelerator, 2 brake; SwitchOn/SwitchNow requested masks; VolumeSwitch. No hook,
backend, device, runtime/default or installed-file changes. This fixture uses
fake callbacks, not actual production trampolines; it verifies protocol mechanics
and cannot qualify game integration.

Capture forwards each query exactly once and returns its exact existing result.
It stores ordered query kind, argument, millisecond tick and integer result in
128 fixed entries with a 2000-ms span bound. Same millisecond means only poll
group, not game frame. No added device polling or full-mask queries. The fixture
does not normalize analog units or translate backend-specific chord/edge results.

Capture cancels on blocked focus/UI, backward time, duration/count overflow or
unsupported queries. Explicit Stop completes only a nonempty nonfailed trace.
Fake playback owns a bounded validated copy and requires exact ordered query
kind/mask and relative sample tick. Mismatch, blocked focus/UI, cancellation or
exhaustion disarms it and leaves output untouched. This strict scheduling is an
offline oracle; wall-clock game replay is not supported. Production routing on
refusal must retain its existing input isolation gate, not blindly forward held
hardware input.

Run `tools/tests/Test-DispatchTrace.ps1` with VS x86 build tools. It compiles only
the standalone fake fixture with warnings as errors and executes no game or
payload DLL. Tests cover current/previous distinction, chord versus edge return,
same-tick query order, immutable replay input, bounds, invalid/incomplete trace,
cancel/focus/UI, timestamp/query mismatch and no last-value hold on exhaustion.

## Shared metadata compatibility boundary

The inspected shared contract is `dbce.wheel.session` version 1, with metadata,
sample, marker and footer records. Useful metadata names are Game,
PluginVersion, ToolkitVersion, StartedUtc, Properties and ChannelUnits. A future
native export adapter should use OutRun2006 PC identity, source/config hashes,
query-interface version, `clock=monotonic-ms-poll-group` and explicit native
return units. It must not claim game frames, normalized controls or physical
forces. No .NET dependency or unverified generic `dbce.session` schema is added.

No serializer is added here: shared v1 samples require strictly increasing times,
whereas this trace intentionally preserves several ordered queries at one tick.
Before export, establish a reviewed update identity or an explicit ordered-query
payload mapping; do not invent timestamps to make it fit. This is the smallest
protocol fixture preceding that adapter, not a wire-compatible shared session.

Next integration gate: establish an actual game update boundary, then test the
same observation callback in existing final input-return dispatch without new
hooks/backend switching. Capture controls, disk export, attended driving capture
and any gameplay playback require separate review. No usable real driving trace
or deterministic game-state restoration is established.
