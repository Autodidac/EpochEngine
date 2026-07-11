# Developer Tools And Dependencies

This is the minimum tooling picture for working on Epoch locally.

## Required

- Git
- CMake 4.4.0+ for the current module-aware CMake build
- LLVM/Clang 22.1.8 with matching `clang-scan-deps`, or GCC 16.1 for its
  supported validation lane
- Ninja 1.13.2+ or Visual Studio 2022/MSBuild
- vcpkg when relying on manifest-managed dependencies

## Recommended

- Doxygen for API docs
- Python 3 for helper tooling
- Visual Studio 2022 or VS Code for Windows-oriented workflows

## Optional / backend-specific

- Vulkan SDK if you are touching the experimental Vulkan path
- SDL3, Raylib, and SFML development packages for backend work
- RenderDoc or equivalent GPU debugging tools for renderer investigations

See `cmake_presets_and_builds.md` and
`../engine/runtime_and_editor_workflows.md` for workflow details.

## Tooling lock

`Engine/unix/current_toolchain.env` owns the Linux build-tool versions, upstream
URLs, sizes, and SHA-256 hashes. `Engine/build.sh --bootstrap-current-toolchain`
installs those tools into a disposable cache and records their provenance; it
does not replace system packages. MSVC remains the supported Visual Studio 2022
exception to the current-version Linux tool lock.
