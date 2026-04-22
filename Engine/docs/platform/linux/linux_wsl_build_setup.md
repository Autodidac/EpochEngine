# Linux WSL Build Setup

This guide covers Linux builds from WSL while keeping the repo layout and
tooling expectations aligned with the current `Engine/` tree.

## 1. Install base packages

```bash
sudo apt update
sudo apt install -y build-essential clang ninja-build cmake git curl zip unzip tar pkg-config
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

- clang 17+ or GCC 14+ inside WSL
- MSVC only for native Windows builds, not for WSL builds

Clean the build directory when switching compilers or module settings.

## 4. Configure and build

```bash
rm -rf build
cmake -S Engine -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_CXX_SCAN_FOR_MODULES=ON
cmake --build build
```

## 5. Optional backend packages

```bash
./vcpkg/vcpkg install sdl3 sdl3-image raylib sfml
```

## Notes

- Use `Engine/CMakePresets.json` when you want the preset flow instead of manual flags.
- WSL is best paired with clang or GCC. Use Windows presets from a Developer
  Command Prompt when you need the native Visual Studio toolchain.
