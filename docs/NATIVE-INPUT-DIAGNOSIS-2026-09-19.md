# Native title input comparison

Stage 5 starts correctly from the game folder, but two Return taps before first
F6 did not advance the title. A, Space and F2 did not establish a player menu.
The cause is unresolved. A live 4K settings/capture smoke is not proof of native
game input or player-camera framing.

## Offline findings

- The final guard is installed after every adapter. With no blocking reason or
  held requested bit, it returns the original aggregate query result unchanged.
  Native SwitchNow/On returns raw mask bits; SDL has all-bits/chord semantics.
  Production x86 trampolines test both. Axes do not latch digital Confirm.
- Focus/UI suspension latches digital controls; a requested bit clears only when
  the original Now route reports released. Another bit cannot keep Confirm
  latched unless the game asks for both in one aggregate query. No live state
  counters existed in stage 5, so a startup-focus/native-state issue is not ruled
  out by the before-first-F6 check.
- Exact installed EXE SHA256
  `68ceb386829066f8455b9d027320af962584321f3e2e8a79c72841495a6134c3`:
  native WndProc RVA `0x17FCB–0x18045` dispatches WM_CHAR to active text entry
  independently of the switch API. The bundled ImGui Win32 backend also reads
  WM_CHAR, then returns zero. The previous wrapper therefore forwarded it.
- Stage 6 consumes only native WM_CHAR while UI/focus suspension applies, and
  retains captured scan codes through held repeats/queued old characters until
  a fresh unblocked key press. ImGui still receives messages first. Lifecycle,
  Alt+F4 and native polling remain intact. Source-linked fixtures cover the
  message helper; actual WndProc/licence-entry acceptance remains pending.
- Stage 6 Advanced Help shows read-only dispatch totals, suspension reasons,
  held mask, exact Confirm edge queries/positive results, release blocking and
  Return window-message delivery. No extra device poll or native query is made
  for diagnostics. Counters include the preceding closed-panel attempt; merely
  opening diagnostics necessarily sets a UI blocking reason.

## Minimal next serialized test

Use two short launches, both from the verified game folder. Do not change input
backends during a run and do not add a camera hook to work around this failure.

1. Snapshot all owner settings, SaveGame and both installed DLLs. Verify FFB Off,
   telemetry/remap Off, `UseNewInput=false`, `VibrationMode=0`,
   `VibrationStrength=0` and `ImpulseVibrationMode=0` in the temporary override.
   Temporarily keep WheelFfb.dll outside the runtime search location to make
   toolkit force initialization unavailable for this input-only comparison.
2. Launch the reviewed stage-6 candidate. At the settled title, try Return twice
   before F6. Open Advanced Help and capture the diagnostic counters. Normal exit.
3. With the game closed, swap only the proxy to the exact pre-session Tweaks
   baseline from `.wheel-settings-backups/20260919-185854-286-07569bf0/dinput8.dll`.
   Verified SHA256
   `116bc609d2af4b366f7f02e3594f7d3be1e57c2f1fd937d9b8f36b9df1cf5cf9`,
   product Outrun2006Tweaks, file version 0.6.1.0. This is the owner's saved
   pre-session binary, not an independently attested official release. It has no
   WheelFfb/DirectInputFFB/remap strings. Retain the same input configuration,
   resolution, game folder, executable and supported automation method.
4. Repeat the settled-title Return attempts and record whether a menu appears.
   Normal exit, then restore the installed candidate/native DLLs and every
   original setting/save. Archive any test-only files, restore their original
   absence, and verify all hashes before releasing the lease.

If both binaries fail, this narrows the issue to native setup/input delivery or
a shared upstream condition; it does not identify which. If only the candidate
fails, use its reason/raw-positive/release counters to localize the regression.
Return-message counts alone do not prove DirectInput sampled a key. If neither
has established native input, player cameras remain unidentified; attract shots
must not be labeled Bonnet/Bumper. No force, tune or physics comparison is part
of this test.

