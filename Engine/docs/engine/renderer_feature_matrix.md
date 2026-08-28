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
| CPU/software | T0 reference, headless, safe fallback | Deterministic raster/contracts plus framebuffer Canvas2D composition build | Live output, resize, and repeated-switch proof remain `Partial` |
| OpenGL | First `T1-GL` desktop presentation and portable technique proof | Context, editor scene, GUI composition, sampled RTT preview, and basic resources exist | Complete Canvas2D/resource/material/settings proof |
| OpenGL ES | T1-GLES mobile target contract | Profile/limits are represented | No production GLES runtime/presentation proof yet |
| SDL3 | OpenGL-derived context/tool adapter | Accepted Windows launcher/loading/editor, high-DPI input/resize, and clean close plus native Canvas2D texture/presentation source builds | Sampled-RTT/Canvas2D reference pixels and repeated-switch/native-memory soak remain `Partial` |
| SFML3 | OpenGL-derived context/tool adapter | Existing window/GUI/RTT evidence plus native Canvas2D texture/presentation and capture-only `copyToImage` evidence source builds | Pixel parity, resize, and repeated-switch proof remain `Partial` |
| Raylib3 | Specialized OpenGL-derived context | Existing scene/ownership repairs plus exact native-context Canvas2D texture/presentation and capture-only screen-image evidence source builds | Pixel parity and single/multicontext lifecycle proof remain `Partial` |
| Vulkan | Explicit backend and future `T2-VK` provider | Existing swapchain/scene/GUI evidence plus Canvas2D upload, compose pipeline, sampling, and transactional residency source builds | Pixel parity, asynchronous animated residency, and repeated-switch proof remain `Partial` |
| DirectX | Active Windows-native D3D11 lane | Existing context/scene/GUI evidence plus native Canvas2D texture, premultiplied blend, sampling, and compose source builds | Pixel parity and repeated-switch proof remain `Partial`; this does not prove D3D12 |
| DirectX 12 | Future `T2-DX` family | Capability vocabulary only | Device, queues, resources, pipelines, and runtime proof are missing |

Normal editor operation owns one backend. Multicontext is diagnostic and does
not prove production performance or feed passive provider selection.

Authorized 2026-08-26 Windows SDL evidence at 150% display scale accepts the
launcher, replay-backed loading transition, current editor GUI, Systems
workspace, resize, World Script Browser, large source editor, wheel input,
120 FPS title, and clean close. SDL owns separate logical and physical
dimensions and normalizes sampled and queued mouse positions through the same
mapping. Sampled-RTT pixels, Canvas2D reference agreement, repeated context
switching, and native memory soak remain open. Matched Windows OpenGL Debug
reruns with and without an explicit 120 FPS override report swap interval 0,
sustain 120-121 measured frames per second with approximately 0.02 ms
`SwapBuffers`, retain the current launcher/editor GUI, display 120 FPS in the
native title, and close cleanly. This closes the current 60 FPS
frame-policy/telemetry discrepancy without claiming broader OpenGL native-pixel,
resize, or repeated-switch proof. None of this evidence changes protected
backend frame lifecycle, queue drain, GUI replay, top-layer, scene, or present
order.

## Current Capability Matrix

