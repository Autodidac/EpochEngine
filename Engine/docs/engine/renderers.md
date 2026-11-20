# Renderer Capability Evidence

## Purpose

This file records current renderer/resource evidence. It is not a second roadmap.
The canonical plan and tier definitions are in
`capabilities.md`; scheduling is in `Changes/roadmap.md`.

## Evidence Policy

Automatic Tier 4 startup tries one Vulkan context on Windows and native Linux.
An ordinary unsuccessful startup retires that attempt before trying OpenGL.
On Windows, an interrupted automatic Vulkan attempt before first present
makes future automatic launches of the same executable use OpenGL. A new
executable gets a fresh attempt. Explicit renderer choices and WSL's
single-context OpenGL startup retain their paths.
The November 17 Windows Release rendered the ordinary launcher on Vulkan and
completed an OpenGL-to-Vulkan Editor replacement in the same parent window.
Native fallback and Linux startup remain unverified. See the exact build and
switch receipts in the [active pass](../../../Changes/active_pass.md).

OpenGL and Android SDL GLES 3 consume the shared density shader. The Android
path draws the shared OpenGL atmosphere behind opaque geometry, then resolves
clouds before SDL overlays. Android 12 matched orbital captures show the
atmospheric limb, black space and unchanged opaque controls. SDL concentrates
its bounded orbital trace inside the projected cloud shell while resolving
against full-frame opaque depth. Stable ground and landscape recovery passed.
Vulkan draws the same density field with a full-resolution trace capped at
16 primary steps. Long rays viewed from inside the shell concentrate those
steps near the eye. CPU replay and shader staging verify the sampling change.
The existing second subpass reads the opaque depth and
composites clouds before Canvas2D and GUI. The D32 depth format, staged
shader pair and frame-owned constants must be available before the context
advertises density clouds. Windows Release Play has coastal motion and
shutdown evidence. Cloud appearance, standalone resize and Linux remain
separate acceptance checks. The project presentation check reports
Vulkan-in-OpenGL as unavailable. Project window embedding remains Windows-only.
[Android atmosphere evidence](../platform/android/android.md#package-a-saved-planetary-view).

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

The initial bounded procedural clouds in the optional compiled Planet Run
path use seeded immutable closed geometry; wind follows simulation time and
body transforms. CPU D/R tests cover geometry, budgets, cancellation, time/session
restoration and host placement. Precise matrix meshes now transform lighting
normals with inverse-transpose scale/rotation, retaining the legacy Euler path.
That geometric path remains compatibility behavior; the density integration
below supersedes it for admitted density recipes. Native appearance and
conservative cloud occlusion remain unaccepted.

November 7 cloud integration: `asset.cloud_volume` owns a bounded, nonnegative
light-extinction recipe and CPU sampling contract. Optional `planet.clouds`
produces that recipe through the v5 project snapshot; older snapshots retain
their exact geometric-cloud compatibility. Projects with older package source
need an explicitly reviewed update and regeneration to obtain density data.
The Editor consumes density metadata from its accepted streaming snapshot;
it does not invent missing recipes from old cloud meshes.

`opengl.cloud_volume` implements an analytic density raymarch with scene-depth
clipping, bounded samples, half-resolution accumulation and depth-aware
composition before helpers/GUI. GPU resources remain context-owned; cloud time
can advance or reverse without rebuilding opaque geometry. Focused module,
shader and Debug/Release CPU checks pass. The November15 full Development
Release Editor build and pure contracts pass for SHA-256
`DABC93B0B018965EF54924848E5221CC570EB57DDE2CF5506E429F3B8A74B5BC`;
`Artifacts/Reports/editor-reviewed-run-20261115/` owns that receipt. This binary
contains the broader weather-front/shear shader, and the showcase project's
47 attached Planet source files match the current Extensions owner and package
receipt. Native orbit/surface appearance, occlusion, resize and GPU timing remain
pending. Other native density backends are not implemented; wireframe deliberately
omits clouds. This is not a general 3D texture API or a completed cross-renderer
shader graph. `Artifacts/Reports/cloud-showcase-20261115/cloud-diagnosis.md`
records the copy's source, provider and display-admission evidence.

The shared preview renderer owns CloudVolume admission and publication.
`cloud_volume_view` also supplies the camera-relative sphere, body rotation,
wind phase, metric density inputs and precision checks for native consumers.
Project Play supplies the accepted recipe, transform and simulation time.
OpenGL owns shader compilation, scene-depth sampling, render targets and
compositing. Editor Preview and Project Play use the native preview capability
registered by the backend and copied by Context cloning. OpenGL registers
density support. Its live depth-target and precision checks still decide
whether a specific pass can draw. Other backends need native passes that
consume the same scene packet, with native composition checks.

The analytic lower deck uses the spherical weather contour for its footprint,
an independent body-relative field for the hanging base and finer billows at
the top. Dry regions leave clear sky and broken clusters. Wet regions bend
fronts through a bounded body-relative displacement. The lower boundary
narrows as the ray footprint resolves the cloud, and coarse samples retain
the broad profile. Strong lower cores gain extinction gradually with density. Thin
edges and the upper deck keep the recipe coefficient. The CPU transport and
both OpenGL fragment paths use this response for absorption. OpenGL also uses
it for each sunlight probe. Existing package recipes and snapshot envelopes
retain their formats.

Keep cloud weather in body coordinates. Observer altitude changes the ray
and its sample footprint. Fixed body position, time and footprints yield the
same density in either deck at all camera altitudes. Each deck uses one
coverage contour on the reference sphere to define cloud bodies and clear
gaps. Filtered regional moisture, wind-stretched clusters and smaller lobes
shape that contour. The lower deck thickens around each connected core and
narrows toward the rim from both above and below. Volumetric billows shift
the column and erode the contour without perforating dense cores. The thinner
upper deck has its own weather pattern, altitude band and shear.

November 17 cloud depth fallback: a reviewed Planetary provider can deliver a
conservative inner terrain radius through density v2 and snapshot v6. OpenGL
clips the cloud view ray at the nearer of that sphere and sampled opaque depth.
The eclipse still uses the nominal planet radius. The provider keeps the v5
density and geometric compatibility routes. Standalone producer/consumer and
renderer checks, transport admission, and all three GLSL stages passed. Native
OpenGL regenerated the showcase from the reviewed provider. The settled
below-cloud washout still reproduces in Solid and Shaded. Wireframe changes
both geometry presentation and cloud composition.
See `Artifacts/Reports/cloud-inner-occluder-20261117/` and the combined build
receipt in the active pass.

Cloud stream admission now checks the system-space mesh receipt against the
body-local stream origin and normalizes the cloud center to the body-local
terrain frame. Native OpenGL shows aligned orbital terrain and clouds. At the
fixed close pose all five colors and depths matched from opaque terrain through
cloud composition. That camera faces a nearly level, snow-dominant plateau.
The six-metre body-local vertex detail raised sampled pre-light luminance range
from 0.000103 to 0.05737 on the resolved close tile. Debug and Release agreed on
positions, normals and shared face/LOD colors. The isolated showcase still needs
the reviewed provider update and matched Shaded captures to verify appearance
and distant filtering. Evaluate shoreline and water LOD with their own coastal
pose. The active pass links the native and CPU receipts.

November17 native showcase follow-up: GameProject01 Release runtime SHA-256
`B1D9F717F3C47B274E9297E743F5E5CE732CF4955BE7124D613499EC272AE949`
loaded terrain and ocean. The initial distant dot was a restored exploration
camera about 75 million metres from the body centre, not evidence of missing
planet admission. Operator review reported better cloud shape but excessive
overcast and inconsistent surface/orbit coverage. This is partial visual feedback,
not cloud acceptance. Preserve orbit-to-ground weather continuity, thick layered
interiors, stable depth/LOD and resize, coherent sun/day-night/atmosphere, and
climate/axis/season-aware polar placement as open gates. Ground material detail,
water transitions and terrain refinement require their own proof; clouds must
not hide unresolved geometry defects. Evidence is under
`Artifacts/Reports/cloud-showcase-20261115/data/Projects/GameProject01/Artifacts/Reports/`
and the current acceptance record in `Changes/active_pass.md`.

**Planetary atmosphere.** The planetary body supplies atmosphere height in metres. The preview scales
it with the body radius and carries it independently of cloud coverage.
Explicit zero describes an airless body. An older provider that omits height
retains the cloud-derived fallback.

The shared preview resolves the visible body, normalizes the rebased camera
to that body and carries the same sun direction to OpenGL and Vulkan. OpenGL
and Vulkan both draw the analytic background before opaque geometry. Windows
Vulkan has native daylight, sunset, orbit and resize proof in the active pass.
Cloud volume, stars and haze over opaque geometry need separate backend proof.

For the sunset check, hold a coastal camera fixed and move the sun
from daylight through the horizon into night. Check for a legible sun disk,
gold near the sun, a pink-red horizon and a cooler upper sky. Then verify
the opposite horizon, the orbital limb, a clear atmospheric world and an
airless moon. Surface haze needs its own coastal check across textured
terrain and water. For direct daylight, use a low sun still above the horizon.
Look for a pale blue sky grading into warm glare around the sun, warm direct
light, and cool, readable shadows. Haze should soften distant terrain while
nearby materials retain contrast. Check with the sun in view and again with it
occluded by foreground geometry so glare follows visibility. Keep this capture
separate from the sunset and orbital checks.

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

Run validates the requested presentation before building and before launch.
An unsupported In Editor request explains the missing host or unaccepted backend
pair instead of silently launching outside. Separate Window remains an explicit
choice. The standalone Editor lacks a docking parent; the parented Editor uses
the existing child-window attachment path for supported backend pairs. Vulkan
inside an OpenGL host retains its stability gate pending native acceptance.

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
OpenGL solid, textured and wireframe passes enable back-face culling. With GLSL
3.30 or newer, wireframe rasterizes filled triangles in both passes. The color
pass shades edges at the exact visible depth and fades subpixel triangles to
avoid solid-looking patches. The legacy shader uses a polygon-line pass. The
preview restores winding, cull mode, polygon mode and blend state before GUI
replay. Software uses its clipped opaque raster for a depth pass and shades
original mesh edges at the same pixel depth. Its CPU check covers overlapping
wire surfaces in both submission orders, occlusion by solid faces and clipping
without invented edges. The Linux Software runtime switched a saved Platformer
scene from filled faces to wire edges in the offscreen native test. Exact
grazing-angle Editor and cross-backend timing checks remain open.
DirectX, Raylib, SDL, SFML and Vulkan still use the shared camera-facing
triangle-edge visitor. Helper gizmos remain separate overlays.
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

Optional planetary terrain/water are composed by Extensions, not mandatory
world services for GUI-only projects. Current owners are
`Engine/dep/EpochEngineExtensions/packages/planetary_terrain/modules/planet.streaming.ixx`,
`water.planetary.ixx` and `tools/planet.stream_export.cpp`; reusable sparse voxel
contracts remain under `Engine/modules/voxel`. Heavy FFT ocean is separately
owned by `packages/fft_ocean/modules/water.fft.ixx`.

`water.planetary` clips waves against the drawn terrain triangles and preserves
skirts/parent interpolation instead of projecting shoreline crossings onto an
analytic sphere. Existing CPU checks cover these contracts; native refined-mesh
acceptance remains open. Invalid faces are refused by the shared mesh validator.
Both initial water generation and animated reconstruction use the same
shallow-breaker rule. Sampled crests brighten in shallow water. The extra
foam fades between 1.75 and 2.75 m vertex spacing, and clipping interpolates
the sampled edge color. At a fine-to-coarse join, the fine patch blends its
foam weight to the neighbor over three rows. This has its own weight because
both sides can still resolve sun glints. Generation and resampling use the
same fade at their admitted spacing.
The provider and host resampler use one wet polygon rule. A zero-depth
sample is outside the wet polygon, and each edge crossing interpolates
from its wet endpoint. A parent-to-child transition must interpolate the
parent triangle's signed depth and water attributes before clipping the
child lattice. Moving previously clipped child vertices keeps the child's
wet topology and can expose a different coastline at the parent endpoint.
Provider logs retain stage, exit and exact local request, excluding environment
values. Authoring/edit durability and package extraction are separate gates;
see [voxel/world ownership](voxels.md#source-of-truth-and-representations).

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
| SDL3 | SDL window and renderer adapter | Accepted Windows launcher/loading/editor, high-DPI input/resize, and clean close plus native Canvas2D texture/presentation source builds | Sampled-RTT/Canvas2D reference pixels and repeated-switch/native-memory soak remain `Partial` |
| SFML3 | SFML window and OpenGL context adapter | Existing window/GUI/RTT evidence plus native Canvas2D texture/presentation and capture-only `copyToImage` evidence source builds | Pixel parity, resize, and repeated-switch proof remain `Partial` |
| raylib | raylib window and OpenGL context adapter | Existing scene/ownership repairs plus exact native-context Canvas2D texture/presentation and capture-only screen-image evidence source builds | Pixel parity and single/multicontext lifecycle proof remain `Partial` |
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

For a sustained CPU Software profile, launch a verified project with
`--backend software --frame-limit unlimited` and set
`EPOCH_SOFTWARE_FRAME_TIMING=1` in that process. The Software log records
five-second foreground samples at a fixed framebuffer size, up to 12 per
context. Each reports throughput and mean time for refresh/clear, scene,
GUI/queue, capture and presentation. Samples restart when focus or framebuffer
size changes. Windows reports only fully transferred framebuffer rows.

| Feature family | Status | Evidence and boundary |
| --- | --- | --- |
| Backend-neutral capability selection | Partial | `capability.profile` defines tiers, evidence, subsystem profiles, project requirements, deterministic selection, and contract checks. Integration through runtime settings/reporting still needs build proof. |
| Window/context bootstrap | Present | OpenGL, SDL3, SFML3, raylib, Vulkan, DirectX/D3D11, software, and noop/headless paths exist. SDL3 has accepted 150%-DPI Windows bootstrap and one clean native close; runtime evidence remains backend-specific. |
| Frame pacing/present policy | Partial | `perf.tier` resolves explicit target-Hz, VSync, and uncapped requests plus foreground/background/minimized policy into one configured/effective plan. Both managed host loops apply that plan before the unchanged backend frame body and retain the shared deadline limiter when native pacing is unavailable or cannot honor a CPU-paced target. OpenGL, DirectX/D3D11, and SDL3 own native VSync enable/disable; SFML3 owns native VSync and target-Hz; raylib owns native target-Hz/uncapped; Vulkan requests FIFO for VSync and Immediate then Mailbox then FIFO for non-VSync, applies changes only through pre-acquire swapchain recreation, and reports native pacing active only after the selected mode is accepted; Software truthfully remains shared-CPU-only. Deterministic 60/120, uncapped, 30-Hz background, 10-Hz minimized, native-success/fallback, present-mode selection, and pending/accepted lifecycle contracts plus MSVC Debug/Release builds pass. Linux background focus classification remains unclaimed, and no native timing/pixel evidence is added. |
| Frame clear/present | Partial | `render.context_frame` resolves one overflow-safe clipped editor viewport, top-left and bottom-left coordinates, normalized viewport, projection aspect, protected overlay/capture order, and deterministic semantic signature. OpenGL, SDL3, SFML3, raylib, Vulkan, DirectX/D3D11, and Software consume the shared plan through their existing viewport/scissor/projection paths without changing queue, overlay, capture, or present boundaries. SDL3's registered and standalone paths now share one native capture-before-present result, release failed capture reservations for a bounded retry, and publish first-present evidence before the managed host marks the backend ready; MSVC Debug/Release builds and `context.sdl_first_present_readiness` pass. SDL3 launcher, replay-backed loading, editor, resize, and clean close remain accepted; native all-context resize/GUI/modal/replacement pixels remain regression-sensitive and unproved by this build-only parity slice. |
| Editor scene lines/helpers | Present | Grid, markers, camera, and helper geometry exist across active editor lanes. |
| Cached/partial scene rendering | Removed; build/contract proof | October30 operator direction selects the existing OpenGL conventional path and removes software whole-frame skip/reuse. Scene framebuffer caches, exclusive pending-damage queues, cache diagnostics, overlays and the SELECTIVE/FULL benchmark are removed. Protected queue/GUI/scene/presentation order remains. Focused CPU checks and combined Debug/Release builds/full pure contracts pass. The combined actual 120 retry still lagged; native image/responsiveness acceptance remains open. Ordinary resources, immutable GUI snapshots and Canvas2D derived raster/texture preparation remain; see [full-frame policy](event_render.md). |
| Editor scene solids | Partial | OpenGL, SDL3, SFML3, DirectX, and Software have accepted current orientation. Raylib and Vulkan have correction candidates awaiting operator eye proof. Android SDL submits the shared opaque scene batch to GLES when the window has a usable depth attachment. It falls back to SDL geometry if that path is unavailable. The Android 12 emulator fixed-shoreline capture verified the depth result and UI after surface recovery. |
| Procedural forest preview | Partial | `ForestAssetDocument` compiles one deterministic morphology sample, renderer-neutral preview geometry, voxel LOD plan, and bounded occupancy. Plant Lab edits, publishes, and reopens one project-owned source/immutable Library pair; Forest Factory places that exact revision and package staging records matching source/compiler evidence. Authored branch count and node spacing affect morphology. Production projection admits finite nonzero segments from their actual length and endpoint radii, then renders oriented tapered eight-sided trunks/branches with a root-only trunk start cap and terminal-only branch end caps; leaf clusters retain deterministic orientation. A separate indexed `forest.preview_mesh` contract proves topology, taper, cap orientation, bounds, logical materials, leaf cards, determinism, budgets, and invalid-segment refusal. Editor and runtime publish through the common solid/selection stream consumed by OpenGL, SDL3, SFML3, raylib, Vulkan, DirectX/D3D11, and Software. Native pixels, resize, teardown, repeated-switch proof, sparse voxel materialization, and production indexed mesh/impostor consumption remain open. |
| Canvas2D planning/runtime | Partial | `render.canvas2d` proves project settings, camera/viewport mapping, logical sprite materials, deterministic quad batching, tile descriptors, immutable submissions, offscreen/final-compose plans, diagnostics, and editor policy persistence. `render.canvas2d_cpu` proves deterministic RGBA8 reference output. `render.canvas2d_runtime` now owns immutable scene/frame/raster reuse for OpenGL and the other active backend adapters; a revision-only publication advances generation and frame evidence while deterministically retaining identical content and Canvas2D hashes. `render.canvas2d_presentation` proves full-frame identity, byte-derived artifact keys, bounded residency, explicit image/surface packets, and staged native dispatch. `project.texture_library`, `project.texture_pipeline`, snapshot format 3, and the Assets controller prove serialized Library persistence, authenticated reopen, semantic material restoration, and owned editor resource closure. Earlier approved OpenGL capture matches T0-CPU across 1,178,872 pixels with zero error; this session-parity repair adds no new native-pixel claim. SDL3, SFML3, raylib, Vulkan, DirectX/D3D11, and Software adapters build in Debug/Release; their live pixels/switch cycles and the Assets interaction automation remain unproved. |
| Tilemap authoring/runtime | Partial | `authoring.tilemap` proves stable source identity, sparse chunks, semantic operations, bounded history, and deterministic compilation. `asset.tilemap_artifact` owns the runtime schema, integrity, collision/object payloads, and bounded serialization. `project.tilemap_library`/`project.tilemap_pipeline` prove exact atomic Library publication/restore and project registry identity. `render.canvas2d_tilemap` proves visible culling, animation selection, transforms, stable sprite ordering, collision, and object output. The EpochGui workspace, canonical source persistence, scene binding, exact Library publication/reopen, and generated-runtime preparation are build-proven; approved live editing/pixel evidence and cache-loss external-run proof remain incomplete. |
| Renderer resource spine | Partial | `render.device`/`render.graph` describe logical buffers, textures, validated upload regions, samplers, shaders, pipelines, materials, meshes/models, render targets, bindings, commands, passes, and graph dependencies. `render.texture.residency` proves bounded generation-checked logical-artifact reuse, eviction, pinning, upload budgets, stale-handle refusal, backend epochs, recreation, and metrics. Native parity is incomplete. |
| Sampled render-to-texture | Partial | Engine Arcade uses one shared content contract with backend-owned scene surfaces in OpenGL, SDL3, SFML3, raylib, Vulkan, DirectX, and Software. Build contracts pass; visual presentation and generic device-spine parity remain incomplete. |
| Texture mapping and residency | Partial | `authoring.texture` produces validated dense RGBA8 artifacts through the runtime-owned `asset.texture_artifact` schema/serializer/validator. `asset.texture_import` adds bounded uncompressed BMP, TGA, and P6 PPM decoding. `project.texture_admission`, `project.asset_registry`, `project.texture_library`, `project.texture_pipeline`, and `project.texture_resources` authenticate identity, atomically persist/reopen compiled bytes, publish bounded T0-CPU bindings, restore exact revisions, and optionally acquire disposable residency. Cache recreation, eviction, pinning, backend epochs, upload accounting, stale-handle rejection, portable path collision refusal, semantic snapshot save/reopen, and exact immutable scene closure are build-proven. Persistent rename/move migration, interactive UI proof, sRGB/compressed/mip-chain execution, atlas, bindless, sparse, and streaming remain incomplete. |
| Material semantics | Partial | Material handles and named logical texture slots flow through graph/device contracts. Portable sprite material, sampler, alpha, cutout, and color-space declarations exist; backend shader/blend consumption and broader PBR/normal/ORM semantics remain incomplete. |
| Camera and transforms | Present | One renderer-neutral stable view descriptor carries scene, purpose, projection, orientation, clipping, and revision state. The editor owns perspective, free orthographic, six locked axis views, independent framing, LMB selection, Alt+LMB orbit, MMB pan, Alt+RMB dolly, RMB fly with WASD/QE and speed modifiers, Focus, and Reset. Authored project-camera persistence, oriented axis grids, mobile/console bindings, and live all-context eye proof remain follow-up. |
| Portal and mirror views | Partial | Stable portal identity, shared camera descriptors, recursive cycle/depth/pixel budgeting, clip-plane intent, logical RTT outputs, disposable physical cache state, metrics, and render-pass view bindings are contract-proven. Native oblique/stencil clipping, traversal, presentation, and live backend pixels are not yet present. |
| Lighting | Partial | `render.lighting` owns stable directional/point/spot lights, bounded frames, environment state, metrics, Euler conversion, and reference raster evaluation. The shared solid preview lights both leaf faces consistently and computes per-tree self-shadowing from the strongest directional light. OpenGL uses the admitted foliage cutout for coverage. Other solid-stream drivers use the forest mesh and its fallback colors. Native light buffers, PBR, scene-wide shadow maps, and native pixel acceptance remain open. |
| CPU ray/spatial queries | Partial | `render.ray` owns validated AABB, sphere, triangle, scene, and voxel-DDA reference queries with explicit status/metrics. Editor selection consumes persistent scene identity through that query spine. Hardware ray query/RT are Missing. |
| Picking and Focus | Present | Editor picking resolves through persistent scene IDs and shared ray queries; Focus changes the active preview camera across presenting child contexts. Higher-tier GPU picking remains optional work, not the accepted baseline. |
| Physics | Partial | `physics.manager`, `physics.solver2d`, and `project.actor2d_runtime` prove stable bodies, deterministic fixed-step AABB/circle collision, layers/masks, bounded static maps, stable contacts, pause/reset, snapshots, map collision, and renderer-neutral actor publication. Authored one-way/slope/custom-box collision, staged editor controls, and exact source/Library/runtime restoration are build-proven (v0.89.23). Native control interaction and the full playable-project acceptance remain unverified. |
| Audio | Partial | Logical clips/sources/buses/listener/scheduling, deterministic stereo mixing, a physical-device boundary, and generation-checked project playback sessions are contract-proven. The optional SDL3 device remains renderer-independent across context replacement; decoded project assets, authored cue bindings, editor controls, and approved live physical-output proof remain incomplete. |
| Sparse voxel/water | Partial | Deterministic sparse voxel storage and explicit-time analytic water queries/projection exist. Generation/residency, mesh/render resources, buoyancy, and native presentation are Missing. |
| Text and GUI | Partial | Engine GUI rendering and expanded EpochGui portable control primitives exist. Engine-wide integration, professional docking, complete text behavior, and portable profile exclusion remain work. |
| Debug/evidence surfaces | Partial | Logging, Systems/System Info, FPS, screenshot capture, build-safe contracts, and deterministic Canvas2D CPU/native pixel comparison exist. One bounded caller-buffer coordinator normalizes origin, channel order, padded native row pitch, and destination stride while refusing absent contexts, excessive capacity, or native readback failure. OpenGL, SDL3, SFML3, raylib, DirectX/D3D11, and Vulkan invoke it only for explicit capture evidence; normal frames do not pay that readback cost. D3D11 stages and maps the composed scene viewport before GUI/top-layer/present. Vulkan stages and fence-synchronizes the native Canvas2D upload before frame command recording because its Canvas2D and GUI draws share one protected render pass; final Vulkan composed-surface comparison therefore remains open. Approved live capture, capability-driven settings, GPU markers, and complete cost visibility remain work. |

Canvas2D native composition now has one deterministic admission policy for
top-left RGBA8, linear color, premultiplied alpha, clamp-to-edge sampling, and
explicit nearest/linear selection. OpenGL, SDL3, SFML3, raylib, Vulkan, and
DirectX/D3D11 consume that policy before their backend-owned filter/sampler
selection; native blend, UV, resource calls, and draw order remain backend-owned.
Software consumes the already-composed deterministic CPU presentation surface
and retains its premultiplied clipped-blit contract. This is source/build
evidence only and adds no pixel claim.

Windows Software startup skips the hidden WGL bootstrap. The host creates
one shared WGL context when it first needs a native OpenGL window, including
late OpenGL attachment and replacement. Debug and Release recording fixtures
exercise the host branches and failed-bootstrap cleanup. Native Windows
docking and real driver behavior still need proof. The Windows desktop can
use its GPU to composite the CPU-rendered Software window.

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
| raylib | Present | Present | Partial | Partial | Missing | Partial | Partial |
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
SFML3 crops `copyToImage`; raylib crops its RGBA8 screen image. Each adapter runs
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
  Project Play suppresses the shared authoring grid, axes and camera focus mark
  for its context independently of camera mode. Returning restores the guides
  with the Editor camera. Visibility changes also invalidate buffered preview
  geometry, including Vulkan's combined scene and guide buffers.
- SDL and Software clip opaque preview triangles against the homogeneous
  view frustum before perspective division. SDL submits the surviving polygon
  as a triangle fan. Software retains its per-pixel depth and original-edge
  masks for Wireframe.
- Raylib filters camera-facing back sides before projected fill.
- DirectX orders its editor-preview geometry as background grid, solids, sampled
  surfaces, then foreground helpers so grid lines cannot composite over terrain.
- Software depth-tests scene-preview triangles in a viewport-sized CPU buffer.
  Perspective interpolates reciprocal camera depth to retain depth precision
  at planetary distances. Orthographic uses normalized clip depth.
  The grid, sampled Arcade screen, helpers, Wireframe, Canvas2D and GUI retain
  their existing draw order. Debug and Release pixel fixtures verify crossing
  surfaces in both orders and triangles straddling the near plane.
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
| Orientation | Shared top-left Canvas2D contract and accepted scene-solid orientation for OpenGL, SDL3, SFML3, DirectX, and Software | Partial: raylib and Vulkan require current-candidate eye/pixel proof |
| Alpha, cutout, and nearest/linear sampling | T0-CPU raster contracts and native adapter semantics exist; OpenGL matches the reference image exactly | Partial: equivalent SDL3, SFML3, raylib, Vulkan, DirectX, and Software presentation evidence is missing |
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

## Shader and material ownership — November 17 audit

The requested rock/grass detail, relief, fine water waves, reflections, depth
and shoreline foam are visual acceptance targets, not evidence of implemented
material parity. Extend existing material/resource owners before adding another
shader system.

| Lane | Current source/build owner | Limit |
| --- | --- | --- |
| OpenGL | `Engine/modules/opengl/opengl.preview.ixx`, `opengl.quad.ixx`: backend-owned GLSL compiled with `glShaderSource`; legacy and modern variants coexist. `opengl.cloud_volume.ixx` owns the density-cloud pass. | Preserve older-context fallbacks; cloud/native material appearance requires its own eye-test. |
| Vulkan | `Engine/assets/vulkan/shaders/*.glsl`; CMake stages eight scene, GUI, atmosphere and cloud SPIR-V files. The cloud pair uses the shared density shader and has checked-in Vulkan 1.0 fallbacks. | The Windows Release route verifies native cloud composition on D32. Quality and Linux parity remain open. |
| DirectX | `Engine/src/renderers/directx/directx.device.cpp` compiles embedded HLSL through D3DCompile for the current D3D11 adapter. | D3D12 and its pipeline/root-signature integration remain future work. |
| Software | Existing CPU scene/Canvas2D raster owners implement explicit supported semantics. | No native shader compiler or automatic GPU-shader translation is established. |

`Engine/modules/render/render.device.ixx` already owns ShaderDesc, PipelineDesc,
MaterialDesc and named base-color/normal/roughness/metallic/emissive/opacity slots.
Declared slots and `normal_mapping_ready` are not proof that every backend
consumes them. Keep authoring/asset/project identity renderer-neutral; native
shader binaries and residency remain disposable backend resources.

A future shared HLSL lane can be evaluated through
[DXC](https://github.com/microsoft/DirectXShaderCompiler) and
[Vulkan HLSL support](https://docs.vulkan.org/guide/latest/hlsl.html), which requires
compilation to SPIR-V. This is an option, not a selected migration: preserve
current GLSL, D3D11, legacy OpenGL and software compatibility until equivalent
resource bindings, builds and pixels are proven. No new compiler is required by
this documentation change.

**Next bounded material gate after clouds:** trace one normal-textured rock or
grass material through existing logical slots, asset/project admission and the
actual preview consumer. Identify the first dropped binding/tangent/light input
before changing it; prove a small lit normal-map sample and fallback in the same
scene. Relief/displacement is separate from normal mapping. Extend equivalent
native consumers only after that causal owner is known. Water's existing
`water.planetary` wave normals and shoreline geometry do not establish fine-wave
shading, reflections, depth absorption or foam: retain those as separate
material/pass acceptance gates, with camera-distance stability and shoreline
continuity. Existing cloud/water CPU contracts remain useful geometry/data proof;
no tests or new native visual verification were run for this source audit.

November17 source inspection narrowed the OpenGL mesh path:
`opengl.preview.ixx::world_texture_shader_sources(mesh=true)` samples one
`uTexture` and multiplies by `vColor`; its mesh setup binds position, UV and
color attributes only. There is no normal-map sampler or tangent/normal input
in this path. This establishes a missing material consumer, not that every
terrain/water draw uses it. Trace the selected showcase draw before extending
the shared contract; keep native appearance and backend parity unaccepted.
The upstream `render.preview_grid.ixx` indexed-mesh path computes
`shade_surface` per vertex, then stores position/color/UV in `TexturedVertex`;
it does not pass the transformed normal to this shader. Adding a sampler alone
therefore cannot supply per-pixel normal-mapped lighting. Preserve the existing
CPU-lit fallback while extending the owned material/vertex path deliberately.

The shared planetary preview bakes a low-cost sky reflection on all water and
uses the existing per-vertex resolution fade for rough sun glints. The
November 18 Windows Debug and Release contract verified unchanged land
vertices and water positions, coarse glint suppression, and the same broad
reflection across fine and coarse ranges. A matched Verdant sunset capture
verified the change in desktop OpenGL. Animated fine ripples, close-shore
foam, reflected scene objects, and higher-tier performance remain open.
Use the same Verdant shoreline route to qualify water from T0 through T5.
Shoreline placement, water level and depth-driven color are shared across
tiers. Extra surface detail fades as its screen footprint becomes unresolved.
In OpenGL and Vulkan, short ripples also fade between 40 and 135 metres of
eye-to-surface distance. Longer swells fade between 75 and 210 metres.
Shared shoreline shading confines depth-limited whitewater to the breaking
front and fades it behind the crest. A matched Windows OpenGL approach
confirmed a smaller bright area in the shallows at the same water level and
time. Mobile and moving-camera qualification remain open.

| Tier | Water acceptance target |
| --- | --- |
| T0 | Deterministic CPU geometry, shoreline clipping and broad water color. |
| T1 | Portable animated surface with stable shoreline shape and transitions. Include GLES on a phone. |
| T2 | Resolved wave normals and glints. Keep the broad water color through mid-altitude LOD changes. |
| T3 | Add capability-qualified shallows, breaking-wave foam and raster reflections within the measured frame budget. |
| T4 | Add scene reflections where inline ray queries meet the budget. Keep the validated raster fallback. |
| T5 | Evaluate full ray-traced reflections against the same raster reference. |

Select optional effects by reported backend capability, project policy and
measured cost. An application's FPS preset is independent of this ladder.
Compare matching passes at orbit, approach, shoreline and the shallows.
Check for LOD seams, stable color, continuous highlights and frame-time
spikes during motion. Record native timing before accepting a quality level.
The tier rows are acceptance targets. The shared raster water path is the
fallback for T4 and T5 until a native scene-reflection path passes the same
route and its frame-time budget. Keep the same authored waterline and depth
color when switching renderers or tiers.
The T1 phone check depends on the [Android host and packaging path](../platform/android/android.md).
Reuse the existing project water worker and shoreline geometry when that path
can build, install and report a native graphics context.
A resolved water range in a terrain log only establishes glint eligibility.
The OpenGL 3.3 preview consumes bounded water-only ranges in the existing
opaque stream. The depth surface and CPU fallback remain available. A
body-anchored phase keeps detail fixed across origin changes, and the pixel
footprint filters unresolved waves. The latest shared detail packet samples
twelve directional crests in three wavelength bands. The OpenGL and Vulkan
shaders use the three longer phases to vary crest groups and shade visible
swell. Their water-only reflection adds sky color by reflection angle and
narrows sun glints while suppressing direct highlights below the horizon.
Vulkan consumes the same packet and marks water in its existing opaque stream.
Debug and Release shared
renderer checks, Vulkan geometry checks, and exact shader compilation passed.
Native Windows OpenGL and Vulkan both rendered the same fixed shallows pose
at 0 and 1.25 seconds. OpenGL reproduced both paused frames byte for byte.
Vulkan rendered animated water before the atmospheric background was added.
The later Vulkan captures include daylight and sunset. Matched controller
flights on Windows OpenGL and Vulkan showed a shared late ripple transition
near 4 m, with stepped shoreline edges. The follow-up build retained ripples
through the largest measured settling-frame changes on both backends. The
coast and reflective highlights still shift as detail changes. Physical
travel, shorter flicker and measured frame cost remain open. Foam and
reflected scene objects require
their own validated inputs. The [active pass](../../../Changes/active_pass.md)
links the captures and receipts.
The ordinary Project Play packet currently enables ripple detail by surface
identity, scale and distance. Before assigning T2 or T3 quality, connect the
existing project admission and backend capability report to that packet,
including a measured frame budget. Keep the portable animated surface
available when advanced detail is outside the budget.

The shoreline footprint regression found that Vulkan differentiated the
warped position inside a water-only branch. Vulkan and OpenGL now take the
position derivatives before branching on per-primitive coverage or distance. The
analytic warp gradient filters the same body-relative waves on each backend,
and unresolved waves skip the crest calculation. Both exact fragment shaders
passed the offline SPIR-V check. Compare the same moving shore and grazing
views on the rebuilt runtime before accepting the visual result.

The shared view packet carries the current ripple phase during Project Play.
The CPU mesh keeps the time at which its shoreline was reconstructed. Camera
updates and delayed geometry commits use the latest view time without
rescheduling geometry. A validated water mesh can carry ripple detail while
its CPU resample is pending. The renderer contract checks forward and reversed
phase updates against the same retained geometry and checks that a delayed
commit uses the latest time.

For supported terrain refinements, the existing project worker prepares up
to four intermediate wet cuts at one retained provider sample. It projects
the parent triangle's signed depths and appearance onto the child grid before
clipping the shoreline. Play displays the complete sequence on the terrain
cadence and restores the previous cut if the view or clock becomes invalid.
When staged meshes fill the retained-output budget, the next batch retries
the prepared phase after their retirement. The budget and source checks still
apply, and only materialized phases count against the playback steps.
It can publish the next admitted cut directly when the source clocks,
neighbor topology or memory budget prevent a staged transition. The retained
CPU fixture checks the footprint and shoreline lighting. Use the matched
OpenGL and Vulkan route to qualify the result on screen.

When staged shoreline playback retires during ordinary playback, Play
advances the CPU shoreline sample toward the current timeline in steps of
at most 0.75 seconds per published mesh at normal speed. Each step evaluates
the provider at its selected time. A pause or seek still requests the
absolute sample, and the shader keeps the current view phase throughout.

The November 17 orbital reference adds a second view to planet acceptance.
Use the same seed, time and sun for an orbital still, a short descent and
a shoreline still. Compare atmospheric limb and night-side falloff, cloud
organization and clear regions, ocean/land separation, surface material
detail and shoreline highlights. For worlds with optional rings, check the
planet's occlusion of the far ring and the near ring's coverage of the planet.
Repeat with the same camera path to spot LOD seams and temporal flicker.

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
A delayed atomic geometry commit carries the latest camera/cloud view, including
camera-only changes after capture via the existing two-argument commit. Shared
rebase math preserves the live pose in the new origin; unchanged poses retain
fitted depth, moving views use conservative clipping until refitted. Actual-module
Debug checks reproduce and repair the backward-camera jump. Matching main builds
and native movement acceptance remain in the active pass.
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
