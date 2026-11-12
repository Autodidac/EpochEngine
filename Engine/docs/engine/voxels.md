# Connected voxel/SDF world architecture

## October26 package boundary

SDF terrain, hybrid voxel terrain and planetary-world functionality belong in separate optional Extensions packages. Share explicitly owned dependency-light contracts where needed; do not activate terrain, world streaming, meshing or model services merely by starting Engine. Supplied EpochPlanet is an implementation source to reconcile, not a second mandatory engine loop. AI chat and other AI capabilities remain separately selectable. Existing in-engine SDF primitives are a tested foundation; full package extraction, registration and independent project consumption remain acceptance work.


October25 approved direction: one reusable world architecture for editable voxel/
SDF volumes, terrain, materials, collision, navigation, rendering and optional AI.
It must support an NMS-like universe profile confined to one planet when desired.
It is optional within Epoch's universal software environment: empty applications,
camera-only software and ordinary GUI programs do not load a universe or models.

This is the target architecture, not a working streamed-planet claim. Existing
`voxel.field`/`voxel.storage` provide bounded metadata and sparse cells;
`voxel.trace` describes query intent, while `render.ray` has CPU reference queries.
The Extensions planetoid sampler and planetary terrain prototype are separate
prototype representations, not a complete coupled world renderer/editor.

## Target Shape

- Multi-informational voxel cells should carry geometry, material, occupancy,
  lighting, navigation, biome, water/atmosphere, and generation metadata.
- Voxel/SDF volume and persistent edits retain caves, overhangs and multiple
  surfaces. Heightmaps are derived views only where single-valued surfaces are
  appropriate; a volume-to-heightmap conversion must not replace authoritative
  concave geometry. Classic meshes/terrain remain valid derived runtime forms.
- Local navigation uses conventional spatial/collision queries as its baseline.
  The operator reports that the spatial voxel-ray localization experiment did
  not show useful local advantage; its long-distance behavior did not establish
  a production win. Preserve this negative result, without claiming an archived
  benchmark receipt was found or that every voxel/DDA query is useless. New
  acceleration must beat comparable end-to-end workloads before selection.
- Vegetation should converge through procedural plant/L-system generation plus
  SpeedTree-like LOD and impostor output tied back into the voxel field.
- Distant objects should use voxel-informed LOD and generated representations
  before the engine reaches for triangle-virtualization-style solutions.

## Core And Package Boundary

The reusable volume implementation is intended for a separate `EpochVoxelEngine`
project. Begin with a bounded Engine integration slice; extraction is complete
only after independent C++23 module consumers, exact dependency identity and
matching Engine integration pass. Do not duplicate module definitions or silently
change source licensing. Initial extraction retains existing Epoch source terms;
new repository publication/terms require an explicit reviewed decision.

Engine owns world/project identity, durable edit admission and composition.
Voxel owns reusable sampling/storage/edit/extraction contracts; Extensions owns
planet/biome/terrain composition and heavy payloads. Renderers own GPU resources,
physics/navigation own derived query data, Platform owns devices/contexts, Gui owns
controls. System One coordinates requests through the existing work services.

Epoch now treats the voxel and plant direction as two layers:

- Engine core owns the lightweight contracts: voxel cell/chunk/LOD metadata,
  trace/query purposes for rendering, lighting, navigation, visibility, smoke,
  and AI, plus deterministic Forest Factory descriptors that can preview inside
  the editor without forcing generated projects to carry forest assets.
- Project/package activation owns heavy payloads: NMS-like planetary terrain,
  multi-terrain authoring, FFT ocean, large renderer experiments, prototype
  demos, source downloads, and generated content that should not inflate every
  software or small-game clone.
- `cache/packages/` is the executable-local cache for downloaded or staged
  source/runtime packages. Package source is pulled into projects only through a
  visible Package Manager/updater-style gate and never by hidden automation.
- Bulky source packages should be curated in
  `https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git` before any reviewed
  subset is promoted into EpochEngine API contracts. The engine repo keeps
  descriptors, package gates, cache/update plumbing, and stable interfaces
  rather than cloning terrain/ocean/server prototype trees into mainline.
