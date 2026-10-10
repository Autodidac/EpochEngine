# Epoch Documentation

## Start Here

Epoch is a reusable engine for software and games, with an optional editor and
an in-development sandbox AI workflow. Source transaction containment and
process supervision do not yet prove OS confinement. These documents have
distinct authority:

| Question | Owning document |
| --- | --- |
| What are we doing next, and in what order? | [Roadmap](../../Changes/roadmap.md) |
| What exact result must this pass prove? | [Active pass](../../Changes/active_pass.md) |
| Which unresolved operator requirements must survive a handoff? | [Mission cache](../../Changes/mission_cache.md) |
| What is already implemented and verified? | [Changelog](../../Changes/changelog.txt), source and named test evidence |
| Which build mode and output folder should I use? | [Build modes](engine/runtime_and_editor_workflows.md#msvc-editor-and-generated-project-entry-ownership) — Debug/Release, Windows optimized symbols, CMake Build All; no combined-build project |
| How do the systems fit together? | Architecture and subsystem contracts below |

Epoch has one forward architecture:

- [AI source discovery architecture](engine/ai_source_discovery_architecture.md) — cumulative, token-budgeted source retrieval and exact-byte patch authority.
- `engine/capability_tier_architecture.md` - canonical product, capability,
  renderer, resource, authoring, temporal, settings, and dependency architecture.

Its focused subordinate contracts are:

- `engine/renderer_feature_matrix.md` - current backend evidence only;
- `engine/temporal_engine_architecture.md` - authoritative world time, events,
  branches, observations, persistence, and replay;
- `engine/temporal_authoring_platform.md` - documents, semantic history,
  compiled artifacts, physical caches, and authoring services;
- [Neuromorphic framework](engine/neuromorphic_engine_framework.md) and
  [event-driven rendering](engine/event_driven_rendering.md) - actual sparse
  CPU/GL integration, truth classes, measured follow-ons and unproven consumers;
- `engine/canvas2d_architecture.md` - renderer-neutral 2D camera, viewport,
  sprite/material batching, tile layers, composition, persistence, metrics,
  and baseline `T0-CPU`/`T1-GL` delivery contract.

Do not treat another engine document as a competing roadmap. Historical passes,
release chronology, abandoned approaches, and durable mission memory live under
`../../Changes/`.

## Current Delivery

Active source and the Windows/Linux release line are v0.90.35.
Exact packaging/publication receipts are recorded in
`../../Changes/release_sync_2026-10-10.md`; prior source synchronization is in
`../../Changes/source_sync_2026-10-08.md`. Historical Windows/Linux v0.90.33 and
macOS v0.89.30 remain immutable. The exact next
acceptance is in `../../Changes/active_pass.md`. No source version, transport
timer or pure-contract pass proves a real model-built candidate or docked
Keep/Choose succession.

Normal EpochEditor builds receive the engine-owned internal entry point;
generated child/static-runtime builds suppress it with `EPOCH_MAIN_IN_MAIN_CPP`
and supply their generated entry source. Windows Debug, Release and
ReleaseWithDebugInfo plus the same-project CMake BuildAll mode pass October 7
build checks; no extra combined-build project exists. Source-folder changes must
preserve that split and update CMake, MSVC items/filters and runtime path probes.

The current editor/rendering integration also includes the production OpenGL event-driven cache, neuromorphic invalidation pressure, A/B selective/full benchmarking, renderer-neutral bounded lighting damage, multi-selection/group editing, focus-owned multicontext input, persistent renderer telemetry, and detached-pane recovery/single-owner AI Chat behavior. These are real integrated systems, but native GPU timing and multicontext visual/runtime behavior still require operator-side acceptance.

Read `../../Changes/systems_consolidation_0.90.24_0.90.33.md` for the compact recent-system map and `../../Changes/active_pass.md` for the exact unresolved proof. The broader product mission remains a reusable software/game engine, playable baseline project flow, and sandboxed local-agent development workflow; renderer experiments and AI autonomy do not replace that product goal.

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

- `../../Changes/systems_consolidation_0.90.24_0.90.33.md` - compact map of the late-September integrated renderer/editor/build systems;
- `engine/runtime_and_editor_workflows.md`
- `engine/gui_library_architecture.md`
- `engine/backend_context_status.md`
- `engine/backend_menu_overlay_status.md`
- `engine/renderer_regression_smoke_plan.md`
- `engine/smoke_capture_and_screenshot_workflow.md`
- `engine/forest_factory_package_contract.md`
- `engine/voxel_planetary_package_track.md`
- `engine/os_ai_tooling_and_evidence_policy.md`
- [Curated model research contract](engine/ai_curated_research_contract.md) - planned domain-separated references, budgets, provenance and access tests;
- [SDK Reference and access contract](engine/sdk_reference_and_access_contract.md) - planned API/SDK coverage, About/website access and private delivery tests;
- `engine/research_import_and_promotion.md`
- `engine/source_naming_architecture.md` - canonical C++ filename, module,
  owner, directory, and temporal-layer naming contract;
- `engine/source_shape_audit.md`
- [Dependency ownership and sync](../dep/README.md) - exact portable GUI mirror versus engine-owned descriptor fixtures; no automatic dependency replacement.
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
- The roadmap alone schedules work; dependency diagrams are not duplicate
  backlogs or promises to restart an old calendar.
- Every pass reconciles source and evidence before selecting its next action.
  Separate missing implementation from implemented-but-unverified behavior.
  Move proven completion to history with mission/evidence traceability; retain
  behavior in its owning contract and every older unfinished goal in the plan.
- The renderer matrix states evidence, not aspiration.
- Settings and controls are documented with the subsystem that owns them.
- `Present` requires implementation and validation evidence.
- Generated caches, local runtime output, and transient diagnostics are not
  documentation.
- Release history and old debugging detail belong under `Changes/`.

- [`engine/event_driven_rendering.md`](engine/event_driven_rendering.md) — persistent scene reuse, dirty-region invalidation, and dense fallback for the 3D renderer.
- [`engine/neuromorphic_engine_framework.md`](engine/neuromorphic_engine_framework.md) — bounded weighted signal graph, event-camera front end, temporal/task-graph adapters, and render-invalidation bridge.