| Feature family | Status | Evidence and boundary |
| --- | --- | --- |
| Backend-neutral capability selection | Partial | `capability.profile` defines tiers, evidence, subsystem profiles, project requirements, deterministic selection, and contract checks. Integration through runtime settings/reporting still needs build proof. |
| Window/context bootstrap | Present | OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX/D3D11, software, and noop/headless paths exist. SDL3 has accepted 150%-DPI Windows bootstrap and one clean native close; runtime evidence remains backend-specific. |
| Frame pacing/present policy | Partial | `perf.tier` resolves explicit target-Hz, VSync, and uncapped requests plus foreground/background/minimized policy into one configured/effective plan. Both managed host loops apply that plan before the unchanged backend frame body and retain the shared deadline limiter when native pacing is unavailable or cannot honor a CPU-paced target. OpenGL, DirectX/D3D11, and SDL3 own native VSync enable/disable; SFML3 owns native VSync and target-Hz; Raylib3 owns native target-Hz/uncapped; Vulkan requests FIFO for VSync and Immediate then Mailbox then FIFO for non-VSync, applies changes only through pre-acquire swapchain recreation, and reports native pacing active only after the selected mode is accepted; Software truthfully remains shared-CPU-only. Deterministic 60/120, uncapped, 30-Hz background, 10-Hz minimized, native-success/fallback, present-mode selection, and pending/accepted lifecycle contracts plus MSVC Debug/Release builds pass. Linux background focus classification remains unclaimed, and no native timing/pixel evidence is added. |
| Frame clear/present | Partial | `render.context_frame` resolves one overflow-safe clipped editor viewport, top-left and bottom-left coordinates, normalized viewport, projection aspect, protected overlay/capture order, and deterministic semantic signature. OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX/D3D11, and Software consume the shared plan through their existing viewport/scissor/projection paths without changing queue, overlay, capture, or present boundaries. SDL3's registered and standalone paths now share one native capture-before-present result, release failed capture reservations for a bounded retry, and publish first-present evidence before the managed host marks the backend ready; MSVC Debug/Release builds and `context.sdl_first_present_readiness` pass. SDL3 launcher, replay-backed loading, editor, resize, and clean close remain accepted; native all-context resize/GUI/modal/replacement pixels remain regression-sensitive and unproved by this build-only parity slice. |
| Editor scene lines/helpers | Present | Grid, markers, camera, and helper geometry exist across active editor lanes. |
| Editor scene solids | Partial | OpenGL, SDL3, SFML3, DirectX, and Software have accepted current orientation. Raylib and Vulkan have correction candidates awaiting operator eye proof. |
| Procedural forest preview | Partial | `ForestAssetDocument` compiles one deterministic morphology sample, renderer-neutral preview geometry, voxel LOD plan, and bounded occupancy. Plant Lab edits, publishes, and reopens one project-owned source/immutable Library pair; Forest Factory places that exact revision and package staging records matching source/compiler evidence. Trunks and sampled branches project as midpoint-aligned, Euler-oriented segments while leaf clusters carry deterministic orientation. Editor and runtime publish the same transform into the common solid/selection stream consumed by OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX/D3D11, and Software. MSVC Debug/Release contracts prove one rotated branch solid plus oriented selected outline. Sparse voxel materialization, mesh/impostor output, native pixels, resize, teardown, and repeated-switch proof across all contexts remain open. |
| Canvas2D planning/runtime | Partial | `render.canvas2d` proves project settings, camera/viewport mapping, logical sprite materials, deterministic quad batching, tile descriptors, immutable submissions, offscreen/final-compose plans, diagnostics, and editor policy persistence. `render.canvas2d_cpu` proves deterministic RGBA8 reference output. `render.canvas2d_runtime` now owns immutable scene/frame/raster reuse for OpenGL and the other active backend adapters; a revision-only publication advances generation and frame evidence while deterministically retaining identical content and Canvas2D hashes. `render.canvas2d_presentation` proves full-frame identity, byte-derived artifact keys, bounded residency, explicit image/surface packets, and staged native dispatch. `project.texture_library`, `project.texture_pipeline`, snapshot format 3, and the Assets controller prove serialized Library persistence, authenticated reopen, semantic material restoration, and owned editor resource closure. Earlier approved OpenGL capture matches T0-CPU across 1,178,872 pixels with zero error; this session-parity repair adds no new native-pixel claim. SDL3, SFML3, Raylib3, Vulkan, DirectX/D3D11, and Software adapters build in Debug/Release; their live pixels/switch cycles and the Assets interaction automation remain unproved. |
| Tilemap authoring/runtime | Partial | `authoring.tilemap` proves stable source identity, sparse chunks, semantic operations, bounded history, and deterministic compilation. `asset.tilemap_artifact` owns the runtime schema, integrity, collision/object payloads, and bounded serialization. `project.tilemap_library`/`project.tilemap_pipeline` prove exact atomic Library publication/restore and project registry identity. `render.canvas2d_tilemap` proves visible culling, animation selection, transforms, stable sprite ordering, collision, and object output. The EpochGui workspace, canonical source persistence, scene binding, exact Library publication/reopen, and generated-runtime preparation are build-proven; approved live editing/pixel evidence and cache-loss external-run proof remain incomplete. |
| Renderer resource spine | Partial | `render.device`/`render.graph` describe logical buffers, textures, validated upload regions, samplers, shaders, pipelines, materials, meshes/models, render targets, bindings, commands, passes, and graph dependencies. `render.texture.residency` proves bounded generation-checked logical-artifact reuse, eviction, pinning, upload budgets, stale-handle refusal, backend epochs, recreation, and metrics. Native parity is incomplete. |
| Sampled render-to-texture | Partial | Engine Arcade uses one shared content contract with backend-owned scene surfaces in OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX, and Software. Build contracts pass; visual presentation and generic device-spine parity remain incomplete. |
| Texture mapping and residency | Partial | `authoring.texture` produces validated dense RGBA8 artifacts through the runtime-owned `asset.texture_artifact` schema/serializer/validator. `asset.texture_import` adds bounded uncompressed BMP, TGA, and P6 PPM decoding. `project.texture_admission`, `project.asset_registry`, `project.texture_library`, `project.texture_pipeline`, and `project.texture_resources` authenticate identity, atomically persist/reopen compiled bytes, publish bounded T0-CPU bindings, restore exact revisions, and optionally acquire disposable residency. Cache recreation, eviction, pinning, backend epochs, upload accounting, stale-handle rejection, portable path collision refusal, semantic snapshot save/reopen, and exact immutable scene closure are build-proven. Persistent rename/move migration, interactive UI proof, sRGB/compressed/mip-chain execution, atlas, bindless, sparse, and streaming remain incomplete. |
| Material semantics | Partial | Material handles and named logical texture slots flow through graph/device contracts. Portable sprite material, sampler, alpha, cutout, and color-space declarations exist; backend shader/blend consumption and broader PBR/normal/ORM semantics remain incomplete. |
| Camera and transforms | Present | One renderer-neutral stable view descriptor carries scene, purpose, projection, orientation, clipping, and revision state. The editor owns perspective, free orthographic, six locked axis views, independent framing, LMB selection, Alt+LMB orbit, MMB pan, Alt+RMB dolly, RMB fly with WASD/QE and speed modifiers, Focus, and Reset. Authored project-camera persistence, oriented axis grids, mobile/console bindings, and live all-context eye proof remain follow-up. |
| Portal and mirror views | Partial | Stable portal identity, shared camera descriptors, recursive cycle/depth/pixel budgeting, clip-plane intent, logical RTT outputs, disposable physical cache state, metrics, and render-pass view bindings are contract-proven. Native oblique/stencil clipping, traversal, presentation, and live backend pixels are not yet present. |
| Lighting | Partial | `render.lighting` owns stable directional/point/spot lights, bounded frames, environment state, metrics, Euler conversion, and reference raster evaluation. Editor and project previews publish shared lighting frames but still use truthful reference-solid shading. Native light buffers, shaders, PBR, and shadows are Missing. |
| CPU ray/spatial queries | Partial | `render.ray` owns validated AABB, sphere, triangle, scene, and voxel-DDA reference queries with explicit status/metrics. Editor selection consumes persistent scene identity through that query spine. Hardware ray query/RT are Missing. |
| Picking and Focus | Present | Editor picking resolves through persistent scene IDs and shared ray queries; Focus changes the active preview camera across presenting child contexts. Higher-tier GPU picking remains optional work, not the accepted baseline. |
| Physics | Partial | `physics.manager`, `physics.solver2d`, and `project.actor2d_runtime` prove stable bodies, deterministic fixed-step AABB/circle collision, layers/masks, bounded static maps, stable contacts, pause/reset, snapshots, map collision, and renderer-neutral actor publication. Authored one-way/slope collision, editor controls, and live acceptance-project proof remain incomplete. |
| Audio | Partial | Logical clips/sources/buses/listener/scheduling, deterministic stereo mixing, a physical-device boundary, and generation-checked project playback sessions are contract-proven. The optional SDL3 device remains renderer-independent across context replacement; decoded project assets, authored cue bindings, editor controls, and approved live physical-output proof remain incomplete. |
| Sparse voxel/water | Partial | Deterministic sparse voxel storage and explicit-time analytic water queries/projection exist. Generation/residency, mesh/render resources, buoyancy, and native presentation are Missing. |
| Text and GUI | Partial | Engine GUI rendering and expanded EpochGui portable control primitives exist. Engine-wide integration, professional docking, complete text behavior, and portable profile exclusion remain work. |
| Debug/evidence surfaces | Partial | Logging, Systems/System Info, FPS, screenshot capture, build-safe contracts, and deterministic Canvas2D CPU/native pixel comparison exist. One bounded caller-buffer coordinator normalizes origin, channel order, padded native row pitch, and destination stride while refusing absent contexts, excessive capacity, or native readback failure. OpenGL, SDL3, SFML3, Raylib3, DirectX/D3D11, and Vulkan invoke it only for explicit capture evidence; normal frames do not pay that readback cost. D3D11 stages and maps the composed scene viewport before GUI/top-layer/present. Vulkan stages and fence-synchronizes the native Canvas2D upload before frame command recording because its Canvas2D and GUI draws share one protected render pass; final Vulkan composed-surface comparison therefore remains open. Approved live capture, capability-driven settings, GPU markers, and complete cost visibility remain work. |

