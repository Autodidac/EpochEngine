# Active Pass

## Gate

Consolidate the capability model and land the first baseline 2D vertical slice:
project requirements, honest backend evidence, temporal texture documents,
physical texture-residency plans, Canvas2D composition contracts, and settings
that expose only what the active build can prove.

This is the first gate in the two-month playable-2D critical path defined by
`Engine/docs/engine/capability_tier_architecture.md`.

## Release Baseline

The published `v0.89.06` Windows/Linux runtime, updater, packaging, release tag,
and stable multicontext branch are sealed. Development source is `v0.89.07` so
the accepted runtime has a genuine newer-source update target without changing
the packaged stable identity.
Preserve these accepted source contracts:

- normal editor operation owns one live backend; multicontext is diagnostic;
- whole-editor context replacement preserves editor state and retires the old
  backend instead of opening another editor;
- Engine Arcade owns the sampled render-to-texture graph proof and procedural
  fallback;
- renderer capability reporting separates descriptors, graph build, native
  allocation, presentation, benchmark, and production evidence;
- reusable GUI state belongs in EpochGui while native hosts and backend drawing
  remain engine adapter responsibilities.

## Current Source Truth

The working tree contains these current or in-progress foundations:

- `capability.profile` extends existing capability, budget, `perf.tier`, and
  render-device vocabulary with backend-neutral tiers, feature/evidence states,
  per-subsystem profiles, project requirements, deterministic selection, and
  build-safe no-overclaim checks;
- `platform.budgets` now owns the single performance-tier recommendation
  function consumed by runtime and capability adapters. Project profiles carry
  typed renderer requirements and explicit experimental/software-fallback
  policy; recommended budgets remain distinct from unknown measured cost;
- generated manifests persist `capability_profile`. Missing legacy fields map
  to the portable default without rewriting project files, while duplicate,
  wrong-type, malformed, unknown, and profile-mismatched values fail closed;
- Project, System Info, Settings, and the status dock distinguish active-editor
  admission from selected project-run admission instead of certifying one
  backend with another backend's evidence;
- `render.math` owns shared renderer-neutral vectors and linear color;
- `render.lighting` owns generation-checked light identity, bounded registries,
  immutable frames, environment state, metrics, and reference raster lighting;
- editor and project preview map scene lights through the shared lighting frame
  while retaining truthful reference-solid output until native shading lands;
- `render.ray` owns validated CPU AABB, sphere, triangle, scene, and voxel-DDA
  queries; editor selection uses it and Focus changes the preview camera;
- `physics.manager` owns stable bodies, bounded deterministic commands,
  fixed-step commit boundaries, snapshots, restoration, and metrics, but not a
  solver;
- `audio.manager` owns clips, sources, buses, listener/spatial state, temporal
  scheduling, mix plans, and metrics, but not physical output;
- `voxel.storage` and `water.system` own deterministic sparse/reference state
  without claiming native rendering;
- `scene.tier0` and terrain foundations establish reusable default-scene data;
- `authoring.document` owns shared generation-checked document identity,
  deterministic content revisions, and bounded history policy;
- `scene.document` owns stable scene object identity, typed scene components,
  semantic operations, atomic transactions, undo/redo, Tier-0 construction,
  and deterministic snapshot projection;
- live editor create, duplicate, delete, drag completion, camera reset, helper
  visibility, script rotation, Canvas2D creation, Arcade/Plant Lab preview
  synchronization, and Forest Factory placement now enter one typed command
  gateway. Atomic document transactions own durable meaning and history; the
  entity vector is rebuilt from the committed projection for rendering and UI;
- `scene.interaction` resolves ray selection, drag ownership, and Focus through
  persistent scene object IDs rather than mutable vector positions;
- `scenesnapshot`, `sceneserializer`, and `scene.persistence` own canonical
  `epoch_snapshot 2`, bounded validation, migration-only legacy readers,
  verified temporary writes, and atomic replacement. `scene.runtime` compiles
  that exact revision into the renderer-neutral project-preview projection;
