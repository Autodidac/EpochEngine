# Renderer Capability Evidence

## Purpose

This file records current renderer/resource evidence. It is not a second roadmap.
The canonical plan and tier definitions are in
`capabilities.md`; scheduling is in `Changes/roadmap.md`.

## Evidence Policy

November7 depth repair: planetary depth fitting now receives viewport aspect.
Offscreen triangles crossing the eye plane are clipped against the view sides
before contributing depth bounds. True visible eye-plane crossings retain the
conservative near plane. The prior owner reproduces a false5cm near plane;
actual-module Debug/Release red/green checks cover all sides, intersections,
precise/nonuniform transforms and resize identity. Native close-approach
terrain/water wireframe acceptance remains open. This changes depth fitting,
not frame order or the hidden-wire prepass.

The optional planner's broader orbital attention passes rectangular viewport
coverage and existing streaming checks in both modes at the same leaf budget.
Near-ground peripheral coverage and runtime smooth-transition integration remain
unfinished. Whole-cut replacement still causes visible popping; high render FPS
alone is not evidence of smooth refinement.

November1 publication repair verifies the complete encoded provider output with
64KiB scratch rather than decoding and validating the just-built geometry again.
Matched Release replay retains identical mesh, patch, frame and stream bytes at
changed camera/water times; median provider time0.757 to0.611s. Cache/path safety,
four-worker scheduling, detail and budgets are unchanged. Native flight latency
and repeated whole-mesh GPU staging still need acceptance.

Initial bounded procedural clouds now exist in the optional compiled Planet Run
path. Seeded closed cloud geometry is immutable; wind follows simulation time and
body transforms. CPU D/R tests cover geometry, budgets, cancellation, time/session
restoration and host placement. Precise matrix meshes now transform lighting
normals with inverse-transpose scale/rotation, retaining the legacy Euler path.
Full Engine/native appearance is pending; static Editor cloud previews,
volumetric rendering and conservative cloud occlusion remain unfinished.

November 7 cloud integration: `asset.cloud_volume` owns a bounded, nonnegative
light-extinction recipe and CPU sampling contract. Optional `planet.clouds`
produces that recipe through the v5 project snapshot; older snapshots retain
their exact geometric-cloud compatibility. Existing projects need an explicitly
reviewed package-source update and regeneration to obtain v5 density data.
The host does not invent missing recipes from old cloud meshes.

`opengl.cloud_volume` implements an analytic density raymarch with scene-depth
clipping, bounded samples, half-resolution accumulation and depth-aware
composition before helpers/GUI. GPU resources remain context-owned; cloud time
can advance or reverse without rebuilding opaque geometry. Focused module,
shader and Debug/Release CPU checks pass. Complete Engine builds and native
appearance, occlusion, resize and GPU timing remain pending. Other native
density backends are not implemented. This is not a general 3D texture API or a
completed cross-renderer shader graph.

The analytic recipe does not allocate an editable voxel weather grid. Future
voxel-backed clouds must reuse `voxel.field`/`voxel.storage` with an explicit
conversion to nonnegative extinction: occupancy and `voxel.sdf` distance are
not automatically opacity or collision. That work remains open. No cloud layer
currently authorizes terrain occlusion or removal; keep fallback terrain ready.

November1 mixed-backend Run evidence separates scene drawing from presentation:
the OpenGL parent logs zero scene triangles while its Vulkan project is attached,
and resumes scene drawing after child retirement. The operator still reports
flicker and an uncapped Vulkan desktop stall; neither is accepted as repaired.
External attachment now applies the same child/sibling clipping styles as normal
engine-created native contexts, and restores the exact original styles on detach.
Actual-owner Debug/Release contracts and dependency validation pass. Native pixel
acceptance remains open; no WGL fallback, frame cap or GPU setting change is used
as a claimed fix. Both owned processes retired normally in that run.

