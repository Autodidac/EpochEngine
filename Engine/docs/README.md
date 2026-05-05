# Epoch Documentation Index

This folder is now grouped by purpose so Linux setup notes stop competing with
engine architecture docs.

## Quick orientation

- `../CMakePresets.json` - build presets for Windows, Linux, and macOS
- `../../Engine.sln` - Visual Studio / MSBuild entry point
- `../../x64/Debug/` - primary local MSVC runtime output
- `../../x64/Release/` - release local MSVC runtime output

## If you are new to the repo

Start here first:

- `build/cmake_presets_and_builds.md` - preset names and first build commands
- `build/local_build_scripts_and_release_packaging.md` - helper scripts, local
  validation, and packaging rules
- `engine/runtime_and_editor_workflows.md` - how launcher, editor, projects,
  scripts, systems, and runtime are supposed to behave
- `../ai/README.md` - live AI sandbox, helper-model, safety, and training-loop
  contract for the repo-safe AI assets under `Engine/ai/`

## Build and packaging docs

- `build/build_configuration_flags.md` - build/config macros, support status,
  and cautions
- `build/cmake_presets_and_builds.md` - CMake preset names and baseline build
  commands
- `build/developer_tools_and_dependencies.md` - required and optional local
  tooling
- `build/local_build_scripts_and_release_packaging.md` - shell scripts,
  validation flow, and release packaging discipline

Build-system reality to keep in mind:

- the repo-root `CMakeLists.txt` is a thin wrapper for CI/simple configure
  entry, not a replacement for the `Engine/` preset workflow
- the wrapper keeps the older `3.22.1` entry explicit, but the current
  module-aware engine path still requires newer CMake and says so on purpose
- GitHub workflows should stay build-only; GUI smoke and screenshot proof still
  belong to asset-bearing local runtime outputs

## Engine and runtime docs

- `engine/current_engine_architecture.md` - current architecture snapshot,
  strengths, cautions, and priorities
- `engine/runtime_and_editor_workflows.md` - project-centric runtime, editor,
  scripting, systems, and updater behavior
- `engine/backend_context_status.md` - backend/context inventory and practical
  guidance
- `engine/backend_menu_overlay_status.md` - backend GUI parity and caution
  notes
- `engine/renderer_regression_smoke_plan.md` - repeatable backend smoke
  expectations
- `engine/smoke_capture_and_screenshot_workflow.md` - capture discipline for
  proofs and README screenshots
- `engine/ai_training_memory_and_dataset_policy.md` - AI storage, iteration,
  capture, and promotion rules
- `../ai/README.md` - live AI content map and self-iteration sandbox controls
- `engine/research_import_and_promotion.md` - staged research intake and
  promotion path
- `engine/repository_layout_reference.txt` - high-level repo layout
- `engine/module_inventory_reference.txt` - active module/header inventory
- `engine/legacy_feature_map.md` - retired legacy surfaces mapped to current
  replacements

## Platform-specific docs

- `platform/android/android_bringup_plan.md` - honest Android-first mobile
  bring-up scope and acceptance criteria
- `platform/linux/linux_wsl_build_setup.md` - WSL/Linux setup and build notes

## Planning and history

These do not live in `Engine/docs/`, but they are the companion references for
the active tree:

- `../../Changes/roadmap.md`
- `../../Changes/changelog.txt`
- `../../Changes/engine_history_and_release_archive.md`
