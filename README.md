<img align="left" src="Images/567.jpg" width="70px"/>

# AlmondShell

**AlmondShell** is a modern **C++23, modules-first runtime engine** that powers the
AlmondEngine project. It focuses on multi-context rendering, hot-reloadable
scripting, and a static-linking-first deployment model while keeping legacy
headers available for migration.

## What AlmondShell provides

- **Multi-context, multi-threaded runtime** with pluggable backends (OpenGL,
  SDL3, Raylib, SFML, and software/no-op renderers).
- **Module-first C++23 API surface** with compatibility headers mirrored under
  `AlmondShell/include/`.
- **Hot-reloadable scripting** with a background compiler loop and diagnostics.
- **Self-updating launcher** that can pull source, rebuild, and swap binaries.
- **Atlas-driven GUI + asset pipeline** designed for multi-backend rendering.

## Documentation map

- **Start here**: `AlmondShell/docs/README.md` (index of docs).
- **Build guides**: `AlmondShell/docs/build_presets.md`,
  `AlmondShell/docs/build_scripts.md`, `AlmondShell/docs/tools_list.md`.
- **Runtime & config**: `AlmondShell/docs/runtime_operations.md`,
  `AlmondShell/docs/aengineconfig_flags.md`.
- **Architecture**: `AlmondShell/docs/engine_analysis.md`,
  `AlmondShell/docs/context_audit.md`.
- **Repository map**: `AlmondShell/docs/file_structure.txt`.

## Quick build (summary)

**Windows (MSVC)**
```powershell
cmake --preset x64-release
cmake --build --preset x64-release
```

**Linux (Ninja)**
```bash
cmake --preset Ninja-Release
cmake --build --preset Ninja-Release
```

**macOS (Ninja)**
```bash
cmake --preset macos-release
cmake --build --preset macos-release
```

For script-driven builds, use `./AlmondShell/build.sh` (see
`AlmondShell/docs/build_scripts.md`).

## Configuration highlights

Build-time switches live in `AlmondShell/include/aengine.config.hpp` and are
documented in `AlmondShell/docs/aengineconfig_flags.md`. Key areas include:
- Entry-point selection (`ALMOND_MAIN_HANDLED`, `ALMOND_MAIN_HEADLESS`).
- Context/back-end enablement (`ALMOND_USING_SDL`, `ALMOND_USING_RAYLIB`, etc.).
- Rendering backend toggles (`ALMOND_USING_OPENGL`, `ALMOND_USING_SOFTWARE_RENDERER`).

## Current snapshot

- **Version:** v0.81.23
- **Changelog:** `Changes/changelog.txt`
- **Roadmap:** `Changes/roadmap.txt`

## Contributing

“We are not accepting PRs at this time.” AlmondShell is a source-available
commercial product. For substantial changes, open an issue first.

## License

`LicenseRef-MIT-NoSell` — see `LICENSE` for full terms.
