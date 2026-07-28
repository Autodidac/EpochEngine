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
| SDL3 | OpenGL-derived context/tool adapter | Window, input, presentation, GUI, sampled RTT, and filled editor triangles have operator evidence | Outward-face correction awaits final eye proof; solids remain `Partial` |
| SFML3 | OpenGL-derived context/tool adapter | Window, presentation, GUI, runtime-gated RTT, and filled complete-triangle presentation have operator evidence | Outward-face correction awaits final eye proof; solids remain `Partial` |
| Raylib3 | Specialized OpenGL-derived context | Context, editor preview, ownership repairs, and operator-proven scene rendering exist | Keep single/multicontext lifecycle and presentation regression coverage |
| Vulkan | Explicit backend and future `T2-VK` provider | Context, swapchain, lines, depth-tested scene-solid pipeline, GUI subpass, descriptors, buffers, and filled-triangle operator evidence exist | Corrected solid culling awaits final eye proof and broader resource parity |
| DirectX | Active Windows-native D3D11 lane | Context, swapchain, clear/present, shaders, preview triangles/lines, GUI replay, clipping, and CPU outward-face filtering exist | Corrected orientation awaits eye proof; formal resource parity and depth/material growth remain; this does not prove D3D12 |
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
| Editor scene solids | Partial | OpenGL and Raylib are the orientation references. SDL3, SFML3, Vulkan, DirectX, and Software now have filled paths plus an outward-face correction candidate. Status remains Partial until build and operator eye proof. |
| Canvas2D camera/mode | Partial | Orthographic preview state and editor mode exist. A complete offscreen compose, sprite batch, scaling, tilemap, and built-project loop are active work. |
| Renderer resource spine | Partial | `render.device`/`render.graph` describe logical buffers, textures, samplers, shaders, pipelines, materials, meshes/models, render targets, bindings, commands, passes, and graph dependencies. Native parity is incomplete. |
| Sampled render-to-texture | Partial | Descriptor and graph proof exist. OpenGL editor presentation and a live SDL target-texture lane exist. Other backends require their own allocation/presentation evidence. Engine Arcade remains the proof consumer. |
| Texture mapping and residency | Partial | GUI/font atlases, uploads, backend texture modules, logical texture work, and a temporal texture/residency foundation exist. Unified sampler, standalone/atlas/bindless/sparse selection, Canvas2D use, and evidence are incomplete. |
| Material semantics | Partial | Material handles and named logical texture slots flow through graph/device contracts. Alpha mode, PBR factors, color-space intent, normal/ORM slots, and portable sprite material need consolidation. |
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

| Backend lane | Descriptor | Graph | Native adapter | Live allocation | Presentation | Overall |
| --- | --- | --- | --- | --- | --- | --- |
| OpenGL | Present | Present | Present | Partial | Partial | Partial |
| SDL3 | Present | Present | Present | Partial | Partial | Partial |
| SFML3 | Present | Present | Partial | Partial | Missing | Partial |
| Raylib3 | Present | Present | Partial | Partial | Missing | Partial |
| Vulkan | Partial | Partial | Missing | Missing | Missing | Partial |
| DirectX/D3D11 | Partial | Partial | Missing | Missing | Missing | Partial |
| CPU/software | Deferred | Deferred | Deferred | Deferred | Deferred | Deferred |

OpenGL's logical device can prove hook/work-order behavior without a live GL
context, but live allocation requires a registered context. SDL/SFML/Raylib
no-runtime refusal proves the guard only. System Info must keep every evidence
layer separate.

## Scene-Solid Repair Contract

The current candidate consumes the shared `object_solid_vertices_for()` stream
without changing queue, GUI replay, or present order.

- OpenGL and Raylib remain the known-good orientation references.
- Shared preview geometry defines the clockwise-outward object convention and
  carries compile-time exterior/opposite-face checks.
- SDL3, SFML3, DirectX, and Software filter camera-facing back sides before
  projected fill; SFML still preserves complete three-vertex groups and SDL
  reports geometry failures.
- Vulkan uses back-face culling only in its depth-tested scene-solid pipeline;
  line and GUI pipelines remain uncullled.
- Near-plane clipping, native material parity, and deeper depth/resource work
  remain backend-specific follow-up.

Status remains `Partial` until Debug/Release build and corrected operator visual
proof. Filled-triangle evidence alone does not prove orientation.

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