# Epoch Canvas2D Architecture

## Authority And Scope

This document is the canonical contract for Epoch's renderer-neutral 2D
composition path. It is subordinate to `capability_tier_architecture.md` and
specializes the document/runtime separation in
`temporal_authoring_platform.md` for the two-month playable 2D objective.

`Changes/active_pass.md` owns the current acceptance gate,
`Changes/roadmap.md` owns scheduling, and `renderer_feature_matrix.md` owns
implementation evidence. This document defines architecture, not evidence or
release history.

Canvas2D must support one authored scene through editor Play, external Run, and
Build without introducing a second renderer spine. Source `v0.88.77` adds the
renderer-neutral project/submission foundation: camera and viewport policy,
stable sprite identity, logical material intent, deterministic bounded quad
batching, tile descriptors, compose plans, diagnostics, editor snapshot policy,
and build-safe contracts. Source `v0.88.78` adds the deterministic `T0-CPU`
reference raster: explicit RGBA8 resources and clips, fixed-point triangle
coverage, texture sampling, alpha composition, offscreen-to-presentation
compose, bounded metrics, deterministic image hashes, and staged failure
evidence. Source `v0.88.79` adds the first physical execution-cache layer:
stable artifact keys, generation-checked residency handles, bounded base-mip
uploads, reuse, pinning, priority/LRU eviction, backend epochs, recreation,
metrics, and context-guarded OpenGL allocation/upload/destruction hooks. Source
`v0.88.80` adds a renderer-neutral raster-to-residency/native-presentation
boundary plus the primary-context OpenGL final compositor. The packet carries
frame/content identity, image semantics, compose policy, and an explicit scene
surface; the adapter confines clears/draws and restores borrowed GL state.
Source `v0.88.81` adds deterministic owning RGBA8 mip compilation and complete
artifact-integrity validation, then maps project-supplied logical identity and
a one-time sealed linear mip 0 into the shared standalone residency cache.
The synthetic-device bridge contract proves synchronous payload ownership,
reuse, transactional recreation, backend
reset, and stale-handle rejection without storing physical state in documents.
Source `v0.88.87` adds
the runtime-owned project asset registry and texture resource service:
generation-checked project handles, portable logical paths, content-derived
artifact revisions, full source/artifact authentication, bounded owning CPU
resource sets, optional residency acquisition, and logical-only tileset
references. Equivalent content at a later temporal source sequence reuses the
same artifact/cache identity.
Source v0.89.10 completes the first saved semantic texture path: project images
compile into authenticated artifacts, persist in the Project Library, restore
through exact logical identity, survive snapshot-format-3 save/reopen, and bind
immutable Canvas2D resource leases. The primary OpenGL adapter consumes that
content in the protected editor scene slot while preserving GUI replay and
present order. Approved native capture exactly matches the T0-CPU image across
1,178,872 pixels, and the generated GUI project passes the standalone external
Run path.
Interactive Assets-tab assignment, secondary GL share groups, non-OpenGL
texture presentation, sRGB/compressed/mip-chain execution, tilemap authoring,
and the complete built-game loop remain Partial.

## Product Contract

The first production profile is deterministic `T0-CPU` correctness plus
`T1-GL` desktop presentation, shaped for later `T1-GLES`. It must be sufficient
for one tile map, a controllable animated actor, collision, audio, save/reopen,
Play/Stop, Run, and Build.

Higher-tier backends may implement the same contracts, but Vulkan, DirectX,
SDL3, SFML3, Raylib3, advanced effects, and broad 3D work cannot redefine or
delay the baseline path. API names never imply capability.

## Ownership Boundaries

- Canvas2D owns renderer-neutral camera, viewport, draw-item, material, sprite,
  batch, tile-layer, culling, ordering, composition, and diagnostics contracts.
- The authoring platform owns stable documents, semantic commands, history,
  dependency invalidation, compilation, and migration.
- `render.device` and `render.graph` own logical resources, passes, commands,
  dependencies, and lifetime declarations shared with other renderer domains.