Canvas2D native composition now has one deterministic admission policy for
top-left RGBA8, linear color, premultiplied alpha, clamp-to-edge sampling, and
explicit nearest/linear selection. OpenGL, SDL3, SFML3, Raylib3, Vulkan, and
DirectX/D3D11 consume that policy before their backend-owned filter/sampler
selection; native blend, UV, resource calls, and draw order remain backend-owned.
Software consumes the already-composed deterministic CPU presentation surface
and retains its premultiplied clipped-blit contract. This is source/build
evidence only and adds no pixel claim.

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
refusal. Immutable semantic editor content now routes through the protected live
scene slot. The origin/stride-aware pixel comparator and shared caller-buffer
readback coordinator are compiled and bounded. OpenGL preserves its scoped GL
readback state; SDL3 converts `SDL_RenderReadPixels` to tight top-left RGBA8;
SFML3 crops `copyToImage`; Raylib3 crops its RGBA8 screen image. Each adapter runs
only on an explicit capture request before GUI/top-layer/present, and Raylib's
legacy diagnostic probe is likewise capture-gated. D3D11 copies the exact
composed scene viewport into a staging texture, normalizes mapped `RowPitch`, and
compares before GUI/top-layer/present. Vulkan capture-only evidence instead copies
the backend-owned RGBA8 Canvas upload into a staging buffer and waits one exact
submission fence before normal frame command recording; it does not split or
reorder the protected Canvas2D-plus-GUI render pass, so final Vulkan surface
agreement remains unclaimed. Live allocation, drawing, project-texture
presentation, and pixel agreement still require an approved registered-context
capture. `Scene surface` records the backend-owned Engine Arcade path and stays
`Partial` until visual proof. System Info must keep every evidence layer separate.

