# Muted raw-input discovery and intermittent menu hang

Private runtime `cbbaa5151ba32a247c44a336a11b72e6978de521` was packaged from
clean source, passed the packaged Windows PowerShell 5.1 installer fixture and
was installed closed-game at 06:19 CT. Proxy SHA-256:
`69A957EC3A6EFD7ED391A31070F7684D819C51DA2EAF9958220D52DBB23D7A49`.
Native remains the reviewed 41-export x86 `50ba139`, SHA-256
`432727681B1A8A6F62D68F2E1ED865FF9F229AECCC628B82FA97147626451968`.
ZIP SHA-256: `06979CBC0E01913F635A52E530F335221AF4E5393B12ED8A95C6722FF4BF4026`.
Installation evidence: `build/force-input-cbbaa51-install/verified.json`.

Claude's second run reached Palm Beach with frame-guided native virtual-pad
navigation and gently accelerated for 20 seconds, then coasted. FFB, telemetry
and rumble were off. Request `880dde01afaa406f9d9608d92532dda6` completed with
7,012 rows / 3,506 ordered pairs, no unmatched pre, no missing paired game update,
234 schema-3 columns and 58.410126 seconds of observed rows. Hook duration was
65 microseconds median, 1,205 maximum. All pre rows have observed, finite force
inputs and saved force-output enabled = 0. Car movement and changing raw input
fields are observed; physical units, field roles and authoritative writers are
not thereby established. Near-equal 1c8/1d0/1d4 values on this mostly straight
drive cannot identify their steering meanings.

Data SHA-256: `B30FF41EE583B28A861C2EDB3B210C7127E8E0CEF676B3B63EAFC27A2F36DE12`.
Private evidence is under `_archive/2026-10-08/outrun-forceinput-20261008-063403`.
The standard analyzer independently accepted it (`build/force-input-live-880dde01.json`).
Independent readback verified current proxy/native hashes and all nine restored
owner files, including the five INIs, login file, log and two saves
(`build/force-input-live-independent.json`). Claude reported normal exit 0.

## First attempt: unresolved menu hang

The 06:30 attempt hung after course-select A, before discovery was armed. CPU
stopped advancing; main thread 58160 remained waiting. The owner files were
restored after a forced close. The same build and route then completed with
slower frame-guided navigation. This does not prove a fast-input race or exclude
a regression. A menu freeze was also recorded on the earlier b43445c candidate;
do not attribute this occurrence to the new native library without evidence.

The retained `outrun-forceinput-20261008-063010/OR2006C2C-hang.dmp` has 113 module
entries and **no WheelFfb.dll**. Its AMD64/WOW64 context shows the main thread
waiting through wow64cpu, but the x86 EBP address 0x1afbd4 and TEB32 memory are
not included. Of 239 memory ranges, the only range below 4 MB is the native
64-bit stack at 0xaed18. A symbols-capable debugger cannot reconstruct the omitted
32-bit game/proxy stack from that dump. Future occurrences need a full-memory
dump or a correct x86 dump writer. No speculative lock or timing fix was made.

This is read-only input discovery, not recorded force replay, normalization,
physical output qualification or gameplay playback. Keep the menu hang open;
the successful retry is not enough to mark the candidate ready for public release.
