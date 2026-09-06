# Epoch Canvas2D Architecture

## Authority And Scope

This document is the canonical contract for Epoch's renderer-neutral 2D
composition path. It is subordinate to `capability_tier_architecture.md` and
specializes the document/runtime separation in
`temporal_authoring_platform.md` for the complete playable 2D product objective.
The roadmap owns its current priority and delivery schedule.

`Changes/active_pass.md` owns the current acceptance gate,
`Changes/roadmap.md` owns scheduling, and `renderer_feature_matrix.md` owns
implementation evidence. This document defines architecture, not evidence or
release history.

Canvas2D carries one authored project scene through save/reopen, editor Play,
external Run, and Build without introducing a second renderer spine. The
current foundation includes renderer-neutral camera/viewport policy, stable
sprite and material identity, deterministic batching, T0-CPU raster and image
hashes, authenticated texture artifacts, disposable residency, semantic scene
assignment, sparse tilemap authoring/compilation, generated-runtime preparation,
and backend presentation adapters for all seven baseline contexts.

Approved OpenGL capture matches the T0-CPU reference across 1,178,872 pixels.
Non-OpenGL live pixel parity, resize and repeated-replacement evidence,
interactive texture/tilemap eye proof, sRGB/compressed/mip-chain execution,
secondary GL share groups, and the complete acceptance-game loop remain
`Partial`. Version chronology belongs in `Changes/changelog.txt`; this document
records the current architectural contract only.

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
Runtime restoration authenticates every texture dependency declared by the map
artifact, but the immutable scene lease contains only unique logical textures
referenced by the visible textured sprite set. A map with declared textures and
no visible textured cells therefore publishes an empty valid lease; it does not
retain an unrelated physical binding merely because the dependency exists.

The editor tile workspace resolves object hit targets through stable map-object
handles before underlying cell tools. Inspector changes use a disposable staged
descriptor; Apply commits one semantic property operation while Duplicate and
Delete use the same temporal document. Object hierarchy rows and runtime preview
output resolve from the canonical snapshot rather than mutable UI indexes.
Direct dragging captures that stable handle, stages clamped movement using the
rotated object bounds, and publishes one semantic operation on release; pointer
motion itself never enters document history.

Layer rows retain a portable UI index only for view restoration; authoring
commands resolve the selected generation-checked layer handle. Name, visibility,
lock, collision-source intent, phase, draw order, opacity, and parallax are
staged together and Apply records one layer-property operation. Create and
Duplicate produce unique canonical names, Duplicate selects a deterministic
topmost draw order, Delete preserves at least one layer, and locked layers reject
cell edits. Handoff, source reload, compiled Library restore, and runtime
visibility metrics must all resolve the same accepted descriptors.

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

GUI Editor owns reusable GUI/layout document authoring and preview. Its canonical
project source is `Assets/Gui/main.epochgui`; the scene/canvas representation is
a disposable projection of that exact temporal document revision. It may use
Canvas2D primitives and authenticated compiled texture artifacts, but it is not
the game scene editor and does not own renderer or project-runtime state.

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

`project.gameplay2d_runtime` exposes one request-driven logical cost snapshot.
It compiles the current immutable Canvas2D submission without rasterizing and
joins sprite/batch geometry, logical RGBA8 texture bytes, actor collision/input
activity, and process-audio residency. The Project Runtime Preview samples that
snapshot on a bounded visible-frame cadence. Native allocations, atlas pressure,
GPU residency, and backend timing remain backend-owned measured evidence.

`platform.budgets` owns the portable limits consumed by that assessment:
logical canvas pixels, logical texture bytes, visible sprites, logical batches,
collision surfaces, and resident audio bytes. Mobile (`T1-GLES`), deck,
desktop, and editor tiers are bounded independently. A nonzero Canvas2D
rejection count is always a budget violation regardless of numeric headroom.

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

## Current Foundation

The shared spine currently provides typed camera, viewport, material, draw-item,
stable ordering, deterministic CPU reference rasterization, immutable image
artifacts, project-scoped logical texture identity, bounded generation-checked
residency, OpenGL presentation, capability admission, Project Library
persistence, semantic scene material bindings, exact save/reopen restoration,
and build-safe contracts. Shared contracts prove resize invalidation/reuse,
exact native packet metadata, malformed-host-surface refusal, rollback after a
failed new native dispatch, 64 same-epoch revisions bounded to two transactional
slots and one live texture, 64 forward-only backend replacements, and idempotent
balanced retirement on a fake device. The `render.canvas2d_limits` adapter
maps selected capability-profile budgets into the same compile, CPU-raster,
native-upload, and residency ceilings for all seven baseline renderer paths.
Presenter-backed paths receive a two-slot transactional output budget so a
rejected replacement cannot evict the currently displayed image; Vulkan uses
the mapped canvas/upload ceiling instead of a desktop-size constant, and
Software applies compile/raster bounds without claiming GPU residency. Raster
cache keys include limits and policy, preventing a stricter tier from reusing
an image admitted under a broader tier. This boundary passes MSVC Debug/Release,
the full managed Clang 22 build, and all 32 Clang CTest contracts.
Physical atlas, bindless, sparse,
streaming, mip, and backend-native caches remain disposable implementations
behind that boundary.

## Remaining Integration And Acceptance

These are unfinished acceptance results, not a second schedule or a request to
rewrite existing foundations. The roadmap owns ordering. Preserve the full
map, actor, animation, collision, audio and GUI product loop through Save,
Close/Reopen, repeated Play/Stop, external Run, Build and cache regeneration.

1. Reuse canonical GUI source compilation, `project.gui_library` publication
   and `project.gui_runtime` restoration already described in
   `runtime_and_editor_workflows.md`. Finish and prove editor-free widget
   action/focus/input behavior against the same accepted artifacts without
   importing authoring history, docking, floating hosts or editor state.
2. Complete native interaction and acceptance-game proof against one saved
   project revision. Process-owned physical controller sampling and semantic
   keyboard/controller binding and dead-zone editing already have production
   owners; reuse them and verify real device input, focus transitions and
   persistence instead of rebuilding those systems. Prove decoded sound cues
   and looping music through physical output, including unavailable-device
   behavior, repeated Play/Stop and isolation from editor-camera controls.
   Authored solid, one-way, rising-right, falling-right, and
   custom-box collision now reaches exact Library/preview artifacts and the
   deterministic reference solver; sensor dispatch remains gated.
3. Complete GL-family and explicit-backend Canvas2D visual evidence through the
   shared submission and residency contracts. Do not duplicate authoring state
   or the renderer spine inside a backend.
4. Prove GLES-shaped limits, editor-free builds, cache deletion/rebuild,
   restart and native resource retirement, per-backend bounded soak, and the
   seven-backend acceptance matrix without weakening T0/T1 behavior.
