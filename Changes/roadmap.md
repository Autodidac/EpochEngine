# Epoch Roadmap

## Mission

Ship a playable baseline 2D project within the remaining two-month window while
building the smallest coherent foundation that can later scale to mobile,
explicit GPUs, ray queries, temporal worlds, and general authoring.

The canonical architecture is
`Engine/docs/engine/capability_tier_architecture.md`. This roadmap schedules that
architecture. It does not create a second design.

## Planning Rules

- The playable 2D loop is the critical path.
- Capability is selected per subsystem, never inferred from API name.
- `T0-CPU` is the universal correctness/headless floor.
- `T1-GL` is the first desktop presentation target; contracts remain suitable
  for later `T1-GLES`.
- A feature and its settings, controls, diagnostics, persistence, and tests move
  together.
- Source documents and semantic history are authoritative. Compiled artifacts
  are reproducible. Physical caches are disposable.
- First-party C++ ownership follows `source_naming_architecture.md`; source moves
  preserve one-dot module/file identity and update every build surface together.
- Project Save, materialize, Build, and Run decisions flow through
  `project.lifecycle` rather than widget-local state machines.
- OpenGL proves portable techniques first without becoming engine architecture.
- Baseline 2D presentation parity across OpenGL, SDL3, SFML3, Raylib3, Vulkan,
  DirectX/D3D11, and Software is a product gate. Advanced backend features,
  mobile specialization, and effects cannot consume the playable-2D schedule.
- Hosted CI confirms faithful local proof; it is not the first place to discover
  ordinary compiler or contract failures.
- The `v0.89.06` release/updater becomes the sealed baseline after its Windows
  and Linux packages pass staging, download-integrity, and startup proof.

## Current Foundation

Current source or in-progress contracts include:

- backend-neutral capability profiles, evidence, project requirements, and
  deterministic per-subsystem selection;
- renderer-neutral math, bounded light frames/reference lighting, and CPU ray
  and voxel queries;
- logical physics and audio managers with fixed scheduling/snapshot contracts;
- sparse voxel storage, analytic water queries, Tier-0 scene/terrain descriptors,
  and fail-closed package evidence policy;
- sampled render-to-texture, render-device/resource descriptors, render graph,
  Engine Arcade graph proof, and a renderer-neutral Canvas2D project/submission
  planner with camera, viewport, sprite, batch, tile, compose, and diagnostics,
  plus a deterministic CPU reference raster with texture/clip bindings,
  fixed-point coverage, final composition, metrics, and image hashes, plus a
  bounded generation-checked physical texture cache, a renderer-neutral
  CPU-canvas-output/native-presentation adapter, context-guarded OpenGL native
  texture hooks, a compiled primary-context OpenGL compositor with scoped
  scene-surface and GL-state ownership, one shared scene-raster session, and
  build-proven adapters for SDL3, SFML3, Raylib3, Vulkan, D3D11, and Software;
- project-scoped texture import, deterministic artifact/Library persistence,
  decoded publication, restore-on-demand, semantic scene materials, snapshot
  format 3, and one World Outliner Assets controller over the same logical
  resource spine;
- canonical temporal tilemap documents with stable IDs, sparse chunks, semantic
  history, deterministic runtime artifacts, collision/object payloads, exact
  Project Library persistence, restore-on-demand, and visible Canvas2D
  compilation;
- a reusable EpochGui tile workspace plus the Game2D engine controller now own
  palette/layer/grid state, texture attachment, map operations, diagnostics,
  canonical Assets/Maps source, exact Library publication, revision-cached
  preview, and context-handoff restoration. Generated standalone runtime map
  loading, cache regeneration, and approved visual interaction remain delivery
  work;
- production project validation that materializes, atomically saves/reopens,
  builds, and child-runs all six generated profiles, plus an exact standalone
  external Run proof for the generated GUI project;
- explicit request-driven temporal mapping and bounded observation history plus
  a temporal texture document/history/compiler/residency foundation;
- reusable EpochGui text, primal multi-line document editing, font, image, input,
  layout, rounded rectangle, toggle, popup, panel, docking, and floating-window
  primitives, synchronized between the embedded and standalone repositories;
- direct launcher entry into three shared-spine editor applications: standard
  Editor, Plant Lab, and GUI Editor, each with separate source, canonical scene,
  surface/camera/dock policy, and authoring/run permissions;
