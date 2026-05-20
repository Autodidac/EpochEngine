# Linux WSL Build Setup

This guide covers Linux builds from WSL while keeping the repo layout and
tooling expectations aligned with the current `Engine/` tree.

## 1. Install base packages

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  clang-18 clang-tools-18 \
  ninja-build cmake git curl zip unzip tar pkg-config \
  libasio-dev libcurl4-openssl-dev libgl1-mesa-dev libsfml-dev \
  libx11-dev libxi-dev libxrandr-dev libxrender-dev
```

## 2. Bootstrap vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

Point CMake at the toolchain when you are not using a preset:

```bash
cmake -S Engine -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## 3. Use a module-capable compiler

- Clang 18 plus `clang-scan-deps-18` for the current full-engine Linux module build
- GCC is kept as a headless validation lane by default because GCC 14 can ICE
  while writing full-engine C++ module BMIs
- MSVC only for native Windows builds, not for WSL builds

Clean the build directory when switching compilers or module settings.

## 4. Configure and build

Full engine with Clang:

```bash
cmake --preset ninja-clang-debug
cmake --build --preset ninja-clang-debug
ctest --preset ninja-clang-debug --output-on-failure
```

Portable GCC/headless validation:

```bash
cmake --preset ninja-gcc-debug
cmake --build --preset ninja-gcc-debug
ctest --preset ninja-gcc-debug --output-on-failure
```

## 5. Optional backend packages

```bash
./vcpkg/vcpkg install sdl3 sdl3-image raylib sfml
```

## Notes

- Use the repo-root `CMakePresets.json` for shared Linux/CI truth. The
  engine-local presets remain available for legacy local workflows.
- Use Clang for Linux full-engine rendering builds today. Use GCC presets for
  headless validation unless you are explicitly investigating the GNU module path.
- `v0.84.35` revalidated the repo-root `ninja-clang-debug` path from WSL with
  configure, build, and ctest passing. DirectX is correctly disabled on Linux;
  OpenGL, SFML, and software fallback remain the current build-time Linux
  renderer coverage in the tested environment.
- Follow-up `v0.84.35` WSL validation confirmed the built Linux binary reports
  `Epoch v0.84.35` and `epoch_ci_headless` passes. Visual proof is still blocked
  on this workstation: OpenGL `--smoke --capture` emitted a black BMP, SFML
  failed in GLX `MakeCurrent` with `BadAccess`, and software did not emit a
  capture file. Treat Linux as build/headless green but not screenshot/release
  proof-complete until WSLg/native Linux visual smoke is fixed.
- Packaged Linux/WSL release assets should be versioned `.tar.gz` runtime
  archives. The normal packaged entry is `epoch`; updater-shell mode is a
  separate bootstrap variant, not the default Linux runtime identity.