- Forest Factory is a core feature because repeatable procedural vegetation and
  voxel occupancy are part of Epoch's runtime/generation spine. It still remains
  project opt-in: Plant Lab may author/preview vegetation without adding package
  assets, but importing or placing a plant through Forest Factory in the main
  editor scene must activate the `engine_forest_factory` package manifest and
  generated assets for that project.
- The production Forest Factory package contract is tracked in
  `forest_factory.md`. The Plant Lab launcher application is
  engine-owned; the external Plant Lab repository is provenance/reference
  source; the Site-hosted `EpochEngineExtensions` repository owns package payload
  routing; EpochEngine mainline owns the renderer-neutral contract and
  activation gate first.

## Source of truth and representations

Planet shape and terrain representation are independent choices. Classic
procedural or fixed terrain remains valid; editable SDF/voxel terrain and an
optional block-world provider require explicit supported capabilities. A coverage
preset or an immutable generated mesh does not make terrain editable. Accepted
edits must persist as authoritative documents/journals, then invalidate derived
mesh/LOD, collision and navigation together through the existing job system.

Object placement and navigation use the selected body's local frame. A surface
query must identify body, time, source/recipe revision, canonical units, position,
terrain normal, gravity-up and sea level separately. Reuse the admitted provider's
sampler; do not infer a planetary surface from a bounding box or global Ground Y.
Placement aligns the object's intended up-axis and offsets its actual transformed
bounds above the chosen surface. Moving planets additionally need persistent
body-local object anchors. A single fixed-time placement is not proof of that
anchoring, collision, editable terrain or voxel navigation.

The October28 size clarification calls for configurable game worlds, not a
mandatory full-size Earth. Small worlds should be practical to traverse; Moon,
Earth and larger radii are optional inputs with separate precision/resource
acceptance. Keep size separate from the camera's local detail budget. The pad,
ordinary objects, movement and planet must agree on units at every chosen size.
New recipes may use a smaller gameplay preset while existing projects retain
their saved size. Any traversal estimate must state its speed and continuous-
travel assumption; generation settings do not guarantee a travel time.

The normalized radius-ten artifact remains an overview representation. October28
source adds a bounded, request-bound `terrain.frame` receipt and optional canonical
`SceneMeshCoordinates`. Engine admits the exact mesh key/counts and scale/origin,
then preserves that metadata through document history, world format6, project
runtime and save/reopen. A shared unit helper applies scale without rewriting the
user's transform; source origin is provenance, not an extra authored translation.
Legacy worlds without this metadata retain exact format4/5 bytes and hashes.
Size settings use recipe6 only when needed; default legacy recipes stay unchanged.

Actual-owner Debug/Release tests cover1m objects,10m motion,25km terrain,14m pad,
body-anchor/light alignment and history/save. Native fresh-project generation,
close/reopen, embedded Debug Run, Land/Walk1.8m and LOD1–15 pass on Windows OpenGL.
Large-world camera framing is widened independently of Canvas2D, whose limits and
normal traces stay unchanged. This closes the demonstrated mixed-unit import gap;
it does not establish complete camera-relative large-world precision. All geometry
and camera must eventually subtract the same double origin before render floats,
without replacing saved positions. Native Moon/Earth ground quality remains open.

Ground detail also needs patch-local precision. The existing merged overview
already converts complete body positions to floats; restoring its scale cannot
recover the lost precision. The supplied generator still creates the original
`parent_delta`, but the current static mesh exporter discards it. Smooth original
LOD needs patch anchors and transition endpoints/weights to reach the existing
per-frame renderer, with bounded outgoing geometry retained until transition
completion. Baking another snapshot on each CLI invocation would still pop.
Patch-local double precision and transition payloads remain unimplemented. Audit:
`Artifacts/Reports/planet-metric-authoring-audit-oct28/result.json`.
Current integration proof: `Artifacts/Reports/planet-size-integration-oct28/`.

Durable state is the generation recipe plus versioned volume/material edits and
project/world identity. Loaded chunks, extracted meshes, heightfield proxies,
collision/navigation structures and GPU buffers are reproducible derived data.
Use temporal/world revisions to reject stale jobs after edits or rewind.

An SDF sample carries an explicitly signed distance in meters: negative inside,
zero at the surface, positive outside. Existing voxel density is not automatically
such a distance. Hard CSG and warped procedural functions may be conservative
distance bounds rather than exact distances; trace acceleration must know that
quality and may not assume unsafe sphere-tracing steps. Preserve a correct query
fallback. Geometry and materials have explicit ownership at cut/deposit surfaces.

