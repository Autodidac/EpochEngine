# Epoch Configuration Flags

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

## Backend support snapshot

| Area | Macro | Default | Current status |
| --- | --- | --- | --- |
| Context | `EPOCH_USING_SDL` | On | Active |
| Context | `EPOCH_USING_RAYLIB` | On | Active |
| Context | `EPOCH_USING_SFML` | On | Active, but more fragile than SDL/Raylib/OpenGL |
| Renderer | `EPOCH_USING_OPENGL` | On | Active primary GPU path |
| Renderer | `EPOCH_USING_SOFTWARE_RENDERER` | On | Active fallback/validation path |
| Renderer | `EPOCH_USING_VULKAN` | Off | Experimental / incomplete |
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

- Vulkan-enabled builds: the codebase contains active Vulkan work, but the
  end-to-end runtime path is still not a stable default.
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

- `v0.82.2` aligns the public docs and configure layer with Epoch naming while
  keeping the lower-level compatibility macros available under the hood.
