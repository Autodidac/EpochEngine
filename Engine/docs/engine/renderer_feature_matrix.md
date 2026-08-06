# Renderer Capability Evidence

## Purpose

This file records current renderer/resource evidence. It is not a second roadmap.
The canonical plan and tier definitions are in
`capability_tier_architecture.md`; scheduling is in `Changes/roadmap.md`.

## Evidence Policy

Capability states are strict:

- `Present`: native implementation exists and relevant validation evidence has
  passed.
- `Partial`: a contract, guard, or incomplete implementation exists.
- `Missing`: no real implementation exists.
- `Deferred`: intentionally excluded from the selected profile or phase.

Evidence is layered:

```text
declaration
-> build contract
-> pure contract test
-> runtime probe
-> presentation/eye proof
-> benchmark
-> production observation
```

An API version, enum, descriptor, successful build, or safe no-runtime refusal
cannot prove presentation. Capability belongs to a subsystem implementation, not
to an API brand.

The compatibility floor targets GTX 1660-era hardware and equivalent APIs. Newer
features must be individually capability-gated.

## Backend Summary

| Backend | Current role | Current evidence | Important gap |
| --- | --- | --- | --- |
| CPU/software | T0 reference, headless, safe fallback | Deterministic contracts and fallback paths exist | Production 2D raster parity and physical output remain bounded work |
| OpenGL | First `T1-GL` desktop presentation and portable technique proof | Context, editor scene, GUI composition, sampled RTT preview, and basic resources exist | Complete Canvas2D/resource/material/settings proof |
| OpenGL ES | T1-GLES mobile target contract | Profile/limits are represented | No production GLES runtime/presentation proof yet |
| SDL3 | OpenGL-derived context/tool adapter | Window, input, presentation, GUI, sampled RTT, and operator-accepted filled orientation exist | Solids and resources remain `Partial` beyond the accepted preview proof |
| SFML3 | OpenGL-derived context/tool adapter | Window, presentation, GUI, runtime-gated RTT, and operator-accepted complete-triangle orientation exist | Solids and resources remain `Partial` beyond the accepted preview proof |
| Raylib3 | Specialized OpenGL-derived context | Context, editor preview, ownership repairs, and scene rendering exist | Camera-facing solid correction awaits eye proof; keep single/multicontext lifecycle coverage |
| Vulkan | Explicit backend and future `T2-VK` provider | Context, swapchain, lines, depth-tested scene-solid pipeline, GUI subpass, descriptors, buffers, and filled-triangle evidence exist | Corrected front face and retirement ownership await repeated-switch eye proof and broader resource parity |
| DirectX | Active Windows-native D3D11 lane | Context, swapchain, clear/present, shaders, preview triangles/lines, GUI replay, clipping, and operator-accepted outward orientation exist | Formal resource parity and depth/material growth remain; this does not prove D3D12 |
| DirectX 12 | Future `T2-DX` family | Capability vocabulary only | Device, queues, resources, pipelines, and runtime proof are missing |

Normal editor operation owns one backend. Multicontext is diagnostic and does
not prove production performance or feed passive provider selection.

## Current Capability Matrix