November1 LOD allocation accepts optional body-local forward/up and aspect ratio.
From orbit it favors a broad central region. Near the surface it smoothly blends
toward three bounded surface queries: forward, 45 degrees down in the rolled
camera frame, and radial down toward the body centre. Earth's transition runs
from 100 km to 5 km ground clearance. Each march has a 16,384-step ceiling and
a conservative height-field derivative bound; exhaustion retains fallback
screen-error priority. These queries allocate refinement, not collision authority.
Six coarse roots, the balanced full-body cut and all memory limits remain.
No-up callers retain their original output. Exact v9 receipts echo orientation;
legacy providers retain v8. Camera turns, roll and resize retarget the existing
job. At 384 leaves, D/R CPU tests improve lower-view detail in 20/24 near cases
and radial detail in all 24, trading at most one centre level. Twelve orbital
cases retain broad coverage; threshold continuity and legacy output are tested.
Combined Debug/Release builds and full pure contracts pass. Native approach
latency and fine-ground quality remain unaccepted; this is not occlusion proof.
Provider jobs refill four bounded slots in deterministic commit order. Measured
worker activity is separate from parent Editor task counters. Verification and
transport remain part of measured latency; more threads do not remove those costs.
Within each provider step, cache validation and wanted-patch decoding now share
one bounded scheduler wave. Each path is still checked immediately before read;
all existing entries must validate before obsolete files can be removed. Matched
D/R export tests preserve every mesh, receipt and hierarchy byte. Alternating
Release warm pairs improve median1.083 to0.788 seconds at the same four workers.
This measures the provider, not complete native frame or camera latency.

Fine planetary snapshots use camera-local metre coordinates instead of normalized
global floats. The source frame stays transient; persisted authoring coordinates
remain unchanged. Actual wet LOD20 passes shared geometry validation at about0.4m
cell spacing under the existing budgets. This does not prove terrain-editing or
the combined ocean/vegetation visual target. Initial cloud geometry does not
establish that target; declarative
visibility-pass markers cannot be used to claim opaque coverage. Any future cloud
occlusion must be conservative, exclude translucent coverage and keep fallback
terrain ready when the view is exposed.

October31 planetary Play repairs keep the render origin transient. The saved
project pose and the provider's double-precision LOD camera stay unchanged. The
host subtracts one common origin before converting mesh positions to floats;
camera, lighting and geometry travel in one immutable publication. A rejected
or incomplete geometry publication keeps its last complete geometry and reports
the refusal, while an origin-safe camera update can still advance navigation.
Framed geometry omits unused legacy edge buffers; the 128 MiB limit remains.
Ordinary editor
views retain their existing path. Renderer consumers hold the selected packet
for their whole render turn, including Vulkan's buffers and draw counts.

Vulkan preview uploads now compare actual immutable geometry, wireframe eye and
the baked surface transform instead of packet revision alone. Camera uniforms
still update each frame. Failed uploads cannot commit the reuse key. CPU tests
prove repeated revision-only packets reuse buffers; native performance parity
remains open, so a faster second run does not establish this repair's benefit.

Preview publication normalizes indexed triangles to the existing clockwise
exterior convention using the source face/vertex normals. Source artifacts and
UVs stay unchanged; already-clockwise meshes keep their order. Ambiguous tangent
or cancelling normal evidence retains the existing clockwise convention. This
is not automatic repair of arbitrary inconsistent imported normals.

Each indexed mesh uses one source front-face convention. Reliable surface normals
establish it once before emission; nearly tangent skirt normals cannot flip single
triangles. Entirely ambiguous normals keep the existing clockwise fallback. Source
artifact bytes/UVs stay unchanged; reflected transforms retain their parity correction.
OpenGL solid, textured and wireframe passes enable back-face culling. Wireframe
uses the same triangles for its depth and polygon-line passes, rather than an
uncullable list of every mesh edge. The preview scope restores winding, cull mode
and polygon mode before GUI replay. DirectX, Software, Raylib, SDL, SFML and
Vulkan reuse the shared camera-facing triangle-edge visitor; helper gizmos remain
separate overlays. Non-OpenGL hidden-surface wire occlusion is still unverified.
The wire packet owns one combined line stream, helper lines and depth triangles;
there is no unused second full mesh-line stream. Admission counts shared source
artifacts once and still counts separately retained geometry. The128MiB ceiling
is unchanged. If a large indexed mesh's bounding box crosses the camera plane,
depth fitting checks its referenced vertices before choosing a near plane. Real
geometry crossings keep the conservative near plane. Native acceptance is separate
from these source/module tests.
The thin, axis-aligned Spawn solid has its actual emitted depth bounds; its
authored Y scale and rotation do not describe that proxy. Unsupported solid
primitives still use the conservative floor. The checked saved-planet scene now
uses24-bit target depth and a fitted near plane, removing the observed overview
seam strips. This is not acceptance of every primitive or close-camera gesture.
Bounded OpenGL evidence queries the current framebuffer's depth attachment using
the core-profile attachment API, not the removed GL_DEPTH_BITS query.
Actual module tests cover all six planetary mesh faces, CW imports, reflection,
UVs and source identity; recording tests verify both OpenGL passes and restoration.
Combined build and native acceptance belong to the active pass.

