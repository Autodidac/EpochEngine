<img align="left" src="Images/567.jpg" width="70px"/>

# Epoch

**Epoch** is a **C++23 modules-first runtime engine** focused on multi-context
rendering, atlas-driven UI, hot-reloadable scripting, and a desktop-first
workflow for editor and game runtime development.

The project emphasizes **modern C++ architecture, high subsystem density, and
clean modular boundaries** rather than raw code volume.

The current engine snapshot is organized around a dense modular runtime,
multi-backend context orchestration, and a compatibility archive under
`Engine/legacy/`.

---

# Design goals

Epoch focuses on a few core principles:

### Modules-first architecture
Engine subsystems are implemented as **C++23 modules** under:

```
Engine/modules/
```

with implementation code in:

```
Engine/src/
```

### Multi-context rendering runtime
Rendering backends can run concurrently across multiple window contexts.

### Atlas-driven UI and rendering pipelines
GUI, sprites, and asset rendering share atlas systems across backends.

### Hot-reloadable scripting
Runtime scripts compile to dynamic modules and reload during execution.

### Desktop-first workflow
The runtime emphasizes desktop development tooling and debugging workflows.

---

# What Epoch currently provides

- Multi-context window and render orchestration across:

  - OpenGL
  - Vulkan
  - SDL3
  - Raylib
  - SFML
  - Software renderer
  - Noop / headless paths

- ECS-style entity and system layers
- Atlas management for textures and GUI
- Editor runtime systems and scene tools
- Runtime scripting compiler and task-graph pipeline
- Diagnostics, telemetry, and update systems

A legacy compatibility archive is preserved under:

```
Engine/legacy/
```

so older integration surfaces remain available while the modern module
architecture evolves.

---

# Architectural scale

Epoch is intentionally **compact but structurally dense**.

Current snapshot (core engine only):

| Metric | Value |
|------|------|
| Code files | ~219 |
| C++ modules | ~165 |
| Headers | ~18 |
| Implementation units | ~36 |

Compared to other engines:

| Engine | Approx size | Scope |
|------|------|------|
| Epoch (current snapshot) | ~219 files | modular runtime engine |
| Hazel | ~400–600 files | learning / indie engine |
| Godot | 8000+ files | full production engine |
| Unreal Engine | 25k+ files | AAA production engine |

Epoch therefore sits in the **early real-engine stage**: beyond prototype size,
but intentionally smaller than large production engines.

The project prioritizes **clean subsystem boundaries and modular architecture**
over raw code volume.

---

# Repository layout

```
Engine/
```

Active engine code, examples, build scripts, documentation, and the legacy
archive.

```
Changes/
```

Changelog, roadmap, and release notes.

```
Images/
```

Repository artwork and README assets.

```
Tools/
```

Helper notes and scripts for local tooling setup.

---

# Documentation map

Start here:

```
Engine/docs/README.md
```

Build documentation:

- `Engine/docs/build_presets.md`
- `Engine/docs/build_scripts.md`
- `Engine/docs/tools_list.md`

Runtime and configuration:

- `Engine/docs/runtime_operations.md`
- `Engine/docs/aengineconfig_flags.md`

Architecture and backend status:

- `Engine/docs/engine_analysis.md`
- `Engine/docs/context_audit.md`
- `Engine/docs/menu_overlay_backend_audit.md`

Legacy migration reference:

- `Engine/docs/legacy_archive.md`

---

# Quick start

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

The public runtime/binary name is:

```
epoch
```

---

# Compatibility note

Public branding, documentation, and release metadata now consistently use
**Epoch** across the active engine tree.

Build and configuration flags prefer the `EPOCH_*` naming scheme.

A compatibility archive still preserves older code and migration surfaces under
`Engine/legacy/`, while the active runtime stays centered in `Engine/modules/`
and `Engine/src/`.

---

# Current snapshot

Version:

```
v0.82.2
```

Changelog:

```
Changes/changelog.txt
```

Roadmap:

```
Changes/roadmap.txt
```

---

# License

```
LicenseRef-MIT-NoSell
```

See `LICENSE` for full terms.
