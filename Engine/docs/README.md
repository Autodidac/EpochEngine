# Epoch Documentation

## Start Here

Epoch has one forward architecture:

- `engine/capability_tier_architecture.md` - canonical product, capability,
  renderer, resource, authoring, temporal, settings, and delivery architecture.

Its focused subordinate contracts are:

- `engine/renderer_feature_matrix.md` - current backend evidence only;
- `engine/temporal_engine_architecture.md` - authoritative world time, events,
  branches, observations, persistence, and replay;
- `engine/temporal_authoring_platform.md` - documents, semantic history,
  compiled artifacts, physical caches, and authoring services;
- `engine/canvas2d_architecture.md` - renderer-neutral 2D camera, viewport,
  sprite/material batching, tile layers, composition, persistence, metrics,
  and baseline `T0-CPU`/`T1-GL` delivery contract.

Do not treat another engine document as a competing roadmap. Historical passes,
release chronology, abandoned approaches, and durable mission memory live under
`../../Changes/`.

## Current Delivery

The active objective is a playable baseline 2D project within two months:

```text
capability/project profile
-> temporal texture document and residency
-> Canvas2D compose and sprite batch
-> tilemap and scene authoring
-> input, 2D physics, audio, animation
-> Play, Run, Build, save, and reopen
```

Read:

- `../../Changes/active_pass.md` for the current bounded gate;
- `../../Changes/roadmap.md` for the eight-week schedule;
- `../../Changes/mission_cache.md` for durable follow-up;
- `../../Changes/changelog.txt` for implementation/release history.

## Build

- `../CMakePresets.json` - engine CMake presets;
- `../../Engine.sln` - Visual Studio/MSBuild entry point;
- `build/build_configuration_flags.md` - feature/build configuration;
- `build/cmake_presets_and_builds.md` - baseline configure/build commands;
- `build/developer_tools_and_dependencies.md` - toolchain dependencies;
- `build/local_build_scripts_and_release_packaging.md` - local scripts and
  packaging discipline;
- `platform/linux/linux_wsl_build_setup.md` - Linux/WSL setup;
- `platform/android/android_bringup_plan.md` - deferred mobile bring-up.

The repo-root CMake file is a thin wrapper. The module-aware `Engine/` presets
and supported toolchains define production builds. Hosted CI confirms faithful
local proof; GUI and presentation claims still need asset-bearing runtime and
operator visual evidence.

## Operational References

These documents describe current subsystem operation or focused policy. They do
not set independent product priorities:

- `engine/runtime_and_editor_workflows.md`
- `engine/gui_library_architecture.md`
- `engine/backend_context_status.md`
- `engine/backend_menu_overlay_status.md`
- `engine/renderer_regression_smoke_plan.md`
- `engine/smoke_capture_and_screenshot_workflow.md`
- `engine/forest_factory_package_contract.md`
- `engine/voxel_planetary_package_track.md`
- `engine/os_ai_tooling_and_evidence_policy.md`
- `engine/research_import_and_promotion.md`
- `engine/source_shape_audit.md`
- `../ai/README.md`

Inventory/reference files:

- `engine/repository_layout_reference.txt`
- `engine/module_inventory_reference.txt`
- `engine/legacy_feature_map.md`

When an operational reference conflicts with the capability-tier architecture or
the active gate, update or archive the stale reference instead of adding another
plan.

## Documentation Rules

- Architecture states current contracts and durable invariants.
- The renderer matrix states evidence, not aspiration.
- Settings and controls are documented with the subsystem that owns them.
- `Present` requires implementation and validation evidence.
- Generated caches, local runtime output, and transient diagnostics are not
  documentation.
- Release history and old debugging detail belong under `Changes/`.