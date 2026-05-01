# Agent Notes

## Repository Shape

- Epoch is a C++23 engine/tooling repo. Active engine work is under
  `Engine/src/`, `Engine/modules/`, `Engine/include/`, `Engine/resource/`,
  `Engine/ai/`, and `Engine/examples/`.
- Start with `README.md`, `Engine/docs/README.md`, and `cpp.md` before making
  broad changes. Build and runtime details live in `Engine/docs/build/` and
  `Engine/docs/engine/`.
- Do not edit generated output folders or local runtime artifacts unless the
  task is explicitly about those artifacts. Common generated/local paths include
  `build/`, `x64/`, `Engine/Bin/`, `Engine/build/`, `Engine/built/`, and
  runtime logs/captures.

## Build Commands

- Visual Studio / MSBuild solution entry point:

  ```powershell
  & "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine.sln /t:ConsoleApplication1 /p:Configuration=Debug /p:Platform=x64 /m:1
  ```

- Useful MSBuild targets in `Engine.sln`: `ConsoleApplication1`, `StaticLib1`,
  and `HeadlessCI`. CI builds `Engine\examples\HeadlessCI\HeadlessCI.vcxproj`
  with `EpochExtraDefines=EPOCH_CI_HEADLESS_BUILD=1`.
- Root CMake presets are the shared local/CI path:

  ```powershell
  cmake --preset windows-msvc-debug
  cmake --build --preset windows-msvc-debug
  ctest --preset windows-msvc-debug
  ```

- Linux full-engine CMake validation currently uses Clang:

  ```bash
  cmake --preset ninja-clang-debug
  cmake --build --preset ninja-clang-debug
  ctest --preset ninja-clang-debug
  ```

- Linux/GCC presets are headless validation by default because GCC 14 can ICE
  while writing full-engine C++ module BMIs. Use `ninja-gcc-debug` for headless
  validation unless intentionally testing the experimental full GNU module path.
- The module-aware CMake path requires CMake 3.28 or newer. If the available
  CMake is older, prefer the checked-in Visual Studio/MSBuild solution.

## Running And Validation

- Run Windows local builds from asset-bearing output folders so runtime assets,
  fonts, shaders, scripts, captures, and logs resolve correctly:

  ```powershell
  Set-Location x64/Debug
  .\ConsoleApplication1.exe
  ```

  ```powershell
  Set-Location x64/Release
  .\ConsoleApplication1.exe
  ```

- Do not treat a source-root GUI launch as runtime proof unless the task is
  specifically testing broken-path behavior.
- For runtime, editor, backend, AI, or capture changes, the documented local
  Windows validation path is to rebuild `ConsoleApplication1` in both
  `Debug|x64` and `Release|x64`, launch from the matching `x64/...` folder,
  verify the intended panes/contexts render and respond, then close live
  windows before finishing.
- Hosted GitHub workflows should stay build-only/headless. Do not add GUI
  launches, desktop focus assumptions, or screenshot capture to CI without a
  runtime-safe automation path.

## Linux Helper Scripts

- From `Engine/`, helper scripts provide a repeatable local path:

  ```bash
  ./build.sh [--no-vcpkg] [--updater-shell] [gcc|clang] [Debug|Release] [-- <extra cmake args>]
  ./run.sh [gcc|clang] [Debug|Release] [-- <runtime args>]
  ./install.sh [gcc|clang] [Debug|Release]
  ./clean.sh
  ```

- `build.sh` writes outputs under `Engine/Bin/` and tries to discover
  `VCPKG_ROOT` unless `--no-vcpkg` is used.

## Change Discipline

- Keep unrelated dirty work intact. This repo often has local generated output
  or staged experiments; do not clean, delete, or revert them unless asked.
- If a change touches runtime/editor/backend/capture behavior, check the
  relevant docs before updating screenshots or README proof images:
  `Engine/docs/engine/smoke_capture_and_screenshot_workflow.md` and
  `Engine/docs/engine/runtime_and_editor_workflows.md`.
- Prefer adding a short TODO when a workflow is uncertain instead of inventing
  a command or support claim.
