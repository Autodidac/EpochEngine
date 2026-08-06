# Epoch Capability-Tier Architecture

## Authority

This is Epoch's single forward architecture and delivery plan. It joins the
capability model, temporal world, temporal authoring, renderer/resource spine,
editor controls, and product profiles into one dependency order.

The supporting documents have narrow roles:

- `renderer_feature_matrix.md` records current implementation evidence.
- `temporal_engine_architecture.md` defines authoritative world time and state.
- `temporal_authoring_platform.md` defines editable documents and compilation.
- `Changes/active_pass.md` defines the current bounded source gate.
- `Changes/roadmap.md` schedules the next eight weeks.
- `Changes/mission_cache.md` preserves durable missions and historical context.

Architecture documents describe current contracts and future invariants. Release
history, abandoned approaches, and pass-by-pass debugging belong under
`Changes/`.

## Mission

Epoch is a procedural, temporal, capability-selected engine that scales from
headless software and ordinary desktop tools through mobile and desktop games
to explicit GPU and ray-tracing workloads.

Every major subsystem may provide multiple implementations. Epoch selects an
implementation per operation using requirements, measured capability, evidence,
quality, determinism, latency, memory, power, stability, and project policy.
No graphics API, cache layout, or physical device becomes authoritative world
state.

The immediate product objective is deliberately smaller:

> Within two months, produce a playable baseline 2D project that can be authored,
> run, built, and reopened through Epoch's normal project workflow.

Higher-tier rendering remains architecturally compatible but cannot displace
that critical path.

## Core Flow

```text
Temporal world/document address
        |
Immutable observation or compiled artifact
        |
Subsystem work request
        |
Project requirements + capability/budget policy
        |
Deterministic implementation selection
        |
Execution/render graph
        |
Logical resource and residency plan
        |
CPU / GLES / OpenGL / Vulkan / DirectX execution
        |
Disposable result, cache, or presentation
```

Authoritative state is stable and portable. Compiled artifacts are reproducible.
Atlases, bindless slots, sparse pages, GPU buffers, pipelines, previews, and
backend command state are disposable physical representations.

## Capability Tiers

Tiers are capability levels, not API rankings.

| Tier | Profiles | Purpose |
| --- | --- | --- |
| T0 | `T0-CPU` | Headless correctness, deterministic CPU/software reference, CI, and low-resource fallback |
| T1 | `T1-GLES`, `T1-GL` | Portable mobile/desktop presentation; compute, storage, and indirect work remain individually capability-gated |
| T2 | `T2-VK`, `T2-DX` | Explicit graphics/compute, queues, synchronization, descriptors, and residency |
| T3 | `T3-VK`, `T3-DX` | Individually proven bindless, sparse, async, subgroup/wave, mesh, and GPU-driven features |
| T4 | `T4-VK`, `T4-DX` | Hardware inline ray-query providers |
| T5 | `T5-VK`, `T5-DX` | Full hardware ray-tracing pipelines |

T0 is intentionally headless. Native or windowed presentation begins at T1; software raster output at T0 remains an offscreen/reference result.

A machine can expose several tiers at once. A project can use T0 CPU spatial
queries, T0 GL presentation, and a T2 explicit upload path in the same run.
Equivalent Vulkan and DirectX capabilities occupy the same numeric tier.

Vendor-specific features are capability packs, not invented higher tiers.
Examples include shader execution reordering, micromaps, work graphs, vendor
upscaling, frame generation, and vendor sparse extensions.

## Capability Evidence

Epoch extends existing capability, budget, performance-tier, render-device, and
runtime-profile ownership. It must not grow a second global tier manager.

`capability.profile` currently defines the backend-neutral foundation:

- backend family and tier;
- feature states and masks;
- device, memory, power, quality, determinism, and cost information;
- validation evidence and stability;
- per-subsystem implementation profiles;
- typed project requirements, admission policy, and deterministic fallback
  selection;
- build-safe checks for CPU, GLES, OpenGL compute, Vulkan/DirectX equivalence,
  missing-feature fallback, software-fallback policy, experimental rejection,
  unknown-cost reporting, and no-overclaim behavior.

`platform.budgets` owns tier-to-budget recommendations. Project manifests name
a capability profile, while the editor reports active-editor admission and
selected project-run admission separately. Missing legacy manifest fields use
the portable default; malformed, duplicated, or unknown values fail closed.

Every capability has one state:

- `Present`: implementation exists and validation evidence supports the claim.
- `Partial`: a contract or incomplete implementation exists.
- `Missing`: no implementation exists.
- `Deferred`: intentionally excluded from the selected profile or phase.

