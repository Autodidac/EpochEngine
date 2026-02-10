# WSL + vcpkg Setup (Linux builds on Windows)

This guide covers running AlmondShell builds from **WSL** while still relying on
Windows-hosted tooling where it makes sense. It assumes you want a Linux build
inside WSL and vcpkg for dependency management.

## 1) Install WSL + base Linux tooling

```bash
sudo apt update
sudo apt install -y build-essential clang ninja-build cmake git curl zip unzip tar pkg-config
```

## 2) Bootstrap vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

Optionally enable global CMake integration:

```bash
./vcpkg integrate install
```

If you prefer per-project usage, point CMake at the toolchain file:

```bash
cmake -S AlmondShell -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## 3) Use a module-capable compiler

AlmondShell is C++23 + modules-first. Use:

- **clang 17+** or **GCC 14+** inside WSL (`-fmodules-ts` enabled by the build scripts/presets).
- **MSVC 17.10+** only when building on Windows directly (not inside WSL).

When switching compilers, delete the build directory to avoid stale BMI files.

## 4) Configure with module scanning

Prefer the CMake presets (see `AlmondShell/docs/build_presets.md`) or run CMake manually:

```bash
rm -rf build
cmake -S AlmondShell -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_CXX_SCAN_FOR_MODULES=ON
cmake --build build
```

CMake 3.27–3.28 require `-DCMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP=ON` instead of
`-DCMAKE_CXX_SCAN_FOR_MODULES=ON`.

## 5) Install backend dependencies (optional)

When you need specific backends, install them via vcpkg (manifest mode covers the
standard list):

```bash
./vcpkg/vcpkg install sdl3 sdl3-image raylib sfml
```

## 6) Networking helpers (optional)

`socat` can be useful for port forwarding or debugging adapters:

```bash
sudo apt install socat
socat -V
```

## Notes on MSVC + WSL

WSL builds should generally rely on clang/GCC. If you need MSVC for a Windows
build, run the Windows preset (`x64-release`, etc.) from a Developer Command
Prompt instead of WSL.
