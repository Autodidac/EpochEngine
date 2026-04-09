<img align="left" src="Images/567.png" width="95px"/>

# Epoch - Creative Software And Game Engine

**Epoch Engine** is a professional **C++23 game engine and creative software
platform** for building games, editors, tools, pipelines, and real-time
interactive systems from a single modern codebase. It combines a
modules-first architecture, two engine AI runtime roles under one
engine-owned surface, custom UI powered by an automated texture-atlas
system, built-in C++23 scripting that compiles with the engine and project,
multi-context rendering, launcher + editor workflows, a project-driven
runtime built around engine projects and scenes, and a shared time-system
spine that treats simulation pacing, pause/resume, scaling, and stepping as
first-class engine ownership instead of ad hoc per-backend behavior.

The active engine lives in:

```text
Engine/src/
Engine/include/
Engine/modules/
Engine/resource/
Engine/ai/
```

with prebuilt MSVC runtime binaries commonly landing in:

```text
x64/Debug/
x64/Release/
```

Those binary folders also carry runtime assets, so launching from the binary
directory is the safest default for local testing.

For Windows multi-context smoke tests, prefer launching directly from
`x64/Debug/` or `x64/Release/` so docked SDL/SFML/Vulkan panes see the same
asset set as the main editor host.

---

# What Epoch provides

- Internalized engine bootstrap and entry-point flexibility. Epoch can own the
  desktop startup path itself or be embedded with handled/headless entry
  configurations through the active `EPOCH_*` runtime macros. See
  [configuration flags](Engine/docs/aengineconfig_flags.md) and
  [runtime operations](Engine/docs/runtime_operations.md).
- Two engine AI runtime roles under one engine-owned surface: the internal
  EpochBot and the local MCP/control layer that can both operate the engine and
  train EpochBot while the engine is being used and built.
- External local LLMs such as LM Studio are development helpers for testing,
  evaluation, dataset cleanup, documentation acceleration, and editor/build
  assistance. They are not a third engine runtime role.
- Multi-context, multi-backend runtime orchestration across OpenGL, Vulkan,
  SDL3, Raylib, SFML, software, and noop/headless paths.
- Project-driven workflow that routes projects into the editor and scene play
  into runtime mode, instead of treating the editor as a loose debug shell or a
  permanent launcher for sample games.
- A real project shell direction with editor-first launcher profiles plus the
  first generated game-project and software/tool-project shell flow, so Epoch
  can bootstrap work the way a serious engine or creative IDE should.
- An explicit 2D production track inside that project shell direction, so the
  launcher/editor path can serve side-scrollers, top-down games, UI-heavy work,
  and faster solo-developer iteration without falling back to fake sample
  labels.
- Generated project shells are expected to support both duplicated engine-source
  layouts and static engine integration through `Engine/include/` when a
  project compiles the engine directly into its own source tree.
- A software-development path alongside the game path, so the same engine shell
  can generate and run creative tools, editors, and application-style projects
  instead of pretending every project is only a game.
- A time-based engine direction in the simulation sense, with fixed-step
  ownership, pacing diagnostics, pause/resume, time scaling, single-step
  control, and future replay/timeline hooks being folded into the core engine
  instead of left to one-off subsystems.
- Desktop-style editor workflow with scene preview control, command surfaces,
  a modular workspace shell for project/scripts/systems/AI/output docks, and
  backend-aware fallback behavior.
- Custom GUI, sprite, and text pipelines built on the engine's own automated
  texture/atlas system rather than copied independently into each backend.
- ECS-style systems, scene plumbing, gameplay modules, and engine-owned runtime
  state.
- Built-in C++23 scripting that compiles as part of the engine/project, with
  editor-triggered run actions, a host API for runtime/editor callbacks, and
  task-graph-backed asynchronous work scheduling.
- Diagnostics, renderer telemetry, runtime logging, and updater plumbing as
  first-class engine systems.
- A Systems workspace that now renders engine-generated frame/task graph
  textures with pan/zoom controls, support-tier diagnostics, and the first
  shared time-spine diagnostics, and is explicitly moving toward deeper
  multithreaded renderer tooling instead of staying a fake placeholder.
- A parented multicontext path that is supposed to expose one honest pane per
  active backend, with real backend child surfaces owning render/input instead
  of fake dock wrappers leaking into the visible layout.
- Broad automatic hardware support as a first-class target, centered on
  6-core / GTX 1660 Ti-era desktops and modern Linux laptops by default, with
  heavier backend/lib support exposed as project-level opt-in tiers.
- A modern but practical renderer direction: GPU-driven baseline first,
  centered on visibility -> surface -> lighting -> temporal ->
  reconstruction -> present, with heavier techniques kept behind support tiers
  or explicit project opt-in.
- A later procedural world and time-node authoring phase for SpeedTree-like
  modular asset/world workflows, with future O2L integration documented as a
  source for that phase instead of a current dependency.