## Scene-Solid Repair Contract

The current candidate consumes the shared `object_solid_vertices_for()` stream
without changing backend frame lifecycle, queue drain, GUI replay, or native
present order. DirectX changes only its internal editor-preview subpass order.

- OpenGL, SDL3, SFML3, DirectX, and Software have operator-accepted current
  orientation.
- The Arcade sampled screen uses one shared front-plane calculation in every
  backend adapter; compilation and graph contracts pass, while the actual
  presentation remains `Partial` pending operator eye proof.
- Shared preview geometry defines the clockwise-outward object convention,
  carries compile-time exterior/opposite-face checks, and owns one bounded,
  camera-target-snapped adaptive grid consumed by every active backend. Authored
  scene geometry uses a solid pass plus an outline only while selected; it does
  not receive a permanent diagnostic wire pass. Editor-only camera and light
  objects use compact, purpose-specific wire gizmos rather than cube stand-ins.
- Raylib filters camera-facing back sides before projected fill.
- DirectX orders its editor-preview geometry as background grid, solids, sampled
  surfaces, then foreground helpers so grid lines cannot composite over terrain.
- Vulkan uses corrected front-face/back-face culling only in its depth-tested
  scene-solid pipeline; line and GUI pipelines remain unculled.
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

### Seven-Backend Acceptance Evidence

