# Build Configuration Flags

Current source version: `v0.89.04`

This guide describes the main build-time switches exposed by the engine. Public
build knobs now prefer the `EPOCH_*` prefix, while lower-level compatibility
macros still retain `EPOCH_*` names internally so older integrations keep
building during the migration.

## Preferred configure options

| CMake option | Default | Purpose |
| --- | --- | --- |
| `EPOCH_ENABLE_RAYLIB` | On | Enable the Raylib backend. |
| `EPOCH_ENABLE_SDL` | On | Enable the SDL backend. |
| `EPOCH_ENABLE_SFML` | On | Enable the SFML backend. |
| `EPOCH_ENABLE_OPENGL` | On | Enable the primary OpenGL renderer path. |
| `EPOCH_ENABLE_SOFTWARE_RENDERER` | On | Enable the software fallback renderer. |
| `EPOCH_ENABLE_VULKAN` | On | Enable the experimental Vulkan build path. |
| `EPOCH_ENABLE_DIRECTX` | On on Windows, off elsewhere | Enable the first-pass Windows DirectX/D3D11 renderer path. Keep this off on Linux/WSL. |
| `EPOCH_GLAD_PROVIDER` | `auto` | Select the OpenGL loader owner: `auto`, `vcpkg`, or `bundled`. |
| `EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES` | Off | Turn missing optional backend deps into configure errors. |
| `EPOCH_ENABLE_AUTHORING_PLATFORM` | On | Declare and compile the shared authoring-document/editor foundation as implementation units land. |
| `EPOCH_ENABLE_TEXTURE_EDITOR` | On | Gate future texture painting, compositing, and texture-node implementation units. |
| `EPOCH_ENABLE_MODEL_EDITOR` | On | Gate future mesh editing, procedural modeling, and sculpt implementation units. |
| `EPOCH_ENABLE_NODE_EDITOR` | On | Gate the future shared typed node-graph editor and evaluator. |
| `EPOCH_ENABLE_MATERIAL_EDITOR` | On | Gate future material graph authoring and preview units. |
| `EPOCH_ENABLE_ANIMATION_EDITOR` | On | Gate future animation graph and timeline authoring units. |
| `EPOCH_ENABLE_AUTHORING_COLLABORATION` | Off | Gate future branch sharing, review, and collaboration contracts. Network/server activation remains separately human-gated. |
| `EPOCH_ENABLE_AUTHORING_METRICS` | On | Gate future document, history, cache, GPU, and evaluation metrics. |

`EPOCH_ENABLE_AUTHORING_PLATFORM` and `EPOCH_ENABLE_TEXTURE_EDITOR` now gate the
`authoring.texture` module, implementation, engine contract, and standalone
texture contract target. Turning either off removes that authoring slice from a
CMake product build while compiled texture artifacts remain a separate runtime
concern. The model, node, material, animation, collaboration, and metrics options
reserve stable build vocabulary until their implementation units land; they must
not be advertised as reducing a product build yet.

## Entry points

All supported entry points consume the same CMake target graph. Visual Studio,
CMake presets, VS Code/Codium, native Linux command lines, and WSL-hosted Linux
builds differ in generator/toolchain selection, not in source ownership or
backend capability definitions.

| Macro | Default | Purpose | Notes |
| --- | --- | --- | --- |
| `EPOCH_MAIN_HEADLESS` | Off | Disable the automatic desktop entry path. | Use when embedding Epoch into another host or tool. |
| `EPOCH_MAIN_HANDLED` | Off | Caller supplies the platform entry point. | Common for custom Windows launchers. |
| `EPOCH_USING_WINMAIN` | Auto on Windows | Enable the Win32 subsystem entry path. | Usually inferred automatically. |

## Window/layout policy

| Macro | Default | Purpose |
| --- | --- | --- |
| `EPOCH_SINGLE_PARENT` | `1` | Keep backend panes docked under one parent host window. |

## Diagnostics and runtime tracing

| Macro | Default | Purpose |
| --- | --- | --- |
| `EPOCH_ENABLE_RENDERER_SLOW_FRAME_LOGS` | On | Emit slow-frame warnings through the renderer logger. |
| `EPOCH_SLOW_FRAME_LOG_STARTUP_GRACE_MS` | `5000` | Suppress slow-frame warnings during early startup. |
| `EPOCH_SLOW_FRAME_LOG_THROTTLE_MS` | `5000` | Limit repeated slow-frame warnings per backend/window. |
| `EPOCH_ENABLE_BACKEND_CONFIRMATION_LOGS` | On | Master switch for one-shot backend confirmation messages. |
| `EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS` | Follows master | Gate backend context bring-up and shutdown confirmations. |
| `EPOCH_ENABLE_BACKEND_UPLOAD_CONFIRMATION_LOGS` | Follows master | Gate backend atlas/texture upload confirmations. |
| `EPOCH_VULKAN_RUNTIME_DIAGNOSTICS` | Off | Enable the ad hoc Vulkan runtime trace file used for deep troubleshooting. |

Backend-specific confirmation switches for OpenGL, SFML, SDL, Raylib, software,
DirectX, and Vulkan inherit from the master backend confirmation macro unless you
override them locally in `engine.config.hpp`.

## Frame pacing and thread accounting

