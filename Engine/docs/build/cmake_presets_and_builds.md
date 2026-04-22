# CMake Presets And Builds

`Engine/CMakePresets.json` is the main preset entry point. Run these commands
from `Engine/`, not from the repository root.

## Prerequisites

- `VCPKG_ROOT` should point to a valid vcpkg checkout when you rely on manifest mode.
- Use a module-capable toolchain and a recent CMake version.

## Windows (MSVC)

```powershell
Set-Location Engine
cmake --preset x64-release
cmake --build --preset x64-release
```

Available presets:

- `x64-debug`
- `x64-release`
- `x86-debug`
- `x86-release`

## Windows (ClangCL)

```powershell
Set-Location Engine
cmake --preset clang-x64-release
cmake --build --preset clang-x64-release
```

## Windows (MinGW/GCC)

```powershell
Set-Location Engine
cmake --preset gcc-x64-release
cmake --build --preset gcc-x64-release
```

## Linux

```bash
cd Engine
cmake --preset Ninja-Release
cmake --build --preset Ninja-Release
```

## macOS

```bash
cd Engine
cmake --preset macos-release
cmake --build --preset macos-release
```

## Notes

- Presets already enable module scanning and set up the expected binary/install directories.
- Clean the build directory when you switch compilers or heavily rename module surfaces.
- If you do not want presets, mirror the same flags manually with `cmake -S Engine -B ...`.