- Texture authoring owns logical texture documents and compiled texture
  artifacts. Canvas2D refers to them by stable handles and revisions.
- `render.texture.residency` resolves compiled artifact identity to disposable
  device textures. It owns budgets, reuse, eviction, pinning, backend epochs,
  recreation, and cache metrics while backend devices own native resources.
- `render.texture.artifact` is the only authoring-to-renderer adapter. The
  project registry supplies logical asset identity; the artifact supplies
  content identity and bytes; the residency cache supplies physical lifetime.
  None may synthesize or absorb the other two identities.
- Compiled artifact schemas and readers are runtime-owned and available without
  authoring UI. Authoring owns compilation. `project.asset_registry`
  authenticates stable project identity/source revision; the Library and
  pipeline provide serialized reads, restore, and atomic publication;
  `project.texture_admission` applies capability/budget policy; and
  `project.texture_resources` binds the first linear RGBA8 base-mip
  CPU/residency lane. Broader formats and backend execution remain Partial.
- Backend adapters own API objects, uploads, shaders, pipelines, render targets,
  synchronization, readback, and presentation evidence.
- Input, physics, animation, and audio own their runtime state and publish
  bounded observations or plans through explicit seams.
- EpochGui owns reusable portable controls and state. Editor adapters own native
  hosts, backend drawing/input translation, project state, and capability
  evidence.

No Canvas2D document may contain a raw pointer, GPU handle, descriptor slot,
atlas coordinate, sparse page, backend command, native window, or cache path as
authoritative identity.

## Four-Layer Invariant

Every editable 2D resource remains separated:

```text
Authoring document
    canonical scene, texture, tileset, animation, and settings meaning

Temporal semantic history
    validated reversible commands, checkpoints, and branches

Compiled artifact
    deterministic runtime scene, sprite, tile, animation, and dependency data

Physical execution cache
    decoded pixels, atlases, descriptors, buffers, targets, pipelines, and batches
```

Documents and meaningful history are portable. Compiled artifacts are
reproducible. Physical caches are disposable and may be rebuilt after deletion
without changing document identity or visible meaning.

## Coordinate And Viewport Contract

Canvas2D uses explicit coordinate spaces:

- world space for project objects and physics;
- logical canvas pixels for authored 2D layout and sprite placement;
- render-target pixels for offscreen composition;
- presentation pixels for the native surface;
- normalized UV space for texture sampling.

A camera declares logical resolution, origin policy, pixels-per-world-unit,
projection bounds, clear policy, and optional pixel snapping. A viewport
declares physical extent, safe area, scale policy, filter policy, and bar color.
Resize changes presentation state, not authored object transforms.

Required scaling policies are:

- integer fit: largest whole-number scale that fits, with letterbox or pillarbox;
- fractional fit: aspect-preserving fit with explicit nearest or linear sampling;
- fill/crop: aspect-preserving fill with deterministic crop bounds;
- stretch: opt-in non-aspect-preserving output, never the default.

The computed content rectangle is the sole mapping between logical and
presentation coordinates. Rendering, picking, pointer input, screenshots, and
diagnostics use the same mapping. Pointer input outside the content rectangle is
reported as outside the canvas unless project policy explicitly consumes bars.
Zero-sized/minimized surfaces suspend target work without destroying documents.

Game Canvas2D scaling is independent of desktop UI DPI. EpochGui may use native
DPI while the game canvas retains its logical pixel contract.

## Sprite And Material Contract

A compiled sprite item contains stable object identity, transform, bounds,
logical texture/artifact reference, UV region, tint, opacity, material,
animation sample, layer, order, visibility, and culling data. It never embeds
physical texture placement.

A 2D material declares portable intent:

- alpha blend, premultiplied alpha, additive, opaque, or cutout mode;
- cutout threshold where applicable;
- nearest or linear sampling and clamp/repeat intent;
- color-space and channel interpretation;
- optional capability-gated effect variant with a deterministic fallback.

Unsupported material intent fails closed or selects a declared fallback. It is
not silently approximated as a higher-tier feature.