Optional planetary Water clips wave edges against the terrain triangles actually
drawn, rather than projecting shoreline crossings back onto an analytic sphere.
This keeps coarse water/ground boundaries on their source faces. CPU checks cover
coarse shoreline positions, skirts and parent interpolation. Native refined-mesh
acceptance remains open; invalid generated faces are refused, not admitted by
relaxing the shared mesh validator. Provider failures retain their stage, exit and
exact local request in the existing build log, excluding environment values.

Bounded OpenGL submission evidence counts actual mesh draw calls, submitted
triangles and upload bytes per frame (wire depth and line passes count separately).
These are workload counters, not measured GPU execution time or visible triangle
counts. Six one-second samples and two thirty-second samples avoid an unbounded
log stream. The displayed NVIDIA utilization remains whole-device usage; other
applications and GPU clock changes can affect it. Rising utilization alone does
not prove an allocation leak. Normalized mesh storage precision, editable terrain,
coherent planetary ocean/vegetation and native LOD transitions remain open.

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

## Windows OpenGL presentation

Acceptance is incomplete: the October31 mixed six-backend fixed30 run caused an
operator-reported desktop lock. Single-Editor VSync/normal-close and CPU/build
proof do not accept that path. Graphics probes are stopped during the ownership
repair; the current evidence and next gate live in `Changes/active_pass.md`.

WGL still creates and activates Windows OpenGL contexts. The historical native
path draws a back buffer and calls `SwapBuffers`; it was not an EpochGui feature
and did not originate in Linux parity work. That path remains available.

`opengl.native_present` chooses once per native context, with that context
current. Automatic selection uses DXGI only for NVIDIA contexts with GL3
framebuffer operations, the two required NV interop extensions, their entrypoints
and a double-buffered drawable. Other contexts keep native presentation;
non-Windows defaults are unchanged. Windows diagnostics may set
`EPOCH_OPENGL_PRESENTATION=auto|dxgi|wgl`. Invalid requests or unsupported forced
DXGI fail explicitly. A selected bridge never silently retries through WGL.

The bridge copies the finished color frame without CPU readback, then unlocks
the shared resource before DXGI presentation. It preserves GL read/draw bindings,
default read selection, scissor and sRGB state. Resize unregisters and releases
the old back buffer before replacement. VSync uses DXGI SyncInterval1;
non-VSync uses0 while the existing host limiter retains its configured policy.
Occlusion does not publish a first-present success. The native presentation
timing includes the complete handoff rather than claiming a `SwapBuffers` call.

The initial opt-in Release Editor passed an upright30FPS visual check, normal
close and a bounded foreground120FPS responsiveness check on RTX5080 per the
operator. Production automatic selection has separate CPU and integration gates;
do not transfer that native verdict to changed source or every GPU. Exact receipts
and remaining resize/VSync/multicontext checks belong in `Changes/active_pass.md`.

## October30 GUI submission repair — source and CPU proof

Current operator direction is conventional full-frame scene rendering. Source
removes the OpenGL cached/partial scene path, its exclusive damage bookkeeping,
and software whole-frame skip/reuse logic. Combined Debug/Release builds and full
pure contracts pass; the same Release still lagged in the operator's actual 120
retry. Native responsiveness and GUI/image acceptance remain open. The proposed
multi-frame animation cache was cancelled. Asset/font/texture residency and GUI command snapshots remain
separate; central events, temporal simulation and jobs remain required.
Neuromorphic rendering is future work and does not establish present capability.