- explicit Save, Play, Build, and Run paths fail closed when durable scene
  evidence cannot be committed or accepted by the runtime projection;
- `authoring.texture` is the first four-layer authoring vertical slice: stable
  document meaning, semantic history, deterministic owning RGBA8 mip artifacts,
  integrity validation, and disposable standalone/atlas/bindless/sparse
  residency planning. Planning is advisory, defaults to standalone-only, and
  does not count atlas/bindless/sparse choices as runtime capability evidence.
  Compressed and color-conversion lanes fail closed;
- `project.asset_registry` now owns deterministic path-derived runtime project/asset
  identity separately from authoring documents, compiled content,
  filesystem case behavior, and
  physical caches. It normalizes portable logical paths, rejects traversal and
  case-fold collisions, issues generation-checked handles and deterministic
  keys, and publishes explicit current source revisions;
- project.texture_resources is bound to one validated project registry,
  authenticates compiled texture source revision, seals full artifact identity,
  and issues Canvas2D references from project asset key plus content-derived
  artifact revision. Foreign project operations fail before handle resolution,
  including identical generation/index handle bits;
- the same project-bound service owns bounded decoded T0-CPU resource sets and
  may acquire disposable native residency through render.texture.artifact.
  Current execution remains an explicit linear RGBA8 mip-0 lane;
- project.texture_admission validates authenticated artifacts against explicit
  sampled-image representation evidence and derives effective dimension,
  sampled-image, resident-memory, and upload limits across project, platform,
  and renderer budgets. Strict policy rejects experimental providers;
- render.canvas2d_scene publishes an immutable exact resource closure. Duplicate,
  missing, stale-revision, invalid, and unexpected texture bindings fail
  atomically while old readers retain their owning resource lifetime;
- authored tilesets now carry logical texture material intent instead of a
  physical `TextureHandle`. Transient render-surface materials retain their
  explicit physical graph-output binding;
- `render.texture.artifact` remains the sole validated artifact-to-residency
  adapter and no longer treats source edit sequence as artifact revision;
- `asset.texture_artifact` now owns the always-built artifact schema, stable
  hash protocol, deterministic little-endian serializer, bounded reader, and
  integrity validator. The authoring compiler re-exports and
  consumes it, while a standalone Clang contract proves artifact consumption
  with the authoring platform and texture editor disabled;
- `package.registry` owns fail-closed extension evidence policy, not download,
  verification, or native activation;
- EpochEngineExtensions now owns the manifest-backed capability technique
  gallery: labeled context/tier scene stations describe required features,
  evidence, fallbacks, and scene intent without importing a second renderer
  spine or claiming unproved native effects;
- Engine Arcade now validates its canonical cabinet/screen scene nodes, sampled
  render-surface material binding, geometry storage, and built-in scene catalog;
- World Outliner now owns `World`, `Assets`, and `Scripting` tabs; the old
  top-level Assets route forwards into the dockable tool surface. Script source
  uses selection-aware caret, clipboard, focus, drag selection, word movement,
  and scrolling behavior instead of a whole-field edit flag;
- EpochGui now owns a reusable primal multi-line text document controller with
  line indexing, revision/dirty state, find/replace, save acknowledgement, and
  standalone tests. Embedded and standalone EpochGui source surfaces are
  synchronized;
- editor Focus updates every active renderer child context that presents the
  selected scene, and default standard/sandbox scenes no longer inject the
  unwanted `StarterCube`;
- OS AI now supports direct offline `llama-cli` inference as a captured,
  timeout-bounded child process beside the existing local API lane. Package
  Manager stages the human-approved Extensions setup plan without starting a
  server or fetching model weights;
- EpochGui dependency work adds portable DPI-aware font, image, input,
  rounded-rectangle, toggle, text, layout, docking, popup, panel, and
  floating-window primitives. Font measurement requires explicit logical-pixel
  height and DPI; the editor adapter exposes rounded controls as an opt-in
  Settings policy;
