<img align="left" src="Images/567.jpg" width="70px"/>

# Epoch

**Epoch** is a C++23, modules-first runtime engine focused on multi-context
rendering, atlas-driven UI, hot-reloadable scripting, and a desktop-first
workflow that still preserves compatibility surfaces for older integrations.

## What Epoch provides

- Multi-context window and render orchestration across OpenGL, SDL3, Raylib,
  SFML, software, and noop/headless paths.
- C++23 module-first engine code under `Engine/modules/` with supporting source
  in `Engine/src/`.
- Atlas-driven rendering and GUI plumbing shared across active backends.
- A scripting and task-graph pipeline intended for hot-reload and tooling
  scenarios.
- A legacy compatibility archive now kept under `Engine/legacy/` instead of a
  separate top-level tree.

## Repository layout

- `Engine/` - active engine code, examples, build scripts, docs, and the legacy archive.
- `Changes/` - changelog, roadmap, and release notes.
- `Images/` - repository artwork and readme assets.
- `Tools/` - helper notes for local tooling setup.

## Documentation map

- Start with `Engine/docs/README.md`.
- Build guidance lives in `Engine/docs/build_presets.md`,
  `Engine/docs/build_scripts.md`, and `Engine/docs/tools_list.md`.
- Runtime and configuration guidance lives in `Engine/docs/runtime_operations.md`
  and `Engine/docs/aengineconfig_flags.md`.
- Architecture and backend status live in `Engine/docs/engine_analysis.md`,
  `Engine/docs/context_audit.md`, and
  `Engine/docs/menu_overlay_backend_audit.md`.
- Legacy migration/reference material lives in `Engine/docs/legacy_archive.md`.

## Quick start

From the repository root:

```powershell
Set-Location Engine
cmake --preset x64-release
cmake --build --preset x64-release
```

On Linux or macOS:

```bash
cd Engine
cmake --preset Ninja-Release
cmake --build --preset Ninja-Release
```

Script-driven builds are also available:

```bash
cd Engine
./build.sh gcc Release
./run.sh gcc Release
```

The public runtime/binary name is `epoch`.

## Compatibility note

Public branding, docs, and release metadata now use **Epoch** consistently.
Public build/config knobs prefer `EPOCH_*` names, while a deeper compatibility
layer still preserves older aliases internally so existing integrations keep
compiling during the remaining migration work.

## Current snapshot

- Version: `v0.82.0`
- Changelog: `Changes/changelog.txt`
- Roadmap: `Changes/roadmap.txt`

## License

`LicenseRef-MIT-NoSell` - see `LICENSE` for full terms.