Material channels need not all occupy every cell: sparse/optional channels hold
material, occupancy, biome, water, lighting or navigation information at appropriate
resolution. Large-world identity uses stable region/chunk coordinates and local
metric positions; do not use one global float3 for universe-scale precision.

The first bounded solid-edit slice supports addition/subtraction/intersection,
including cave/overhang and snow-like deposition samples. This is geometry editing,
not evidence of granular snow, fluid simulation, meshing or a playable planet.
Each later feature needs controls, persistence, resources and acceptance together.

October25 implemented slice: `Engine/modules/voxel/voxel.sdf.ixx` owns finite analytic
sphere/axis-aligned-box samples, explicit sample validity/distance quality/material
and hard CSG. `EditVolume` retains at most64 ordered edits; rejected edits leave
the volume unchanged. `Engine/src/voxel/voxel.sdf_contract.cpp` proves units/signs,
materials, concave intervals, overhang/deposition, invalid input and capacity in
independent Windows Debug/Release consumers. Engine's isolated Debug build and
SDF integration check pass; broader Engine acceptance remains in active_pass.
This is not persistence, chunk streaming, meshing, collision or native terrain proof.

## Terrain transitions

Extensions should compose classic and volumetric terrain without a lossy circular
voxel → heightmap → voxel authority chain. Preserve separate prototype sources
until an explicit composition recipe identifies which data controls each region.
SDF edits must remain three-dimensional through caves, cliffs and overhangs.

Implement representation/LOD transitions against the same world revision. Shared
boundary samples, extraction conventions, normals/material rules and collision
handoff must prevent cracks, duplicate surfaces and stale collision. Heightfield
proxies must identify unsupported concave regions and use volume/mesh coverage
there. Biome/material blending is distinct from geometric seam correctness.
No specific prototype or algorithm is accepted merely because its metadata exists.

## Central event rendering and System One

```text
human or permitted bot action
  → validated committed world/project edit
  → affected regions + revision
  → derived geometry/material/collision/navigation work
  → shared render invalidation
  → backend-owned reconstruction + presentation
```

Use existing events, temporal identity, taskgraph and work-admission owners; the
current event bus is not a durable world journal or cross-module serialization
format. Centralize semantic change classification and render-work decisions,
while preserving backend resource/thread ownership and frame/overlay order.
October30 operator direction removes OpenGL cached/partial scene rendering and
software whole-frame reuse from source. Conventional full-frame composition is
current policy; combined Debug/Release builds/full pure contracts pass. The same
Release still lagged at actual 120; native responsiveness/image acceptance remains
open. Shared events,
geometry/lighting revisions and generic invalidation primitives remain; future
partial rendering would need separate scope and proof.

ARCHITECT 1/System One applies policy through those services, coordinating bots,
generation, region work and resources. It does not replace the scheduler or own
every subsystem. Bots consume observations and submit permitted proposals; a
model prediction cannot suppress authoritative damage or become world truth.
Drawing, clocks and software operation must work with all models disabled.

Event-driven is universal software behavior, not a requirement to sleep until a
neural event. Explicit simulation time, fixed-step physics, media playback,
camera movement, GUI invalidation and presentation deadlines all produce work
when required. The same-cache SELECTIVE/FULL benchmark is removed, and the proposed
animation frame cache is cancelled. Measure future consumers against conventional
rendering with total cost and correctness. Ordinary texture/asset/font residency,
GUI snapshots and derived Canvas2D raster preparation remain; this is not a removal
of every resource cache. See [full-frame policy](event_render.md).

## Dynamic loading and shared resource admission

