# OutRun 2006 PC lifecycle candidate

This isolated source candidate moves consumer teardown from process detach to
the inspected normal loop-return boundary. It is for independent review and
device-free verification, not installation or physical FFB acceptance. Keep FFB
Off. The retained x86 WheelFfb.dll remains unchanged (SHA256
`95db6175354db9018ef6143291e293e75864d96919c22928aa19e6d11c8cbe31`).

The supported disk EXE SHA256 is
`68ceb386829066f8455b9d027320af962584321f3e2e8a79c72841495a6134c3`.
Runtime bootstrap at RVA `0x18080` verifies that disk identity and seven loaded
instruction signatures, accounting for relocated absolute operands and the
candidate's own hook bytes. Partial hook installation and conflicting patches
refuse actuator readiness. Consumer and system proxy modules are pinned outside
DllMain. Successful native loading pins its module before the first ABI call;
failure unloads only an uninitialized load. Successful modules and hook objects
remain until process termination; explicit hot-unload is unsupported.

The loop-entry callsite at RVA `0x176EE` marks operational loop entry. Initialization
failure that bypasses this call cannot acquire FFB. A valid HWND owned by the
current thread/process and successful owner-thread window subclass are required
before native acquisition. Constant and periodic output recheck readiness.

Close and session-query requests pause admission and silence reversibly without
PanicStop or resource release. If a producer is active, silence defers until the
outermost lease ends, avoiding a window callback waiting on its own producer or
another producer waiting for window dispatch. A canceled close resumes only at
the inspected loop continuation at RVA `0x17E10`, with no pending quit/game exit
and no unresolved session query. WM_ENDSESSION(FALSE) cancels the session pause;
TRUE/WM_DESTROY close admission but rely on the normal outer boundary for cleanup.
There is no promise that session termination grants enough time to reach it.

At RVA `0x176F3`, after the loop returns, one finalizer closes admission and drains
existing and queued consumer leases without holding their operation mutex. It
calls native PanicStop/FreeDirectInput, closes owned telemetry resources and
releases consumer-created remap handles once, followed by owned SDL controller
handles. Game/proxy DirectInput instances and the game HWND are borrowed and
never released/destroyed here. The SDL HWND wrapper is retained for process
lifetime; global SDL_Quit is not called. SafetyHook relocates the displaced game
cleanup CALL; the callback does not change its context or call game cleanup itself.

FFB, remap producers, SDL input/rumble and binding UI, wheel UI, watchdog and
telemetry settings/output share admission. Device selection is a serialized
recoverable transition: old output completes before zero/release; a reentrant
selection defers until the outermost lease ends.
Pending selection overlapping a close/session pause is reconciled while admission
is still paused, before canceled-request recovery can admit normal output. This
also applies when cancellation arrives after the original producer has ended.
Existing settings/defaults, force shaping and caps are unchanged. Process detach does no explicit foreign
ABI, wait, logging or FreeLibrary; the InputManager is retained so its destructor
cannot close SDL controllers under loader lock.

## Verification and remaining gates

`tools/tests/Test-ConsumerLifecycle.ps1 -ExactExecutable <path>` compiles x86 fake
ABI and synthetic-host fixtures. The supplied EXE is opened only for hashing and
never executed or copied. Tests cover unknown host/hash, relocated signatures,
owned-hook conflict, partial installation, pin/window/thread/subclass refusal,
startup failure, reversible/reentrant requests, queued/concurrent producer drain,
competing finalizers, delayed/reentrant device selection, stopped producer refusal
and the real x86 cleanup trampoline's registers/flags/stack/order. The DllMain
detach check is a source contract, not proof of every dependency's detach behavior.
`tools/tests/Test-WheelSettings.ps1` covers existing settings, FFB gates, calibration,
input dispatch and fake COM ownership cleanup.

The retained toolkit native 0.6.0 still ignores completion of a 500 ms worker
wait before resource release. This consumer change does not fix or validate that
native worker lifecycle. A matched reviewed toolkit shutdown build, delayed-worker
integration test and binary provenance remain required before physical rollout;
do not replace the DLL on the strength of these fake tests. Driving, wheel feel,
camera and telemetry receiver acceptance remain separate gates. TerminateProcess,
external kill, crashes and sudden loss can skip this normal route; neither module
pinning nor this hook guarantees force cessation on those paths. No universal
ExitProcess detour is installed.