- a shared temporal morphology spine used by the separate Plant Lab authoring
  editor for custom trees and forest configurations, with Forest Factory kept
  as the placement portal inside the standard editor;
- one baked primary context slot in the Windows parent host, with secondary
  diagnostic contexts and routed pane popouts remaining optional;
- a validated procedural Engine Arcade cabinet and camera-facing preview
  culling, plus build-proven backend-owned sampled scene surfaces across all
  seven contexts pending visual evidence.

These are not blanket runtime claims. The active integration batch must rebuild
and re-prove them.

## Product Definition

The acceptance project is a small 2D game with:

- one tile-based map;
- a controllable animated actor;
- camera and deterministic draw ordering;
- collision against the map and scene bodies;
- at least one sound effect and one music/ambient bus;
- save, close, reopen, and deterministic scene restoration;
- Play/Stop in the editor;
- Run and Build producing a project-owned executable/runtime;
- cache deletion followed by successful artifact regeneration;
- visible capability, performance, memory, and failure diagnostics.

A polished demo is useful, but the core acceptance is the complete project loop.

## Eight-Week Critical Path

### Week 1: Capability And Tier-0 Scene

Completed foundation:

- one typed project capability policy now reaches project profiles, generated
  manifests, active-editor and project-run admission, System Info, Settings,
  recommended budgets, and build-safe contracts without adding a tier registry.

Deliver:

- add a project profile for `T0-CPU` plus `T1-GL` and a headless test profile;
- finish ray-based editor selection and working Focus;
- make default ground, light, camera, spawn, and starter object use one saved
  project scene in editor and runtime;
- ensure Run/Build consumes saved state rather than a separate preview shell;
- expose only proven backend/profile choices in settings.

Exit gate:

- deterministic capability selection and no-overclaim tests pass;
- default scene saves, reopens, focuses, and runs;
- Debug/Release build-safe proof passes.

### Week 2: Texture Document And Residency

Deliver:

- stable texture document identity, revisions, sparse tiles, layers, semantic
  operations, undo/redo, checkpoints, bounded history, and deterministic compile;
- logical texture artifacts independent of placement;
- standalone, atlas, bindless, and sparse physical plans selected by capability
  and budget;
- source/library/history/cache separation and cache recreation tests;
- texture settings for history budget, sampling intent, compilation, residency,
  and cost visibility.

Exit gate:

- equivalent document revisions compile identically;
- undo/redo and sparse storage remain bounded;
- changing physical residency does not change authoring identity.

### Week 3: Canvas2D And Sprite Batch

Foundation status: renderer-neutral project/submission planning, deterministic
batch compilation, tile validation, editor policy, and build-safe contracts are
present. Deterministic T0-CPU raster, sampling/blending, final composition,
metrics, and image hashes are build-proven on MSVC and Clang. Bounded physical
texture residency, deterministic RGBA8 artifact serialization, runtime project
asset authentication, content-derived logical revisions, project-scoped CPU
resource binding, optional residency, evidence-backed sampled-image admission,
strict/experimental policy, cross-budget limit reduction, exact immutable scene
closure, raster-to-residency presentation staging, OpenGL texture hooks, the
primary compositor, and semantic editor scene-slot routing are build-proven.
Project Library persistence, bounded BMP/TGA/P6 import, semantic scene
materials, snapshot save/reopen, runtime restoration, and Assets-workspace
controls are build-proven. Approved native capture proves exact OpenGL/T0-CPU
Canvas2D parity across 1,178,872 pixels, and the generated GUI executable
passes the editor-equivalent external Run path. One shared raster session and
native adapters for all seven baseline contexts pass MSVC Debug/Release builds
and contracts. Approved live interaction, non-OpenGL pixel/switch evidence,
secondary GL share groups, and sRGB/compressed/mip execution remain work.

Deliver:

- Canvas2D offscreen target and final compose graph;
- pixel-aware camera, resize, letterbox, integer-scale, and viewport policy;
- sprite material, batch, transform, UV, tint, alpha, sampler, layer, and stable
  draw-order contracts;
- nearest/linear sampling and blend/cutout modes;
- diagnostic default textures from license-verified CC0 assets or generated
  engine-owned data;
- OpenGL compatibility presentation with CPU/reference contract checks.

Exit gate:

- deterministic batch order;
- correct scaling and alpha in build-safe tests plus operator visual proof;
- cache loss recreates textures and render targets.

