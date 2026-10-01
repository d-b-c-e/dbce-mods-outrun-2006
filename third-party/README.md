# Third-party distribution inventory

The index identifies exact source revisions, embedded notices and their SHA256
hashes for the reviewed OutRun 2006 PC runtime. Notices are copied from pinned
source files or extracted from source comments; DirectX newline normalization
is explicitly documented below. Retaining
all relevant notices is the packaging policy, including licenses with an
executable-object exception. It is not legal clearance or runtime certification.

INI provenance: import commit 2242d9b86378e678b422075d004b0215a1fad7fa
explicitly names SSARCandy/ini-cpp 138ae81911379e79bf1a9be86726cb77af4a57b0.
The shipped header is a locally modified derivative, blob
bbfa74698ab0e925f887a3e72405e45598ebabe5. Its pinned upstream BSD notice is
retained; local Unicode-path, settings and error-handling changes remain in the
existing fork history. It is not represented as an unmodified upstream file.

ProggyClean font blob 0270cdfe3caa811e1b252cdcc5b179989c1b8ad6 matches both
the pinned ImGui font and the author's bluescan/proggyfonts tree
139ec08a38096161291792313ef5803fc4f0e37b, whose MIT notice is included.
Toolkit notices were retrieved through authenticated pinned source reads for
v0.8.0 and native override dd0ef20ad0cdaccc7a67f10a707dbd2a27a6efe9.
Zycore revision is verified by the pinned Zydis submodule API entry.

DirectX factual provenance is verified against Microsoft DirectX-Headers revision
e3515aca8141c98afd1a7b8436c843ef484ea877 and pinned SDL. This is a complete
content match, not a claim about the original importer's checkout. The index
records Microsoft/import/final blobs and the two SDL modification commits:
SAL fallbacks, compiler pragma guards and five Gpu-to-GPU token edits. Reversing
those edits reproduces the complete normalized upstream headers. Both declare MIT.
The matching-revision license blob is 44378268be1b87777827ab64b1ea2d98e808850e
(1093 bytes). The retained notice is 1074 bytes: CRLF was normalized to LF and
one terminal newline added; otherwise text is identical. It is not byte-identical.
The generated Xbox macros and their SDL-licensed generator are also identified.

The resolved DirectX provenance issue is removed from distribution blockers.
Physical driving/force/camera acceptance, failed historical native title
confirmation and owner/checkpoint review remain open. Shared toolkit v0.13
shutdown ignores completion of its 500 ms watchdog wait before resource release;
no failure was reproduced. Physical FFB rollout requires delayed-worker testing
or fix and binary provenance review. Keep FFB Off; no unattended FFB.
These remaining gates keep distributionReady false. Normal packaging rejects
blockers. -ReviewOnly
permits an explicitly marked local review artifact; it does not grant release.

Schema-2 payload privacy is independently enforced by a source-owned exact path
allowlist. Recataloging private captures, ROMs, settings, logs or unknown notice
files cannot extend it, including with -ReviewOnly. The copied source notice tree
and frozen schema-2 input are checked before output directory creation. Legacy
schema-1 installer compatibility is retained without implying this new policy
retroactively applies to old manifests.

The actual Release link inputs include SDL, FLAC, Ogg, JsonCpp, MiniUPnPc,
SafetyHook, Zydis/Zycore and spdlog, plus directly compiled source/header
dependencies. Optional-code removal by the linker is not treated as grounds to
drop notices. zlib v1.3.1 was configured but not linked or shipped; its source
CMake renamed tracked zconf.h to zconf.h.included and generated zconf.h in the
build directory. The recorded deletion is this configure transformation, not a
uniformly clean source tree. No zlib source/binary is in the package.

SafetyHook emits C4834 at utility.cpp:12: UnprotectMemory's destructor ignores
vm_protect's return while restoring page protection. The helper can report
VirtualProtect failure, which this destructor does not surface. This warning
does not prove a restoration failure occurred. No upstream runtime fix is part
of this packaging milestone.

dinput8.dll is freshly built at the recorded runtime commit; WheelFfb.dll is
retained from the native 0.6.0/v0.13.0 pin. Microsoft OS and VC runtime imports
are prerequisites, not files shipped here. Segoe UI is loaded from Windows;
other unused ImGui font files are not shipped. No game executable, ROM, asset,
private session or generated owner artifact is part of the payload.
