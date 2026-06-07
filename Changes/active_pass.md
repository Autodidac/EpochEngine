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
- `render.arcade` now makes `engine_arcade.screen` a real two-pass proof shape:
  the populate pass targets the sampled render texture with a tiny scene model,
  and the cabinet pass samples that render surface through a material slot.
- SDL3, SFML3, and Raylib sampled-RTT capability reporting remains
  runtime-availability-gated; contract-only paths are still `Partial`.
- Build evidence: MSVC Debug x64 `ConsoleApplication1` passes after the real
  OpenGL hook contract wiring.

## Allowed Source Areas

- `Engine/modules/render.device.ixx`
- `Engine/modules/render.graph.ixx`
- `Engine/modules/render.arcade.ixx`
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

## Mission Cache Pointer

Durable cross-pass mission memory lives in `Changes/mission_cache.md`. Keep this
file focused on the current gate; do not widen a source pass because the cache
contains broader roadmap work.