| Feature family | Status | Evidence and boundary |
| --- | --- | --- |
| Backend-neutral capability selection | Partial | `capability.profile` defines tiers, evidence, subsystem profiles, project requirements, deterministic selection, and contract checks. Integration through runtime settings/reporting still needs build proof. |
| Window/context bootstrap | Present | OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX/D3D11, software, and noop/headless paths exist. Runtime evidence remains backend-specific. |
| Frame clear/present | Partial | Core paths exist; resize, GUI replay, modal ordering, and repeated replacement remain regression-sensitive. |
| Editor scene lines/helpers | Present | Grid, markers, camera, and helper geometry exist across active editor lanes. |
| Editor scene solids | Partial | OpenGL, SDL3, SFML3, DirectX, and Software have accepted current orientation. Raylib and Vulkan have correction candidates awaiting operator eye proof. |
| Canvas2D planning/runtime | Partial | `render.canvas2d` proves project settings, camera/viewport mapping, logical sprite materials, deterministic quad batching, tile descriptors, immutable submissions, offscreen/final-compose plans, diagnostics, and editor policy persistence. `render.canvas2d.cpu` proves deterministic RGBA8 reference output. `render.canvas2d.presentation` proves full-frame identity, byte-derived artifact keys, bounded residency, explicit image/surface packets, and staged native dispatch. The primary `opengl.canvas2d` compositor compiles with confined viewport/scissor and state restoration, but live editor-slot pixels, project-authored texture binding, GL share-group adapters, and the built-project loop remain unproved. |
| Renderer resource spine | Partial | `render.device`/`render.graph` describe logical buffers, textures, validated upload regions, samplers, shaders, pipelines, materials, meshes/models, render targets, bindings, commands, passes, and graph dependencies. `render.texture.residency` proves bounded generation-checked logical-artifact reuse, eviction, pinning, upload budgets, stale-handle refusal, backend epochs, recreation, and metrics. Native parity is incomplete. |
| Sampled render-to-texture | Partial | Engine Arcade uses one shared content contract with backend-owned scene surfaces in OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX, and Software. Build contracts pass; visual presentation and generic device-spine parity remain incomplete. |
| Texture mapping and residency | Partial | `authoring.texture` produces validated dense RGBA8 artifacts and deterministic payload hashes through the always-built `authoring.texture.artifact` schema/validator. A standalone Clang contract proves that runtime boundary with authoring and texture-editor features disabled. `render.texture.artifact` contract-maps one-time sealed RGBA8-linear mip 0 into the existing standalone residency cache using synthetic-device proof. Immutable source ownership, plan-then-commit miss eviction, transactional recreation, bounded transient replacement, forward-only backend epochs, upload accounting, and stale-handle rejection are build-proven. Serialized artifact reading, production asset resolution, capability-derived admission and budgets, sRGB native consumption, live Canvas2D binding, atlas, bindless, sparse, streaming, and mip-chain residency remain incomplete. |
| Material semantics | Partial | Material handles and named logical texture slots flow through graph/device contracts. Portable sprite material, sampler, alpha, cutout, and color-space declarations exist; backend shader/blend consumption and broader PBR/normal/ORM semantics remain incomplete. |
| Camera and transforms | Present | Perspective and Canvas2D cameras, preview rigs, transforms, and editor controls exist. Shared math adoption should continue as touched. |
| Lighting | Partial | `render.lighting` owns stable directional/point/spot lights, bounded frames, environment state, metrics, Euler conversion, and reference raster evaluation. Editor/runtime preview wiring is in progress. Native buffers, shaders, PBR, and shadows are Missing. |
| CPU ray/spatial queries | Partial | `render.ray` owns validated AABB, sphere, triangle, scene, and voxel-DDA reference queries with explicit status/metrics. Editor selection integration is in progress. Hardware ray query/RT are Missing. |
| Picking and Focus | Partial | Selection and camera controls exist. Shared ray-based depth-correct picking and actual camera Focus are in the active integration batch. |
| Physics | Partial | Stable body/command/fixed-boundary/snapshot contracts exist. A deterministic 2D solver adapter and runtime integration are Missing. |
| Audio | Partial | Logical clips/sources/buses/listener/scheduling/mix-plan contracts exist. Decode/mix/device output is Missing. |
| Sparse voxel/water | Partial | Deterministic sparse voxel storage and explicit-time analytic water queries/projection exist. Generation/residency, mesh/render resources, buoyancy, and native presentation are Missing. |
| Text and GUI | Partial | Engine GUI rendering and expanded EpochGui portable control primitives exist. Engine-wide integration, professional docking, complete text behavior, and portable profile exclusion remain work. |
| Debug/evidence surfaces | Partial | Logging, Systems/System Info, FPS, capture, and contract tests exist. Capability-driven settings, GPU markers, and complete cost visibility remain work. |

## Sampled RTT Evidence

| Backend lane | Descriptor | Graph | Native adapter | Live allocation | Presentation | Scene surface | Overall |
| --- | --- | --- | --- | --- | --- | --- | --- |
| OpenGL | Present | Present | Present | Partial | Partial | Partial | Partial |
| SDL3 | Present | Present | Present | Partial | Partial | Partial | Partial |
| SFML3 | Present | Present | Partial | Partial | Missing | Partial | Partial |
| Raylib3 | Present | Present | Partial | Partial | Missing | Partial | Partial |
| Vulkan | Partial | Partial | Missing | Missing | Missing | Partial | Partial |
| DirectX/D3D11 | Partial | Partial | Missing | Missing | Missing | Partial | Partial |
| CPU/software | Deferred | Deferred | Deferred | Deferred | Deferred | Partial | Deferred |

OpenGL's logical device now proves texture hook/work-order behavior without a
live GL context, and the native adapter implements context-guarded allocation,
upload, readiness, and destruction. The renderer-neutral presenter and primary
OpenGL compositor are compiled and contract-proven, including safe no-context
refusal, but the compositor is not yet routed through the live editor scene
slot. Live allocation, drawing, and Canvas2D pixel presentation
still require registered-context runtime evidence. SDL/SFML/Raylib no-runtime
refusal proves the generic-device guard only. `Scene surface` records the
backend-owned Engine Arcade path and stays `Partial` until visual proof.
System Info must keep every evidence layer separate.

## Scene-Solid Repair Contract