`platform.work_admission` keeps GPU driver calls on one explicitly started
session worker. Status/UI/render reads only copy its cached result; CPU/RAM
sampling is independent. Refresh waits one second after completion. Results
older than five seconds, or queries lasting over five seconds, are unavailable.
Stop invalidates late results and precedes graphics teardown. A bounded stop
joins only a returned thread; an unreturning driver retains its joinable owner
and DLL until process exit. No detached worker can access destroyed state.
Telemetry-OFF builds create no driver worker.

Windows host observations cache DWM/DPI/window state for100ms and invalidate on
native events, resize or handle changes. WindowData logical dimensions are read
coherently under the registry mutex; native calls happen after releasing it.
Other Context dimension races remain open. First-paint invalidation is queued,
without synchronously entering the native owner from the WGL thread.

These repairs pass actual CPU owner tests and combined Debug/Release builds.
The operator's October30 model-free test remains a negative desktop result:
30/60FPS responsive,120FPS lag. Fast draw/present wall times do not prove
compositor responsiveness; pacing wall time does not measure thread CPU usage.
Do not mark desktop stability or native parity accepted from these source fixes.

OpenGL uses its existing two-frame completion bound for foreground uncapped
submissions. Capped/VSync modes retire prior fences without creating new ones.
This does not impose an FPS ceiling or prove desktop responsiveness. Uncapped
native acceptance remains stopped; the new presentation route does not waive it.

The sprite streamer now appends each texture run to a different part of one
context-owned buffer. It requests fresh storage once per native frame, shared
by the existing GUI replays and top layer. Empty GUI frames request none.
Storage starts at8192sprites and grows from the previous workload to a maximum
of65536sprites (6MiB per context). Excess sprites use the existing immediate
quad path; the storage budget never drops content. Frame/queue/overlay order,
atlas-version ordering and context-owned cleanup remain unchanged.

Actual CPU recording checks preserve geometry and ordering across both passes,
overflow, missing capabilities and two contexts. For5630sprites in182runs,
buffer-store API requests fall from182 to1; vertex payload and182draws remain
the same. These are recorded API calls, not measured driver allocations or GPU
performance. The earlier uncapped retest still froze Windows; the operator
stopped native probes. Desktop responsiveness and native parity remain open.

October28 planetary unit integration preserves optional canonical mesh units
through scene history/save/runtime and applies the same conversion to focus,
picking, camera inverses and ordinary runtime geometry/light frames. Shared3D
camera extent supports the larger overview without changing Canvas2D limits.
Actual-owner Debug/Release checks and Windows OpenGL25km native generation,
reopen, embedded Run and landed LOD1–15 pass. Ground material fidelity, patch-local
double precision, smooth transitions, huge-world and other-backend acceptance
remain open. Evidence: `Artifacts/Reports/planet-size-integration-oct28/`.

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
| Cached/partial scene rendering | Removed; build/contract proof | October30 operator direction selects the existing OpenGL conventional path and removes software whole-frame skip/reuse. Scene framebuffer caches, exclusive pending-damage queues, cache diagnostics, overlays and the SELECTIVE/FULL benchmark are removed. Protected queue/GUI/scene/presentation order remains. Focused CPU checks and combined Debug/Release builds/full pure contracts pass. The combined actual 120 retry still lagged; native image/responsiveness acceptance remains open. Ordinary resources, immutable GUI snapshots and Canvas2D derived raster/texture preparation remain; see [full-frame policy](event_render.md). |
| Editor scene solids | Partial | OpenGL, SDL3, SFML3, DirectX, and Software have accepted current orientation. Raylib and Vulkan have correction candidates awaiting operator eye proof. |
| Procedural forest preview | Partial | `ForestAssetDocument` compiles one deterministic morphology sample, renderer-neutral preview geometry, voxel LOD plan, and bounded occupancy. Plant Lab edits, publishes, and reopens one project-owned source/immutable Library pair; Forest Factory places that exact revision and package staging records matching source/compiler evidence. Authored branch count and node spacing affect morphology. Production projection admits finite nonzero segments from their actual length and endpoint radii, then renders oriented tapered eight-sided trunks/branches with a root-only trunk start cap and terminal-only branch end caps; leaf clusters retain deterministic orientation. A separate indexed `forest.preview_mesh` contract proves topology, taper, cap orientation, bounds, logical materials, leaf cards, determinism, budgets, and invalid-segment refusal. Editor and runtime publish through the common solid/selection stream consumed by OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX/D3D11, and Software. Native pixels, resize, teardown, repeated-switch proof, sparse voxel materialization, and production indexed mesh/impostor consumption remain open. |
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
| Physics | Partial | `physics.manager`, `physics.solver2d`, and `project.actor2d_runtime` prove stable bodies, deterministic fixed-step AABB/circle collision, layers/masks, bounded static maps, stable contacts, pause/reset, snapshots, map collision, and renderer-neutral actor publication. Authored one-way/slope/custom-box collision, staged editor controls, and exact source/Library/runtime restoration are build-proven (v0.89.23). Native control interaction and the full playable-project acceptance remain unverified. |
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

