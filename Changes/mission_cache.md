# Mission Cache

This file preserves operator-confirmed missions without turning `AGENTS.md` or
`Changes/active_pass.md` into a living novel. Agents should use it as memory
after choosing the current source gate from `Changes/active_pass.md`.

## Renderer Spine

- Build a truthful renderer spine where reusable feature families prove
  themselves in OpenGL first, OpenGL-derived backends share the same contract,
  and Vulkan/DirectX keep equivalent native contracts without drift.
- Formalize handles/descriptors/resource ownership for buffers, textures,
  samplers, shaders, pipelines, materials, meshes, models, render targets,
  binding sets, command lists, render passes, and frame graph work.
- Near-term feature order after sampled render-to-texture: model import, normal
  mapping, skybox, instancing, shadows, deferred/G-buffer, and SSAO.
- Capability honesty is mandatory: `Present` means native backend implementation
  exists and build verification proves it; contracts alone are `Partial`.

## Context And Backend Parity

- OpenGL, SDL3, SFML3, and Raylib3 should share one OpenGL-derived
  renderer-resource contract while preserving backend-specific context nuances.
- DirectX and Vulkan implement equivalent native contracts in their own models.
- Software is a debug/safe-launch fallback, not production parity.
- All production contexts need shared visual palette, object colors, opacity,
  helper colors, and GUI theme inputs from the central spine.
- Floating GUI/tool panes should become dockable/undockable context windows that
  clone the selected backend family by default. Mixed-backend grids are
  diagnostic/preview mode, not normal editor workflow.

## GUI And Editor Workflow

- Build reusable GUI primitives in `engine.gui` first: selectable/editable text,
  right-click context menus, copy/paste, word wrap, scroll bounds, modal focus,
  progress bars, list rows, tabs, docking chrome, theme tables, and floating
  windows.
- Preserve the proven draw model. Command menus and modals stay above the scene,
  close/deselect predictably, and must not reintroduce OpenGL flicker.
- Modal, dropdown, progress, and timeline surfaces share the same GUI containment
  contract: top-layer popups render above their owner, hit testing respects modal
  capture, and progress/timeline bars must fit their content rect without bleed.
- Package Manager should use a real list with per-row status/action controls,
  visible transfer/build progress, license evidence, source gates, cache
  validation, cancel behavior, and no fake-progress claims.
- Console Dock is evidence/status only. Editor controls belong in workspaces and
  reusable GUI surfaces, not duplicated in output panes.

## Runtime, Projects, And Packages

- Render-to-texture belongs in core engine. Arcade/game packages consume RTT
  later for in-game cabinets and runtime-minis.
- Engine Arcade must resolve to a real upright arcade cabinet asset from
  `Autodidac/EpochEngineExtensions` or executable-local `cache/packages/`,
  including screen, marquee, control deck, coin/door panels, and cabinet shell.
  Procedural boxes are only the fallback when no reviewed package model exists.
- Built-in mini-runtimes remain kernel-owned and exposed to projects as
  package/script assets.
- Play In Editor runs the saved scene inside the editor. External project launch
  is separate, chooses one backend, does not reset scene edits, and does not
  spawn multicontext clones unless requested.
- Scene persistence must remove unwanted starter junk, keep camera/light/default
  essentials, and make parser/serializer own preview/runtime loading.
- Package payloads belong under executable-local `cache/packages/` or the
  separate EpochEngineExtensions repo, not in mainline dumps.

## Forest Factory, Voxel, And Time

- Forest Factory is a core editor workspace with its own 3D scene/window,
  temporal graph growth, repo-derived asset lineage, voxel-node LOD and
  hit-detection descriptors, and timeline integration.
- Heavy planetary terrain, multi-terrain authoring, FFT ocean, imported
  prototypes, and game-specific world stacks remain package candidates.
- Epoch is time-based. The Video workspace owns visible timeline graph,
  playhead, cadence, streaming-save hooks, and project/video timing controls
  near the scene instead of hiding them in System Info.
- Add universal configurable input profiles, freecam/FPS-style 3D controls,
  center/reset hotkeys, package-exportable input contracts, and project camera
  style selection.

## OS AI And Models

- OS AI is not EpochBot and not an internal persona. Use operator-selected
  external/source-available models only.
- Qwen and Nemotron handle coding/review. Bonsai, Wan, TRELLIS, and FLUX
  fallback are package-managed creative lanes with license/notice gates.
- Model scan is inventory-only. Choosing a model must initialize exactly that
  model, persist the selection under executable-local cache state, and never
  silently fall back to stale defaults.
- Hidden model reasoning must not surface in chat or become training data.

## Updater, Linux, And Source Shape

- Updater is binary-first, platform-build-gated, and source-fallback only when
  no compatible package exists. It must show visible modal/progress/cancel/
  restart evidence, use executable-local cache, and never self-close before
  verified handoff.
- Linux/WSL defaults to single-context OpenGL proof. DirectX is disabled, Vulkan
  is explicit validation only, and software remains a debug fallback.
- Continue small build-proven file moves into owned folders. Public headers move
  to `include`, internal headers stay near owners, and backend files move into
  backend folders only with MSVC/CMake/module proof.
- Documentation comes after source proof and should record only changed
  contracts, completed systems, safety gates, and durable operator observations.
