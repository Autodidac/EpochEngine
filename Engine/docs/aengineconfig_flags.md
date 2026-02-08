# AlmondShell Engine Configuration Flags

This guide summarises the build-time switches exposed from `modules/aengine.ixx` and clarifies which combinations are currently supported. Use it as a quick reference before enabling or disabling backends in multi-context builds.

## Entry-Point Controls

| Macro | Default | Purpose | Notes |
| --- | --- | --- | --- |
| `ALMOND_MAIN_HEADLESS` | Off | Opts out of AlmondShell's automatic entry point when embedding the runtime in a host application. | Define when you provide your own `main` and do **not** want the runtime to spawn windows or contexts. |
| `ALMOND_MAIN_HANDLED` | Off | Signals that the consumer will provide a platform-specific entry point. | On Windows the engine falls back to `WinMain` automatically when neither headless nor handled macros are defined. |
| `ALMOND_USING_WINMAIN` | Auto | Windows-only helper enabling the Win32 subsystem entry point. | Set implicitly on `_WIN32` unless the build is explicitly headless. |

## Context Layout

| Macro | Default | Purpose | Supported Values |
| --- | --- | --- | --- |
| `ALMOND_SINGLE_PARENT` | `1` | Indicates whether child contexts share a single top-level parent window. | `1` keeps multi-backend layouts contained inside the main shell window. Set to `0` only when running fully detached platform windows. |

## Backend Availability

| Category | Macro | Default | Support Notes |
| --- | --- | --- | --- |
| Context provider | `ALMOND_USING_SDL` | On | SDL3 context backed by the runtime multiplexer. Works alongside Raylib for tooling scenarios. |
| Context provider | `ALMOND_USING_RAYLIB` | On | Raylib context hosted inside the multiplexer. Disabled only when building ultra-minimal shells. |
| Context provider | `ALMOND_USING_SFML` | On | SFML 3.0.2+ context support backed by the multiplexer. Requires SFML graphics/window/system dependencies to be available to the build. |
| Renderer | `ALMOND_USING_SOFTWARE_RENDERER` | On | CPU rasteriser used as a fallback and validation layer. |
| Renderer | `ALMOND_USING_OPENGL` | On | Primary GPU renderer. Required for editor-style workflows. |
| Renderer | `ALMOND_USING_VULKAN` | Off | Stubbed configuration. Enabling requires matching updates in the examples and is not supported in AlmondShell today. |
| Renderer | `ALMOND_USING_DIRECTX` | Off | Reserved for AlmondEngine integration. |

## Supported Combinations

The runtime ships with SDL + Raylib contexts active simultaneously, backed by both the OpenGL and software renderers. This configuration is validated by the multiplexer and keeps asset streaming consistent across backends.

Supported mixes:

- **Default multi-backend** – SDL + Raylib contexts with OpenGL and software renderers enabled (`ALMOND_SINGLE_PARENT == 1`).
- **Headless tooling** – Define `ALMOND_MAIN_HEADLESS` and keep renderer macros enabled when running asset generation or script compilation pipelines.
- **Single-context builds** – Undefine either `ALMOND_USING_SDL` or `ALMOND_USING_RAYLIB` when targeting constrained platforms. At least one renderer macro must remain defined for the runtime to upload atlases.
- **Raylib-only shells** – Leaving only `ALMOND_USING_RAYLIB` defined now automatically pulls in the Win32 WGL headers when targeting Windows and links against the appropriate OpenGL loader libraries (`opengl32` via MSBuild, `OpenGL::GL` in CMake). Ensure those SDK components are present when customising your generator presets.

Unsupported mixes:

- **Vulkan or DirectX** – These paths are stubbed and intentionally disabled until their respective integrations stabilise.
- **Renderer-less builds** – Disabling both `ALMOND_USING_OPENGL` and `ALMOND_USING_SOFTWARE_RENDERER` prevents the texture managers from initialising and is not supported.

## SFML dependencies

Ensure SFML's graphics, window, and system libraries are available to the linker (`sfml-graphics`, `sfml-window`, `sfml-system`). The Visual Studio items project includes these by default; other generators should link the same modules.

## Linux packaging guidance

Raylib distributes its own OpenGL loader on Linux, but AlmondShell now links the standalone glad loader in every configuration so all render backends resolve `gladLoadGLLoader` consistently. Distributions that rely exclusively on Raylib's symbols should keep shipping the glad artefacts alongside the engine or provide an alternative wrapper that exports the loader entry points AlmondShell expects.

## Module toolchain requirements

- Configure CMake with module scanning enabled. Presets and helper scripts set `CMAKE_CXX_SCAN_FOR_MODULES=ON` and fall back to `CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP=ON` for CMake 3.27–3.28, but manual invocations should mirror those flags.
- Build with a module-capable compiler configuration: `/std:c++latest` for Visual Studio 2022 17.10+, or `-fmodules-ts` on both clang 17+ and GCC 14+ so BMI generation stays aligned with the scanner output. Switching compilers or toggling Modules TS flags requires a clean build to avoid stale BMI artefacts.

## Change Log

- **v0.81.23** – Noted that updater/config metadata should mirror the runtime version helpers so launch manifests track the v0.81.23 release tag, and documented the C++23 module-first baseline for tooling alignment.
- **v0.72.7** – Documented the expanded module surface (command queue, systems, menu overlay, diagnostics, and updater system partitions) and aligned metadata with the v0.72.7 release.
- **v0.72.5** – Finalised the context/back-end module conversion (`aengine.context`, `aengine.context.window`, `aengine.context.render`, and backend partitions) and refreshed documentation to advertise the import targets and module-aware build defaults.
- **v0.72.1** – Tracked module coverage through the runtime and renderer interfaces, highlighted clean rebuild requirements when swapping compilers, and reinforced Modules TS flag usage across presets and manual invocations.
- **v0.72.0** – Documented that module scanning now defaults on in presets and helper scripts (`CMAKE_CXX_SCAN_FOR_MODULES` with the `CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP` fallback) and that VS 2022, clang, and GCC builds should compile with their Modules TS flags enabled.
- **v0.70.3** – Recorded that the standalone glad loader is now linked on every platform so AlmondShell's OpenGL calls resolve uniformly across shell scripts, CMake presets, and VS Code tasks.
- **v0.70.2** – Clarified that CMake now keeps Raylib's loader active by default while suppressing duplicate glad symbols, and noted that packagers can still flip `-DALMOND_FORCE_GLAD=ON` when a standalone loader is required.
- **v0.70.1** – Documented that Linux builds reuse Raylib's bundled loader when available and introduced the `-DALMOND_FORCE_GLAD=ON` escape hatch for explicitly linking the standalone glad artefacts in packaging scenarios where Raylib omits them.
- **v0.63.0** – Recorded that Raylib-only builds always include the Win32 WGL loader when available and require the platform OpenGL import library even if the OpenGL renderer macro is disabled.
- **v0.62.5** – Logged that Raylib now caches its original WGL handles and only reacquires the context on demand so single-backend builds no longer fail to activate the GL context.
- **v0.62.4** – Recorded that Raylib now seeds its fitted viewport from the live framebuffer during creation so docked GUIs align immediately.
- **v0.62.3** – Noted the Raylib viewport scissor fix and the idempotent software renderer cleanup.
- **v0.62.2** – Clarified that forcing the updater also downloads and expands the source tree so release binaries can bootstrap local development without a fresh clone.
- **v0.62.0** – Documented the `--update`/`--force` flow so command-line runs can check for updates without automatically applying them.
- **v0.58.2** – First publication of this guide in conjunction with the roadmap milestone for documenting configuration options.