## Executed comparison — 2026-09-19

The coordinator granted 21:10–21:18 UTC. Both runs completed and the lease was
released early at **21:13:42 UTC**. Startup logs from both binaries show
`UseNewInput=false` and all three vibration settings at zero. WheelFfb.dll was
absent beside the executable for both runs, with its verified copy retained in
the evidence directory outside the game.

Both the stage-6 candidate and exact saved pre-session proxy failed two Return
taps at the settled title. Neither reached a player menu; the ordinary attract
movie followed. The candidate received **two Return window messages**. Its live
Advanced Help frame recorded **31,464 queries, 15,896 forwarded, 15,568 UI-blocked
and zero release-blocked** queries. The current blocking reason was 6, as expected
with settings/overlay open. Exact single-action Confirm edge counters were all
zero: this diagnostic does not identify what other aggregate masks the title
requested, and Return window delivery is not a native DirectInput sample.

The observed release gate did not hold any query. Reproduction on the saved
baseline also rules out a failure unique to the new UX code in this comparison.
It does **not** identify whether native setup, the supported automation's key
delivery, or another shared upstream condition caused the failure. An attended
ordinary keyboard/controller input check is the next useful discriminator; do
not add a speculative camera hook or call this a proven game-camera constraint.
The stock player camera views and actual licence text-entry isolation remain
unverified. No test licence was created and no driving or torque occurred.

Evidence under `build/camera-live-stage6/`:

- `candidate-help-diagnostics-3840.png`: actual 3840x2160 Advanced Help.
- `baseline-after-return-3840.png`: baseline still in title/attract flow.
- `before-baseline/OutRun2006Tweaks.log`, `after/OutRun2006Tweaks.log`: candidate
  and saved-baseline output gates respectively.
- `prepared.json`, `preflight.json`, `baseline-ready.json` and
  `restore-verification.json`: runtime/configuration identities and exact restore.

Both runs exited normally through Alt+F4. Stage-6 proxy/native DLL hashes, all
five original setting files, absent user/layout overrides and the empty SaveGame
were restored and checked before release.

## October 5 follow-up: review before another input attempt

The missing-data startup crash is resolved separately; see
`STARTUP-ASSETS-2026-10-05.md`. Claude's 15:38 startup/normal-exit test sent no
input. His later 16:16–16:18 virtual-pad attempt did not establish menu entry.
Read-only review of current source `f254ac8` and retained logs found:

- The pad connected at 16:16:42.255, before the game log starts at
  16:16:45.232. Eight `press A 200` commands completed from 16:17:26.021 to
  16:18:03.983. No Start or direction command is logged during this run.
  Command completion establishes submission, not game receipt.
- The game logged `UseNewInput=false`, `ControllerHotPlug=false` and
  `UseDirectInputRemap=true`. Late pad creation is therefore worth checking in
  general, but is not supported as this run's cause by these timestamps.
- `WheelInputGuard::BlockingReason` rejects all switch queries when the game
  HWND is not foreground. The virtual-pad helper's generic comment about
  operating without focus does not bypass this consumer rule. Foreground state
  during the failed presses was not recorded here.
- `DirectInputRemapHook::SwitchOn_dest` and `SwitchNow_dest` return the remapped
  mask alone whenever the requested mask contains any menu-direction bit. This
  intentionally suppresses parked pedal navigation, but also omits original
  non-direction bits in a mixed query. Whether the title uses such a query is
  unobserved; this is a source finding, not a diagnosed runtime cause.
- Existing Confirm diagnostics count only edge queries whose mask is exactly
  `SwitchId::A` (0x4). They omit Start (0x1) and mixed masks. Historical zero
  Confirm counts cannot establish that native input was never queried.

The next useful test keeps the backend and display fixed, confirms the actual
foreground HWND and neutral release, then tries Start and A separately through
the already-connected pad. Capture native requested masks, raw positive results
and suspension reasons if it fails. Do not repeat an A-only loop, switch input
backends at runtime, move focus with agent tooling, or remove focus isolation to
force a pass. A naturally unfocused run is evidence of that gate, not permission
to bypass it. The mixed-mask source defect is now fixed offline as described below;
whether the title requests such masks still needs observation.

