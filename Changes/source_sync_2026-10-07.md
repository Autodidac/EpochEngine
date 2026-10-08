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
downloads, manifests and historical source admissions are preserved. Stable
branch advancement is not included in the current request.

## Delivery Status

GitHub push and Site source activation are pending at this note's creation.
The Site owner has the authorized preflight request; exact pushed commit/archive
hashes will be supplied only after verification. No runtime READY is issued.

Task-owned obsolete utility-project output may be removed after identity checks.
Unrelated deleted workspace files, original screenshots, authored Projects,
candidate ancestry and prior runtime artifacts are excluded from this commit
and cleanup. Current usable builds and validation receipts are retained.
