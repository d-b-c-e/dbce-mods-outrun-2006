# OutRun2006Tweaks-FFB working notes

**October 8 current checkpoint:** `e5b44f8` force-delivery candidate installed
privately with reviewed 41-export native `50ba139`, five INIs retained, no new
physical test. Later schema-3 discovery source adds raw force inputs/settings
while FFB remains Off (402 discovery checks, 12 analyzer tests, legacy golden
replay and Release build pass). It does not fabricate original force commands.
Read `docs/TICK-DISCOVERY.md` and
`docs/2026-10-08-force-delivery-acknowledgment.md`; live input-window qualification
and gameplay replay remain open. Earlier installed-candidate notes are historical.

**October 7 live discovery readback:** the reviewed 069fe41 private candidate is
now installed (proxy 73432838; native/profile unchanged). Claude's stationary
Palm Beach case has 5,878 rows / 2,939 ordered pairs, independently checked;
45 package hashes, three installed runtime files and eight restored owner files
match. Two retained frames show 000 km/h and an advancing timer. Read
`docs/2026-10-07-live-discovery-readback.md`. This supersedes the installed
f021736/no-live-run statements below, which describe the earlier checkpoints.
The hooked call changes the observed pose fields; complete pose ownership,
matrix roles, moving-car units and a writer remain unqualified. Producer-preserving
output mute and gameplay replay are still unfinished. No stationary rerun needed.

**October 7 read-only discovery review:** 069fe41 closes all four a181519
findings. The 100-check discovery suite, production lifecycle suites and 33
independent source-linked fault checks pass. Cleanup precedes fallible saving;
data completion is checked, final unmatched pre fails, and same-model replacement
is serialized. Packaging is clear from source review; no live discovery or
physical-output qualification yet. Read `docs/2026-10-07-tick-discovery-review.md`.
Installed f021736 proxy still matches. Claude owns runtime follow-up; output mute
and gameplay replay remain separate unfinished steps. Read the outcome's dataFile
to handle the non-overwriting retry; missing/failed completion is not success.

**October 6 installed input candidate:** `f021736` is installed, DLL
`E18455496EFB59EB33A9F592889F9B12991C1519EB8A8E6D37268AF132D1AE78`.
Its per-game-update snapshots and explicit temporary pad mappings passed the
12:45 bounded check: Start then A entered the Single Player mode menu; Claude
reported no freeze and normal exit 0. Independent readback verifies the three
payloads and all eight backed-up owner files. Owner mappings remain unchanged.
The later 16:49 run reached the Palm Beach start line through Single Player ->
OutRun -> Dino 246 GTS (Novice) -> Automatic -> Splash Wave, with neutral axes.
First/last captures show 000 km/h and a running timer (96 -> 66); Claude reports
normal exit 0. Both runs' eight original owner files and three installed payloads
independently match. No game launch is needed to repeat this prerequisite.
The TS-UFB01B-X device was confirmed by connected/disconnected enumeration to be
the tooling's virtual pad; automatic wheel selection had applied saved buttons
31/34 to its actual 0/7. Pin the verified instance for diagnostics.
Earlier b43445c focused retries froze once and reached title/normal exit once;
the freeze cause remains unexplained. Do not add a millisecond polling fallback
that breaks the fixed update contract. Read docs/2026-10-06-focused-input-followup.md
for exact evidence, reciprocal review and next-check constraints. Next is
read-only pre/post local-car-tick discovery with separate output admission;
driving input, physical wheel/FFB and gameplay recording/replay remain unqualified.

**Source follow-up cf6e176 (not installed):** primary auto selection excludes
gamepad/supplemental types and unreadable/zero-axis candidates. It does not require
DRIVING: the observed R12 reports 1STPERSON (0x00010318). Explicit GUID choices and
optional slots retain their behavior. Real production InitSlot/enum fake-COM cases
fail before the change and pass afterward; the full input/UI fixture and x86
Release build pass. Preserve installed f021736 for the next combined probe check.

**October 5 startup recovery:** the Stream Deck copy lacked 110 script/BK game
assets. Restored only missing files from two agreeing local installs; all hashes
and 31 protected root/save files verified. Claude's 15:38 CT post-repair test
reached the title at 7680x1440 and exited normally. Post-run check retained all
30 non-log root/save hashes and all 110 repaired asset hashes; the new log is saved.
Run `tools/Test-GameData.ps1` before another launch. See
`docs/STARTUP-ASSETS-2026-10-05.md`; do not blame hooks or resolution without evidence.

Recording/playback intake: read `docs/STAGE-PLAYBACK.md`. The existing 128-frame
force recorder passed x86 offline regression again on October 5, but has no
external runtime arm/save or gameplay playback. FFB Off skips calculation;
preserve the model stream when adding a separate output mute. This intake did
not change the accepted install or game hooks.

