<img align="left" src="Images/567.png" width="95px"/>

# Epoch - Creative Software And Game Engine

**Epoch Engine** is a professional **C++23 game engine and creative software
platform** for games, editors, tools, automation, and AI-assisted workflows
from one codebase. The active source tree is centered on a project-centric
runtime, engine-owned UI/text tooling, C++23 scripting, multicontext renderer
work, and a staged two-role AI workspace that keeps automation reviewable.

<p align="left">
  <img src="https://img.shields.io/badge/Current_Source-v0.83.87-1F7A4C?style=for-the-badge" alt="Current source v0.83.87" />
  <img src="https://img.shields.io/badge/Published_Release-v0.83.86-2C6A8A?style=for-the-badge" alt="Published release v0.83.86" />
  <img src="https://img.shields.io/badge/Project--Centric_Runtime-1F6F78?style=for-the-badge" alt="Project-centric runtime" />
  <img src="https://img.shields.io/badge/Multicontext_Tooling-486B4A?style=for-the-badge" alt="Multicontext tooling" />
  <img src="https://img.shields.io/badge/C%2B%2B23_Scripting-8A5C2F?style=for-the-badge" alt="C++23 scripting" />
  <img src="https://img.shields.io/badge/AI--Assisted_Engine_Ops-5A4D86?style=for-the-badge" alt="AI-assisted engine operations" />
</p>

## What Epoch Is

For newcomers:

- Epoch can launch projects, edit scenes, build scripts, inspect systems, and
  run different rendering backends from one engine-owned shell.
- The launcher and editor are intentionally separate surfaces: launcher for
  project/context/update flow, editor for runtime, scripts, systems, and AI.
- Current packaged releases are lean runtime/bootstrap builds. Full source and
  deeper engine work still live in the repository.

For engine/tooling developers:

- active engine code lives under `Engine/src/`, `Engine/modules/`,
  `Engine/include/`, `Engine/resource/`, and `Engine/ai/`
- local MSVC runtime outputs usually live under `x64/Debug/` and `x64/Release/`
- Windows multicontext validation should run from asset-bearing output folders,
  not from the repo root
- the repo is moving toward one active backend at a time in normal use:
  editor -> OpenGL, launcher -> software, with explicit switching and teardown

## Current Snapshot

- Source is currently `v0.83.87`.
- The latest published runtime release is `v0.83.86`.
- Windows and Linux packaged runtime assets now use versioned names such as
  `epoch_win10_x64_v*.zip` and `epoch_linux_x64_v*.tar.gz`.
- Bootstrap updater-shell releases are separate from the main runtime package
  and are meant to update into the current runtime release, then fall through
  to source only when packaged parity is already reached.
- Phase 1 and Phase 2 of the active roadmap are complete. Current work is
  concentrated in systems tooling, time ownership, AI control/capture, and UI
  maturity.

## What Epoch Provides Right Now

- A project-centric runtime shell that creates, selects, builds, and plays real
  project shells instead of trapping the editor in fake sample flows.
- Generated game and software/tool project shells with explicit build, script,
  output, and manifest proof surfaced in the editor.
- Engine-owned GUI/text rendering with reusable controls and workspace tabs
  instead of middleware-owned editor UI.
- Engine-owned C++23 scripting with project-local source resolution, validation,
  build actions, and runtime execution from the live shell.
- Multicontext renderer orchestration across Raylib, SDL3, SFML, Vulkan,
  OpenGL, software, and headless/noop paths.
- A Systems workspace that already shows real renderer/runtime tooling surfaces
  and is being extended with pacing and ownership diagnostics.
- A time-system spine with fixed-step ownership, pause/resume, scaling,
  single-step, and early editor-facing diagnostics.
- A staged AI workspace centered on two in-engine roles only:
  `EpochBot` and the local MCP/control layer.

## In Action

These proof images come from asset-bearing outputs, not stripped updater-shell
builds.

- A valid Windows six-context proof must visibly show `Raylib`, `SDL`, `SFML`,
  `Vulkan`, `OpenGL`, and `Software`.
- The undock proof must show a real promoted window outside the parent.
- The redock proof beside it is a live desktop capture so the detached and
  returned states can be compared directly.
- The Linux proof comes from an asset-bearing WSL build output, not a source
  tree launched without runtime assets.

Windows fullscreen six-context multicontext proof, source `v0.83.87`:

