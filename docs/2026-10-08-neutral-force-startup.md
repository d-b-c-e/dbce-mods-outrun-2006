# Accepted neutral startup — October 8

`DeferredInit` now starts output with `SetDeviceForcesXY(0,0)` and requires its
success before creating auxiliary effects or completing initialization. It no
longer calls unchecked `StartEffect`, which can replay stored nonzero parameters
on a retained effect. If the zero is refused, it releases the force device,
clears initialized/slot state, reports the failure and waits for explicit Refresh
or selection before retrying. No force arithmetic, profile, native pin or saved
setting changes. This is source/build qualification; not installed or felt.

The x86 production-linked fixture now models a retained force of 5,000 and a
setter which can refuse zero. It verifies accepted-zero startup, no StartEffect,
release and no auxiliaries after refusal, no automatic retry, and successful
explicit retry. Compiling those tests against unchanged **3516805** fails the
retained-force assertion (`build/neutral-start-negative`).

Checks passed:

- `tools/tests/Test-WheelSettings.ps1`: settings, production FFB gates, input
  calibration/remapping and dispatch fixtures, including the new startup cases.
- `tools/tests/Test-ConsumerLifecycle.ps1 -ExactExecutable <pinned game EXE>`:
  production gate/drain/reentry, device switching, silence/finalization and exact
  synthetic host tests. The EXE is read/hash checked, never launched.
- `cmake --build build/mixed-switch-candidate --config Release`.

Private logs: `build/neutral-start-tests-20261008.log`,
`build/neutral-start-lifecycle-20261008.log`, and
`build/neutral-start-build-20261008.log`. No WheelFfb DLL is loaded by the fake
consumer tests, no device is opened and no physical force was sent.

The installed moving-camera discovery **6ccd7dd** remains unchanged. Its replay
and normalization gaps are independent of this lifecycle fix.