Read [CLAUDE.md](CLAUDE.md) for build architecture and known force-signal issues,
then [the UX adoption inventory](docs/UX-OVERNIGHT-2026-09-16.md) and the latest
deployment receipt under `docs/`. Offline fixtures and deployment are not proof
of live controls, physical force or camera acceptance.

- `src/overlay/wheel_settings.cpp`: the production F6 Simple/Advanced UI and
  atomic user-INI saves. One shared Settings store; no backend change on view
  switches. Stop FFB/F8 persists Off.
- `src/axis_calibration.hpp`: finite/range validation and opt-in normalized
  axes. Existing uncalibrated transforms must remain unchanged. Binding, identity
  and calibration save together; failed/cancelled captures never flush later.
- `src/hooks_inputremap.cpp`: game input adaptation. Do not switch the SDL and
  DirectInput backends at runtime. Device and camera capability gaps are listed
  honestly in the inventory; do not invent a game handbrake route.
- `src/wheel_input_gate.cpp/.hpp`: final input dispatch isolation, installed
  after all adapter hooks. Keep hardware polling during settings; release-latch
  both digital and analog menu navigation. The x86 fixture uses real local
  trampolines and an explicit `asInvoker` manifest, never game/device injection.
  `SuppressTextMessage` is called after ImGui's actual Win32 handler; stock
  WM_CHAR entry bypasses switch dispatch. Read-only Advanced Help counters and
  the executed baseline comparison are in `docs/NATIVE-INPUT-DIAGNOSIS-2026-09-19.md`.
- `src/hooks_dinputffb.cpp`: game force signals and output gates. Preserve owner
  tunes. Native output is a reviewed **v0.13.0 override** on the **v0.8.0 model/
  profile/encoder baseline**. Read `lib/toolkit/NATIVE-PROVENANCE.json` before sync.
- `tools/tests/Test-WheelSettings.ps1`: real ImGui/memory-only input/fake ABI
  tests. `render_imgui_fixture.py <run-folder>` renders actual ImGui draw data
  with NumPy/Pillow. It does not launch a game or acquire a wheel.
- `tools/tests/Test-WheelInstall.ps1`: synthetic-executable installer/restore,
  retention and rollback tests. `tools/Check-IniCoverage.ps1` checks every key.
- `tools/tests/Test-WheelInstallPlayer.ps1`: actual Windows PowerShell 5.1
  `-File` and shipped batch routes from another CWD, with spaced paths, omitted
  package path, prompted game path, restore, explicit override and failures.
  Use `-PackagedDirectory` to test an immutable package with a synthetic EXE.
  The child uses its own default PS5.1 module path, not inherited PS7 modules.
- `tools/Package-WheelSettings.ps1` packages the existing x86 Release build;
  package `Install.bat` calls `Install.ps1`. Deploy only while the game is closed,
  verify hashes and preserve owner settings. Never kill a game for deployment.
  For installer-only repacks, use `-RuntimePackageDirectory` pointing at the
  verified frozen package. `sourceCommit`/`runtimeSourceCommit` retain its runtime
  identity; `installerSourceCommit`/`packagingSourceCommit` identify the repack.

Build this checkout with `cmake --build build/mixed-switch-candidate --config Release --target outrun2006tweaks`.
The legacy `build/CMakeCache.txt` names the old repository path; preserve its
evidence and do not use its stale output. Package the verified new bin folder
with `-RuntimeBuildDirectory build/mixed-switch-candidate/bin`.
On this machine CMake is under VS2022 BuildTools `Common7/IDE/CommonExtensions/
Microsoft/CMake/CMake/bin`. Commit/push changes with `[skip ci]` when hosted CI
is not requested. Live game/device tests require the coordinator's serial slot;
no unattended nonzero force.

For a live smoke, launch the EXE from its game folder (or set shortcut Start in).
A direct launch without that working directory showed blank game frames despite
a working F6 panel. Stage 5 verified actual 4K F6/pages and shortcut capture/
cancel. Stage 6 reproduced native title confirmation failure on both the new
candidate and exact saved pre-session proxy, with two Return window messages
and zero release-blocked queries. Exact Confirm edge counters were zero, so
do not infer successful native keyboard sampling or a specific root cause.
Actual 4K Advanced Help was readable. All owner state was restored after both
normal exits; current installed source is d3d7f23. Player cameras, native text
entry isolation and physical wheel/force remain unaccepted. See the deployment
receipt and diagnosis result before repeating a live test.

## Toolkit standards

At the start of every session, compare the wheel toolkit's ledger
(`E:\Source\toolkits\dbce-wheel-mod-toolkit\STANDARDS.md`) with this repo's
`TOOLKIT-ADOPTION.md`. Report any entry that is `pending`, `unchecked` or missing
from the adoption file, and bring it in when your work touches that area. When you
adopt one (or find it does not apply), update `TOOLKIT-ADOPTION.md` in the same
commit. When you set a new family-wide standard, append it to the toolkit ledger
and commit it in the same turn; do not leave it only in an uncommitted file.
