# Native lifecycle candidate

OutRun adopts toolkit `50ba139bcaee14aee080abe438d6beec4f6a2b47`, tagged
`native-0.6.0-retained-identity`: x86 native API 600, 41 exports, SHA-256
`432727681B1A8A6F62D68F2E1ED865FF9F229AECCC628B82FA97147626451968`.
The numeric model/profile/header baseline is retained. No force-model, strength,
input mapping, telemetry or camera change accompanies this native update.

This is the reviewed locking/shutdown and strict retained-device successor to
f8f0619, not the DLL inside the old v0.13.0 release archive. On strict mismatch,
the retained device/effects are released and initialization is refused while
input readers, watchdog and exit guards survive. A later explicit initialization
may open the selected device. Both architectures passed production fake-device
identity/burst/worker tests and actual-DLL 41-export ABI/cleanup/restart/unload
checks; Claude cross-reviewed them. No real device was acquired.

The consumer's separately reviewed accepted-zero startup (a980945) is included.
Native version 600 alone is insufficient to identify the fix; the source/hash
and build evidence live in NATIVE-PROVENANCE.json. Its old detailed evidence is
retained under historicalBuildEvidence and is not reused for the new binary.

A native-only update now triggers relinking/staging. Packaging also refuses a
runtime native DLL whose hash differs from the provenance it would ship, for
both a build directory and a frozen-runtime repack. This closes the same stale
POST_BUILD-only staging issue found in F-Zero. The real native file in packaging
fixtures is copied/hash-checked only, never loaded.

At this source checkpoint, consumer gates and installation are pending. Existing
moving-camera evidence remains pinned to 6ccd7dd. Stage playback/triples and
physical force calibration remain separate open work; this change does not
promote them to Ready to Test.
