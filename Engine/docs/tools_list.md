# Tooling Checklist

This is the minimum tooling picture for working on Epoch locally.

## Required

- Git
- CMake 3.29+ preferred
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

See `build_presets.md` and `runtime_operations.md` for workflow details.