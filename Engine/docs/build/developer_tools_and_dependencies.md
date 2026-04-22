# Developer Tools And Dependencies

This is the minimum tooling picture for working on Epoch locally.

## Required

- Git
- CMake 3.28+ for the current module-aware CMake build
- A C++23 compiler with module support
- A supported generator: Ninja, Visual Studio/MSBuild, or a compatible GCC/Clang setup
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

## Tooling floor note

Epoch should stay honest about older baseline environments. Right now the
module-aware CMake path still expects `3.28+`; older setups around `3.22.1`
should use the checked-in solution/build scripts until the lower-floor
compatibility pass lands. Do not silently raise the floor in docs or CI without
writing that change down.