- Cross-platform build freedom: Visual Studio, MSBuild, CMake presets, VS Code,
  shell-script workflows, and multiple compiler families across Windows, Linux,
  and macOS.
- Module-first public engine surface centered around active C++23 modules and
  the exported [epochengine module](Engine/modules/epochengine.ixx).
- Naming is still converging. Legacy/orphan names from older `aengine*` eras
  remain transitional debt, and the roadmap now treats consistent professional
  module/file naming as a real cleanup track instead of leaving it implicit.

---

# In action

These README captures are editor/source proofs, not updater-shell screenshots.
If a packaged bootstrap release looks older than these, it has not caught up to
the current source/editor state yet.

- A valid six-context proof must visibly show `Raylib`, `SDL`, `SFML`,
  `Vulkan`, `OpenGL`, and `Software`.
- Parent/helper host windows must not become stray fake panes. For the current
  hosted SDL/SFML model, the visible `EpochChild` host may still own the pane as
  long as the real `SDL_app` or `SFML_Window` child is alive and rendering
  inside it honestly.
- Black or empty software captures do not count as proof.
- Refresh the multicontext proof at least every 10th feature version, or
  sooner whenever renderer color, docking, context visibility, or layout
  behavior changes enough to make the older image misleading.
- Capture from an asset-bearing `x64/Debug/` or `x64/Release/` launch only.

Windows editor six-context multicontext proof, source `v0.83.72`:

<p align="center">
  <img src="Images/readme/windows-multicontext-editor-v08372.png" alt="Epoch Windows editor six-context multicontext proof" width="1400" />
</p>

---

Windows editor backend proofs, source `v0.83.41`:

<p align="center">
  <img src="Images/readme/windows-opengl.png" alt="Epoch Windows OpenGL editor proof" width="32%" />
  <img src="Images/readme/windows-sdl.png" alt="Epoch Windows SDL editor proof" width="32%" />
  <img src="Images/readme/windows-software.png" alt="Epoch Windows software editor proof" width="32%" />
</p>

---

WSL/Linux editor proof, source `v0.83.42`:

<p align="center">
  <img src="Images/readme/linux-sfml.png" alt="Epoch Linux WSL SFML editor proof" width="960" />
</p>

WSL/Linux note:

- The Linux screenshot above comes from the engine's own frame capture under WSLg, which avoids the extra-window behavior that can make desktop grabs misleading.
- The broader Linux multi-window view is still visually inconsistent under WSLg, so the README is using the clean single-backend editor proof for now.

---

# Repository layout

```text
Engine/
```

Engine code, build configuration, resources, examples, docs, assets, and
editor/runtime systems.

```text
Engine/ai/
```

Repo-safe AI datasets, schemas, evals, manifests, tokenizer metadata, and
prompt templates. Raw captures, checkpoints, and model weights stay local.

```text
x64/
```

MSVC runtime outputs and colocated runtime assets for local launches.

```text
Changes/
```

Active changelog, roadmap, and current release notes.

```text
Images/
```

Repository artwork and README assets.

```text
Tools/
```

Local helper scripts and tooling notes.

---

# Build systems

## Visual Studio 2022

Open the solution at:

```text
Engine.sln
```

Typical configuration:

- `Debug | x64`
- `Release | x64`

Primary engine project surfaces:

- [Engine/Engine.vcxitems](Engine/Engine.vcxitems)
- [Engine/examples/StaticLib1/StaticLib1.vcxproj](Engine/examples/StaticLib1/StaticLib1.vcxproj)
- [Engine/examples/ConsoleApplication1/ConsoleApplication1.vcxproj](Engine/examples/ConsoleApplication1/ConsoleApplication1.vcxproj)

## MSBuild

From the repository root in Developer PowerShell:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine.sln /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
```

To build the example app only:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine.sln /t:ConsoleApplication1 /p:Configuration=Debug /p:Platform=x64 /m:1
```

## CMake Presets

Presets live at [Engine/CMakePresets.json](Engine/CMakePresets.json).

Windows MSVC:

```powershell
Set-Location Engine
cmake --preset x64-debug
cmake --build --preset x64-debug
```

Windows Clang:

```powershell
Set-Location Engine
cmake --preset clang-x64-debug
cmake --build --preset clang-x64-debug
```

Windows GCC / MinGW:

```powershell
Set-Location Engine
cmake --preset gcc-x64-debug
cmake --build --preset gcc-x64-debug
```

Linux:

```bash
cd Engine
cmake --preset Ninja-Debug
cmake --build --preset Ninja-Debug
```

macOS:

```bash
cd Engine
cmake --preset macos-debug
cmake --build --preset macos-debug
```

## VS Code

VS Code workspace configuration lives in:

```text
Engine/.vscode/
```

Key files:

- [Engine/.vscode/tasks.json](Engine/.vscode/tasks.json)
- [Engine/.vscode/launch.json](Engine/.vscode/launch.json)
- [Engine/.vscode/settings.json](Engine/.vscode/settings.json)
- [Engine/.vscode/c_cpp_properties.json](Engine/.vscode/c_cpp_properties.json)
- [Engine/.vscode/cmake-kits.json](Engine/.vscode/cmake-kits.json)

Recommended flow:

1. Open `Engine/` in VS Code.
2. Select a CMake preset or task matching your compiler.
3. Build through the bundled tasks or the CMake Tools extension.

## Shell scripts

Scripted build helpers:

- [Engine/build.sh](Engine/build.sh)
- [Engine/run.sh](Engine/run.sh)
- [Engine/install.sh](Engine/install.sh)
- [Engine/clean.sh](Engine/clean.sh)

Examples:

```bash
cd Engine
./build.sh gcc Release
./run.sh gcc Release
```

WSL note:

- The packaged Linux updater shell can also be smoke-tested under Windows WSL2.
- Use a WSLg/X11 setup with working OpenGL, extract the Linux release asset inside WSL, then launch `./epoch`.
- The Linux updater shell now prefers OpenGL by default on Linux/WSL; you can still force `--backend software` if you need the CPU path.

---

# Running the MSVC binaries

Launch from the binary directory so the colocated assets resolve cleanly:

```powershell
Set-Location x64/Debug
.\ConsoleApplication1.exe
```

Release build:

```powershell
Set-Location x64/Release
.\ConsoleApplication1.exe
```

Relevant runtime asset roots:

- `x64/Debug/assets/`
- `x64/Release/assets/`
- `Engine/assets/`

---

# Documentation map

Documentation index: [Engine/docs/README.md](Engine/docs/README.md)

Useful entry points:

- [Engine/docs/build_presets.md](Engine/docs/build_presets.md)
- [Engine/docs/build_scripts.md](Engine/docs/build_scripts.md)
- [Engine/docs/ai_build_memory.md](Engine/docs/ai_build_memory.md)
- [Engine/docs/tools_list.md](Engine/docs/tools_list.md)
- [Engine/docs/runtime_operations.md](Engine/docs/runtime_operations.md)
- [Engine/docs/aengineconfig_flags.md](Engine/docs/aengineconfig_flags.md)
- [Engine/docs/engine_analysis.md](Engine/docs/engine_analysis.md)
- [Engine/docs/context_audit.md](Engine/docs/context_audit.md)
- [Engine/docs/menu_overlay_backend_audit.md](Engine/docs/menu_overlay_backend_audit.md)
- [Engine/docs/renderer_regression_plan.md](Engine/docs/renderer_regression_plan.md)
- [Engine/docs/wsl_vcpkg_setup.md](Engine/docs/wsl_vcpkg_setup.md)
- [Changes/release_notes_archive.md](Changes/release_notes_archive.md)

---

# Current snapshot

Version:

```text
v0.83.72
```

Highlights:
- The README proof above is now the real `v0.83.72` six-context live capture
  from an asset-bearing maximized `x64/Debug` editor run, with the visible
  `GLFW30`, hosted `SDL`, hosted `SFML`, `Vulkan`, `OpenGL`, and `Software`
  panes present.
- The project shell now pushes the embedded-engine path further into reality by
  generating an `epoch.project.cmake` fragment, using include fallback logic,
  and treating `Engine/include/` as a first-class path for generated projects.
- The Systems workspace now exposes deeper time-spine pacing state, including
  the live step budget and the max-steps-per-frame clamp alongside the existing
  fixed-step, accumulator, and simulated-time diagnostics.
- The shared preview marker now prefers the real center camera ray against the
  grid plane, then reuses the last honest grid hit before any editor-focus
  fallback, so the visible look spot stays closer to the actual view direction
  instead of drifting with camera-follow bias.
- The Windows parented multicontext path continues converging on real backend
  child-surface ownership, with SDL/SFML restored to hosted child rendering,
  the docked Raylib child staying pinned to its real slot after maximize, early
  `SFML_Window` close no longer killing the parent editor, and the flat launcher
  replacing the old
  `Projects/Games/Tools` layered shell.
- The AI workspace now promotes staged MCP/control snapshots into curated
  datasets too, instead of leaving that part of the two-role training loop as
  documentation-only.
- The roadmap is now GitHub-ready markdown at
  [Changes/roadmap.md](Changes/roadmap.md), and it explicitly tracks naming
  cleanup debt, the six-month 2D priority track, the time-system spine, the
  two-role AI training path, and the later procedural/time-node authoring
  phase.
- Detailed release history lives in [Changes/changelog.txt](Changes/changelog.txt),
  [Changes/release_notes_archive.md](Changes/release_notes_archive.md), and the
  current version notes under [Changes/](Changes/).

Changelog:

[Changes/changelog.txt](Changes/changelog.txt)

Roadmap:

[Changes/roadmap.md](Changes/roadmap.md)

---

# License

```text
LicenseRef-MIT-NoSell
```

See [LICENSE](LICENSE) for full terms.