- Engine Arcade now uses one validated cabinet mesh with screen/control details
  and camera-facing solid culling instead of overlapping preview boxes;
- one shared Arcade attract-pattern contract now drives backend-owned sampled
  scene surfaces in OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX, and Software;
  build-safe contracts prove routing and ownership while visual presentation
  remains `Partial` pending the corrected operator eye test. Every adapter uses
  the shared positive-Z cabinet plane and shared front-view predicate, so the
  sampled display is not visible through the rear face;
- `editor.application` is the shared application registry. Standard Editor,
  Plant Lab, and GUI Editor own separate C++23 implementation units, canonical
  scene seeds, surface masks, camera/dock defaults, pane policy, and
  run/authoring permissions while reusing one editor shell and service spine;
- Plant Lab is the separate launcher editor for authoring custom tree assets and
  reusable forest configurations. It uses `authoring.morphology` for stable-ID
  deterministic temporal branching and owns `PlantLabPreview` authoring objects.
  Forest Factory is the standard-editor placement portal. Its current
  compatibility adapter demonstrates explicit placement from a default authored
  preview; project-library browsing and compiled Plant Lab output import remain
  unfinished. The two products do not merge surfaces, documents, or ownership;
- `temporal.request` owns explicit global/sample time mapping, rates, anchors,
  forward/reverse/frozen direction, bounded exact/nearest/bracket observation,
  truth/reconstruction evidence, retained-history metrics, and
  generation-checked subject retirement; `ecs.entityhistory` is its typed ECS
  facade rather than a dead private history implementation;
- `render.canvas2d` owns validated project settings, camera/viewport scaling,
  generation-checked sprites, logical texture/material declarations,
  deterministic quad batching, tile descriptors, immutable frame submissions,
  final-compose plans, diagnostics, and build-safe contracts; editor snapshots
  and the Canvas2D Project surface preserve and expose its core policy;
- `render.texture.residency` owns bounded, generation-checked physical texture
  records keyed by stable compiled artifact identity, with deterministic reuse,
  priority/LRU eviction, pinning, upload/entry/byte budgets, stale-handle
  rejection, forward-only backend epochs, plan-then-commit miss eviction,
  transactional recreation, explicit transient-replacement limits, upload
  accounting, metrics, and staged fake-device proof. Failed allocation, upload,
  or readiness cannot retire an existing entry;
- `render.device` validates explicit texture upload regions/row pitches, while
  the OpenGL-family device and `opengl.textures` provide context-guarded native
  allocation, base-mip upload, readiness, and destruction hooks without moving
  backend handles into authoring or Canvas2D state;
- `render.canvas2d_presentation` validates complete frame/raster identity,
  derives artifact digests from actual pixel bytes, acquires disposable output
  through the residency cache, and emits an explicit surface/image/native
  packet; `opengl.canvas2d` implements the primary-context final compositor with
  viewport-confined clears, top-left coordinate conversion, context-owned
  texture validation, and scoped GL state restoration. An immutable per-context
  scene exchange now maps committed editor entities to semantic solid sprites
  and invokes that compositor in the protected live scene slot;
- `project.lifecycle` centralizes Save, materialize, Build, Run, wait, and
  focus-existing-runtime decisions with generation-safe attempt tracking;
- all 353 first-party C++ files follow the canonical one-dot owner grammar,
  exact module/file identity, and owned directory layout enforced by the source
  naming validator;
- the launcher opens the three editor applications, selects a live context
  before launch, and keeps update/exit actions direct;
- the Windows parent host elects exactly one baked primary renderer surface. The
  first context and successful promotions cannot undock, while secondary
  diagnostic contexts and routed pane windows retain popout/redock.

