# CMake Presets And Builds

The repository-root `CMakePresets.json` is the shared cross-compiler preset
layer for CI and portable local validation. `Engine/CMakePresets.json` remains
available for existing engine-local workflows, but cross-platform build truth
should be kept synchronized through the root wrapper.

## Prerequisites

- `VCPKG_ROOT` should point to a valid vcpkg checkout when you rely on manifest mode.
- Use a module-capable toolchain and a recent CMake version.
- The current module-driven CMake path needs `3.28+`. If you are on an older
  baseline such as `3.22.1`, do not assume the CMake path is compatible yet;
  use the checked-in Visual Studio/MSBuild solution until the lower-floor
  compatibility pass is finished.

## Windows (MSVC)

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

Available presets:

- `windows-msvc-debug`
- `windows-msvc-release`
- `windows-msvc-cpp26-debug`

## Windows (ClangCL)

```powershell
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
```

## Linux (GCC Headless Validation)

```bash
cmake --preset ninja-gcc-debug
cmake --build --preset ninja-gcc-debug
```

The GCC presets intentionally route to `epoch_ci_headless` by default because
GCC 14 can ICE while writing full-engine C++ module BMIs. Only opt into the
full GNU module build with `-DEPOCH_ALLOW_GCC_MODULE_ENGINE=ON` when you are
testing compiler/module behavior locally. In the default GCC headless lane,
`epoch_ci_headless` is intentionally module-free and uses a small C ABI logger
shim so CMake does not need compiler import-graph discovery.

## Linux (Clang Full Engine)

```bash
cmake --preset ninja-clang-debug
cmake --build --preset ninja-clang-debug
```

The Clang preset is the current Linux full-engine CMake path. In hosted CI the
matching `linux-clang-engine` lane installs runner-safe OpenGL/software/SFML
build dependencies, builds the real `epoch` target, then runs the headless
CTest smoke without launching GUI windows.

## macOS

```bash
cd Engine
cmake --preset macos-release
cmake --build --preset macos-release
```

## Notes

- Presets already enable module scanning and set up the expected binary/install
  directories except for the GCC headless smoke target, which disables scanning
  by design.
- C++23 remains the default baseline; `*-cpp26-*` presets are optional
  `/std:c++latest` or `-std=c++2c` validation lanes only.
- Clean the build directory when you switch compilers or heavily rename module surfaces.
- If you do not want presets, mirror the same flags manually with
  `cmake -S Engine -B ...` or use the repo-root wrapper with `cmake -S . -B ...`.
- Hosted GitHub Actions split validation intentionally:
  - Required Windows and Linux hosted CMake jobs build and test `epoch_ci_headless`.
  - The MSBuild hosted job builds and runs the `HeadlessCI` Visual Studio project.
  - The Linux Clang engine lane builds the full `epoch` target with OpenGL,
    software renderer, and SFML enabled at build time, but still does not launch
    GUI windows on hosted runners.
  - The headless smoke target verifies public script-host ABI and filesystem probes without launching GUI contexts or requiring renderer packages.