## Deterministic Ordering And Batching

Draw order is determined from authored semantics, not allocation order. The
canonical stable key is equivalent to:

```text
camera/pass -> layer -> sort group -> explicit order -> stable object identity
```

Depth-derived sorting is an explicit project policy with deterministic tie
breaking. Cache rebuilds, atlas repacks, thread scheduling, and backend changes
must not alter visible order.

Batch compilation groups only order-compatible items with equivalent material,
sampling, blend, clip, and physical binding requirements. A backend may split a
logical batch for limits or residency, but may not merge across semantic order
boundaries. Batch output is bounded by project budgets and reports why flushes
occurred.

## Tilemap Contract

Tilesets, palettes, tilemaps, layers, chunks, cells, and map objects use stable
generation-checked identity. Tile values reference logical tileset entries,
not atlas coordinates. Tileset texture material intent uses the same project asset key
and content-derived artifact revision as sprites; authored tilesets never retain
a physical texture handle.

Compilation produces deterministic layer/chunk bounds, visible-cell data,
sprite instances, animation tables, collision artifacts, and dependency keys.
Runtime culling uses camera bounds plus explicit margins and emits visible
chunks in stable order. Empty or unloaded chunks consume no draw allocation.

Large maps are chunked and budgeted. Editing one region invalidates only the
affected chunks and causal dependents. Renderer buffers, atlas placement, and
physics broadphase structures remain disposable caches.

## Input, Physics, Animation, And Audio Seams

Input maps device events into project actions before gameplay consumes them.
Pointer actions use the shared presentation-to-canvas transform. Recorded action
frames, not native event addresses, are the deterministic replay boundary.

Gameplay submits stable body commands to `physics.manager`. A deterministic
fixed-step 2D solver adapter owns integration, collision, contacts, layers, and
masks. Canvas2D consumes immutable accepted transforms; it does not advance
physics during rendering.

`project.sprite_animation` resolves explicit fixed-tick project time to a stable
frame and event set. Its canonical source and compiled Library artifact retain
logical texture identity, source rectangles, pivots, and playback policy;
Canvas2D receives a sampled logical material/UV result and never stores physical
residency handles. Compiled-only game builds restore the same artifact.

Gameplay submits clip/source/bus events to `audio.manager`; `audio.mixer` resolves
bounded decoded PCM into deterministic output frames. `audio.playback_runtime`
owns one optional SDL3 device for the process while Play scenes own only
generation-checked sessions. Renderer replacement does not reopen the device,
and unavailable, saturated, or failed output cannot invalidate scene or
Canvas2D state.
## Render-Graph Composition

Canvas2D contributes logical work to the shared render graph:

```text
resolve immutable scene/artifact observation
    -> resolve texture residency
    -> cull tile chunks and sprite bounds
    -> compile deterministic draw items and batches
    -> clear and render the offscreen canvas target
    -> optional capability-gated canvas effects
    -> compose the content rectangle into the presentation target
    -> editor/runtime overlays as explicitly ordered consumers
    -> present or expose the T0 offscreen result
```

Pass dependencies, target usage, and read/write transitions are explicit.
Canvas2D does not call raw backend APIs, own a second frame loop, or bypass GUI
replay and presentation order. Game UI authored as Canvas2D content is part of
the canvas pass; editor chrome remains an engine/EpochGui overlay.

## Backend Adapter Obligations

Every Canvas2D provider must:

1. report capability and evidence honestly;
2. validate target size, format, sampler, blend, clip, and resource limits;
3. allocate and retire physical resources through backend-owned lifetimes;
4. execute the shared ordering and viewport mapping without API-specific drift;
5. recreate targets and caches after resize, loss, or deletion;
6. preserve alpha, sampling, and color-space semantics or reject the path;
7. expose bounded metrics and actionable failure evidence;
8. release all Canvas2D resources on context replacement or shutdown.

`T0-CPU` must produce a deterministic offscreen/reference result and require no
display. `T1-GL` is the first native presentation proof. Other adapters remain
`Partial` until their own implementation and visual/runtime evidence exist.
Build success alone never promotes them to `Present`.

