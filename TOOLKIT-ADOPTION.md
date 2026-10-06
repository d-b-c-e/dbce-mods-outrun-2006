# Toolkit standards adoption

Which entries of the wheel toolkit's standards ledger
(`E:\Source\toolkits\dbce-wheel-mod-toolkit\STANDARDS.md`) this mod has brought in.
Update a row in the same commit that adopts it. Statuses: `adopted`, `partial`,
`pending`, `n/a` (say why), `unchecked` (nobody has looked yet).
Seeded 2026-10-04 from what was verified that day; `unchecked` rows need a look.

| Standard | Title | Status | Notes |
|---|---|---|---|
| STD-001 | One mod per game | unchecked |  |
| STD-002 | Recording and playback from launch | n/a | Superseded by STD-012; input determinism is not established. |
| STD-003 | Normalized FFB strength | unchecked |  |
| STD-004 | Consistent settings UX | unchecked |  |
| STD-005 | Camera numpad layout 8/2 9/3 4/6 7/1 +/- 5 | unchecked |  |
| STD-006 | Camera step sizes are settings | unchecked |  |
| STD-007 | Triple screens in one wide window | unchecked |  |
| STD-008 | Display changes: game applies once | unchecked |  |
| STD-009 | Dashboard telemetry matches the HUD | unchecked |  |
| STD-010 | Install the latest build for testing | adopted | b43445c runtime installed by Claude from private review package 685361c; independent payload/owner-state readback recorded in docs/2026-10-06-b43445c-check.md. No gameplay/force acceptance inferred. |
| STD-011 | Work lands on main | adopted | This scoped playback intake was merged/pushed to master; accepted runtime remains independently identified. |
| STD-012 | Reproduce the route and preserve original signals | pending | Native hook/output/startup boundaries inspected; existing 128-frame force-calculation recorder passes offline tests but lacks external runtime capture and gameplay replay. The 110 missing assets were restored; Claude's title/normal-exit retest passed. The installed b43445c pad test was naturally unfocused and inconclusive; offline race entry remains unqualified. Read-only asset preflight now handles Start-Job relative paths. See docs/STAGE-PLAYBACK.md. |
| STD-013 | The installed build launches plainly | partial | Claude's repaired Stream Deck copy reached title and exited normally with no arguments at saved AutoDetect 7680x1440. Menu confirmation, race entry and recording/playback remain unqualified. |
| STD-014 | Request reciprocal review when progress stalls | adopted | Codex reviewed Claude's failed virtual-pad attempt using actual pad/game logs and source. Mixed-mask native input suppression is now reproduced and fixed with 34 source-linked cases; live title causation remains unknown. Findings and the candidate go through the portfolio inbox for Claude's review. See the October 5 addendum in docs/NATIVE-INPUT-DIAGNOSIS-2026-09-19.md; the subsequent installed check passes startup/exit but remains inconclusive for input while Windows Security held focus. |
| STD-015 | Triples on Surround and separate monitors | unchecked | Native in-race aspect and independent projected views remain unqualified; a wide title/attract window is insufficient. |
| STD-016 | Forza Horizon telemetry on by default | unchecked | Encoder/units/defaults require the current release audit; telemetry was temporarily off in the latest menu check, then restored. |
| STD-017 | Hide empty settings pages | unchecked | Current panel has not been audited against this newer standard. |
| STD-018 | Handling changes never reach online scores | unchecked | Online/offline eligibility and score paths must be established before any gameplay replay or handling change. |
| STD-019 | Centre menus and gameplay HUD | unchecked | Title/attract images do not qualify menus, loading, F6 and in-race HUD across the requested layouts. |
| STD-020 | Standard portfolio feature checklist | partial | All current ledger rows are now explicit; remaining unchecked features require review, not automatic adoption. |