API names, descriptors, enum rows, safe refusal, or build success alone do not
prove presentation. Evidence is layered: declaration, build contract, pure
contract test, runtime probe, presentation probe, benchmark, and production
observation.

## Subsystem Selection

Selection applies independently to:

- rendering and presentation;
- spatial and editor-picking queries;
- texture residency and streaming;
- procedural generation;
- physics;
- audio;
- navigation;
- voxel and volume traversal;
- temporal reconstruction;
- offline capture;
- future ray-query and ray-tracing work.

An implementation declares its required features, supported representations,
quality range, determinism, stability, evidence, startup cost, latency, memory,
power, and fallback behavior. Deterministic tie-breaking uses stable
implementation identity rather than implicit backend preference.

Operator or project overrides remain explicit. Passive benchmark evidence may
recommend a provider, but multicontext diagnostics never feed automatic
selection because concurrent backends distort timing and resource pressure.

## Product Profiles

Profiles compile and activate only what the product needs.

| Profile | Required floor | Typical optional paths |
| --- | --- | --- |
| Headless/software | `T0-CPU` | SIMD, software raster, compute device |
| Mobile 2D | `T0-CPU` + `T1-GLES` | Compute/storage features when individually proven |
| Desktop compatibility | `T0-CPU` + `T1-GL` | Compute/storage features when individually proven |
| Vulkan desktop | `T0-CPU` + `T2-VK` | GL fallback, T3-T5 Vulkan |
| DirectX desktop | `T0-CPU` + `T2-DX` | GL fallback, T3-T5 DirectX |
| Full hybrid | CPU reference + selected graphics providers | Compute, ray query, RT pipeline |

Games, mobile apps, console targets, servers, and headless tools can exclude
editor workspaces, native floating-window hosts, and heavy authoring code while
retaining compiled artifact readers and runtime controls.

## Two-Month Baseline 2D Critical Path

The first product profile is a deterministic, playable `T0-CPU` plus `T1-GL`
desktop 2D project, with contracts shaped for later `T1-GLES`. It must include:

1. **Canvas2D composition**
   - orthographic camera and pixel-aware viewport policy;
   - offscreen color target and final compose;
   - deterministic layer and draw ordering;
   - alpha blend/cutout and nearest/linear sampling;
   - resize, letterbox, and integer-scale policies.
2. **Temporal texture documents**
   - stable texture identity and revisions;
   - sparse editable tiles, layers, semantic operations, undo/redo, checkpoints,
     deterministic compilation, and bounded history;
   - logical texture references independent of physical placement.
3. **Texture residency**
   - standalone, atlas, bindless, and sparse plans selected by capability and
     budget;
   - atlases remain useful physical caches, never canonical asset meaning;
   - CPU/reference and compatibility upload paths remain valid.
4. **Sprite and tile rendering**
   - sprite material, batch, animation frame, tile layer, culling, sorting,
     camera, and diagnostic contracts;
   - one playable map with stable object and collision identity.
5. **Input, physics, and audio**
   - configurable actions and device bindings;
   - deterministic fixed-step 2D body/collision adapter behind
     `physics.manager`;
   - clips, buses, spatial policy, and a physical output adapter behind
     `audio.manager`.
6. **Authoring and project loop**
   - 2D workspace, asset browser, scene hierarchy, inspector, palette/tile tools,
     play/stop, save/reopen, and visible diagnostics;
   - commands mutate documents through semantic operations rather than widget
     access to internal state;
   - Run and Build produce a project-owned executable/runtime using the saved
     scene and selected profile.

The acceptance project is small on purpose: one map, controllable actor, camera,
collision, animation, sound, restart, save/reopen, and packaged run. It proves
the engine loop before advanced rendering expands.

## Delivery Phases

### Phase A: Capability And Control Alignment

Completed source checkpoint:

- `capability.profile` centrally derives renderer and subsystem profiles from
  render-device evidence and evaluates typed project requirements;
- `platform.budgets` owns tier recommendations without depending on the
  high-level runtime-profile module;
- project manifests carry a capability profile with a portable legacy default
  and fail-closed malformed, duplicated, unknown, or mismatched values;
- Project, Settings, status, and System Info surfaces report editor-backend and
  project-run admission separately, including the recommended budget;
- build-safe contracts cover project policies, manifest parsing, fallback
  policy, experimental rejection, and unknown renderer cost.

### Phase B: Canvas2D And Texture Spine

- Complete the texture document/history/compiler/residency vertical slice.
- Add sprite material and batch contracts.
- Add Canvas2D offscreen composition and diagnostic default textures.
- Prove CPU/reference behavior and OpenGL compatibility presentation.

### Phase C: Playable Scene