- Core frame pacing is owned by `perf.tier` and the editor/project
  `--frame-limit` path. Backends should not add hidden frame caps on top of the
  core limiter; DirectX/D3D11 preview present uses sync interval `0` so the
  shared limiter and UI presets remain authoritative.
- Editor status displays distinguish live engine-spawned threads from detected
  CPU/hardware thread capacity. Scheduler workers, task-graph workers,
  Windows/Linux context render threads, AI chat calls, self-iteration builds,
  and project build async tasks are counted by the shared
  `epoch::systems::threading` RAII guard.

## Backend support snapshot

| Area | Macro | Default | Current status |
| --- | --- | --- | --- |
| Context | `EPOCH_USING_SDL` | On | Active |
| Context | `EPOCH_USING_RAYLIB` | On | Active |
| Context | `EPOCH_USING_SFML` | On | Active, but more fragile than SDL/Raylib/OpenGL |
| Renderer | `EPOCH_USING_OPENGL` | On | Active primary GPU path |
| Renderer | `EPOCH_USING_SOFTWARE_RENDERER` | On | Active fallback/validation path |
| Renderer | `EPOCH_USING_VULKAN` | On | Experimental / active preview path |
| Renderer | `EPOCH_USING_DIRECTX` | On on Windows | Active first-pass D3D11 preview and GUI replay path |
| Headless | `EPOCH_USING_NOOP_HEADLESS` | Off | Minimal placeholder path |

## Safe combinations

- Default Windows desktop builds: SDL + Raylib + SFML contexts with OpenGL,
  DirectX, Vulkan, and software fallback rendering available when dependencies
  are present.
- Headless/tooling builds: define `EPOCH_MAIN_HEADLESS` and keep only the
  backends you need for asset or script workflows.
- Reduced builds: disabling individual context providers is fine as long as at
  least one active renderer remains.

## Combinations to treat as experimental

- Vulkan-enabled builds: the codebase contains active Vulkan work, and the
  preview path now tracks the OpenGL editor palette more closely, but it is
  still not the stable default renderer.
- DirectX-enabled builds: active Windows-only first-pass D3D11 backend. It is
  valid for multicontext preview/GUI proof and now has split implementation
  units, but renderer-resource/material/depth parity remains experimental.
- Linux/WSL builds: DirectX must remain disabled. Use Clang full-engine presets
  for Linux renderer validation and GCC headless presets unless intentionally
  testing the experimental GNU module path. The normal Linux and WSL lanes use
  vcpkg; `--no-vcpkg` is only an explicit system-package/diagnostic path.
- Renderer-less builds: disabling both OpenGL and software rendering leaves the
  atlas/texture path without a supported submission backend.

## Dependency notes

- SFML builds require graphics, window, and system packages.
- SDL builds require SDL3, and SDL image support where texture ingestion needs it.
- Raylib-only configurations still rely on the expected GL loader plumbing on
  desktop platforms.
- Linux vcpkg defaults intentionally keep optional desktop/audio dependency
  stacks small: SDL3 uses X11 and Vulkan without D-Bus/IBus/Wayland/audio,
  SFML uses graphics/window/system, and Raylib is built without optional audio.
  Expanding those feature sets is a backend ownership decision and must update
  Linux package prerequisites.
- GLAD is single-owner per target. `EPOCH_GLAD_PROVIDER=auto` prefers vcpkg
  `glad::glad` and falls back to Epoch's checked-in loader. Use `vcpkg` to
  require the package target or `bundled` to force the checked-in loader. Do
  not link both loaders, add random system fallbacks, or hide duplicate symbols
  with `/FORCE:MULTIPLE`.
- Module-aware builds should keep `CMAKE_CXX_SCAN_FOR_MODULES=ON` enabled.
- LLVM 22.1.8 Linux Release builds keep the engine at `-O3` while compiling
  only `modules/core.commandline.ixx` and `modules/network.core.ixx` at `-O0` to avoid
  reproducible LLVM CGSCC/inliner and `globalopt` crashes. These are
  source-local compiler workarounds, not a reduced Linux, updater, renderer, or
  context build.
- The normal MSVC x64 multicontext editor target uses the dynamic vcpkg lane
  (`x64-windows`, `/MD`, `RAYLIB_DLL`) so Raylib, SFML, SDL3, GLAD, and DirectX
  can coexist without third-party static duplicate-symbol conflicts. DLLs beside
  `EpochEditor.exe` in `x64/Debug` or `x64/Release` are expected for this lane;
  use `dumpbin /DEPENDENTS` to distinguish true runtime dependencies from stale
  files left by older copy passes.
- MSVC static-vcpkg experiments must keep dependency ownership consistent:
  `image.stb.cpp` is the only private STB implementation owner, SFML static and
  dynamic libraries must not be linked together, raylib static builds use
  `RAYLIB_STATIC` instead of DLL-import macros, and the final app target carries
  SDL3's required Windows system libraries. Do not use `/FORCE:MULTIPLE`; static
  all-backend support needs owned or isolated GLAD/STB/math providers before it
  can be promoted as the default app lane.

## History

Version-specific build and release chronology belongs in
[`Changes/changelog.txt`](../../../Changes/changelog.txt). This document records
only current build controls and supported toolchain policy.