# Build Configuration Flags

Current source version: `v0.83.88`

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
| `EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES` | Off | Turn missing optional backend deps into configure errors. |

## Entry points

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
and Vulkan inherit from the master backend confirmation macro unless you
override them locally in `aengine.config.hpp`.

## Backend support snapshot

| Area | Macro | Default | Current status |
| --- | --- | --- | --- |
| Context | `EPOCH_USING_SDL` | On | Active |
| Context | `EPOCH_USING_RAYLIB` | On | Active |
| Context | `EPOCH_USING_SFML` | On | Active, but more fragile than SDL/Raylib/OpenGL |
| Renderer | `EPOCH_USING_OPENGL` | On | Active primary GPU path |
| Renderer | `EPOCH_USING_SOFTWARE_RENDERER` | On | Active fallback/validation path |
| Renderer | `EPOCH_USING_VULKAN` | On | Experimental / active preview path |
| Renderer | `EPOCH_USING_DIRECTX` | Off | Reserved / not implemented |
| Headless | `EPOCH_USING_NOOP_HEADLESS` | Off | Minimal placeholder path |

## Safe combinations

- Default desktop builds: SDL + Raylib + SFML contexts with OpenGL and software
  rendering available.
- Headless/tooling builds: define `EPOCH_MAIN_HEADLESS` and keep only the
  backends you need for asset or script workflows.
- Reduced builds: disabling individual context providers is fine as long as at
  least one active renderer remains.

## Combinations to treat as experimental

- Vulkan-enabled builds: the codebase contains active Vulkan work, and the
  preview path now tracks the OpenGL editor palette more closely, but it is
  still not the stable default renderer.
- DirectX-enabled builds: reserved scaffolding only.
- Renderer-less builds: disabling both OpenGL and software rendering leaves the
  atlas/texture path without a supported submission backend.

## Dependency notes

- SFML builds require graphics, window, and system packages.
- SDL builds require SDL3, and SDL image support where texture ingestion needs it.
- Raylib-only configurations still rely on the expected GL loader plumbing on
  desktop platforms.
- Module-aware builds should keep `CMAKE_CXX_SCAN_FOR_MODULES=ON` enabled.

## Current release note

- `v0.83.88` is the current source line above the published `v0.83.86`
  packaged release.
- Normal desktop/runtime builds should stay on the main runtime path by default.
- `EPOCH_UPDATER_SHELL_BUILD` is now an explicit bootstrap-mode switch, not the
  default identity for packaged Linux or Windows releases.