RTT is shared Engine infrastructure, not an Arcade subsystem. Logical resources
and pass ordering belong to `render.device` and `render.graph`; each backend/head
owns its native allocations and synchronization. Arcade supplies the game and
cabinet/screen content. Native handles are local to their creating device/context
and must never be treated as cross-driver image identities.

November1/2 ownership repair: the generic OpenGL hook registry now keys RTTs by
context and native color texture, rather than a device-local slot. Actual-owner
Debug/Release CPU tests cover two devices sharing slot1, repeated native IDs in
different contexts, wrong-context refusal, forged framebuffer records and
independent context retirement. Receipt:
`Artifacts/Reports/rtt-context-ownership-nov02/552150f4251e/result.json`.
This is not native multihead or playable-cabinet acceptance. The generic hooks
are exercised by contracts; live backends still have separate Arcade screen
paths. Wiring those consumers to the central service, binding compiled graphs to
their creating device, and proving resize/context-loss/game input remain open.
The resource-residency generation ledger does not automatically validate raw
device-local graph handles.

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

## Shared preview geometry and lighting updates

The shared preview solid path now retains immutable geometry in its existing
marker publication. Unchanged markers can reuse transformed/shaded vertices
while another body moves. The key includes the mesh content, transform, display
state, exact light/environment data and eye position when shading depends on it.
Backends acquire the shared geometry at their existing draw sites; frame and
overlay order are unchanged. Context retirement cannot publish a stale rebuild.
Additional cache construction/retention has a128MiB working-set admission check,
including old geometry and retained source artifacts. Larger already-admitted
legacy frames use the existing uncached path; this is not a bound on all renderer
memory or an FPS ceiling.