### Week 4: Tilemap And Scene Authoring

Foundation status: stable temporal map identity, sparse chunks, semantic
operations, deterministic artifacts, bounded serialization/culling, animated
Canvas2D submission, collision/object output, exact Project Library restore,
canonical Assets/Maps source, and the reusable EpochGui/Game2D authoring
workspace are build-proven. Save and the current editor Run publication consume
the same document; whole-editor context replacement restores unsaved map
history and portable workspace state.

Deliver:

- load the exact compiled map revision in generated standalone project runtimes;
- regenerate Library/TileMaps from canonical source after cache deletion;
- complete object selection/transform/delete and hierarchy/inspector integration
  through semantic map and scene operations;
- prove save/reopen, Play, external Run, Build, interaction, and cost diagnostics
  over one authored map;
- capture approved visual evidence without weakening headless/game compile-out.

Exit gate:

- one map can be authored without editing source files;
- authored map survives restart, runs externally, and regenerates its disposable
  compiled artifact from source.
### Week 5: Input And 2D Physics

Deliver:

- configurable actions, keyboard/controller bindings, dead zones, and project
  input profile;
- deterministic fixed-step 2D solver adapter behind `physics.manager`;
- AABB/circle baseline shapes, body modes, layers/masks, contacts, and stable IDs;
- actor movement, map collision, spawn, reset, and pause behavior;
- physics controls and diagnostics aligned with actual solver support.

Exit gate:

- repeated input replay produces the same accepted simulation result;
- save/reopen and Play/Stop do not leak or duplicate bodies.

### Week 6: Audio, Animation, Run, And Build

Deliver:

- physical audio adapter consuming `audio.manager` frame plans;
- clip import, buses, volume/mute, loop, event playback, and clean device failure;
- sprite animation document/runtime artifact and deterministic frame selection;
- editor Play/Stop, external Run, and Build use the same scene and selected
  capability profile;
- project output excludes unused editor/floating GUI/backends where configured.

Exit gate:

- actor animates, collides, and plays sound in editor and built runtime;
- stopping/restarting releases audio, physics, and renderer resources cleanly.

### Week 7: Integration And Portability

Deliver:

- complete acceptance game loop and project template;
- project migration/version checks and actionable errors;
- resource budgets, atlas pressure, batch counts, physics/audio cost, and frame
  timing in diagnostics;
- `T1-GLES` limits represented in contracts and desktop-only assumptions removed;
- generated/project builds, MSVC, CMake/Clang, and headless contract lanes aligned.

Exit gate:

- new project to built game succeeds from documented commands;
- caches can be removed and rebuilt;
- unsupported settings fail closed.

### Week 8: Hardening And Release Readiness

Deliver:

- fix only acceptance blockers and regressions;
- Debug/Release builds and contract tests on supported compiler lanes;
- repeated editor/runtime restart, save/reopen, Play/Stop, Run, and Build tests;
- bounded soak for memory, handle generations, queues, and cache growth;
- operator screenshot/eye proof for the acceptance project;
- concise release notes only after proof, if the operator opens a release gate.

Exit gate:

- the playable project meets every product-definition item;
- renderer/capability matrix contains no unsupported `Present` claims;
- source checkpoint is clean, reproducible, and documented.

## Parallel Work That May Proceed

Parallel work is allowed only when it does not collide with the critical path:

- Raylib/Vulkan scene-solid orientation eye proof;
- Vulkan repeated-replacement retirement proof;
- EpochGui portable primitive integration and tests;
- renderer-neutral material/view/post-process descriptors;
- documentation and build metadata kept synchronized with proven source;
- extension manifests or assets in their owning repositories after direct audit.

Parallel work may not change frame/queue/GUI replay order casually and may not
claim runtime success without visual evidence.

## Backend Truth And Repair Queue

- OpenGL remains the first T1 desktop presentation lane.
- SDL3, SFML3, DirectX, and Software have operator-accepted current solid
  orientation.
- Vulkan has a dedicated scene-solid pipeline; its corrected front face and
  retirement ownership now need repeated-switch eye proof.
- Raylib remains a specialized OpenGL-derived context with its own ownership and
  presentation evidence.
- DirectX currently means the active D3D11 lane; D3D12 is a planned capability
  family and must not be inferred from that implementation.
- Software remains the deterministic/headless and safe fallback.

