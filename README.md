<img align="left" src="Images/567.png" width="270px"/>

# Epoch

**Epoch** is a **world-class C++23 modules-first, AI-enabled game engine**
built for serious real-time tooling: multi-context rendering, a launcher +
editor workflow, atlas-driven UI, hot-reloadable scripting, and a runtime that
can drive multiple backends at once without giving up engine-level control.

The active engine lives in:

```text
Engine/modules/
Engine/src/
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

- Concurrent backend contexts across OpenGL, Vulkan, SDL3, Raylib, SFML,
  software, and noop/headless paths
- A launcher-first workflow that routes projects into the editor and games into
  scene/runtime mode
- Atlas-driven GUI and sprite pipelines shared across the runtime
- Editor-facing scene preview paths and backend fallback behavior
- ECS-style runtime systems, scene plumbing, and gameplay modules
- Hot-reloadable scripting and file-watch driven iteration
- Diagnostics, telemetry, updater, and task-graph support

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

- `Engine/Engine.vcxitems`
- `Engine/examples/StaticLib1/StaticLib1.vcxproj`
- `Engine/examples/ConsoleApplication1/ConsoleApplication1.vcxproj`

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

Presets live at:

```text
Engine/CMakePresets.json
```

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

- `Engine/.vscode/tasks.json`
- `Engine/.vscode/launch.json`
- `Engine/.vscode/settings.json`
- `Engine/.vscode/c_cpp_properties.json`
- `Engine/.vscode/cmake-kits.json`

Recommended flow:

1. Open `Engine/` in VS Code.
2. Select a CMake preset or task matching your compiler.
3. Build through the bundled tasks or the CMake Tools extension.

## Shell scripts

Scripted build helpers live in:

```text
Engine/build.sh
Engine/run.sh
```

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

Documentation index:

```text
Engine/docs/README.md
```

Useful entry points:

- `Engine/docs/build_presets.md`
- `Engine/docs/build_scripts.md`
- `Engine/docs/tools_list.md`
- `Engine/docs/runtime_operations.md`
- `Engine/docs/aengineconfig_flags.md`
- `Engine/docs/engine_analysis.md`
- `Engine/docs/context_audit.md`
- `Engine/docs/menu_overlay_backend_audit.md`

---

# Current snapshot

Version:

```text
v0.82.13
```

Highlights:

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

```text
Changes/changelog.txt
```

Roadmap:

```text
Changes/roadmap.txt
```

---

# License

```text
LicenseRef-MIT-NoSell
```

See `LICENSE` for full terms.