Codex supplied these findings to Claude for reciprocal review through
`dbce-project-mgmt/inbox/from-astra.md`; no new game run, installed payload,
setting, input or display change was made. An unattended offline race route and
gameplay capture/replay remain unimplemented/unqualified.

Private exact log copies and hashes:
`%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/outrun-input-review-ba2e2750dc7d411db68a5d7408a0320f/receipt.json`.

### Mixed native/remapped switch fix — October 5, after the 16:22 report

The production edge and held hooks now share `DInputRemap::MergeSwitchQuery`.
It excludes only the four native direction bits from the original query, merges
the original return with remapped any-bit input, and skips an empty native query.
Previously, any direction bit caused the entire original query to be skipped.
For example, native A held during an `A | SelectionUp` query returned zero.
Filtering only directions preserves pedal-navigation suppression, native
A/Start/camera input, and the original raw-bit or Boolean return convention.
The SDL backend and outer focus/UI/release gates are unchanged.

The regression first ran against the old policy extracted without a behavior
change into that shared production function. It failed the mixed-A case:
`result=0 expected=4; native reads=0 expected=1; native mask=0 expected=4`.
After the fix, all **34 remapper cases** passed with raw-bit and Boolean native
returns. The existing actual-source suite also passed its 144 legacy axis cases,
calibration/device identity/finalization checks, FFB output gates, actual ImGui
settings transactions and x86 dispatch/held-release/WM_CHAR/focus isolation.
These tests use memory-only devices and local trampolines, not game or OS input.

Reproduce with `./tools/tests/Test-WheelSettings.ps1`. Local evidence is
`build/mixed-switch-before.log` (expected failure), `build/mixed-switch-after.log`
and `build/wheel-settings-fixture/run-4cd1d7a268cd40e2a3da0384dfe7eb9d`.
This establishes a source defect and its correction, **not** the cause of the
failed title confirmation: neither query masks nor foreground state were captured
at the failed presses. No game launch, deployment, display change or force test
was performed for this fix. The accepted installed runtime remains `86599699`.

The complete Win32 Release target also built successfully in the separate
`build/mixed-switch-candidate` directory using the existing dependency sources.
The moved legacy `build/CMakeCache.txt` still names `E:/Source/OutRun2006Tweaks-FFB`
and cannot regenerate; it and its historical artifacts were retained. Fresh
configuration and build output: `build/mixed-switch-configure.log` and
`build/mixed-switch-release-fresh.log`. This local pre-commit diagnostic DLL has
SHA256 `1E61F51AB877AD317EDB4B9C7AC61DCECA5DD52B5BD4D6A083B70C8F01CD8276`;
it is not a release package or a deployed candidate. Read-only verification still
finds installed DINPUT8.dll SHA256
`1F534047BF52E19062B30E194FC56148AD3A8F38EA53BF249485121F441FBBA6`.

### October 6 installed candidate — unfocused test is inconclusive

The preceding source-only/old-install statements are historical. Claude packaged
and installed the fixed runtime, then connected the pad before the no-argument
launch from the correct working directory. Start, A and subsequent A presses did
not leave title/attract. The foreground remained Windows Security (`PickerHost`),
so this does not exercise native focused dispatch and does not disprove the fix.
Normal close returned 0 and the owner's files were restored. Codex verified the
package, installed payloads, required assets and backed-up owner files read-only,
and inspected the title/attract contact sheet. See
[the exact evidence](2026-10-06-b43445c-check.md).

Do not repeat input while the same focus condition persists. Do not turn off
focus protection to make it progress. After the owner resolves the dialog, the
next bounded test can distinguish native focused title/menu entry; only actual
offline player control can qualify the discovery recorder's game tick and pose
fields. A title image or attract loop is not that evidence.