These facts are contracts, not blanket runtime claims. Current checkpoint proof
includes MSVC Debug/Release editor builds and contracts, the managed Clang 22
full-engine Release build against GCC 12/libstdc++12 on the Ubuntu 22.04/GLIBC
2.35 baseline, all five no-display Linux engine CTests, the portable
`core.format` contract, and 5/5 standalone EpochGui feature tests. Operator
evidence proves correct filled scene orientation
in SDL3, SFML3, DirectX, and Software. Raylib/Vulkan orientation and repeated
Vulkan replacement remain `Partial`. The seven Arcade sampled scene-surface
implementations compile and pass build-safe contracts but remain `Partial` until
the current source candidate receives visual and switch-cycle proof. Canvas2D
has deterministic `T0-CPU` reference raster and image-hash proof on MSVC and
Clang. Renderer-neutral residency, cache recreation, OpenGL-family hook routing,
presentation packet staging, the compiled primary OpenGL compositor, and
real-hook no-context refusal are build-proven on MSVC and managed Clang 22.
MSVC contracts also prove deterministic temporal texture payload compilation,
artifact-integrity rejection, project-scoped logical identity, exact sampled
image admission, foreign-registry rejection, collision-safe resource keys,
atomic immutable scene closure, cache reuse, backend recreation/reset,
stale-handle rejection, and synchronous upload copy. A clean authoring-disabled
managed Clang configuration independently proves the runtime artifact schema,
hashing, and validator without authoring document/UI linkage. Immutable scene
publication, replacement lifetime, semantic entity mapping, CPU shading, and
protected OpenGL scene-slot routing are build-proven. Live OpenGL
allocation/drawing, operator-visible project textures, Project Library
filesystem integration, editor material binding, and secondary GL share-group
adapters remain Partial.

## Completed Capability Checkpoint

Source v0.89.07 carries forward the v0.89.06 release baseline and establishes
explicit sampled-image capability truth,
evidence-backed texture admission, project-scoped texture publication and
residency, and exact immutable Canvas2D resource closure. Debug contracts prove
foreign-project rejection even for identical handle bits, strict versus
experimental admission, budget reduction, duplicate/missing/stale/unexpected
binding rejection, atomic publication failure, and replacement-safe old-reader
lifetime. It does not claim Project Library filesystem persistence, live native
pixel correctness, editor material assignment, secondary share groups, or
built-game presentation.

## Immediate Implementation Order

1. Persist serialized compiled artifacts in the Project Library and bind
   authenticated project materials into immutable editor scene publication.
2. Compare live T1-GL scene-slot pixels against the T0-CPU reference and verify
   resize, retirement, fallback, GUI replay, and present order.
3. Add explicit SDL3/SFML3/Raylib3 and secondary OpenGL share-group adapters.
4. Extend settings and controls with project texture policy, evidence,
   effective limits, and budget diagnostics.
5. Expose SceneDocument undo/redo and transaction diagnostics through EpochGui
   controls after command ownership is proven in the live editor.

## Backend Repair Within This Gate

Operator evidence proves correct filled editor scene orientation in SDL3, SFML3,
DirectX, and Software. Raylib and Vulkan remained inside-out, and logs prove
Vulkan retirement stopped after native-child destruction but before replacement
creation.

- shared preview geometry defines the clockwise-outward object convention;
- SDL3, SFML3, DirectX, and Software are accepted orientation references for
  their current projected/native paths;
- Raylib rejects camera-facing back sides before projected fill;
- Vulkan selects the corrected scene-solid front face while leaving line and
  GUI pipelines uncullled;
- Vulkan retirement atomically owns the application through device-idle cleanup
  and destroys both graphics pipelines before the logical device;
- queue, GUI replay, depth, and present order remain unchanged;
- Raylib/Vulkan remain `Partial` until build and operator eye proof passes;
- this parity work must not delay the `T1-GL` 2D product unless shared contracts
  regress.

## Settings And Control Rule

Every maturing system must update its settings, controls, diagnostics, and
persistence scope with the implementation:

