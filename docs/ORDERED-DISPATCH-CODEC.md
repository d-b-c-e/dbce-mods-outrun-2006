# Offline ordered dispatch codec v1

Review candidate extending the published test-only dispatch fixture at
`1ec0aab83371fdfc5def6742d24dcacdab35f801`. No production input hooks,
runtime capture/playback writer, backend, default, toolkit or installed-file
changes. Gameplay playback remains unqualified.

Prior art inspected: existing `src/signal_recording_codec.hpp` uses explicit
little-endian numeric fields, a bounded complete footer and corruption checksum;
shared `Dbce.Wheel.Recording` documents `dbce.wheel.session` v1 with strictly
increasing sample times. Dispatch queries can share one millisecond tick, so
this codec keeps exact order without inventing frame IDs or timestamps. It is
not wire-compatible with shared sessions and does not introduce a generic
`dbce.session` format or a .NET dependency.

## Exact format

All integers are little-endian. No native structure padding is serialized.

| Field | Bytes | Meaning |
|---|---:|---|
| Magic/version | 8 | ASCII `ORDIQ001`; unknown versions refused |
| Count | 4 | 1..128 queries |
| Source SHA256 | 32 | Caller-declared consumer source identity |
| Config SHA256 | 32 | Caller-declared config identity, no embedded config |
| Each record | 20 | Sequence, kind, argument, absolute millisecond tick, signed int32 result; each 4 bytes |
| Footer | 20 | `END!`, completed=1, repeated count, FNV-1a-64 over all preceding bytes |

Kinds retain existing fixture values: 0 Current, 1 Previous, 2 SwitchOn,
3 SwitchNow, 4 VolumeSwitch. Analog/menu arguments are channels 0..2; switch
arguments are nonzero requested masks. Result bits preserve the full signed
int32 return, including backend-specific chord/edge behavior. The codec performs
no normalization and does not infer result ranges for backend-specific returns.

Maximum file size is 2656 bytes (76-byte header, 128*20-byte queries, 20-byte
footer). Ticks are nondecreasing, first-to-last span at most 2000 ms; wrap or
backward ticks are refused. Same tick means millisecond poll group, not proven
game frame. Sequence is mandatory and contiguous. Footer, count, checksum,
version, query arguments, exact total length and trace validity are checked
before admitting a decoded trace. Incomplete captures cannot be encoded.

Hashes are caller supplied, not authenticated. FNV detects accidental corruption,
not malicious editing; tests reseal intentionally malformed records to prove
structural validation independently of checksum. External fixture SHA256 binds
the exact emitted file. No pointers, GUIDs, paths, game memory, private assets
or credentials enter the format.

## Tests and review boundary

Run `tools/tests/Test-DispatchCodec.ps1` with VS x86 tools. `/W4 /WX` and an
explicit asInvoker manifest are used. Tests cover canonical memory/file
round-trip, preserved declared hashes, decoded fake playback preserving exact
query order/current-previous/chord-edges, every truncated prefix, every single
byte corruption, trailing data, and resealed version/count/sequence/kind/
argument/footer/time errors. Signed int32 extrema, exact capacity and incomplete
encode refusal are tested. The existing dispatch fixture remains independently
available for cancel/focus/UI/count/time/exhaustion behavior.

Only the synthetic test executable writes a new fixture file in its unique
build directory. The reusable codec has no filesystem, device or runtime
injection API. Fake playback still requires exact relative ticks and queries;
it is an offline comparison oracle, not wall-clock gameplay playback.

A future shared adapter can use the established Game/PluginVersion/
ToolkitVersion/StartedUtc/Properties/ChannelUnits metadata conventions and
explicit query-interface/clock units, but equal-time ordered events need a
reviewed mapping first. No actual update identity or game-state restoration has
been established. Runtime integration, controls, live capture and any playback
require separate review; publication/deployment is held.