## Editor, GUI Editor, And Runtime Separation

The standard Editor owns project scene and 2D game authoring: hierarchy,
selection, transform, camera, sprite/tile placement, inspector, Play/Stop,
Run, and Build. It edits the same saved documents consumed by runtime.

GUI Editor owns reusable GUI/layout document authoring and preview. It may use
Canvas2D primitives and compiled texture artifacts, but it is not the game scene
editor and does not own renderer or project-runtime state.

Runtime reads compiled artifacts and selected project settings. Game, mobile,
console, server, and headless profiles can exclude authoring workspaces,
docking, floating windows, and unused backends. Shared artifact readers and
Canvas2D runtime contracts must not depend on editor source.

All editor changes route through validated authoring commands. Widgets never
mutate scene, tilemap, texture, or animation internals directly.

## Settings, Metrics, And Evidence

Project settings own logical resolution, pixels-per-world-unit, scale policy,
default sampling/blend policy, camera behavior, batch/chunk budgets, capability
requirements, and fallback policy. User settings may own editor presentation
preferences. Window size, transient zoom, active tool, and diagnostics filters
are session state unless explicitly promoted.

Controls expose only compiled and evidenced choices. `Partial` choices are
labeled experimental; unavailable choices are absent or disabled with a reason.

Required metrics include:

- logical, target, and presentation extents plus content rectangle and scale;
- visible/culled sprites, tiles, chunks, layers, and cameras;
- draw items, logical batches, physical splits, flush reasons, and draw calls;
- texture lookups, residency misses, uploads, atlas pressure, and cache bytes;
- target bytes, transient bytes, compose time, CPU build time, and backend time;
- dropped/invalid items, fallback selections, recreations, and failures.

Metrics distinguish CPU, GPU, disk/cache, authoring/history, and presentation
costs. They are observations, not canonical state and not passive multicontext
benchmark evidence.

## Persistence And Migration

Projects persist versioned Canvas2D settings and stable references in source
documents. Save, reopen, Play, Run, and Build resolve the same revisions and
project profile. Schema migration is explicit, deterministic, and rejects data
it cannot preserve.

Generated sprite batches, tile chunks, decoded images, render targets, backend
variants, and preview captures live in `Library/` or `Cache/`. Deleting them
must trigger bounded regeneration from source documents and compiled inputs.

## Required Tests

Build-safe contract tests must cover:

- viewport/content-rectangle mapping for resize, aspect mismatch, integer fit,
  fractional fit, fill/crop, stretch, minimized surfaces, and pointer inversion;
- deterministic sprite/tile ordering and stable ties across cache rebuilds;
- material blend, cutout, sampler, UV, tint, and fallback validation;
- batch compatibility, budget splitting, and backend-limit refusal;
- tile chunk compilation, culling, invalidation, and stable collision identity;
- identical document revisions producing identical compiled artifacts;
- physical atlas/bindless/standalone changes preserving logical identity;
- cache deletion and recreation without visible or semantic changes;
- stale handles, invalid revisions, missing textures, zero extents, and device
  loss failing without partial state;
- fixed-step input/physics/animation replay agreement;
- save/reopen/Play/Run/Build consuming the same project state;
- profiles compiling without editor, floating GUI, or unused backend code.

`T0-CPU` must supply reference image/hash and headless tests. `T1-GL` adds native
presentation, resize, alpha, sampling, and operator visual proof. Each later
backend earns evidence separately.

## Delivery Phases

### Phase 1: Canvas Core

Landed in `v0.88.77`: typed camera, viewport, material, draw-item, stable
ordering, tile validation, immutable submission, compose planning, metrics, and
build-safe contracts behind the existing capability and render spines.

Landed in `v0.88.78`: deterministic CPU raster/reference image output, texture
and clip bindings, fixed-point coverage, nearest/linear sampling, alpha modes,
final presentation composition, bounded metrics, image hashes, staged contract
diagnostics, and MSVC/Clang build-safe proof.