- Add tilemap document/runtime artifact, deterministic sorting, culling, and
  animation.
- Connect input actions, fixed-step 2D physics, and audio output.
- Make editor selection, focus, transform, spawn, run, save, reopen, and build
  operate on the same project scene.

### Phase D: Hardening

- Prove Debug and Release builds, contract tests, project restart, asset
  portability, cache deletion/rebuild, and bounded memory.
- Add GLES-oriented limits and remove desktop-only assumptions from portable
  contracts.
- Keep Vulkan, SDL, SFML, Raylib, and DirectX evidence honest; they do not block
  the baseline 2D product unless they break shared contracts.

## Settings And Controls

Settings are a product surface, not an afterthought. Every maturing subsystem
must ship its control and evidence model in the same bounded pass.

The editor should expose:

- project capability profile and fallback policy;
- active backend and measured evidence;
- Canvas2D resolution, scaling, sampling, blend, batching, and memory budgets;
- texture history, compilation, residency, and cache costs;
- input actions, physics step/bounds, audio buses, and runtime diagnostics;
- advanced options through progressive disclosure.

Reusable control state belongs in EpochGui. Engine adapters own renderer input,
drawing, native windows, project state, and backend evidence. Floating/docking
hosts remain optional desktop tooling and are excluded from portable game
profiles.

## Renderer And Scene Integration

The shared renderer spine owns logical buffers, textures, samplers, materials,
meshes, targets, binding sets, passes, commands, and graph dependencies.
Backends implement those contracts without leaking API objects into world or
authoring state.

Current shared scene contracts include renderer-neutral math, bounded logical
lights and reference lighting, CPU ray/voxel queries, deterministic physics
command/snapshot ownership, logical audio plans, sparse voxel storage, analytic
water queries, and Tier-0 scene descriptors. Native shaders, shadows, physical
audio, collision solving, water rendering, and hardware ray paths require their
own evidence.

OpenGL-derived adapters can share contracts and data, but each backend still
owns its context and presentation proof. Vulkan and DirectX implement equivalent
native resources in their own execution models.

## Temporal Integration

Time and branch identity remain part of authoritative world addresses. Runtime
systems consume immutable observations. Authoring systems produce validated
semantic operations. Past edits invalidate only their causal future.

The four authoring layers remain mandatory:

```text
Authoring document
Temporal semantic history
Compiled runtime artifact
Disposable physical execution cache
```

The 2D critical path uses this architecture narrowly for texture, tilemap, and
scene documents. It does not wait for multiplayer, persistent AI, planetary
terrain, or the full general node editor.

## Selective OpenGL Technique Ingestion

`Autodidac/tiered_gfx_OpenGL_modular_context_demo` is a technique laboratory,
not an authority or donor engine.

Use it selectively:

- translate quality budgets and feature controls into existing capability and
  budget types;
- extract material semantics, view/post-process policy, light inputs, RTT,
  final composition, and proven OpenGL techniques behind Epoch contracts;
- convert direct pass order into render-graph dependencies;
- ingest only license-verified default assets through package/asset manifests.

Do not copy its resource spine, WGL/X11/GLX scaffolding, duplicated module
names, vendored EpochGui, hard-coded scenes, direct uniform ownership, raw GL
objects outside the OpenGL adapter, or unsupported capability labels.
MIT-derived code must retain its required notice. Curated assets without the
default pack's provenance chain remain quarantined.

The fastest useful subset is RTT/final composition, texture policy, instancing
ideas, and small CC0 diagnostic textures. PBR, shadows, tessellation, water,
cloth, volumetrics, and other effects follow only after the playable 2D loop.

## Deferred Horizon

After the baseline 2D product is stable:

1. T1 GLES/OpenGL compute and GPU-driven 2D/procedural work.
2. T2 Vulkan/DirectX resource parity.
3. T3 bindless, sparse, async, subgroup/wave, and mesh features.
4. T4 hardware ray-query providers.
5. T5 full RT pipelines.
6. General typed node authoring, 3D material/model/effects editors.
7. Persistent regions, collaboration, networking, planetary/astronomical
   packages, and policy-driven hybrid execution at scale.

Each phase must preserve lower tiers and prove capability per subsystem.

## Completion Invariants

- CPU/software remains the universal correctness and headless floor.
- API brands never define capability by themselves.
- `Present` always has implementation and evidence.
- Selection is deterministic and per subsystem.
- Projects compile out unused backends, editor hosts, and optional packages.
- Source documents and semantic history are portable.
- Physical caches are disposable.
- Settings and controls match what the active build can actually do.
- The playable 2D project can be authored, run, built, reopened, and reproduced
  after deleting generated caches.