<p align="center">
  <img src="Images/readme/windows-multicontext-editor-v08387.png" alt="Epoch Windows fullscreen six-context multicontext proof" />
</p>

Current Windows per-backend startup proofs:

<p align="center">
  <a href="Images/readme/windows-raylib-v08387.png"><img src="Images/readme/windows-raylib-v08387.png" alt="Epoch Windows Raylib editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-sdl-v08387.png"><img src="Images/readme/windows-sdl-v08387.png" alt="Epoch Windows SDL editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-sfml-v08387.png"><img src="Images/readme/windows-sfml-v08387.png" alt="Epoch Windows SFML editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-vulkan-v08387.png"><img src="Images/readme/windows-vulkan-v08387.png" alt="Epoch Windows Vulkan editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-opengl-v08387.png"><img src="Images/readme/windows-opengl-v08387.png" alt="Epoch Windows OpenGL editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-software-v08387.png"><img src="Images/readme/windows-software-v08387.png" alt="Epoch Windows Software editor proof" width="15.6%" /></a>
</p>

Windows promoted-window undock proof, live validation:

<p align="center">
  <img src="Images/readme/windows-undock-proof-v08385.png" alt="Epoch Windows promoted-window undock proof" width="49%" />
  <img src="Images/readme/windows-redock-proof-v08387.png" alt="Epoch Windows six-context live redock validation proof" width="49%" />
</p>

WSL/Linux editor proof, latest asset-bearing WSL capture:

<p align="center">
  <img src="Images/readme/linux-opengl-v08386.png" alt="Epoch Linux WSL OpenGL editor proof" width="960" />
</p>

## Quick Start

### Run the local Windows build

Launch from the binary directory so colocated assets resolve cleanly:

```powershell
Set-Location x64/Debug
.\ConsoleApplication1.exe
```

Release build:

```powershell
Set-Location x64/Release
.\ConsoleApplication1.exe
```

### Build with Visual Studio / MSBuild

Solution:

```text
Engine.sln
```

Typical configurations:

- `Debug | x64`
- `Release | x64`

Example app only:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine.sln /t:ConsoleApplication1 /p:Configuration=Debug /p:Platform=x64 /m:1
```

### Build with CMake

Windows MSVC:

```powershell
Set-Location Engine
cmake --preset x64-debug
cmake --build --preset x64-debug
```

Linux:

```bash
cd Engine
cmake --preset Ninja-Debug
cmake --build --preset Ninja-Debug
```

### Run under WSL/Linux

- use an asset-bearing output such as `Engine/Bin/Clang-Release/`
- launch `./epoch` from the output directory
- updater-shell mode is explicit/bootstrap-only on Linux, not the default

## Repository Layout

```text
Engine/    engine code, resources, examples, docs, build config
Changes/   changelog, roadmap, release-note archives, current planning text
Images/    README and repo artwork
Tools/     local helper scripts and validation utilities
x64/       MSVC local outputs with colocated runtime assets
```

## Documentation

Documentation index: [Engine/docs/README.md](Engine/docs/README.md)

Recommended starting points:

- [Engine/docs/build_presets.md](Engine/docs/build_presets.md)
- [Engine/docs/build_scripts.md](Engine/docs/build_scripts.md)
- [Engine/docs/runtime_operations.md](Engine/docs/runtime_operations.md)
- [Engine/docs/ai_build_memory.md](Engine/docs/ai_build_memory.md)
- [Engine/docs/smoke_capture_automation.md](Engine/docs/smoke_capture_automation.md)
- [Engine/docs/wsl_vcpkg_setup.md](Engine/docs/wsl_vcpkg_setup.md)
- [Changes/roadmap.md](Changes/roadmap.md)
- [Changes/changelog.txt](Changes/changelog.txt)
- [Changes/release_notes_archive.md](Changes/release_notes_archive.md)

## Roadmap Direction

The current roadmap is focused on:

1. Growing the Systems workspace into a stronger renderer/runtime ownership and
   pacing surface.
2. Carrying the time-system spine deeper into runtime and scene ownership.
3. Tightening the two-role AI capture, review, and promotion loop.
4. Improving UI/editor maturity without regressing the honest project-centric
   runtime flow.

See [Changes/roadmap.md](Changes/roadmap.md) for the full phase-by-phase plan.

## License

```text
LicenseRef-MIT-NoSell
```

See [LICENSE](LICENSE) for full terms.