| Requirement | Current evidence | Status |
| --- | --- | --- |
| Tier-derived Canvas2D admission | One renderer-neutral mapper derives compile, CPU-raster, native-upload, and residency ceilings from the selected capability profile; all seven baseline paths consume it, presenter lanes use a bounded two-slot transaction, Vulkan has no desktop-size upload constant, stricter limits invalidate raster reuse, and over-budget output fails before native hooks | Build-safe on MSVC Debug/Release and managed Clang 22 with all 32 CTest contracts; measured native allocation, resize, memory stability, and switch-soak evidence remain `Partial` |
| Texture identity, import, assignment, and reopen | Authenticated artifacts, Project Library persistence, semantic snapshot restoration, immutable resource closure, and all seven presentation adapters build in Debug/Release | Partial: interactive assignment and six non-OpenGL pixel lanes still need approved proof |
| Tile palette, layers, chunks, collision, and map compilation | Temporal document, EpochGui workspace state, deterministic artifact, exact Library reopen, runtime preparation, culling, collision, and object output are contract-proven | Partial: live editing and generated-runtime pixels remain unproved |
| Orientation | Shared top-left Canvas2D contract and accepted scene-solid orientation for OpenGL, SDL3, SFML3, DirectX, and Software | Partial: Raylib3 and Vulkan require current-candidate eye/pixel proof |
| Alpha, cutout, and nearest/linear sampling | T0-CPU raster contracts and native adapter semantics exist; OpenGL matches the reference image exactly | Partial: equivalent SDL3, SFML3, Raylib3, Vulkan, DirectX, and Software presentation evidence is missing |
| Resize, letterbox, and scaling | Shared viewport mapping, deterministic software clipping, and runtime-session resize invalidation followed by stable reuse are contract-proven; SDL3 150%-DPI GUI resize and input remapping are accepted | Partial: SDL3 minimized/restore and every other native-context resize/minimized-restore lane still need live evidence |
| Resource teardown and repeated switching | Every adapter exposes retirement/reset ownership and current source builds in Debug/Release; the shared presenter owns one current lease and passes 64 content revisions plus 64 backend replacements with bounded fake-device state and balanced retirement; one Windows SDL3 native close is accepted | Partial: each native context still needs approved repeated switch/resize/cache-loss soak with stable native memory and handles; OpenGL context-bound hooks and Raylib deferred-delete error paths remain explicit repair gates |
| Save/reopen, Build, and external Run | Six generated project profiles pass materialize, atomic save/reopen, build, and child self-test; GUI Editor passes the exact external-Run argument path | Partial: the same acceptance project has not yet been exercised through all seven selected backend outputs |
| Complete playable 2D project | One central gameplay runtime joins input, actor physics, sprite animation, audio events, authenticated textures/tilemaps, GUI, Canvas2D output, and request-driven logical cost evidence; fresh generated-child acceptance proves deterministic execution, teardown, and zero `T1-GLES/mobile_30` budget violations | Partial: one authored project has not yet visually proved Play/Stop and native presentation across the selected backends |

## Settings And Evidence Surfaces

Renderer/settings integration must expose:

- selected project profile, tiered 2D budget, and deterministic fallback policy;
- active backend family, tier, features, limits, evidence, and stability;
- Canvas2D resolution/scaling/sampling/blend policy;
- texture memory, history, compilation, residency, and cache budgets;
- logical batch, texture, physics, and audio costs plus native tile,
  render-target, upload, residency, atlas-pressure, and frame metrics;
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
