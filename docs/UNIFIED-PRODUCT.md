# DBCE Mods: OutRun 2006 product boundary

Canonical repository URL: https://github.com/d-b-c-e/dbce-mods-outrun-2006.
The existing public repository was renamed in place on 2026-10-01, preserving
numeric ID `1163595970`, history, visibility, default branch and source lineage.
The legacy URL https://github.com/d-b-c-e/OutRun2006Tweaks-FFB redirects here.
This PC product is distinct from `dbce-mods-outrun-arcade`.
The stable product ID `outrun2006-c2c-pc` and runtime field
`OutRun2006Tweaks-FFB` are compatibility identities, not new repository names.

This repository is the runtime and package source for the OutRun 2006: Coast 2
Coast PC mod. `product.json` records the game identity and implemented versus
accepted features. Wheel, force feedback and telemetry already share one x86
proxy, one settings store and one installer. This milestone retains that layout.
Triple-screen rendering, session recording and driving-input playback are not
implemented by this package. Camera/render hooks are research footholds only.

## Consolidation decision

Retain the OutRun2006Tweaks-FFB runtime history and existing setup/package scripts.
Do not merge the separate private historical
[Redux repository](https://github.com/d-b-c-e/outrun2006-redux) into this runtime. Its older texture,
configuration and game-content work has different provenance and acceptance;
its historical completion statements do not supersede this fork's latest gaps.
ReduxConfig and assets are not shipped here. Reconcile exact owner, licensing,
configuration authority and payload identity before any later integration.
Keep CannonBall and OutRun 2 SP SDX as separate products.

## Setup and legacy settings

Use the existing packaged `Install.bat`, then F6 Setup. Preserve all existing
root INI/CFG/JSON/XML files byte for byte, including Redux-associated settings,
unknown keys and bindings. Seed defaults only when absent. No settings conversion,
rename, reset, backend change or force enable is part of consolidation. Runtime
restore retains subsequent settings. Existing unknown-proxy refusal and x86 game
guards remain. Game-folder working directory is required for normal launch.

## Release and preservation

One immutable ZIP and `package-manifest.json` describe the installable runtime,
installer, settings templates, product descriptor and hashes. Runtime and
installer commits remain distinct for repacks; do not relabel historical DLLs
as rebuilt. Preserve existing tags, ZIPs, backup receipts and toolkit pins:
v0.8.0 model baseline and v0.13.0 native/header ABI baseline (native 0.6.0),
with the matched native shutdown source override `f8f0619` described in
`lib/toolkit/NATIVE-PROVENANCE.json`.
`product.json` is metadata, not a runtime enable switch or support certification.
Legacy schema-1 packages without it remain installable. New schema-2 packages
hash every shipped file recursively, validate required third-party notices and
provenance before runtime replacement, and record distribution blockers.
Normal packaging rejects outstanding distribution gates; -ReviewOnly permits a marked
local candidate only. See third-party/README.md. No public release is claimed.

## Review gates

Resolve the consumer/actuator owner and current installation checkpoint before
promotion. Native title confirmation failed historically on candidate and baseline;
physical driving, force and player-camera acceptance remain incomplete. Keep FFB
Off. No game/device/display test, deployment or remote change is authorized by
this source consistency milestone. Preserve F-Zero Deluxe's reservation.

DirectX content provenance is verified; see third-party/index.json for upstream
matching revision, SDL edits and license newline normalization. This factual
finding does not complete the remaining acceptance gates. The isolated native
shutdown adoption replaces the unchecked 500 ms wait with actual worker joining;
see `docs/NATIVE-SHUTDOWN-ADOPTION.md` for exact provenance and device-free tests.
Independent integration review and physical driver/actuator acceptance remain
required. Blocking driver calls have no bounded shutdown guarantee. No unattended FFB.

At intake, runtime HEAD was cb0f0f1 on codex/ux-simple-settings-2026-09-16.
Related Redux main was 5d2eb52, with five untracked owner files (INI and
ReduxConfig/manifest outputs). None is imported or edited by this candidate.
