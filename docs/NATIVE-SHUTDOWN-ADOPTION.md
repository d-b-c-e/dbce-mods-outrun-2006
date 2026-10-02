# OutRun 2006 PC native shutdown adoption candidate

**Qualified PC candidate: independent review required before promotion.**
The general toolkit API permits an unsynchronized plain g_lockInit worker read
when SetHoldTimeoutMs starts a worker before initialization. OutRun's production
source never calls SetHoldTimeoutMs; it is the only native worker-creation route,
and g_holdMs starts at zero. Therefore this consumer never starts that worker.
All public native initialization/output/cleanup calls use consumer lifecycle
serialization; cleanup drains admission before FreeDirectInput and retains the
module pinned. No native lock flag is reset on selection cleanup. This excludes
the reported worker-before-initialization race for this exact PC call pattern,
without claiming general toolkit API race safety. A shared toolkit publication
repair remains a follow-up. The device-free actual-DLL harness deliberately starts
workers without initializing devices, but performs no concurrent initialization;
its passing cycles are not a race-safety proof.

The consumer's `CheckWatchdog` runs from the overlay hook and therefore depends
on continued overlay execution. Its timeout predicate requires a previous update
and nonzero constant demand (`prevConstantLevel != 0`). Although its silence call
zeros effect families, periodic-only or condition-only demand does not satisfy
that predicate. It is neither independent stall protection nor an all-effect
watchdog. Native worker omission does not imply equivalent stall protection.

The separately reported low-demand restart defect does **not** reproduce in this
41-export source: after ZeroForces, periodic demand 100 and condition coefficient
100 both return success, issue SetParameters with DIEP_START and mark started.
Periodic `wheel_burst::needs_update` and condition filtering account for stopped
effects. Evidence uses exact-source fake effects; no physical output is claimed.

Consumer integration base: `8a84b9daecc5d3446a6bea85cd13d72421445380`.
Reviewed lineage: `8a84b9d` → `00457ff` (matched native integration) →
`4154962` (initial general-API hold) → `c6f6509` (qualified PC call pattern).
The direct parent of `c6f6509` is `4154962`, not the integration base.
Toolkit source: `f8f0619b5588f2d11b44f4becd4198775d4a8bcf`, tree
`d9f59422cff93383f9cf27f3cd641fd2adf00a35`.
Selected x86 DLL: 166400 bytes, SHA-256
`4f7ffbb590c30e2a8d33f2e7a3d7e8c770bdd2466053a2333d67c55d98bb08b4`.

This is a matched reviewed local build of published source, not a new toolkit
release. Native version 600 and 41 exports remain unchanged. Only the DLL is
replaced; all consumer headers, force shaping, profiles, settings, defaults and
caps remain byte-identical to the integration base. Historical sync hashes for existing
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

The frozen `c6f6509` review ZIP (SHA-256
`d9b044bb31fde5e64d8713827ba17ca80fe84c00348b4bed13465a5c23c5993a`)
was freshly extracted for exact-package installer review. Actual Windows
PowerShell 5.1 and shipped batch workflows passed omitted/explicit package paths,
unrelated CWD, spaced paths, prompt, settings retention, restore, locked-file
rollback and corrupt-package failure propagation. Targets contained a synthetic
EXE that was never launched. ZIP bytes and owner installations remain unchanged.
The earlier absent `native-final-player.log` reference is superseded by the saved
`native-exact-zip-player-review.log` and structured receipt in task evidence.

Independent review is required before default promotion. FFB remains Off for
physical acceptance. Driver Stop, file I/O or worker calls can block indefinitely;
there is no bounded shutdown or force-cessation guarantee on crash/forced kill.
No game, screen, display, device, installation or release operation is performed.
