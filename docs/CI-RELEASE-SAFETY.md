# OutRun2006 PC CI review-artifact safety

The inherited `build.yml` downloaded the proprietary `OR2006C2C.EXE` from an
upstream release and uploaded all of `build/bin/`. This candidate removes that
acquisition step and the directory upload. It leaves public source/history,
existing artifacts, runtime/native ABI, owner WIP and installed files intact.
No historical workflow/artifact is run, downloaded, deleted or replaced here.

The corrected workflow is **manual-only** (`workflow_dispatch`) with read-only
contents permission and checkout credential persistence disabled. It has not
been dispatched or enabled through repository settings. No push/PR auto-run is
retained in this first correction. The hosted x86 Release build and its existing
dependency workarounds remain, with explicit CMake failure checks.

## Checks and upload boundary

Before staging, the workflow runs CI artifact safety tests, INI coverage, the
production-linked legacy calculation/recorder fixtures, synthetic product package
tests, synthetic installer and actual PS5.1/batch player-route tests. It then
creates a **review-only** mod package and runs its recursive inventory/privacy
suite and packaged PS5.1 player-route tests. No synthetic game stub is executed,
no runtime DLL is loaded by these tests, and no device/game is accessed.

Each test runs in a child PowerShell host. Thrown failures and explicit nonzero
script exits propagate to the CI job. Existing suites explicitly validate native
invalid-input exits, so a correctly asserted refusal is not misreported as a
failed suite merely because its last native process returned 1 or 2. Regression
tests cover thrown failure, `exit 17` and a successfully asserted native exit 2.
An optional dependency root permits local fixture compilation from already
verified public dependency headers/libs; CI defaults to its own fresh build.

`Prepare-CiArtifact.ps1` accepts only a validated, clean-source, current-commit,
review-only x86 package. The source-owned package path allowlist cannot be extended
through manifests. Shipped defaults, profiles, scripts, notices, provenance and
the pinned native DLL must match public checkout bytes, independent of rehashed
package metadata. `dinput8.dll` must have bounded x86 PE DLL characteristics,
so renaming an ordinary game EXE to that filename is refused. These checks detect
accidental private payloads; they do not authenticate arbitrary malicious DLL
content or establish legal clearance/physical acceptance.

Staging creates a fresh build-local directory containing exactly:

- `outrun2006-ci-review.zip`, with flat allowlisted package entries;
- `artifact-inventory.json`, with a fixed metadata shape and file hashes.

The final guard rereads the ZIP itself, refuses unknown/private paths, duplicate
or linked entries, decoded-size bounds, missing public files, mismatched hashes,
modified defaults, unexpected metadata and extra/linked artifact paths. It checks
the exact two-file outer inventory and ZIP hash immediately before upload. It
never extracts or executes a ZIP payload. Output paths stay within this checkout's
build tree, and linked output ancestry is refused.

The upload action names those two files explicitly, has `if-no-files-found: error`,
uses seven-day retention and runs only after success. Raw `build/bin/`, fixture
EXEs, synthetic game stubs, tests, logs, recordings, owner settings and arbitrary
build/package directories are never upload paths. The unchanged distribution
blockers remain enforced for release packaging; a CI review artifact is not a
release or accepted runtime.

## Private input and local validation limits

The exact-host lifecycle suite requires a locally supplied private executable for
hash verification. It is deliberately excluded from public CI. No URL or public
artifact route for that input is introduced. An owner may separately run that
existing local test with their private file; neither it nor local evidence belongs
in this review artifact.

Local validation uses digest-verified official actionlint v1.7.12 and PowerShell
AST parsing, executes the selected suites, and tests final archive staging/guards.
Artifact safety fixtures use an explicit 128-byte synthetic PE header and the
already public pinned toolkit DLL; neither is loaded. The local end-to-end checks
reuse the prior verified PC build output and public dependency libraries because
production source inputs are unchanged. They do not establish a fresh hosted
CMake build, GitHub runner compatibility, upload service success or passing CI.
No GitHub Actions workflow has run during this work. The old public workflow
remains a risk until this isolated candidate is independently reviewed/promoted.

Official input/trigger references:
[upload-artifact v4.3.3](https://github.com/actions/upload-artifact/blob/v4.3.3/README.md#inputs),
[GitHub workflow syntax](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#onworkflow_dispatch).
