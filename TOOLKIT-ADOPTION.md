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
| STD-010 | Install the latest build for testing | adopted | Exact f021736 runtime installed by Claude for the 12:45 menu pass; three payloads and all eight backed-up owner files independently verified. Exact receipt in docs/2026-10-06-focused-input-followup.md. No gameplay/force acceptance inferred. |
| STD-011 | Work lands on main | adopted | This scoped playback intake was merged/pushed to master; accepted runtime remains independently identified. |
| STD-012 | Reproduce the route and preserve original signals | pending | Installed f021736 passed offline Palm Beach entry and still matches. Source-only 069fe41 closes discovery review findings: 100 discovery checks, lifecycle suites and 33 independent source-linked fault checks pass (docs/2026-10-07-tick-discovery-review.md). No live discovery or gameplay replay. The 128-frame force recorder lacks runtime arm/save; producer-preserving output mute is unfinished. Candidate includes cf6e176 automatic pad-selection fix; prior freeze remains unexplained. |
| STD-013 | The installed build launches plainly | partial | Repaired copy and installed f021736 passed ordinary no-argument startup, the complete offline menu route to Palm Beach and reported normal exit. Diagnostic pad mappings were temporary and restored; normal owner-wheel driving remains unqualified. |
| STD-014 | Request reciprocal review when progress stalls | adopted | Reviewed Claude's focused retries, reproduced mapping and update-sampling defects, and prepared a tested input candidate for reciprocal review. The responsive probe is not a frozen-process stack; no speculative lifecycle-lock change. See docs/2026-10-06-focused-input-followup.md. |
| STD-015 | Triples on Surround and separate monitors | unchecked | Native in-race aspect and independent projected views remain unqualified; a wide title/attract window is insufficient. |
| STD-016 | Forza Horizon telemetry on by default | unchecked | Encoder/units/defaults require the current release audit; telemetry was temporarily off in the latest menu check, then restored. |
| STD-017 | Hide empty settings pages | unchecked | Current panel has not been audited against this newer standard. |
| STD-018 | Handling changes never reach online scores | unchecked | Online/offline eligibility and score paths must be established before any gameplay replay or handling change. |
| STD-019 | Centre menus and gameplay HUD | unchecked | Title/attract images do not qualify menus, loading, F6 and in-race HUD across the requested layouts. |
| STD-020 | Standard portfolio feature checklist | partial | All 25 ledger rows explicit as of 2026-10-07; missing STD-021..025 added during discovery review. Pending/unchecked features require evidence, not automatic adoption. |
| STD-021 | art of rally is the FFB reference | pending | No matched original OutRun force capture or attended comparison against Art at 50; no gain change from discovery. |
| STD-022 | One triple-screen selector | pending | Off / Surround / Separate monitors requires a qualified projected-triples implementation; native wide aspect alone does not establish it. |
| STD-023 | Frame-rate readout in settings and log | unchecked | Existing framerate hooks have not been audited for the standard 10 s mean/1% low/worst summary and 30 s log. |
| STD-024 | Optional on-screen frame-rate counter | unchecked | Saved default-off counter, centre-screen placement and render cost have not been audited. |
| STD-025 | One shared force model for tyre games | pending | Current legacy custom model remains. Research available physical inputs before an AxleForceCurve adapter or a documented arcade exception; named pose fields do not establish tyre-force units. |
