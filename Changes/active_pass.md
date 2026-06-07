# Active Pass

## Gate

Backend-native sampled render-to-texture for OpenGL-derived contexts.

## Why This Gate Matters

This is the first vertical proof of Epoch's renderer-resource spine. It
validates shared descriptors, backend-native allocation, render-target
ownership, material texture binding, graph pass execution, and the Engine
Arcade screen path without turning the arcade package into the whole mission.

## Current Spine Goal

Build a truthful renderer spine where every modern OpenGL feature lands as a
reusable engine feature family, proves itself in OpenGL first, reports
capability truth per backend, and leaves Vulkan/DirectX with clean equivalent
contracts instead of drift.

## Current Evidence

- OpenGL owns the first real native sampled-RTT hook factory for FBO/color
  texture/depth renderbuffer/sampler allocation.
- The engine contract harness now imports that real hook factory, installs it
  on the OpenGL-family device, and verifies the shared `engine_arcade.screen`
  descriptor/handle/work-order path can allocate and destroy records without
  claiming live GPU allocation when no GL context is registered.
- `render.graph` now rejects `MaterialTextureSlot::render_surface` bindings
  unless the referenced texture is owned by a sampled render-texture asset with
  a sampler. Plain texture handles no longer count as arcade/runtime screen
  surfaces.
- SDL3, SFML3, and Raylib sampled-RTT capability reporting remains
  runtime-availability-gated; contract-only paths are still `Partial`.
- Build evidence: MSVC Debug x64 `ConsoleApplication1` passes after the real
  OpenGL hook contract wiring.

## Allowed Source Areas

- `Engine/modules/render.device.ixx`
- `Engine/modules/render.graph.ixx`
- `Engine/src/epoch.render.graph.cpp`
- OpenGL backend resource/context/render files
- SDL3 OpenGL-backed context/resource files
- SFML3 OpenGL-backed context/resource files
- Raylib3 OpenGL-backed context/resource files
- `package.registry` only if the Engine Arcade render asset contract needs a
  small correction
- System Info renderer capability reporting if existing code supports it
- `Engine/docs/engine/renderer_feature_matrix.md`
- `Changes/roadmap.md`

## Forbidden Source Areas

- Software renderer parity
- Shadows
- Deferred rendering
- Skeletal animation
- Particles
- D3D12
- Broad GUI redesign
- Broad Package Manager redesign
- OS AI/model/tooling changes
- Unrelated source-shape cleanup
- Documentation-only pass

## Acceptance

- OpenGL-derived contexts allocate real native texture, sampler,
  framebuffer/render-target, and optional depth/stencil objects from shared
  descriptors.
- Render graph compilation resolves the sampled render texture into readable
  texture/sampler bindings and writable render-target bindings.
- A render pass can target the render texture.
- A later pass/material can sample the render texture through
  `MaterialTextureSlot::render_surface`.
- Engine Arcade declares and uses `engine_arcade.screen` as a proof surface.
- Capability reporting says present only for actually implemented behavior;
  otherwise partial/missing/deferred.
- Build/check passes using the safest command allowed by `AGENTS.md`.

## Stop Conditions

Stop and report partial progress if:

- backend context ownership prevents safe resource creation,
- handle mapping needs a new backend registry,
- existing capability reporting has no present/partial/missing/deferred model,
- build fails twice on the same issue.

Do not broaden scope to compensate.

## Mission Cache

Use this cache to avoid losing operator-confirmed work while keeping each pass
focused on the active gate.

- Renderer spine: formal modules for handles/descriptors/resource ownership,
  backend-native maps, material/model binding, render-to-texture, then model
  import, normal mapping, skybox, instancing, shadows, deferred/G-buffer, and
  SSAO in that order.
- OpenGL-derived parity: OpenGL, SDL3, SFML3, and Raylib3 should share one
  renderer-resource contract while keeping backend-specific context nuances.
  Software is a debug/safe-launch fallback, not production parity.
- Context flesh: all six production contexts need shared visual palette,
  object colors, opacity, helper colors, and GUI theme inputs from the central
  spine. DirectX and Vulkan implement equivalent contracts in their own models.
- Floating GUI windows: editor GUI panes should become dockable/undockable
  tool windows that clone the selected context family by default. Mixed grids
  are diagnostic/preview mode, not the normal editor workflow.
- GUI primitives: selectable/editable text, right-click context menus,
  copy/paste, word wrap, scroll bounds, modal focus, progress bars, list rows,
  tabs, docking chrome, and theme tables belong in `engine.gui` first.
- Command menus and modals: preserve the proven draw model, keep command menus
  above the scene, close/deselect predictably, and do not reintroduce OpenGL
  flicker while changing UI.
- Package Manager: use a real list with per-row status/action controls, visible
  transfer/build progress, license evidence, source gates, cache validation,
  cancel behavior, and no fake-progress claims.
- Engine Arcade: render-to-texture belongs in the engine. Arcade/game packages
  consume RTT later for in-game cabinets and runtime-minis.
- Forest Factory: core editor workspace with a real 3D scene/window, temporal
  graph growth, repo-derived asset lineage, voxel-node LOD/hit-detection
  descriptors, and timeline integration. Heavy terrain/ocean stacks remain
  packages.
- Video/timeline: Epoch is time-based. The Video workspace owns the visible
  timeline graph, playhead, cadence, streaming-save hooks, and project/video
  timing controls near the scene rather than buried in System Info.
- Input: add universal configurable input profiles, freecam/FPS-style 3D
  controls, center/reset hotkeys, package-exportable input contracts, and
  project camera style selection.
- Project run/build: Play In Editor runs the saved scene inside the editor;
  external project launch is separate, chooses one backend, does not reset scene
  edits, and avoids spawning multicontext clones unless requested.
- Scene persistence: remove unwanted starter junk, keep camera/light/default
  essentials, and make scene parser/serializer own preview/runtime loading.
- OS AI: no EpochBot persona. Operator-selected external models only. Qwen and
  Nemotron handle coding/review; Bonsai, Wan, TRELLIS, and FLUX fallback are
  package-managed creative lanes with license/notice gates.
- Updater: binary-first, platform-build-gated, source fallback only when no
  compatible package exists, visible modal/progress/cancel/restart evidence,
  executable-local cache, no self-close before verified handoff.
- Linux/WSL: default to single-context OpenGL proof. DirectX disabled. Vulkan
  explicit validation only. Software remains debug fallback.
- Source shape: continue small build-proven file moves into owned folders;
  move public headers to `include`, internal headers near owners, and backend
  files into backend folders without broad aesthetic churn.
- Documentation rule: after source proof, update roadmap/matrix/changelog/docs
  only for changed contracts, completed systems, safety gates, and durable
  operator observations.