This reduces repeated CPU work. It does not provide GPU indexed residency,
native lighting, smooth terrain geomorph, or complete ground-world precision.
Cold geometry and view-dependent lighting still cost work when the view changes.
The planetary project runtime now sends this CPU preparation to its existing
task scheduler, matching the Editor's prepare/commit boundary. Each context
retains at most one in-flight display request. The frame owner only commits a
complete packet and keeps the last complete terrain navigable while work runs.
Body, water/cloud visibility and wireframe changes invalidate stale work;
project shutdown cancels and joins it before releasing its source snapshots.
The renderer can read a packet while the scene owner is still updating it.
A delayed geometry commit therefore carries the latest camera and cloud view
inside that same atomic publication. It does not expose the worker's old camera
and then wait for a second update to catch up. Camera-only progress made after source
capture is preserved by the existing two-argument commit as well. The shared
rebase math keeps the live view in the new packet's coordinate origin; unchanged
poses retain their fitted depth range. Moving views use conservative clipping
until their own fit arrives. Actual-module Debug tests reproduce the old backward
camera jump and pass with the repair; matching main builds and native movement
acceptance remain in the active pass.
Metric terrain export subtracts a body-local origin before narrowing positions
to floats. Near the planet this remains the camera position. A sufficiently
distant camera uses the shared bounded-origin policy in `project.planetary_snapshot`,
so an ordinary planet does not move outside the mesh codec's position limits.
The provider opts in through admitted source marker `epoch.planet.stream-origin.v2`;
the host checks the same policy. Older providers still require the exact camera
origin. Mesh units, water-origin metadata and coordinate restoration remain
unchanged; the codec still rejects invalid geometry. Actual provider/codec and
host-admission checks pass Debug/Release. Matching main/native acceptance remains
in the active pass; this is not support for arbitrarily large bodies.
Cooperative cancellation invalidates the retained request even if display policy
has already switched back, so the same desired frame can be retried. A genuine
input/admission refusal remains deduplicated. Bounded timing records distinguish
committed, cancelled, superseded, preparation-refused and commit-refused results.
Large mixed terrain/textured frames use the same compact joined untextured
stream as untextured frames. They keep each required textured UV/material stream,
without also retaining an untextured per-marker copy of the joined terrain.
Admission counts actual joined/textured capacity, scratch and retained snapshots
under the existing128MiB working-set ceiling. Tiny legacy frames keep their
existing per-marker reuse policy. This saves CPU memory; GPU submission bytes
and the backend draw order are unchanged.
Pending wireframe requests are acknowledged after publication or refusal,
rather than being rejected merely because CPU preparation is still queued.
The existing terrain stream worker also returns its exact source generation only
after final source and recipe verification. Runtime completion compares that
receipt with its admitted root, manifest and package hash; it does not repeat the
full disk inventory/hash on the frame owner. Missing or stale evidence retains
the previous terrain and reports source refusal separately from memory pressure.
This is bounded asynchronous verification, not an atomic filesystem lease.
Bounded runtime evidence separates scheduler wait, CPU preparation, owner commit
and total latency. Provider worker counts describe a separate generation stage;
they do not measure display preparation or GPU upload. Actual-module CPU replay
and cancellation/retirement proof are under
`Artifacts/Reports/runtime-planetary-publication-nov07/`. Matching native camera
travel, peak memory and hitch acceptance remain required; this does not claim
faster total generation or remove the backend's existing mesh uploads.
Native75's30FPS control sequence passes with four provider workers and no mixed
frame refusal. A separate actual47-file warm package check measured172.428ms for
the removed owner verification; this cost was outside the display-phase timings.
Source-receipt mutation/cancellation/retry evidence is under
`Artifacts/Reports/planet-source-admission76-nov07/`. Matching76 Development
Debug/Release and audited Public Release/full contracts pass. Native76 rebuilt
and docked the saved planet/forest at30FPS, reached LOD20 on four provider workers,
and completed orbit/land/pause/resume without source or memory refusal. Five
sampled display commits took0.975–2.591ms; provider work still took455–531ms.
Receipt: `Artifacts/Reports/native-combined76-nov07/result.json`. These are bounded
observations, not a percentile study, high-speed travel or all-backend acceptance.
CPU equivalence/allocation/lifetime tests and separate Debug/Release measurements
are in `Artifacts/Reports/runtime-preview-controls-oct28/`; native FPS requires
its own matched project/backend evidence.
The earlier CPU-reuse checkpoint passed Development Debug/Release Editor builds
and full contracts. Those receipts do not accept the new asynchronous publication
change. That earlier Windows Release OpenGL embedded-project/control smoke passed; landing still
exposes a ground-geometry defect. This does not establish a native rendering
speedup, ground precision, or fidelity to the original planetary demonstration.

Lighting source publication and geometry revisions remain separate from authoring
commands/history. October30 removes the exclusive per-context pending-damage
queues and their OpenGL partial-redraw consumer. Generic `render.lighting`
diff/coalescing primitives remain independently exercised by lighting, ambient
and planetary contracts. No retained pending-damage admission or live OpenGL
neuromorphic bridge is claimed. Combined Debug/Release builds/full pure contracts
pass; native responsiveness and image acceptance remain open after the failed 120 retry;
see [full-frame policy](event_render.md).

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
