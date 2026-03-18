<img align="left" src="Images/567.png" width="95px"/>

# Epoch - Creative Software And Game Engine

**Epoch Engine** is a **world-class, modules-first, AI-enabled C++23 game
engine** built for serious real-time tooling: internal engine bootstrap,
multi-context rendering, launcher + editor workflow, atlas-driven UI,
engine-owned compiled scripting, and a runtime that can drive multiple
backends at once without giving up engine-level control.

The active engine lives in:

```text
Engine/src/
Engine/include/
Engine/modules/
```

with prebuilt MSVC runtime binaries commonly landing in:

```text
x64/Debug/
x64/Release/
```

Those binary folders also carry runtime assets, so launching from the binary
directory is the safest default for local testing.

---

# What Epoch provides

- Internalized engine bootstrap and entry-point flexibility. Epoch can own the
  desktop startup path itself or be embedded with handled/headless entry
  configurations through the active `EPOCH_*` runtime macros. See
  [configuration flags](Engine/docs/aengineconfig_flags.md) and
  [runtime operations](Engine/docs/runtime_operations.md).
- Multi-context, multi-backend runtime orchestration across OpenGL, Vulkan,
  SDL3, Raylib, SFML, software, and noop/headless paths.
- Launcher-first workflow that routes projects into the editor and games into
  scene/runtime mode, instead of treating the editor as a loose debug shell.
- Desktop-style editor workflow with scene preview control, command surfaces,
  and backend-aware fallback behavior.
- Atlas-driven GUI, sprite, and text pipelines shared across the runtime rather
  than copied independently into each backend.
- ECS-style systems, scene plumbing, gameplay modules, and engine-owned runtime
  state.
- Engine-owned compiled scripting with editor-triggered run actions, a host API
  for runtime/editor callbacks, and task-graph-backed asynchronous work
  scheduling.
- Diagnostics, renderer telemetry, runtime logging, and updater plumbing as
  first-class engine systems.
- Cross-platform build freedom: Visual Studio, MSBuild, CMake presets, VS Code,
  shell-script workflows, and multiple compiler families across Windows, Linux,
  and macOS.
- Module-first public engine surface centered around active C++23 modules and
  the exported [epochengine module](Engine/modules/epochengine.ixx).

---

# Repository layout

```text
Engine/
```

Engine code, build configuration, examples, docs, assets, and editor/runtime
systems.

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
v0.82.24
```

Highlights:

- `0.82.24` fixes the updater to query the latest real GitHub release instead
  of `main`, so version detection and `main.zip` download stay aligned.
- `0.82.23` was the post-release bump after the fixed `0.82.22` package tag, so
  the updater had a newer live target after the rerelease.
- `0.82.22` fixes the updater to target the shipped `main.zip` runtime package,
  extract it in place, and report status through Epoch logging so the captured
  output no longer leaks raw CR/LF glyphs.
- `0.82.21` is the post-release bump that follows the `0.82.20` tag so the
  updater has a newer target than the packaged release snapshot.
- `0.82.20` is the fresh documentation/version bump so the updater has a new
  live target to detect and pull during self-update testing.
- The self-update path now targets the currently running executable instead of
  the old hardcoded `updater.exe` flow, and failed update handoffs are reported
  instead of silently looking successful.
- The Windows editor script compiler now uses a true direct executable launch
  for absolute LLVM paths, so `clang++` under `Program Files` no longer gets
  split into a broken `Files/...` argument.
- Historical versioned release-note markdowns are now consolidated into a single
  archive file under `Changes/`, while new release notes continue as individual
  version files.
- Temporary Windows script-build artifacts are now ignored, and the accidental
  tracked `.exp` file has been removed from the repo.
- The editor script compiler no longer launches `clang++` through a fragile
  shell string on Windows, so spaces in the LLVM install path stop breaking the
  `Run` action.
- The editor now owns a real `Run` action for compiled engine scripts, with a
  host-facing script API and a default `rotate_all_entities` script in the
  active source tree.
- Shared preview cameras now support reusable `Editor` and `FPS` modes with
  keyboard motion and right-mouse look instead of the old fixed preview view.
- The updater now parses either plain version text or the full version module
  cleanly, so remote checks stop printing raw file banners or BOM garbage.
- The launcher now owns projects, games, and tool entry points instead of
  overloading the editor command surface.
- The editor now behaves more like a real desktop tool, with `File`, `Edit`,
  `Scene`, `Command`, and `Help` menus across the top bar.
- The shared scene/base color now tracks the darker Vulkan palette across the
  active backends instead of diverging by renderer.
- SFML now docks ahead of Vulkan in the parent grid, and its shared preview is
  clipped back into the scene viewport instead of bleeding into the GUI.
- Scene preview switching now lets the editor move between `Editor` and `None`
  preview modes without leaving the current session.
- The in-app update action is now confirmation-gated before it can replace
  binaries and restart the session.
- The current local launch/test baseline is still `x64/Debug` or `x64/Release`
  so colocated runtime assets resolve exactly as the binaries expect.

Changelog:

[Changes/changelog.txt](Changes/changelog.txt)

Roadmap:

[Changes/roadmap.txt](Changes/roadmap.txt)

---

# License

```text
LicenseRef-MIT-NoSell
```

See [LICENSE](LICENSE) for full terms.