Landed in `v0.88.79`: renderer-neutral validated texture uploads and a bounded,
generation-checked residency cache with logical-artifact reuse, pinning,
priority/LRU eviction, upload/byte/entry budgets, backend reset/recreation,
metrics, staged fake-device proof, and context-guarded OpenGL native texture
hooks. The first contract intentionally supports standalone base-mip sampled
color resources; atlas, bindless, sparse, streaming, and mip-aware policies
remain extensions of the same cache boundary.

Landed in `v0.88.80`: full-frame CPU raster presentation through the physical
residency cache, byte-derived artifact identity, explicit image/surface packets,
atomic backend-epoch hook replacement, context-owned OpenGL texture records,
and a primary OpenGL final compositor with viewport/scissor confinement and
complete borrowed-state restoration. MSVC Debug/Release and managed Clang 22
build proof passes; no live-pixel claim is made.

Landed in `v0.88.81`: deterministic RGBA8 artifact payload compilation,
integrity validation, and the project-logical-identity-to-standalone-residency
bridge. Build-safe synthetic-device contracts prove immutable sealing,
synchronous upload copying, exact reuse, transactional backend recreation/reset,
stale-handle rejection, and mutation refusal. Ordinary misses plan eviction
without mutation, reserve host bookkeeping before native allocation, and commit
victims only after the replacement is uploaded and ready. Failed replacement
work therefore preserves prior handles and gauges, while explicit transient
limits expose the temporary physical-memory cost of atomic swaps.

### Phase 2: Sprite Composition

Landed in `v0.88.87`: a runtime-owned project asset registry and texture
resource service authenticate project/source/artifact identity, derive logical
artifact revisions from content, bind owning CPU resource sets, and acquire
optional disposable residency. Contracts cover portable path identity,
collisions, stale handles/revisions, equivalent-content temporal reuse,
duplicate/missing bindings, cache recreation, retirement, and bounded metrics.
Tilesets now depend on logical texture material intent rather than physical
handles.
Landed through v0.89.10: sampled-image capability admission, project-scoped
texture services, serialized Project Library persistence, bounded image import,
semantic scene materials, exact immutable resource closure, snapshot save/reopen,
runtime restoration, and exact OpenGL/T0-CPU capture evidence. Contracts reject
foreign registries, unsupported providers, budget violations, stale or
incomplete bindings, and failed replacement without mutating the published
scene. Next complete interactive Assets assignment, tilemap authoring, and
explicit GL-family/backend adapters without duplicating the renderer spine.

### Phase 3: Tilemap Authoring

Land tileset/palette/tilemap documents, semantic commands, chunk compilation,
culling, collision artifacts, standard-editor tools, and save/reopen.

### Phase 4: Playable Loop

Connect configurable input, deterministic 2D physics, animation, physical
audio, Play/Stop, Run, and Build to the same project-owned scene.

### Phase 5: Portability And Hardening

Prove GLES-shaped limits, editor-free builds, cache deletion/rebuild, restart
and resource retirement, bounded soak, diagnostics, and backend evidence without
weakening T0/T1 behavior.

## Completion Criteria

Canvas2D is complete for the baseline product only when:

- the acceptance map and actor are authored without source edits;
- logical resolution, scaling, letterbox/crop, input mapping, alpha, sampling,
  and deterministic ordering are proven;
- sprites, animation, tilemaps, collision, and audio form one playable loop;
- editor Play, external Run, and Build consume the same saved scene/artifacts;
- `T0-CPU` reference and `T1-GL` presentation pass their required evidence;
- unsupported profiles fail closed and no backend is overclaimed;
- deleting `Library/` and `Cache/` regenerates equivalent output;
- save/reopen and repeated Play/Stop do not leak or duplicate resources;
- runtime products can exclude authoring UI, floating hosts, and unused systems;
- settings, metrics, diagnostics, persistence, tests, and documentation agree
  with the implementation.

The final invariant is:

> Canvas2D expresses stable 2D project meaning once, compiles it
> deterministically, and lets capability-selected providers realize disposable
> physical output without changing the authored game.
