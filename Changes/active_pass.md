# Active Pass

## Gate

Consolidate the capability model and land the first baseline 2D vertical slice:
project requirements, honest backend evidence, temporal texture documents,
physical texture-residency plans, Canvas2D composition contracts, and settings
that expose only what the active build can prove.

This is the first gate in the two-month playable-2D critical path defined by
`Engine/docs/engine/capability_tier_architecture.md`.

## Sealed Baseline

The published `v0.88.69` runtime and updater remain accepted and frozen. Do not
edit updater code or UI, handoff/build scripts, packaging, release metadata,
tags, or release assets unless the operator explicitly reopens that lane.
Development-source version metadata may advance independently.

Preserve these accepted source contracts:

- normal editor operation owns one live backend; multicontext is diagnostic;
- whole-editor context replacement preserves editor state and retires the old
  backend instead of opening another editor;
- Engine Arcade owns the sampled render-to-texture graph proof and procedural
  fallback;
- renderer capability reporting separates descriptors, graph build, native
  allocation, presentation, benchmark, and production evidence;
- reusable GUI state belongs in EpochGui while native hosts and backend drawing
  remain engine adapter responsibilities.

## Current Source Truth

The working tree contains these current or in-progress foundations:

- `capability.profile` extends existing capability, budget, `perf.tier`, and
  render-device vocabulary with backend-neutral tiers, feature/evidence states,
  per-subsystem profiles, project requirements, deterministic selection, and
  build-safe no-overclaim checks;
- `render.math` owns shared renderer-neutral vectors and linear color;
- `render.lighting` owns generation-checked light identity, bounded registries,
  immutable frames, environment state, metrics, and reference raster lighting;
- editor and project preview map scene lights through the shared lighting frame
  while retaining truthful reference-solid output until native shading lands;
- `render.ray` owns validated CPU AABB, sphere, triangle, scene, and voxel-DDA
  queries; editor selection uses it and Focus changes the preview camera;
- `physics.manager` owns stable bodies, bounded deterministic commands,
  fixed-step commit boundaries, snapshots, restoration, and metrics, but not a
  solver;
- `audio.manager` owns clips, sources, buses, listener/spatial state, temporal
  scheduling, mix plans, and metrics, but not physical output;
- `voxel.storage` and `water.system` own deterministic sparse/reference state
  without claiming native rendering;
- `scene.tier0` and terrain foundations establish reusable default-scene data;
- `authoring.texture` is the first four-layer authoring vertical slice: stable
  document meaning, semantic history, compiled artifact, and disposable
  standalone/atlas/bindless/sparse residency plan;
- `package.registry` owns fail-closed extension evidence policy, not download,
  verification, or native activation;
- EpochEngineExtensions now owns the manifest-backed capability technique
  gallery: labeled context/tier scene stations describe required features,
  evidence, fallbacks, and scene intent without importing a second renderer
  spine or claiming unproved native effects;
- Engine Arcade now validates its canonical cabinet/screen scene nodes, sampled
  render-surface material binding, geometry storage, and built-in scene catalog;
- EpochGui dependency work adds portable font, image, input, rounded-rectangle,
  text, layout, docking, popup, panel, and floating-window primitives.

These facts are contracts, not blanket runtime claims. Current checkpoint proof
includes MSVC Debug/Release editor builds and contracts, the managed Clang 22
full-engine Release build, 4/4 Linux engine CTests, and 5/5 standalone EpochGui
feature tests. SDL/SFML/Vulkan scene-solid presentation remains `Partial` until
operator visual evidence exists.

## Immediate Implementation Order

1. Integrate `capability.profile` once through the existing capability/budget
   owners, project profiles, System Info, and settings. Do not add another tier
   registry.
2. Finish editor ray selection, Focus, default ground/light/spawn behavior, and
   Run/Build persistence for a Tier-0 scene.
3. Finish the texture document contract and connect logical texture artifacts
   to bounded physical residency plans.
