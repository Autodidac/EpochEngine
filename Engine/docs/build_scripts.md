# Build & Utility Scripts

AlmondShell ships with helper scripts in the repository root. They are optional,
but convenient when you want a consistent path layout or automatic API docs
generation.

## `build.sh`

Builds AlmondShell with a selected compiler and build type. The script also
tries to enable vcpkg manifest mode automatically.

```bash
./AlmondShell/build.sh [--no-vcpkg] [gcc|clang] [Debug|Release] [-- <extra cmake args>]
```

**What it does**
- Creates a build directory under `AlmondShell/Bin/<Compiler>-<Config>`.
- Enables CMake module scanning (`CMAKE_CXX_SCAN_FOR_MODULES=ON`).
- Attempts to locate `VCPKG_ROOT` and injects the toolchain file.
- Builds the target and runs the `docs` target when Doxygen is detected.

**Examples**

```bash
./AlmondShell/build.sh gcc Release
./AlmondShell/build.sh --no-vcpkg clang Debug -- -DALMOND_ENABLE_RAYLIB=OFF
```

## `run.sh`

Runs the built executable from the `Bin/<Compiler>-<Config>` directory.

```bash
./AlmondShell/run.sh [gcc|clang] [Debug|Release] [-- <runtime args>]
```

**Example**

```bash
./AlmondShell/run.sh clang Release -- --update --force
```

## `install.sh`

Installs a previously built tree into `AlmondShell/built/bin/<Compiler>-<Config>`.

```bash
./AlmondShell/install.sh [gcc|clang] [Debug|Release]
```

## `clean.sh`

Removes generated build outputs for a clean start.

```bash
./AlmondShell/clean.sh
```

**Removes**
- `AlmondShell/Bin/`
- `AlmondShell/build/`
- `AlmondShell/built/`
- `AlmondShell/CMakeCache.txt`, `AlmondShell/CMakeFiles/`

## Related docs

- `AlmondShell/docs/build_presets.md` for preset-based builds.
- `AlmondShell/docs/tools_list.md` for required tooling.
