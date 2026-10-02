# OutRun 2006 PC native shutdown adoption candidate

**HOLD: not accepted for default promotion, deployment or physical testing.**
Safety follow-up found plain `g_lockInit` publication read by `ApplyHoldTimeout`
while public initialization writes it. `SetHoldTimeoutMs` can start the worker
before the critical section is initialized; the API mutex does not protect the
worker read. Exact source lines 116, 243/510, 1610 and 1634-1636 establish the
unsynchronized path. Normal FreeDirectInput joins before release and does not
reset this flag; DllMain resets it on detach. No runtime data-race detector result
or observed device failure is claimed. A reviewed toolkit fix and newly matched
binary provenance are required before acceptance.

The separately reported low-demand restart defect does **not** reproduce in this
41-export source: after ZeroForces, periodic demand 100 and condition coefficient
100 both return success, issue SetParameters with DIEP_START and mark started.
Periodic `wheel_burst::needs_update` and condition filtering account for stopped
effects. Evidence uses exact-source fake effects; no physical output is claimed.

Consumer parent: `8a84b9daecc5d3446a6bea85cd13d72421445380`.
Toolkit source: `f8f0619b5588f2d11b44f4becd4198775d4a8bcf`, tree
`d9f59422cff93383f9cf27f3cd641fd2adf00a35`.
Selected x86 DLL: 166400 bytes, SHA-256
`4f7ffbb590c30e2a8d33f2e7a3d7e8c770bdd2466053a2333d67c55d98bb08b4`.

This is a matched reviewed local build of published source, not a new toolkit
release. Native version 600 and 41 exports remain unchanged. Only the DLL is
replaced; all consumer headers, force shaping, profiles, settings, defaults and
caps remain byte-identical to the parent. Historical sync hashes for existing
consumer header edits remain preserved. The earlier v0.13 archive provenance
describes the retained ABI/header baseline, not the new DLL.

All 146 source blobs match the published Git tree. All 301 x86 input hashes and
34 bundle receipt hashes were reverified read-only. Compiler 19.44.35229,
toolset 14.44.35207, SDK 10.0.26100.0; static CRT, /O2 /W4 /EHsc /DNDEBUG.
The retained component provenance includes manifest and input-receipt hashes.
Repeat-build evidence isolates timestamp differences; general bit reproducibility
is not claimed. Original bundle and frozen packages remain immutable.

Native cleanup stops effects under its effect lock, drops that lock, joins actual
watchdog completion and only then releases resources. Public API serialization
prevents concurrent cleanup/restart; worker internals avoid joining through the
API lock. The consumer closes admission, drains producers and invokes native
cleanup outside DllMain; modules remain pinned for process lifetime.

Verification uses copied exact-source fake DirectInput effects with a delayed
worker beyond the former 500 ms wait, and a separately compiled actual-DLL
device-free harness. The latter resolves 41 exports, checks version 600, exercises
empty cleanup and worker restart/stop across 32 load/unload cycles. It calls no
device enumeration, InitDirectInput or force output. The actual DLL ABI has no
delayed-worker injection; delayed ordering evidence comes from exact-source fakes.

Independent review is required before default promotion. FFB remains Off for
physical acceptance. Driver Stop, file I/O or worker calls can block indefinitely;
there is no bounded shutdown or force-cessation guarantee on crash/forced kill.
No game, screen, display, device, installation or release operation is performed.