- unavailable options are absent or disabled with a reason;
- partial capabilities are labeled experimental;
- defaults come from project profile, measured limits, and budgets;
- advanced controls use progressive disclosure;
- EpochGui owns portable control state and layout;
- engine adapters own backend input/drawing, project state, native hosts, and
  evidence;
- mobile/game/headless profiles can omit floating and docking hosts.

## Selective Reference Intake

`Autodidac/tiered_gfx_OpenGL_modular_context_demo` is a technique lab. Only
license-audited, surgically translated ideas may enter Epoch:

- capability/quality controls;
- material and view descriptors;
- RTT/final composition;
- OpenGL techniques behind Epoch render-device/graph contracts;
- manifest-backed CC0 diagnostic assets.

Do not import its alternate resource spine, platform scaffolding, vendored GUI,
hard-coded scenes, raw GL ownership outside the OpenGL adapter, or capability
claims. Advanced effects follow the playable 2D loop.

## Forbidden Expansion

Do not use this gate to:

- edit the sealed updater/release lane;
- claim native PBR, shadows, water, collision solving, physical audio, hardware
  ray query, or RT pipelines without implementation and proof;
- start multiplayer, persistent unscripted AI, planetary terrain, or a broad 3D
  authoring campaign;
- add a second capability registry, renderer resource spine, texture identity,
  node framework, or GUI library;
- make atlases, descriptors, GPU buffers, pipelines, previews, or caches
  canonical authoring state;
- copy unreviewed code/assets from the OpenGL demo or `addons/`.

## Acceptance

The active gate is accepted when:

1. CMake and MSVC metadata contain each new module/source exactly once.
2. Debug and Release `ConsoleApplication1` build.
3. Debug and Release `--engine-contract-self-test` pass.
4. Capability selection proves CPU-only, GLES baseline, OpenGL compute,
   equivalent explicit tiers, deterministic fallback, project matching, and
   no-overclaim behavior.
5. Texture tests prove deterministic revisions, sparse boundedness, semantic
   undo/redo, reproducible compilation, and physical-plan independence.
6. Tier-0 scene tests prove selection, Focus, ground, light, spawn, save/reopen,
   and Run/Build use the same project-owned state.
7. Canvas2D contracts prove deterministic ordering, scaling, blend/sampling,
   offscreen-compose planning, resource-binding validation, and bounded failure.
8. Renderer docs keep Raylib/Vulkan orientation and Vulkan repeated replacement
   `Partial` until build and eye proof.
9. Engine Arcade geometry is nondegenerate, camera-facing culling removes rear
   solids, and the sampled screen remains bound to the cabinet scene contract.
10. Launcher actions open the standard editor, Plant Lab, and GUI Editor
    with prelaunch context policy and without duplicate editor shells.
11. The parent host maintains exactly one non-detachable primary surface while
    secondary diagnostic/routed windows retain popout and redock.
12. Each editor application validates one canonical scene/camera, rejects
    cross-application surfaces, and enforces pane/run/entity policy through the
    shared shell. The standard editor retains Forest Factory import/placement;
    Plant Lab owns the dedicated custom-tree and forest-configuration authoring scene; Forest Factory owns standard-editor placement.
13. Temporal request tests prove forward/reverse/frozen mapping, exact,
    nearest, bracket, boundary clamp, bounded retention, reconstruction flags,
    cumulative metrics, and stale-handle rejection.
14. Texture residency tests prove reuse, bounded eviction, pinning, stale-handle
    rejection, upload refusal, backend reset, and deterministic recreation;
    OpenGL hooks fail safely without an active native context.
15. No updater, release, generated cache, or unrelated operator file is staged.

## Next Gate

Promote the sprite/tile descriptor foundation into executable runtime artifacts,
then add configurable input, deterministic 2D physics, physical audio, and the
editor tools needed to author the acceptance project. The canonical schedule is
`Changes/roadmap.md`; durable follow-up is
`Changes/mission_cache.md`.