4. Add the renderer-neutral Canvas2D compose, sprite material/batch, tile-layer,
   deterministic sorting, sampling, alpha, scaling, and diagnostics contracts.
5. Prove `T0-CPU` reference behavior and `T1-GL` presentation before broadening
   portable or explicit backend claims.
6. Extend settings and controls in the same pass as each capability so users can
   select project policy, inspect evidence, and tune budgets without stale UI.

## Backend Repair Within This Gate

SDL3, SFML3, and Vulkan currently show editor scene solids as wireframe-only in
operator evidence.

- SDL3 and SFML3 adapter work may consume the existing shared solid triangle
  stream while preserving queue, GUI replay, and present order.
- SFML projection must preserve complete three-vertex triangle groups.
- SDL render failures must report evidence and fail the frame visibly.
- Vulkan needs a separate scene-solid triangle path, correct depth policy, and
  preview-geometry invalidation; its GUI triangle pipeline is not scene proof.
- These lanes remain `Partial` until build and operator visual proof passes.
- They must not delay the `T1-GL` 2D product unless shared contracts regress.

## Settings And Control Rule

Every maturing system must update its settings, controls, diagnostics, and
persistence scope with the implementation:

- unavailable options are absent or disabled with a reason;
- partial capabilities are labeled experimental;
- defaults come from project profile, measured limits, and budgets;
- advanced controls use progressive disclosure;
- EpochGui owns portable control state and layout;
- engine adapters own backend input/drawing, project state, native hosts, and
  evidence;
- mobile/game/headless profiles can omit floating and docking hosts.

## Selective Reference Intake

`Autodidac/tiered_gfx_OpenGL_modular_context_demo` is a technique lab. Only
license-audited, surgically translated ideas may enter Epoch:

- capability/quality controls;
- material and view descriptors;
- RTT/final composition;
- OpenGL techniques behind Epoch render-device/graph contracts;
- manifest-backed CC0 diagnostic assets.

Do not import its alternate resource spine, platform scaffolding, vendored GUI,
hard-coded scenes, raw GL ownership outside the OpenGL adapter, or capability
claims. Advanced effects follow the playable 2D loop.

## Forbidden Expansion

Do not use this gate to:

- edit the sealed updater/release lane;
- claim native PBR, shadows, water, collision solving, physical audio, hardware
  ray query, or RT pipelines without implementation and proof;
- start multiplayer, persistent unscripted AI, planetary terrain, or a broad 3D
  authoring campaign;
- add a second capability registry, renderer resource spine, texture identity,
  node framework, or GUI library;
- make atlases, descriptors, GPU buffers, pipelines, previews, or caches
  canonical authoring state;
- copy unreviewed code/assets from the OpenGL demo or `addons/`.

## Acceptance

The active gate is accepted when:

1. CMake and MSVC metadata contain each new module/source exactly once.
2. Debug and Release `ConsoleApplication1` build.
3. Debug and Release `--engine-contract-self-test` pass.
4. Capability selection proves CPU-only, GLES baseline, OpenGL compute,
   equivalent explicit tiers, deterministic fallback, project matching, and
   no-overclaim behavior.
5. Texture tests prove deterministic revisions, sparse boundedness, semantic
   undo/redo, reproducible compilation, and physical-plan independence.
6. Tier-0 scene tests prove selection, Focus, ground, light, spawn, save/reopen,
   and Run/Build use the same project-owned state.
7. Canvas2D contracts prove deterministic ordering, scaling, blend/sampling,
   offscreen compose, and cache recreation.
8. Renderer docs keep SDL/SFML/Vulkan solids `Partial` until build and eye proof.
9. No updater, release, generated cache, or unrelated operator file is staged.

## Next Gate

Add sprite/tilemap runtime artifacts, configurable input, deterministic 2D
physics, physical audio, and the editor tools needed to author the acceptance
project. The canonical schedule is `Changes/roadmap.md`; durable follow-up is
`Changes/mission_cache.md`.