The current candidate consumes the shared `object_solid_vertices_for()` stream
without changing queue, GUI replay, or present order.

- OpenGL, SDL3, SFML3, DirectX, and Software have operator-accepted current
  orientation.
- The Arcade sampled screen uses one shared front-plane calculation in every
  backend adapter; compilation and graph contracts pass, while the actual
  presentation remains `Partial` pending operator eye proof.
- Shared preview geometry defines the clockwise-outward object convention and
  carries compile-time exterior/opposite-face checks.
- Raylib filters camera-facing back sides before projected fill.
- Vulkan uses corrected front-face/back-face culling only in its depth-tested
  scene-solid pipeline; line and GUI pipelines remain uncullled.
- Vulkan retirement transfers registry ownership before teardown, retains
  callback lifetime, waits for device idle, and resets both graphics pipelines
  before destroying the logical device.
- Near-plane clipping, native material parity, and deeper depth/resource work
  remain backend-specific follow-up.

Raylib/Vulkan status remains `Partial` until corrected operator visual proof;
Vulkan additionally needs repeated switch-away proof. Filled-triangle evidence
alone does not prove orientation or teardown safety.

## Baseline 2D Renderer Gate

The next renderer product gate is not PBR. It is:

1. logical sprite material and texture references;
2. deterministic sprite batching and draw order;
3. Canvas2D offscreen color target and final compose;
4. alpha blend/cutout and nearest/linear sampling;
5. resize, letterbox, integer-scale, and pixel-aware camera policy;
6. tile-layer/chunk culling and runtime artifacts;
7. texture document compilation and physical residency selection;
8. CPU/reference contract proof and OpenGL presentation proof;
9. capability-aware settings and cost diagnostics;
10. Run/Build consumption in a real project.

This gate targets `T0-CPU` plus `T1-GL` and shapes contracts for later
`T1-GLES`. It does not require advanced explicit or ray features.

## Settings And Evidence Surfaces

Renderer/settings integration must expose:

- selected project profile and deterministic fallback policy;
- active backend family, tier, features, limits, evidence, and stability;
- Canvas2D resolution/scaling/sampling/blend policy;
- texture memory, history, compilation, residency, and cache budgets;
- batch, tile, render-target, upload, and frame metrics;
- disabled/experimental reasons for unavailable/partial features.

EpochGui owns portable controls/state. Engine adapters own backend input/drawing,
project persistence, native hosts, and measured evidence.

## Selective OpenGL Technique Intake

`Autodidac/tiered_gfx_OpenGL_modular_context_demo` is a reference laboratory.
Translate, do not transplant:

- quality budgets and settings into existing capability/budget types;
- material/view/post-process semantics into renderer-neutral descriptors;
- direct pass order into render-graph dependencies;
- RTT/HDR/final composition into OpenGL backend implementations of shared
  resources;
- manifest-backed CC0 diagnostic textures into an optional/default asset pack.

Do not copy its alternate resource spine, platform bootstrap, vendored EpochGui,
hard-coded scenes, direct uniforms, raw GL ownership outside the OpenGL backend,
or unsupported capability labels. Preserve required MIT notice for derived code.

PBR, sky, shadows, terrain, water, foliage, particles, cloth, and advanced post
processing follow the playable 2D loop.

## Cross-API Mapping

| Engine contract | OpenGL/GLES | Vulkan | DirectX |
| --- | --- | --- | --- |
| Resource binding | Uniform/buffer/image/texture bindings under explicit Epoch ownership | Descriptor sets/push constants/resources | Root/descriptor bindings for future D3D12; current D3D11 adapter maps equivalent intent |
| Pipeline | Program + vertex/state discipline | Graphics/compute pipeline | D3D11 shader/state objects now; PSO/root signature in D3D12 |
| Render target | FBO attachments | Image/view plus render pass/dynamic rendering | RTV/DSV resources |
| Draw/dispatch | Direct, instanced, indirect where proven | Command-buffer draw/dispatch | D3D11 draw now; D3D12 command lists later |
| Debug/evidence | GL debug/timers | Validation/debug utils/queries | Debug layer/markers/queries |

Shared contracts define intent and ownership. Backend-specific types never become
world, authoring, material-document, or project identity.

## Deferred Renderer Work

After the playable 2D product:

1. T1 GLES/OpenGL compute and accelerated batching/procedural work.
2. Model import, richer materials, instancing, normal maps, and sky.
3. First native lighting and shadow paths.
4. T2 Vulkan/DirectX resource parity.
5. Deferred/G-buffer, SSAO, particles, skeletal animation, and advanced post.
6. T3 bindless/sparse/async/subgroup/mesh/GPU-driven features.
7. T4 hardware ray query.
8. T5 full RT pipelines.

Every later feature preserves lower-tier fallbacks and earns evidence per backend
and subsystem.