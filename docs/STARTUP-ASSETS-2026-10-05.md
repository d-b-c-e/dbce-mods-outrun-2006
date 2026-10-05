# Startup crash and missing assets — October 5, 2026

The archived Stream Deck target was missing 110 game assets: all 80 files in
`Scripts/bin` and 30 files in `BK/Bin`. This is a game-data defect independent of
the source/build gates. The exact cause of their disappearance is unknown.

Claude's 14:06 and 14:33 tests crashed at EXE RVA `0xF121A`, both at 7680 and
2560 widths. The retained second crash log has ECX `0x84BCB0`, ESI zero and
return address `0x4EF303`. Exact EXE disassembly traces that call through the
OutrunMiles script-table lookup (`PRICES`/`SETUP`); the referenced
`Scripts/bin/OutrunMiles.bin` was absent. This is strong evidence for the
missing-data diagnosis. The post-restoration startup passed as recorded below.
Do not patch out the null read or blame the lifecycle hook from the stack alone.

On October 5 at 20:00:33 UTC, only the 110 absent assets were restored from the
existing LaunchBox Redux installation. Each matched the independent Tweaks
installation byte-for-byte; both use the exact same game EXE SHA-256
`68CEB386829066F8455B9D027320AF962584321F3E2E8A79C72841495A6134C3`.
All restored hashes passed. Added data total 1,883,961 bytes. All 31 existing
root files and saves retained their hashes, including the mod DLLs, INI files
and FFB Off setting. No game launched or display changed.

Private receipt:
`%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/outrun-assets-20261005-150032`.
Original crash evidence:
`E:/Source/_archive/2026-10-05/outrun-test/OR2006C2C.EXE.20261005143340.zip`.
The game target remains
`E:/Source/_archive/2026-10-04/outrun2006/outrun2006-redux/game`.
No game bytes were added to source control.

## Post-repair startup passed

Claude's coordinated 15:38:32–15:40:05 CT test reached the title at 7680x1440
under Surround and exited normally. The title/attract content remained centred
at 4:3; this does not establish race wide rendering or true triples. Coordinator
evidence is dbce-project-mgmt commit a318c81. Native title confirmation and the
offline route to driving remain unqualified.

Claude's detailed handoff reports an observation interval ending 15:40:14,
normal window-close exit within 15 seconds, no new dump and no input sent.
Its title capture is
`E:/Source/_archive/2026-10-05/outrun-test/repaired-t045-title.png`.
The later failed virtual-pad attempt is reviewed separately in the October 5
addendum to `NATIVE-INPUT-DIAGNOSIS-2026-09-19.md`.

Readback afterward verified all 110 repaired asset hashes and all 30 original
non-log root/save hashes. The remaining original root file is the runtime log,
which correctly changed during the new run. Its exact new bytes are retained at
the private receipt above under retest-20261005-1540; log SHA-256:
DF093C783F16EC7172C2332C7339B4663001CC6644EF4F420049A1604DA2CA59.
This resolves the observed startup crash; it is not a recording/playback test.

Run this read-only check before another unattended launch:

```powershell
./tools/Test-GameData.ps1 -GameDir '<actual OutRun installation>'
```

It validates the exact EXE and the 110 restored assets against the committed
path/size/hash manifest. This is a minimum known prerequisite, not a complete
game-file integrity check. A different legitimate script mod must be reviewed
and fingerprinted; the check never overwrites it. A successful check does not
establish startup, recording, playback, rendering or physical force acceptance.
