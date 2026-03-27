# Epoch Documentation Index

Use this index to jump to the current docs set for the active engine tree under
`Engine/`.

## Core entry paths

- `../CMakePresets.json` - CMake preset definitions for Windows, Linux, and macOS.
- `../.vscode/` - VS Code tasks, launch settings, and kit configuration.
- `../../Engine.sln` - Visual Studio / MSBuild entry point for the MSVC workflow.
- `../../x64/Debug/` - primary local MSVC runtime output folder.
- `../../x64/Release/` - release runtime output folder.

## Start here

- `build_presets.md` - CMake preset names and baseline commands.
- `build_scripts.md` - `build.sh`, `run.sh`, `install.sh`, and `clean.sh`.
- `tools_list.md` - required and optional tooling.
- `wsl_vcpkg_setup.md` - WSL-oriented setup notes.

## Runtime and configuration

- `runtime_operations.md` - launcher, scripting, logging, and runtime workflow.
- `aengineconfig_flags.md` - build/config macros, support status, and cautions.

## Architecture and backend status

- `engine_analysis.md` - current architecture snapshot and priorities.
- `context_audit.md` - active, experimental, and archival context surfaces.
- `menu_overlay_backend_audit.md` - backend GUI parity notes.
- `renderer_regression_plan.md` - smoke-test plan for renderer regressions.

## Repository and migration reference

- `file_structure.txt` - high-level repo layout.
- `filelist.txt` - condensed module/header inventory.
- `legacy_feature_map.md` - map of retired legacy surfaces to their active
  Epoch-owned replacements.
