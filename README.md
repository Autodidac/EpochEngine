[![CMake Build](https://github.com/Autodidac/EpochEngine/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=main)](https://github.com/Autodidac/EpochEngine/actions/workflows/cmake-multi-platform.yml)
[![MSBuild](https://github.com/Autodidac/EpochEngine/actions/workflows/msbuild.yml/badge.svg?branch=main)](https://github.com/Autodidac/EpochEngine/actions/workflows/msbuild.yml)

# Epoch - Creative Software And Game Engine

<p align="left">
  <img src="https://img.shields.io/badge/Current_Source_Development-v0.87.08-1F7A4C?style=for-the-badge" alt="Current development source v0.87.08" />
  <img src="https://img.shields.io/badge/Published_Stable_Release-v0.84.38-2C6A8A?style=for-the-badge" alt="Published stable release v0.84.38" />
</p>

<p align="center">
  <img src="Images/readme/epoch-engine-overview-v08435.png" alt="Epoch Engine overview card" />
</p>

<p align="left">
  <img src="https://img.shields.io/badge/Project--Centric_Runtime-1F6F78?style=for-the-badge" alt="Project-centric runtime" />
  <img src="https://img.shields.io/badge/Multicontext_Tooling-486B4A?style=for-the-badge" alt="Multicontext tooling" />
  <img src="https://img.shields.io/badge/AI--Assisted_Engine_Ops-5A4D86?style=for-the-badge" alt="AI-assisted engine operations" />
  <img src="https://img.shields.io/badge/C%2B%2B23_Scripting-8A5C2F?style=for-the-badge" alt="C++23 scripting" />
  <img src="https://img.shields.io/badge/Functional_Static_Programming-365D7D?style=for-the-badge" alt="Functional static programming" />
  <img src="https://img.shields.io/badge/AI--Software_Creation-6A4F8A?style=for-the-badge" alt="AI software creation" />
  <img src="https://img.shields.io/badge/AI--Game_Creation-6B7A32?style=for-the-badge" alt="AI game creation" />
  <img src="https://img.shields.io/badge/Universal_Build_Projects-6D5C36?style=for-the-badge" alt="Universal build projects" />
  <img src="https://img.shields.io/badge/Automatic_Entry_Point_Handling-8A473D?style=for-the-badge" alt="Automatic entry point handling" />
  <img src="https://img.shields.io/badge/Zero--Dependency_Core_Goal-34495E?style=for-the-badge" alt="Zero-dependency core goal" />
</p>

<p align="center">
  <strong>Worlds First</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/AI_Software_Engine-0E7490?style=for-the-badge" alt="AI software engine" />
  <img src="https://img.shields.io/badge/Fully_Featured_Vibe--Coded_Engine-A855F7?style=for-the-badge" alt="Fully featured vibe-coded engine" />
</p>

---

<p align="center">
  <img src="Images/readme/epoch-engine-story-v08431.png" alt="The story of Epoch Engine timeline" />
</p>

## What Epoch Is

For newcomers:

- Think of Epoch as one big workbench for making games and tools. Engines like
  Unity, Unreal, and Godot try to give you one main place to build things, and
  Epoch is aiming for that same kind of all-in-one home in its own way.
- The launcher is the front door. You pick a project, choose settings, check
  updates, and then open the editor.
- The editor is the work room. You can look at scenes, run the project, build
  scripts, check systems, and use AI tools there.
- The AI workspace has its own sub-workspaces. `Sandbox` is the separate
  self-iteration control room, `Harness` runs editor tool scripts and captures
  before/after state, `Assistant` is for normal game-engine/project guidance,
  `Launcher` tracks project/build evidence, and `Training` handles
  evidence-backed promotion.
- Epoch can also draw the same project in different ways. Those are called
  rendering backends, but you can think of them as different drawing engines
  under the hood.
- The downloadable builds are kept small on purpose. The full source code and
  the deeper engine work still live here in the repository.

For engine/tooling developers:

- active engine code lives under `Engine/src/`, `Engine/modules/`,
  `Engine/include/`, `Engine/resource/`, and `Engine/ai/`
- local MSVC runtime outputs usually live under `x64/Debug/` and `x64/Release/`
- Windows multicontext validation should run from asset-bearing output folders,
  not from the repo root
- the repo is moving toward one active product backend at a time in normal use:
  editor -> OpenGL today, Windows-native product rendering -> DirectX/D3D11 as
  it matures, and software kept as fallback/debug/headless support

## Current Snapshot

- Source is currently the active development line at `v0.87.08`.
- The latest published stable runtime release is `v0.84.38`.
- Windows and Linux `v0.84.38` runtime packages use the production package
  layout: one editor/runtime executable, a root `assets/` folder, and public
  README/LICENSE files. Generated atlases, source-shaped `Engine/` folders,
  headless smoke binaries, and duplicated compatibility output folders are not
  part of the public runtime payload.
- Windows and Linux packaged runtime assets now use versioned names such as
  `epoch_win10_x64_v*.zip` and `epoch_linux_x64_v*.tar.gz`.
- MSVC Debug/Release multicontext editor builds use the dynamic-vcpkg
  `x64-windows` lane so Raylib, SFML, SDL3, GLAD, OpenGL, Vulkan, and DirectX can
  coexist without static duplicate-symbol collisions. DLLs beside the debug
  executable are expected runtime dependencies for that editor lane.
- Bootstrap updater-shell releases are separate from the main runtime package
  and are meant to update into the current runtime release, then fall through
  to source only when packaged parity is already reached.
- Update/install policy is binary-first across install types: packaged Windows
  installs use the newest matching `.zip`, packaged Linux/WSL installs use the
  newest matching `.tar.gz`, and source checkouts rebuild from source only after
  packaged-runtime parity or when no newer packaged runtime exists.
- Phase 1 and Phase 2 of the active roadmap are complete. Current work is
  concentrated in systems tooling, time ownership, AI sandbox/capture, and UI
  maturity.
- Latest source checkpoint: Linux/Clang full-engine `epoch` builds again after
  the C++23 module/API type include fix, and Raylib's native sampled
  render-texture lane now fails closed in build-only/no-runtime validation
  instead of attempting GPU allocation without a live Raylib context.

OS AI model, tooling, and evidence rules live with the AI assets in
`Engine/ai/README.md` and the engine policy docs.

## In Action

Epoch's visual proof comes from asset-bearing editor outputs, not stripped
bootstrap shells. The current public gallery is below; the top proof shows the
Windows multicontext editor carrying every active backend lane.

<p align="center">
  <img src="Images/readme/windows-multicontext-editor-v08700.png" alt="Epoch Windows multicontext editor proof" />
</p>

## What Epoch Provides Right Now

- Project-centric editor/runtime flow: launcher, generated project shells,
  single-context project runs, and an engine-shaped self-iteration lane.
- Workspaces: `3D Scene`, `2D Scene/UI`, `Assets`, `Plant Lab`, `Video`,
  `Project`, `Intelligence`, and `System Info`, with Console Dock kept as
  compact status evidence instead of a duplicate control surface.
- Rendering spine: Raylib, SDL3, SFML, Vulkan, OpenGL, DirectX, software, and
  headless/noop lanes are orchestrated by the same project/editor contracts,
  with render-graph material, mesh, model, and render-target bindings now
  flowing through the shared device contract.
- GUI spine: shared C++23 controls for windows, tabs, select boxes, scroll
  panels, progress bars, modals, text editing, theme preferences, and future
  popout/docking work.
- Time and input spine: core time, timeline/video sequencing, streaming-save
  contracts, frame-limit presets, and universal input profiles are engine data,
  not scattered per-backend behavior.
- Package spine: local runtime minis, Forest Factory/voxel/ocean research
  gates, model assets, and future downloadable source packages route through
  executable-local `cache/packages/` and human-approved build/install gates.
- OS AI spine: Qwen/Nemotron coding lanes plus Bonsai, Wan, TRELLIS, and FLUX
  creative model lanes are on-demand external model assets under `cache/models/`
  with license/notice and evidence gates.
- Validation spine: headless contract tests, project-shell self-tests, CI lanes,
  asset-bearing runtime proof, and visible editor notes keep promoted behavior
  tied to evidence.

## Visual Proof Gallery

These proof images come from asset-bearing outputs, not stripped updater-shell
builds.

- A valid Windows six-context proof must visibly show `Raylib`, `SDL`, `SFML`,
  `Vulkan`, `OpenGL`, and `DirectX`.
- Floating-window proof is archived in `Images/readme/`; the active README
  gallery now favors the current `v0.87.00` multicontext editor capture so old
  promoted-window screenshots do not look like current UI proof.
- The Linux proof comes from an asset-bearing WSL build output, not a source
  tree launched without runtime assets.

Current Windows per-backend startup proofs from the same `v0.87.00` capture
round:

<p align="center">
  <a href="Images/readme/windows-raylib-v08700.png"><img src="Images/readme/windows-raylib-v08700.png" alt="Epoch Windows Raylib editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-sdl-v08700.png"><img src="Images/readme/windows-sdl-v08700.png" alt="Epoch Windows SDL editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-sfml-v08700.png"><img src="Images/readme/windows-sfml-v08700.png" alt="Epoch Windows SFML editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-vulkan-v08700.png"><img src="Images/readme/windows-vulkan-v08700.png" alt="Epoch Windows Vulkan editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-opengl-v08700.png"><img src="Images/readme/windows-opengl-v08700.png" alt="Epoch Windows OpenGL editor proof" width="15.6%" /></a>
  <a href="Images/readme/windows-directx-v08700.png"><img src="Images/readme/windows-directx-v08700.png" alt="Epoch Windows DirectX editor proof" width="15.6%" /></a>
</p>

Windows multicontext editor proof from the same current capture round:

<p align="center">
  <a href="Images/readme/windows-multicontext-editor-v08700.png"><img src="Images/readme/windows-multicontext-editor-v08700.png" alt="Epoch Windows multicontext editor proof" width="960" /></a>
</p>

WSL/Linux editor proof, latest asset-bearing visual capture:

<p align="center">
  <img src="Images/readme/linux-opengl-v08438.png" alt="Epoch Linux WSL OpenGL editor proof" width="960" />
</p>

`v0.84.38` WSL Clang build/headless validation is green with DirectX disabled,
and WSL/OpenGL visual proof is current for the single-context Linux path.

## Quick Start

### Run the local Windows build

Launch from the binary directory so colocated assets resolve cleanly:

```powershell
Set-Location x64/Debug
.\EpochEditor.exe
```

Release build:

```powershell
Set-Location x64/Release
.\EpochEditor.exe
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

Generated shell self-test:

```powershell
.\x64\Debug\EpochEditor.exe --editor-project-self-test sandbox
.\Projects\Sandbox\bin\windows\Debug\x64\Sandbox.exe --project-self-test
.\x64\Debug\EpochEditor.exe --editor-project-self-test projectlauncher
.\Projects\ProjectLauncher\bin\windows\Debug\x64\ProjectLauncher.exe --project-self-test
```

### Build with CMake

Windows MSVC:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

Linux:

```bash
cmake --preset ninja-clang-debug
cmake --build --preset ninja-clang-debug
```

Linux/GCC currently uses the headless validation presets by default because
GCC 14 can ICE while writing full-engine C++ module BMIs. Use
`ninja-gcc-debug` for headless validation, use Clang for full Linux engine
builds, or explicitly opt into the experimental GCC module path with
`-DEPOCH_ALLOW_GCC_MODULE_ENGINE=ON`.

Hosted CI mirrors that split: required portable lanes build/test headless, and
the Linux Clang engine lane builds the real `epoch` target with OpenGL,
software renderer, and SFML enabled at build time without launching GUI windows.

### Run under WSL/Linux

- use an asset-bearing output such as `Engine/Bin/Clang-Release/`
- launch `./epoch` from the output directory
- updater-shell mode is explicit/bootstrap-only on Linux/WSL, not the default
  runtime identity

## Repository Layout

```text
Engine/    engine code, internal source grouped toward platform/render, examples, assets, and docs
Changes/   changelog, roadmap, release-note archives, current planning text
Images/    README and repo artwork
Tools/     local helper scripts and validation utilities
x64/       MSVC local outputs with colocated runtime assets
```

## Documentation

Documentation index: [Engine/docs/README.md](Engine/docs/README.md)

If you're new:

- [Engine/docs/build/cmake_presets_and_builds.md](Engine/docs/build/cmake_presets_and_builds.md)
- [Engine/docs/build/local_build_scripts_and_release_packaging.md](Engine/docs/build/local_build_scripts_and_release_packaging.md)
- [Engine/docs/engine/runtime_and_editor_workflows.md](Engine/docs/engine/runtime_and_editor_workflows.md)
- [Engine/docs/platform/linux/linux_wsl_build_setup.md](Engine/docs/platform/linux/linux_wsl_build_setup.md)

If you're digging into engine behavior:

- [Engine/docs/engine/current_engine_architecture.md](Engine/docs/engine/current_engine_architecture.md)
- [Engine/docs/engine/backend_context_status.md](Engine/docs/engine/backend_context_status.md)
- [Engine/docs/engine/backend_menu_overlay_status.md](Engine/docs/engine/backend_menu_overlay_status.md)
- [Engine/docs/engine/os_ai_tooling_and_evidence_policy.md](Engine/docs/engine/os_ai_tooling_and_evidence_policy.md)
- [Engine/docs/engine/smoke_capture_and_screenshot_workflow.md](Engine/docs/engine/smoke_capture_and_screenshot_workflow.md)

Project planning and release history:

- [Changes/roadmap.md](Changes/roadmap.md)
- [Changes/changelog.txt](Changes/changelog.txt)
- [Changes/engine_history_and_release_archive.md](Changes/engine_history_and_release_archive.md)

## Roadmap Direction

The current roadmap is focused on:

1. Tightening the 3D Scene, 2D Scene/UI, Plant Lab, Video, Intelligence, and
   System Info workspaces into professional docked editor surfaces.
2. Carrying the renderer-resource spine into backend-native mesh/model,
   render-to-texture, material, and import paths.
3. Tightening the OS-model capture, review, and promotion loop with a
   separate self-iteration sandbox and watchable scene/tool evidence tasks.
4. Improving UI/editor maturity without regressing the honest project-centric
   runtime flow.

See [Changes/roadmap.md](Changes/roadmap.md) for the full phase-by-phase plan.

## License

```text
LicenseRef-MIT-NoSell
```

Epoch is free for non-commercial use. For commercial use or any legal edge
case, read [LICENSE](LICENSE) directly and follow the full agreement text
instead of relying on the README summary.
