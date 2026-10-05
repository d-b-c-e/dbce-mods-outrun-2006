# Startup crash and missing assets — October 5, 2026

The archived Stream Deck target was missing 110 game assets: all 80 files in
`Scripts/bin` and 30 files in `BK/Bin`. This is a game-data defect independent of
the source/build gates. The exact cause of their disappearance is unknown.

Claude's 14:06 and 14:33 tests crashed at EXE RVA `0xF121A`, both at 7680 and
2560 widths. The retained second crash log has ECX `0x84BCB0`, ESI zero and
return address `0x4EF303`. Exact EXE disassembly traces that call through the
OutrunMiles script-table lookup (`PRICES`/`SETUP`); the referenced
`Scripts/bin/OutrunMiles.bin` was absent. This is strong evidence for the
missing-data diagnosis, but a post-restoration runtime test is still required.
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

Run this read-only check before another unattended launch:

```powershell
./tools/Test-GameData.ps1 -GameDir '<actual OutRun installation>'
```

It validates the exact EXE and the 110 restored assets against the committed
path/size/hash manifest. This is a minimum known prerequisite, not a complete
game-file integrity check. A different legitimate script mod must be reviewed
and fingerprinted; the check never overwrites it. A successful check does not
establish startup, recording, playback, rendering or physical force acceptance.
