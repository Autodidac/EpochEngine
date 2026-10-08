# October 7 — Source, Build Modes And Documentation Checkpoint

Source identity is **v0.90.35**. This consolidates the local October 7 repairs,
not a replacement runtime release or native self-coding acceptance.

## Scope

- Owned source folders with synchronized CMake/MSVC references and filters.
- Typed model/provider failure and request-owned streaming/activity reporting;
  foreground input cancellation and covered-pointer guards.
- Transactional source discovery, actual excerpt fingerprints and unchanged
  fallback detection without discarding accepted context.
- Existing Windows projects: Debug, Release, ReleaseWithDebugInfo and BuildAll.
  BuildAll uses CMake for three sequential builds; no extra combined project.
- Exact root build inputs in candidate snapshots. Portable EpochGui source
  ownership matches MSVC/CMake; its pinned standalone mirror is unchanged.
- Current docs/README/version resources refreshed together. README contains no
  website URL. Operator workbench screenshot retains its real v0.90.34 identity;
  historical screenshots stay archived, not mislabeled as new proof.

## Validation And Remaining Gates

Before version stamping, all three direct production MSVC configurations pass.
After stamping, the real same-project BuildAll CMake workflow builds all three
v0.90.35 Editor configurations successfully (zero MSBuild errors/warnings;
optional Vulkan shader generator and Doxygen configure warnings remain). Windows
resource FileVersion/ProductVersion both read 0.90.35. The version-stamped
Release pure engine contracts pass; the existing freshly rebuilt CPU suite
passes 15/15 again. Naming/layout/filter/dependency/version/diff guards pass.
Remote EpochGui remains `c52e91293749a7a34959bfc042c582c3ac182504`; Extensions
remains `eccef139f982a9e6b64295cd9c8b976905ef0990`. No dependency source changes
or invented releases are needed.

Evidence is retained locally in `build/build-modes-20261007/`, the final
`build/windows-msvc-debug/Engine/Release/logs/Engine.Editor.SelfTest.log`, and
`build/source-layout-20261007/cmake/Testing/Temporary/LastTest.log`.

Native Qwen/PID/docked comparison/Keep-Choose/successor, Project Assistant,
foreground interaction and OS-confinement acceptance remain open. No GUI,
renderer or model launch is part of this publication pass. Linux runtime/package
requalification is not performed here. These gaps prevent an easy honest new
binary release; do not manufacture one from Windows build success.

Public GitHub source and Site private paired source admission are distinct.
Windows/Linux packaged authority stays v0.90.33 at
`f33d3d6758abf402747870df4f8887e7887a2035`; macOS stays v0.89.30. Existing tags,
downloads, manifests and historical source admissions are preserved. The
operator's subsequent explicit request authorizes stable-branch fast-forwarding
to mirror main until the accepted software/context-base freeze gate.

## Delivery Status

GitHub source checkpoint `2cf90fec325d312bb907866270574753d2c8d279` is pushed;
tree `5bf98e5e35e589096acc2f20c35d90a8f9f50569`, 1,078 committed files.
Both remote main and multicontext-base-stable were independently confirmed at
that commit. Stable advancement was fast-forward only; the original
`ad6c416d930b348a61bc37ceb7d4522742be084a` remains in history. The final receipt
follow-up is documentation-only and keeps both refs matched.

GitHub-rendered README contains v0.90.35 and the refreshed image with no website
URL. The screenshot blob was independently read back and SHA-256 matched.

Exact-commit archives use prefix `EpochEngine-0.90.35-source/`:

- ZIP: 63,983,432 bytes; SHA-256
  `bc646800670c9d8cdde3a965e8e97817fbb0bc631837832961e5007b244128cd`.
- TAR.GZ: 63,422,662 bytes; SHA-256
  `7b7ad9b955e244621024aa91014bb9170ff3bd69363710d964aacee2c9ffdb13`.

ZIP member paths/count match the exact committed tree; no generated cache or
runtime bucket is admitted. Site **v112** independently regenerated the archives,
verified both encrypted live downloads and exact offline decryption, then
activated the private source pair atomically. Site commit:
`8b25df25e5f8e6ea1c10a7bd5c8621f584da2526`; deployment
`appgdep_6ac6e5e149f0819180b5ae9625d64d12` succeeded. No runtime READY is issued.

All 89 Site contracts/build pass; lint has zero errors and ten pre-existing
warnings. The nine runtime assets were freshly read back with unchanged bytes,
hashes and download headers; latest/integrity response bodies remain unchanged
and the Ed25519 signature verifies. Public Site plaintext/Git requests remain
404; anonymous/forged private/admin requests remain 401. Prior private-source
rows/objects are retained. Site dependencies still have two disclosed high
advisories (sharp/librsvg and source-map-js); this pass does not repair them or
claim security-clean dependencies.

Site verification receipts are retained by the Site owner under
`work/release-verification/epoch-source-v0.90.35/`. It removed 151,265,389 bytes
of task-owned staging/helper output, preserving live objects and history.

Hosted MSBuild run `37708216729` and CMake run `37708216682` were started for the
checkpoint. MSBuild and Windows MSVC/cpp26 plus headless Linux Clang/GCC passed
at receipt time; full Linux Clang engine validation remains in progress.
Full hosted completion is not inferred from the local checks.

Removed the two obsolete utility-project output files and 1,861,531,845 bytes
of duplicate task-owned v0.90.34 validation binaries/libraries/PDBs after exact
path/reparse/owned-process checks. Those bytes are reproducible by rebuilding;
validation logs were copied into retained-runtime-logs before removal.
Root ignore rules exclude executable-local workspace cache after the unrelated
workspace .gitignore deletion; this hides generated state without deleting it.
After confirmed Site admission, the two task-owned local transfer archives
(127,406,094 bytes) are removed; exact bytes can be regenerated from the named
Git commit with the documented prefix. Final Windows builds and proof remain.
Unrelated deleted workspace files, original screenshots, authored Projects,
candidate ancestry and prior runtime artifacts are excluded from this commit
and cleanup. Current usable builds and validation receipts are retained.
