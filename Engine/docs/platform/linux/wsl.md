# Linux WSL Build Setup

This guide covers Linux builds from WSL while keeping the repo layout and
tooling expectations aligned with the current `Engine/` tree.

## Planned Editor-managed WSL target

November15 direction: discover an existing WSL environment as a selectable Linux
build/run target in Epoch, with eventual presentation inside the Windows Editor.
This is planned integration, not a new renderer or a claim of embedded support.
Reuse the existing project/build/platform ownership and dependency admission.

Current source already labels Linux under WSL in
[`epoch.version`](../../../modules/epoch.version.ixx), detects WSL2 in the
[build-tools helper](../../../../Tools/epoch_install_build_tools.ps1), and can
select a distribution when [staging Linux releases](../../../../Tools/build/stage_epoch_linux_release.ps1).
Those separate facilities do not prove a managed Editor target or window bridge.
Historical build results below likewise do not establish current GUI acceptance.

The implementation and acceptance order is:

1. **Discover and qualify the environment.** Record the selected distribution,
   WSL version, architecture, Linux source/build/data roots and resource bounds.
   Distinguish inventory from a probe that starts a stopped distribution. The
   official inventory commands expose installed distributions and state; they
   do not prove a usable compiler or graphics context.
   [Microsoft command reference](https://learn.microsoft.com/en-us/windows/wsl/basic-commands#list-installed-linux-distributions).
2. **Qualify the existing build lane.** Reuse `Engine/build.sh` and its toolchain
   checks for compiler/module scanner, CMake, Ninja, vcpkg baseline and triplet.
   Check the requested target's actual link/runtime dependency closure and staged
   assets/notices, including shared-library resolution. SDL, SFML and Raylib are
   libraries/context providers, not graphics drivers to install blindly. Keep
   Linux outputs separate from Windows and preserve Public/Development source
   admission; a Public target cannot borrow private Development inputs.
3. **Qualify native presentation, then launch externally.** Probe the selected
   distribution's display connection, actual backend/device and required features.
   WSLg supports Linux X11/Wayland GUI applications under WSL2; accelerated OpenGL
   depends on the graphics stack. Its presence does not prove Vulkan device,
   extensions, surface or swapchain availability: test those separately and report
   missing capabilities without an automatic backend switch.
   [Microsoft GUI support](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps).
   First acceptance is a supervised external Linux project window with working
   logs, input, resize, exit and owned cancellation. Default controlled proof
   remains one OpenGL context; headless/software checks remain separate evidence.
4. **Prove an Editor surface bridge before offering In Editor.** WSLg presents
   Linux windows through Weston and RDP/VAIL; desktop integration alone is not
   evidence that a Linux surface can be reparented as a normal Windows child
   HWND. [Microsoft WSLg architecture](https://github.com/microsoft/wslg#weston).
   Review the supported host/surface transport against Epoch's current context
   abstraction, then measure frame delivery, resize/DPI, keyboard/mouse/focus,
   clipping, minimize/restore, process exit/crash and reconnect. Preserve renderer
   ownership and prevent simultaneous editor/project draws. Until this is proven,
   expose external launch honestly rather than a nonfunctional embedding option.

This proposal does not authorize installing/upgrading WSL, distributions,
drivers or packages, changing services, stopping unrelated tests, or adding a
network control surface. Supervise only owned jobs; do not use distribution-wide
shutdown to cancel an Epoch build. Any proposed setup changes need a visible,
specific operation and the existing operator approval rules.

## 1. Install base packages

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  git curl zip unzip tar pkg-config autoconf automake libtool \
  libgl1-mesa-dev libegl1-mesa-dev libvulkan-dev \
  libx11-dev libxi-dev libxrandr-dev libxrender-dev \
  libudev-dev libxext-dev libxft-dev libxcursor-dev libxinerama-dev libxtst-dev
```

Install LLVM/Clang 22.1.8 plus its matching `clang-scan-deps`, CMake 4.4.0,
and Ninja 1.13.2, or let the build script prepare the verified cache-local
toolchain described below.

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

The normal Epoch Linux/WSL build uses vcpkg. `Engine/build.sh` discovers
`VCPKG_ROOT`, validates that the manifest builtin baseline is available in the
local vcpkg clone, and fetches the missing baseline when the clone is stale.
Use `--no-vcpkg` only when deliberately testing a system-package lane.

Manifest dependencies remain vcpkg-owned. X11 and OpenGL are host integration
libraries: while resolving only those two packages, Epoch's CMake restores host
library search alongside the vcpkg find root, then restores the previous
`CMAKE_FIND_ROOT_PATH_MODE_LIBRARY` policy. This prevents a vcpkg toolchain
prefix from hiding valid distribution libraries without weakening dependency
ownership for the rest of the build.

## 3. Use a module-capable compiler

- Clang 22.1.8 plus matching `clang-scan-deps` for the full-engine Linux build
- GCC 16.1 is kept as a headless validation lane by default while its full
  C++ module path remains experimental
- MSVC only for native Windows builds, not for WSL builds

Clean the build directory when switching compilers or module settings.

## 4. Configure and build

Full engine with Clang:

```bash
cd Engine
./build.sh --bootstrap-current-toolchain clang Release
```

Repo-root preset path:

```bash
cmake --preset ninja-clang-debug
cmake --build --preset ninja-clang-debug
ctest --preset ninja-clang-debug --output-on-failure
```

The full Development `epoch_engine_contract` writes many small fixture files.
Two known-good Release runs on a Windows-mounted checkout took 9m23 and 9m47. Use a
deadline sized for the full contract, and read the runtime's
`logs/Engine.Editor.SelfTest.log` alongside CTest output. CTest can truncate
the captured output on timeout while the category log records later stages.

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

The checked-in manifest owns all Linux context dependencies. SDL3 uses only
its X11 and Vulkan features, SFML uses graphics/window/system, and Raylib is
built without optional audio. The narrow SDL feature set avoids pulling the
unrelated D-Bus/IBus/systemd chain into updater builds.

Epoch selects the tracked `x64-linux-epoch` triplet. Dependencies remain
static except SFML, whose shared libraries are staged under `lib/`; this keeps
SFML's embedded STB implementation isolated from Raylib's embedded copy.

## Notes

- Use the repo-root `CMakePresets.json` for shared Linux/CI truth. The
  engine-local presets remain available for legacy local workflows.
- Full-engine C++23 module builds require Ninja or another module-aware
  generator. Do not use raw Unix Makefiles for the full engine.
- Use Clang for Linux full-engine rendering builds today. Use GCC presets for
  headless validation unless you are explicitly investigating the GNU module path.
- The full Linux build requires OpenGL, SDL, SFML, Raylib, Vulkan, and software
  contexts. DirectX remains Windows-only; WSL runtime proof still defaults to
  one OpenGL context rather than a multicontext or Vulkan fallback.
- `v0.87.54` validates the current full-engine Clang Release lane with OpenGL,
  Vulkan, SDL, SFML, Raylib, and software enabled together. DirectX remains
  Windows-only.
- Follow-up `v0.84.35` WSL validation confirmed the built Linux binary reports
  `Epoch v0.84.35` and `epoch_ci_headless` passes. The staged Linux package at
  `C:\tmp\epoch_release\epoch_linux_x64_v0.84.35.tar.gz` also reports
  `Epoch v0.84.35` and passes `./epoch_ci_headless .` from the package root
  after including `Engine/assets`, `Engine/resource`, and `Engine/ai/control`.
  Treat Linux as build/headless/package green; refreshed Linux visual proof
  remains a follow-up gate before replacing README screenshots.
- Packaged Linux/WSL release assets should be versioned `.tar.gz` runtime
  archives. The normal packaged entry is `epoch`; updater-shell mode is a
  separate bootstrap variant, not the default Linux runtime identity.
