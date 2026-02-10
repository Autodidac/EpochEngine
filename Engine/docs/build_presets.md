# CMake Presets Reference

AlmondShell includes `CMakePresets.json` for common build configurations with
vcpkg manifest mode enabled by default.

## Prerequisites
- Set `VCPKG_ROOT` to a valid vcpkg checkout.
- Use CMake 3.29+ (or 3.27–3.28 with module scanning flags).

## Linux (Ninja)

```bash
cmake --preset Ninja-Release
cmake --build --preset Ninja-Release
```

Available presets:
- `Ninja-Debug`
- `Ninja-Release`

## macOS (Ninja)

```bash
cmake --preset macos-release
cmake --build --preset macos-release
```

Available presets:
- `macos-debug`
- `macos-release`

## Windows (MSVC)

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
```

Available presets:
- `x64-debug`, `x64-release`
- `x86-debug`, `x86-release`

## Windows (ClangCL)

```powershell
cmake --preset clang-x64-release
cmake --build --preset clang-x64-release
```

Available presets:
- `clang-x64-debug`, `clang-x64-release`

## Windows (MinGW / GCC)

```powershell
cmake --preset gcc-x64-release
cmake --build --preset gcc-x64-release
```

Available presets:
- `gcc-x64-debug`, `gcc-x64-release`

## Notes
- Presets enable module scanning and vcpkg manifest mode by default.
- If you switch compilers, delete the build directory to avoid stale BMI files.
- For manual configuration, mirror the flags in `CMakePresets.json`.
