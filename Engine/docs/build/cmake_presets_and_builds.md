# CMake Presets And Builds

`Engine/CMakePresets.json` is the main preset entry point. The repository root
also has a thin wrapper `CMakeLists.txt` for CI and simple root-level
configure/build commands, but the Engine presets remain the authoritative local
entry path.

## Prerequisites

- `VCPKG_ROOT` should point to a valid vcpkg checkout when you rely on manifest mode.
- Use a module-capable toolchain and a recent CMake version.
- The current module-driven CMake path needs `3.28+`. If you are on an older
  baseline such as `3.22.1`, do not assume the CMake path is compatible yet;
  use the checked-in Visual Studio/MSBuild solution until the lower-floor
  compatibility pass is finished.

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
- If you do not want presets, mirror the same flags manually with
  `cmake -S Engine -B ...` or use the repo-root wrapper with `cmake -S . -B ...`.