Raylib and Vulkan solid orientation remains `Partial` until the correction has
eye proof. Vulkan retirement also needs repeated switch-away proof, especially
to Software. This parity work is valuable, but the 2D acceptance project does
not require every editor backend to become a production renderer in the same
eight weeks.

## Settings And Editor Controls

Each subsystem pass includes:

1. typed settings and defaults;
2. project/user/session persistence ownership;
3. EpochGui control state and layout;
4. engine adapter input/drawing;
5. capability-aware availability;
6. diagnostics and error evidence;
7. tests for invalid, unavailable, and stale states.

Near-term controls include:

- project profile and fallback policy;
- backend selection and evidence display;
- Canvas2D resolution, scaling, sampling, blend, and batch budgets;
- texture history, compile, residency, and cache budgets;
- tilemap grid/chunk/collision controls;
- input actions and bindings;
- physics fixed-step, gravity, layers, and diagnostics;
- audio buses, device state, and volume;
- Run/Build profile and included systems.

World Outliner owns the dockable World/Assets/Scripting tabs. The script lane
uses the shared EpochGui text-control contract; syntax, diagnostics, document
tabs, and large-file virtualization extend that one controller. Focus, input
capture, selection, and redock are acceptance behavior for every pane, not
optional polish. Miscellaneous Tools actions migrate to their owning surface.

Direct OS AI supports an OpenAI-compatible API lane and an offline `llama-cli`
lane. The optional Extensions package stages a human-approved build plan and
never starts a server; GGUF model acquisition and licensing remain separate.
Professional docking, floating panes, and advanced 3D controls continue in
EpochGui/desktop editor work but remain optional to game/mobile/headless builds.

## Temporal And Authoring Scope

The 2D campaign uses the temporal architecture only where it creates immediate
product value:

- texture documents and sparse tile history;
- tilemap and scene semantic operations;
- save/reopen, undo/redo, and deterministic runtime compilation;
- explicit physics time and animation frame addressing.

The complete event-sourced world, persistent AI, collaboration, networking,
planetary simulation, and general graph authoring remain future phases. Their
invariants are preserved without making them prerequisites for the first game.

## OpenGL Technique Lab Intake

`Autodidac/tiered_gfx_OpenGL_modular_context_demo` supplies reference techniques,
not architecture.

Near-term intake order:

1. quality budgets and feature settings translated into existing capability
   types;
2. material/view/post-process descriptors;
3. RTT, HDR target, and final composition behind `render.device`/`render.graph`;
4. small manifest-backed CC0 diagnostic textures;
5. later OpenGL PBR, sky, shadows, terrain, water, foliage, particles, and other
   effects only after the playable 2D loop.

Do not copy its resource spine, platform/context scaffolding, vendored EpochGui,
hard-coded scene construction, raw GL ownership outside the OpenGL backend, or
unsupported capability labels. Preserve MIT notices for derived code and
quarantine assets without provenance.

## Repository Boundaries

- EpochEngine mainline owns stable contracts, project/runtime integration,
  capability truth, validation, safe fallback, and core default behavior.
- EpochGui owns reusable portable C++23 GUI controls/state and its own tests.
- EpochEngineExtensions owns bulky optional source/content such as advanced
  terrain, FFT ocean, game-specific stacks, and reviewed package payloads.
- The OpenGL demo remains a separate technique laboratory.
- Games and generated projects include only selected systems and dependencies.

## Source Acceptance

Every source checkpoint must:

- preserve unrelated operator work;
- build the closest faithful Debug and Release targets;
- pass build-safe contract tests;
- keep CMake/MSVC/module metadata exact;
- update capability evidence and settings with source behavior;
- avoid generated caches and runtime artifacts;
- keep release/updater files untouched unless explicitly reopened;
- record durable follow-up in `Changes/mission_cache.md` instead of expanding
  architecture documents with debugging chronology.

## After The 2D Objective

The next dependency order is:

1. T1 GLES/OpenGL compute and accelerated 2D/procedural work.
2. Shared typed node graph and broader material/model/effects authoring.
3. T2 Vulkan/DirectX renderer-resource parity.
4. T3 bindless, sparse, async, subgroup/wave, and GPU-driven execution.
5. T4 hardware ray-query providers.
6. T5 full RT pipelines.
7. Persistent regions, collaboration, networking, and planetary/astronomical
   packages.

Lower tiers remain complete and selectable as higher tiers arrive.