World pages and models share resource observations/budgets, not one destructive
generic unload operation. Their existing owners retain save/cancel/retirement
responsibility. Reuse [AI model handoff](ai_plan.md#shared-model-loading-and-unloading-plan)
for Flash/Qwen and future bots; region streaming uses world revisions and durable
edits. Neither paused inference nor a changed residency label frees device memory.

Planned region lifetime: requested → loading/reconstructing → validated resident →
in use/pinned → retiring → evicted. Loading can fail/cancel; stale work is discarded
before publication. Unloaded is not empty: collision/navigation queries must
return unavailable/pending or a declared safe fallback, not fictitious clear space.

Admission accounts for world pages, visible meshes/textures, collision/navigation,
renderer contexts, model weights/KV, generation scratch, IO/decompression buffers,
RAM/commit and other applications. Near-player collision/gameplay and visible
rendering have protected budgets; background AI/world generation waits. Predictive
prefetch is cancellable and cannot evict pinned resources. Use high/low watermarks
and bounded queues to prevent thrashing; establish values from measurements.

Eviction must persist edits first, wait for readers/in-flight GPU work, retire only
owned resources and preserve identity/revision. Reload reproduces the same world
and reapplies durable edits. Cache loss cannot erase authored terrain. Teleport,
fast travel, camera motion and multiple viewpoints must respect bounded peaks.
No automatic cloud fetch, server/listener or model substitution is implied.

## Dependency-ordered implementation slices

1. Finish current repair/package/dependency acceptance and keep the reusable
   software/2D gates intact; integrate only an isolated typed SDF CPU foundation.
2. Prove one editable volume's add/subtract/deposit behavior, versioned persistence
   and exact unload/reload without heightfield loss. Extraction/collision remain
   separate acceptance until actual consumers exist.
3. Prove neighboring chunk and LOD seams, representation transitions, materials,
   collision handoff and bounded region lifecycle under memory pressure.
4. Propagate one committed edit through rendering, collision and affected
   navigation; test permitted bot and human actions against the same authority.
5. Expand to the single-planet composition profile, then multiple regions/planets.
   Universe scale must not be claimed from a planet descriptor or empty demo.
6. Extract/pin the reusable voxel dependency after independent consumers and
   matching Engine integration pass; audit licenses/source before publishing.

Tests include empty/camera-only software, all bots disabled, concave cave/overhang,
deposition/cutting material ownership, invalid/nonfinite input, deterministic
sampling, boundary/LOD cracks, save/reopen, restart/cancel, stale results, missing
pages, teleport, repeated eviction/reload, multi-view demand and active renderer/
Qwen contention. Compare navigation with conventional queries and rendering with
the full path. Record implementation, CPU proof, native pixels and measured
benefit separately; none substitutes for the others.


## Package Strategy

### Terrain coverage modes — October26 operator direction

The optional terrain/planetary extension presents one progression. Its
`planet.coverage` C++23 module now generates bounded CPU meshes for all five
modes through the reviewed `planet.terrain` owner. Editor controls, project
activation and visible Engine integration are not yet accepted.

| Mode | What the user gets | Required distinction |
| --- | --- | --- |
| Classic | One bounded flat terrain grid | Planar coordinates, editable dimensions and an explicit outside boundary |
| Tiled | Several joined grids, for example4x4 | Configurable coverage, matching seams, stable tile IDs and bounded streaming |
| Hemisphere | Half a planetary body | Real curved geometry, declared hemisphere orientation and an intentional edge |
| Planet | One complete body | Seamless cube-face/LOD transitions, large-world coordinates and surface-relative camera |
| Universe | Multiple planetary bodies | Independent body identities/origins, selection and distance-based residency budgets |

The independent Debug and Release consumers now each pass four compiled tests,
including the optional mesh exporter and a real Engine artifact-codec consumer.
With four cells per tile and a64-meter tile span, the coverage test produces one
Classic grid,16 grids for4x4 Tiled,12 curved northern-hemisphere patches with an
open equator, six Planet cube faces and36 Universe patches across the supplied
six-body world. These are actual mesh buffers, not descriptors or displayed
worlds. Requests carry mode, seed, body index, cells, tile count, metric span and
time; results retain body IDs, double-precision body centers/tile anchors,
vertices and32-bit indices. Ocean and vegetation geometry remain separate.

Patch count and estimated CPU allocation are checked before mesh generation.
The estimate includes retained buffers, reserve slack, sampling scratch and
patch metadata; it is not a measured process-memory or GPU-residency guarantee.
Invalid inputs fail admission. The caller's stop token cancels generation and
returns no partial patches. Global planar sample coordinates and neighboring
height samples keep shared tile positions and normals aligned. The contract
checks index ranges, finite vertices/normals, bounds, planar spacing/seams,
hemisphere edge, face/body identities, capacity rejection and pre-cancellation.
Receipts: `build/validation/planetary-coverage-oct26/{debug,release}-final-tests.log`.
Live mid-job cancellation, curved LOD seams and native rendering remain gates.

The opt-in `planet.mesh_export` tool writes a real `terrain.epoch_mesh` plus a
`terrain.preview.json` recipe and refuses an existing destination. All five mode
exports decode through Engine's actual `asset.mesh_artifact` owner; Debug/Release
bytes agree. The recipe retains seed/settings, body/patch origins and an explicit
meters-to-preview-units transform. Its combined float mesh is a normalized
overview. It is not metric world placement, streamed terrain or a replacement
for authoritative double-precision body/tile data. The package's default core
build does not require Engine; the export/codec consumer is opt-in.

Engine now has a generic data-import/scene/preview bridge for this artifact:
Asset → Import Mesh, format4 mesh references, existing preview publication and
explicit mesh reload. The codec, scene roundtrip and preview consumers pass
Debug/Release, and the changed Debug Editor/full build-safe contracts pass.
Native visible import, project save/reopen and provider removal still need
acceptance. Source-only Package Manager review/staging is implemented; approved
source build/activation remains separate. See [authoring](authoring.md#imported-indexed-meshes--current-contract),
[workflows](workflows.md), `build/validation/mesh-bridge-oct26/result.json` and
`build/validation/planetary-coverage-oct26/{result,mesh-export-proof}.json`.

Rendering only a small patch of a sphere is not the same as planar Classic mode.
Keep coverage separate from curvature, tile resolution, detail distance and the
resident-memory budget. The current synchronous preview builds only admitted
bounded coverage. Full world streaming must select resident tiles rather than
generate an entire world at once.
Use the existing task/event ownership for generation and residency; do not import
the supplied viewer's independent scheduler. Preserve double-precision body and
tile anchors until the existing renderer receives camera-relative coordinates.

### October27 — original exploration connected; native acceptance pending

The optional `planet.exploration_runtime` provider now uses the supplied
EpochPlanet0.2.3 controller and builds six real coarse body meshes. The shared
`project.planetary_runtime` module defines its frame, input and exact
project/source/edition binding. It does not import the optional terrain into
ordinary Engine startup. The existing ProjectPlay scene owns the caller:

- Choose a body, then Orbit, Land or Cruise; Walk/Flight, gravity and level
  assistance use the original controller. Movement/look respects input focus,
  project GUI capture and the runtime panel. The generic camera does not also
  move while the provider controls it.
- The existing scene tick supplies bounded elapsed time. No second scheduler or
  clock loop was added. Time can advance with an idle camera, or pause while
  camera movement remains available. The original moon follows its parent.
- Before conversion to render floats, the host subtracts the selected anchor's
  center in double precision and scales its radius to ten preview units. The
  authored planet transform supplies the scene coordinate frame without being
  rewritten. Six meshes use existing marker publication; streamed LOD replaces
  only the current anchor's coarse mesh. Body changes cancel old requests;
  source, request and body checks reject late results.
- Save Exploration explicitly stores the provider's bounded session in
  `Assets/Worlds/Planet.epoch_exploration`. The host uses current source identity,
  an exclusive file lease and the last observed bytes before replacement.
  Reopen asks the provider to validate/restore the saved camera, bodies, travel
  mode, assistance and simulation clock. Corrupt, foreign or concurrently
  changed saves remain intact. Loading a project does not silently overwrite
  the session.

The optional exporter accepts `--seconds <finite value>` with
`0 <= seconds < 1e12`. Explicit time, including zero, produces an ordered
`epoch.planet.stream.v3` receipt: the old base fields through `indices`, then
`ocean_surface`, then `simulation_seconds`. The host verifies the captured
request time, not a later continuously advancing clock. Body-local patch cache
identity excludes time. Omitting this option retains dry-v1/wet-v2 transport.
Static Universe export at nonzero time is also tested; it is a baked overview,
not proof of a live multi-body renderer.

Actual Windows Debug/Release CPU consumers each pass404 original-controller and
projection checks. They cover Earth/moon anchors, orbital motion, camera basis,
double subtraction before float conversion, invalid frames and the session
codec. Six Settings/static/stream CLI checks also pass across both modes,
including timed near/far cache reuse and invalid-time refusal. The adapter's
module-fragment declaration warning was fixed and its closest consumers pass
again. Matching Development Release and Debug Engine builds/full contracts
pass and their owned contract processes retire. A later fresh native project
provider build fails C1083 on the existing `simulation.hpp` in its deeply nested
source path; the optional build-path repair remains active.
These results do not accept native controls, six visible moving bodies,
Save→close→reopen, walking-depth precision or any Linux/Public delta.

The next eye-test must use newly reviewed package source. Keep earlier approved
source copies immutable. Full original terrain geomorph, original atmosphere/
water shading, metric depth precision, complete multi-body detail and dynamic
DLL delivery remain separate gates. The existing normalized mesh format still
omits original `parent_delta`; larger mesh budgets do not restore geomorph.
Proof: `build/validation/planet-lod-quality-oct27/{timed-result,project-play-projection-result}.json`,
`build/validation/planet-runtime-controller-oct27/portability-result.json` and
`build/validation/project-configuration-oct26/planet-original-runtime-oct27-r2-{Release,Debug}-result.json`.

### October27 detail-parity audit — still unfinished

The active native Universe project's saved recipe still selects 16-cell tiles,
maximum level 18,48 leaves and 64 MiB patch cache, with the old implicit 4 MiB
snapshot limit. Its actual cached plan contains 62 needed patches and 48 leaves:
16 at level 1 and 32 at level 2. It has exhausted that leaf budget; increasing
maximum level alone cannot make the cut finer. This observation does not
establish the cause of the separately reported 9 FPS.

The supplied demo's profiles are Fast (level 17/288 leaves/260-pixel target),
Balanced (18/384/210) and Fine (19/576/170), with hysteresis 0.18 for Fast and 0.15
otherwise. The current Detailed preset matches Balanced's level/leaf target
with 16-cell tiles and explicit 128 MiB cache/32 MiB snapshots; selecting/saving
that preset is separate from merely building its supporting code. The repaired
host also admits Fine at level19/576 leaves/170 pixels,192 MiB cache and32 MiB
snapshots. Version5 saves those controls; older recipes preserve their limits.
Actual Debug/Release provider and Engine settings/receipt contracts pass. This
does not accept ground-level materials, terrain editing or metric-size authoring.

Further differences remain after a larger budget: the original all-world
planner can refine any nearby body; the current bridge streams the anchor and
keeps the other five bodies as 8-cell coarse meshes. The original retains
52-byte vertices with parent deltas and performs 0.20-second render-time
transitions; the current 44-byte aggregate does not carry that morph data.
Original asynchronous streaming publishes up to four ready tiles per frame
with 1–4 workers and replans around 150 ms. The current transport restores its
disk cache, builds up to32 tiles and rewrites/reads a full aggregate per
supervised step. Successful incomplete steps and successful stale-camera
retargets now continue without an extra idle delay; contention, cancellation
and failures keep250 ms backoff. The previous
host completion rule also discarded every cut once the camera moved beyond
the small replan threshold. Those are concrete source differences, not a
benchmark showing which one dominates renderer cost. A bounded CLI comparison
of the same96 patches measured2.176s with8-patch batches versus0.771s with32;
cached geometry was byte-identical. It excludes host checks and rendering.

The selected-body planner now retains its existing camera-priority split and
2:1 prerequisite order, instead of replacing it with a global level/key sort.
The all-world planner retains its previous ordering. Same-leaf camera changes
refresh pending order without invalidating valid residency or in-flight work.
At the same500m-altitude Fine test pose,96 generated patches reach nearby cut
level5 instead of2; all766 needed patches converge to576 balanced leaves.
Both provider configurations pass8/8 CPU tests. Source/tests/negative controls:
`Artifacts/Reports/planet-stream-priority-oct28/result.json`.

The new completion rule accepts a modestly older view only for the same
admitted project, source, settings, body and latest request. The provider must
still supply a complete six-face cut. The shared `stream_view_reusable` policy
requires valid finite cameras, matching viewport/FOV and bounded translation:
one quarter of the larger altitude, clamped to 0.001–0.25 in radius-ten units.
Cancelled results, changed body/anchor/settings/source and older request IDs
cannot replace the current mesh. The newest view is queued afterward. This
would not be safe for a frustum-culled artifact; the current provider retains
all six roots and parent fallback until children are ready.

`python -m unittest Tests.packages.test_planetary_stream_completion` passes
Debug/Release with the real shared module and compiled host admission
expressions using recorded identity/source-verifier inputs. It covers 200
moving-camera completions plus stale identity, cancellation and view-limit
refusals. The unchanged real provider consumer separately passes six-face
area/parent-fallback/2:1/budget/stale/cancellation checks. Receipts are under
`Artifacts/Reports/planet-moving-completion-oct27/`. This is CPU proof; native
moving-camera quality and performance remain pending.

Compare the same camera pose, FOV, viewport and quality profile before claiming
demo detail parity. First finish immutable geometry reuse and measure the
actual publication/render cost; then validate complete-cut reuse and evaluate
coalesced streaming through the existing owners. More leaves or workers alone
do not establish performance, seam or geomorph correctness. The current static
edge tests cover coincident samples; unmatched fine vertices versus coarse
triangles, skirt overlap and near-plane clipping still need separate proof.

October26 adds a tested **bounded selected-body LOD provider**, distinct from
the complete streamed-world target above. The optional `planet.streaming`
C++23 module reuses the reviewed `plan_terrain`, `build_tile` and `balance_cut`
owners. A metric camera requests finer terrain nearby and coarser terrain far
away. The caller owns the state and existing job queue. Parents stay available
until all four child regions are ready, and the resident draw cut retains
complete body coverage and the upstream2:1 edge rule. Skirts cover the mixed
detail boundaries; a native seam eye-test is still required.

Admission budgets retained meshes, one meshing job and bounded planning
metadata. In-flight work retains its allocation reservation until completion;
cancelled or obsolete generations cannot publish. Moving away evicts unused
detail. October27 repairs a causal budget error: the original all-world
planner reserved thirty roots for invisible bodies. A selected-body entry point
now shares the same planner implementation and spends the full leaf budget on
that body; the original all-world API stays intact. The default48-leaf native
path still requires new acceptance beyond the earlier6→18 cut. This is not a
complete streamed universe, editable terrain or collision system. Optional
opaque ocean geometry is separately implemented; it is not water simulation.

The optional producer's `--stream-step` and `--stream-step-preview` commands
reuse a project-local patch cache and the prior LOD plan. They build a bounded
number of new patches per call, preserve detail hysteresis, validate cache
identity and refuse unknown files or redirects. Recognized interrupted patch
writes are discarded and rebuilt. Each call writes a fresh output directory
and publishes an ordered `terrain.stream` receipt last. Snapshot scale remains
fixed at10 divided by the selected body radius. Body-local overview camera
coordinates are converted to metric world coordinates inside the provider.
The core creates no scheduler, renderer, model service or network listener.

The Engine transport owner is `project.planetary`: `Settings` saves the LOD
toggle and limits. It reads version1–4 recipes. Old representable dry-v2 and
wet-v3 bytes remain unchanged; v4 carries explicit larger snapshot/cache/leaf
budgets. Compact defaults stay48 leaves/16 MiB patch cache/4 MiB snapshot;
Detailed384/128/32 is an explicit choice, not an automatic hardware guarantee.
`stream_step` rechecks the attached source identity, invokes the already built
provider through the existing child-process supervisor, validates the request
receipt and decodes the actual mesh. Successful temporary step outputs are
retired after the immutable mesh is loaded; failures retain diagnostics. Camera
work is queued through the existing Editor task owner. The saved fallback mesh
and its scene reference remain authoring data; camera detail is disposable
render state and must not generate scene edits or rewrite the fallback asset.

Cross-process cache access uses the existing `platform.filesystem` owner's
move-only `ExclusiveFileLease`. A nonblocking native lease on the project's
empty coordination file prevents an Editor and a running project from changing
the same patch cache together. Busy work stays queued. The child supervisor
retains the lease until the owned worker actually retires, including failed
retirement paths; a borrowed/focused process never adopts another caller's
lease. Windows uses exclusive handle sharing; the Linux implementation uses
`flock` and refuses symlink traversal. Native Windows Debug/Release tests prove
two-process contention, release after forced holder exit, move/reset, and
rejection of nonempty files, hard links, symlinks and junction parents.
The Linux branch still needs its own native check. This is resource ownership,
not an OS confinement claim. Proof:
`build/validation/platform-file-lease-oct26/result.json`.

The generated project's build hook now builds and stages a project-local
provider with a source, edition, protocol and executable-identity receipt.
Production Debug/Release transport checks run that staged provider with no
`build/planet` generation cache. Modified identities/bytes and unmanaged files
are refused before launch. This proves the staging/transport path; the matching
generated application link and visible standalone LOD still need acceptance.
Proof: `build/validation/planet-project-oct26/provider-stage-result.json`.

Provider Debug/Release each pass six contracts, including real far/near cuts,
memory limits, parent coverage, edge balancing, stale work, cancellation,
cache reuse, eviction and interrupted writes. The production Engine transport
also passes actual Debug/Release provider steps: far snapshots are reused;
near snapshots refine with a different validated content key; settings remain
unchanged. Receipts are
`build/validation/planetary-streaming-oct26/result.json` and
`build/validation/planet-project-oct26/stream-{debug,release}-run.{log,exit}`.
These CPU/worker results do not accept live Editor camera transitions. The
fixed overview already reopened visibly after normal close, with the
operator's placement and deletions retained
(`build/validation/planet-editor-integration-oct26/native-reopen-result.json`).
Native camera LOD, transform changes, cancellation/retry, unrelated scene edits,
runtime project presentation and save/reopen with LOD enabled remain acceptance
gates. Metric world placement, surface-relative navigation, multi-body
streaming, full original-example features and dynamic DLL loading remain open.

Save mode, seed, size, tile layout, body parameters, materials and edits in the
project. Changing a mode is an explicit, undoable conversion or new world, not
silent destruction of existing edits. Hemisphere boundaries need a visible
choice for the horizon/edge and collision/navigation limits. Universe does not
imply orbital simulation or unrelated networking is required.

Test one grid, tiled seams, hemisphere boundary, closed-planet seams/poles,
multiple-body origin changes, cancellation, bounded streaming, save/reopen and
package removal. SDF and hybrid voxel providers remain separately selectable
dependencies where requested, as does Forest Factory. All heavy implementation
belongs in Extensions; a minimal Engine application has none of it installed.
The compiled CPU coverage tests accept the bounded mesh-generation scope only;
they do not accept Editor controls, streaming, persistence or visible worlds.

Do not import these prototypes directly into mainline engine source. The first
safe path is a package track:

1. Stage the prototype as research with provenance.
2. Create a local package manifest that records source, hash, license/provenance,
   build commands, runtime entry points, tests, and known limitations.
3. Keep package source in a special review branch, a separate repo, or a local
   package cache until the API boundary is clean.
4. Build and run the package through the same human-approved updater/package
   gate used for future downloadable source packages.
5. Promote only the smallest reusable engine interfaces into mainline Epoch:
   voxel data contracts, terrain chunk interfaces, generation jobs, renderer
   bridge points, and asset outputs.

## Acceptance Gates

Before any voxel package becomes tracked engine code:

- The package builds through CMake/MSBuild or the updater-style package build
  path without hardcoded developer paths.
- Tests prove deterministic chunk generation, stable LOD keys, seam behavior,
  and bounded memory behavior.
- The package does not auto-create servers, listeners, hidden model channels, or
  model-accessible bypass surfaces.
- Runtime proof shows the package can feed an editor-visible asset, preview, or
  generated scene without destabilizing current OpenGL/DirectX/multicontext
  editor behavior.
- The import adds one clear engine-owned API boundary instead of another
  competing terrain/render framework.

## Deferred Engine Work

- Keep the first-pass `voxel.field`, `voxel.trace`, `forest.factory`, and
  `package.registry` contracts small, build-tested, and renderer-agnostic before
  wiring them into editor workspaces or runtime packages.
- Forest Factory now has a build-tested occupancy estimate that can describe
  trunk, branch, and foliage cells without binding to one renderer; the next
  gate is making that occupancy consumable by real LOD/hit/path/lighting systems
  rather than only displaying editor evidence rows.
- Add package-manager support for local research packages with provenance and
  build/test evidence.
- Complete the Plant Lab 3D authoring application and the standard-editor Forest
  Factory import/placement surface once the GUI library has stable tabs,
  dropdowns, scrollable text, modals, and copy/paste. The first package
  activation record now exists; the next gate is consuming the deterministic
  profile in both workflows instead of only staging manifest files.
- Connect OS AI planning to package evidence only after the gate can reject
  low-evidence or hidden-reasoning model output.
