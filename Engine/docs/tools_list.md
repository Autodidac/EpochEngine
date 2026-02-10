# Tooling and Environment Checklist

This list captures the tooling typically used with AlmondShell. The *Required* section
is the minimum for a local build; everything else is optional or situational.

## Required
- **Git** – clone the repository and fetch submodules.
- **CMake 3.29+** – module scanning works out of the box. Use 3.27–3.28 with
  `-DCMAKE_CXX_SCAN_FOR_MODULES=ON` or `-DCMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP=ON`.
- **C++23 compiler with modules** – Visual Studio 2022 17.10+, clang 17+, or GCC 14+.
- **Build generator** – Ninja is recommended; MSBuild/Visual Studio is supported.
- **vcpkg** – required for manifest mode unless you provide dependencies manually.

## Recommended
- **Doxygen** – generates the API reference via the `docs` target.
- **Python 3** – useful for small automation tasks and local tooling.
- **Text editor/IDE** – Visual Studio 2022, VS Code, or your preferred editor.

## Optional / Backend-specific
- **Vulkan SDK** – only needed if you work on Vulkan backends.
- **SDL3/Raylib/SFML developer tools** – if you plan to iterate on their backends.

If you are missing a dependency, see `AlmondShell/docs/runtime_operations.md` for
runtime-specific prerequisites or `AlmondShell/docs/build_presets.md` for preset usage.
