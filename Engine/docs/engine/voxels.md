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

October25 implemented slice: `Engine/modules/voxel.sdf.ixx` owns finite analytic
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
OpenGL partial scene reuse exists; cross-backend partial rendering remains open.
Unsupported/global/nonlocal changes fall back to correct full reconstruction.

ARCHITECT 1/System One applies policy through those services, coordinating bots,
generation, region work and resources. It does not replace the scheduler or own
every subsystem. Bots consume observations and submit permitted proposals; a
model prediction cannot suppress authoritative damage or become world truth.
Drawing, clocks and software operation must work with all models disabled.

Event-driven is universal software behavior, not a requirement to sleep until a
neural event. Explicit simulation time, fixed-step physics, media playback,
camera movement, GUI invalidation and presentation deadlines all produce work
when required. The existing same-cache SELECTIVE/FULL benchmark remains the
comparison lane; report synchronized total cost and correctness, not cached FPS.

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

The optional terrain/planetary extension presents one progression. These are
planned presets over shared generation and material contracts, not five engines:

| Mode | What the user gets | Required distinction |
| --- | --- | --- |
| Classic | One bounded flat terrain grid | Planar coordinates, editable dimensions and an explicit outside boundary |
| Tiled | Several joined grids, for example4x4 | Configurable coverage, matching seams, stable tile IDs and bounded streaming |
| Hemisphere | Half a planetary body | Real curved geometry, declared hemisphere orientation and an intentional edge |
| Planet | One complete body | Seamless cube-face/LOD transitions, large-world coordinates and surface-relative camera |
| Universe | Multiple planetary bodies | Independent body identities/origins, selection and distance-based residency budgets |

Rendering only a small patch of a sphere is not the same as planar Classic mode.
Keep coverage separate from curvature, tile resolution, detail distance and the
resident-memory budget. Increasing coverage must not generate every tile at once.
Use the existing task/event ownership for generation and residency; do not import
the supplied viewer's independent scheduler. Preserve double-precision body and
tile anchors until the existing renderer receives camera-relative coordinates.

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
Current CPU planetary-core proof does not accept these modes or visible worlds.

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
