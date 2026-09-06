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
| How do the systems fit together? | Architecture and subsystem contracts below |

Epoch has one forward architecture:

- `engine/capability_tier_architecture.md` - canonical product, capability,
  renderer, resource, authoring, temporal, settings, and dependency architecture.

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

The immediate acceptance objective is the real local-model self-coding loop:
plain-language request, automatic bounded context, plan and edits, sandbox
build/test, separately supervised candidate context, Keep/Choose, then another
actual build from the chosen sandbox. Live source and projects remain outside
that loop. Component implementations are not proof that this entire workflow
already works; the active pass identifies the remaining evidence.

The roadmap's September 6 batch also retains current EpochGui/Extensions and
loadable CLI/native-window/GUI projects, a proven stable-base compatibility freeze
before major game-specific changes, separate Sim/Space demo/library integration,
then private SDK/About/Site delivery last. The full playable 2D game remains a
major goal, not replaced by the demos. Platform extraction is incremental;
completed foundations are reused, not rescheduled as new work. Exact-build
release checks apply to accepted release scope, not every deferred feature.

Architecture alignment includes input/output ownership, performance evidence,
cross-platform software-to-accelerated-graphics profiles and context-specific
needs. Planned curated references give Qwen separate Engine self-coding and
project/game-development collections; they are retrieval, not training or
automatic permission to read/send every document.

Read:

- `../../Changes/active_pass.md` for the current bounded gate;
- `../../Changes/roadmap.md` for the ordered remaining milestones;
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
- [Curated model research contract](engine/ai_curated_research_contract.md) - planned domain-separated references, budgets, provenance and access tests;
- [SDK Reference and access contract](engine/sdk_reference_and_access_contract.md) - planned API/SDK coverage, About/website access and private delivery tests;
- `engine/research_import_and_promotion.md`
- `engine/source_naming_architecture.md` - canonical C++ filename, module,
  owner, directory, and temporal-layer naming contract;
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
