# Epoch Temporal Authoring Contract

## Role

This document defines Epoch's editable-document architecture. It is subordinate
to `capability_tier_architecture.md` and does not maintain a separate roadmap.

`temporal_engine_architecture.md` owns persistent world time/events/branches.
This document owns authoring documents, semantic commands/history, compilation,
previews, workspaces, and source/artifact/cache separation.

The immediate domain is texture and 2D scene authoring. Other domains preserve
the same contract and follow after the playable baseline 2D project.

## Four-Layer Invariant

Every editable asset has four distinct layers:

```text
Authoring document
    canonical editable meaning

Temporal semantic history
    reversible, branchable user intent

Compiled artifact
    optimized, portable runtime representation

Physical execution cache
    atlas slots, bindless descriptors, sparse pages, GPU buffers, pipelines,
    decoded data, previews, and backend state
```

Undo and redo operate on documents/history. Runtime systems consume compiled
artifacts. Physical caches are never canonical and may be deleted.

Examples:

- texture document -> compiled mip/tile artifact -> standalone/atlas/bindless/
  sparse physical image;
- tilemap document -> compiled visible chunks/collision artifact -> vertex/index/
  upload caches;
- material graph -> validated program/layout -> shader/pipeline/binding cache;
- model document -> optimized geometry/collision artifact -> GPU/software data.

## Build And Activation

CMake policy gates include authoring platform, texture, model, node, material,
animation, collaboration, and metrics families. Compilation and project
activation are separate decisions.

A game-only build may exclude authoring UI while reading compiled artifacts. A
headless asset worker may compile documents without renderer presentation. A
mobile runtime may exclude native floating windows, desktop docking, and heavy
editors while using portable EpochGui controls it needs.

Feature gates remove source/dependencies only after real ownership boundaries
exist. A compile definition alone does not prove implementation.

## Shared Document Contract

Every document owns:

- stable generation-checked handle;
- document kind and schema version;
- current content revision and branch;
- dependency references;
- canonical editable state;
- semantic operation journal;
- checkpoint/history policy;
- validation diagnostics;
- compilation state and artifact identity;
- preview requests/state;
- migration policy.

Public identity never uses raw pointers, array offsets, atlas coordinates,
descriptor slots, or cache paths.

A document revision is content-derived plus an ordered sequence where needed.
Equivalent canonical content must produce equivalent compilation/cache keys.

## Commands And Operations

All editing routes through validated commands. Widgets do not mutate document
internals.

```text
UI/tool/script/automation command
-> validate against document and capability policy
-> produce typed semantic operation
-> apply atomically
-> invalidate affected dependencies
-> append journal entry
-> schedule compilation/preview as derived work
```

Operations describe intent:

- create/reorder texture layer;
- apply deterministic brush stroke;
- connect graph nodes;
- paint tile region;
- move scene object;
- extrude selected faces;
- change animation key.

They support deterministic apply, inversion/undo, redo, serialization, branch
creation, dependency invalidation, migration, and optional semantic merge.
Replacing a complete memory buffer is not a preferred operation when intent can
be represented compactly.

## History Policy

History is configurable per document/domain:

- disabled;
- semantic operations;
- checkpointed operations;
- lossless deltas;
- lossy bounded deltas;
- full snapshot debug.

Policies declare checkpoint cadence, memory/disk budgets, compression,
deduplication, and branching. Examples:

- imported runtime texture: no pixel history;
- paint canvas: sparse tile deltas plus checkpoints;
- tilemap/scene: semantic region/object/property operations;
- graph: semantic node/pin/connection operations;
- mesh: topology operations plus sparse attributes;
- sculpt: spatial chunk deltas plus checkpoints.

Budget overflow must checkpoint, compact, reject, or evict according to explicit
policy. It cannot silently lose authoritative edits.

## Texture Document

Texture is the first production authoring domain because it unlocks the Tier-0
2D project.

A texture document supports:

- stable identity, dimensions, format/color-space intent, channels, and revision;
- sparse logical tiles and mip coordinates;
- ordered content and spatial-mask layers, opacity/blend, selection, transforms,
  and filters. Mask layers use sparse single-channel R8 tiles and content layers
  bind them by generation-checked handle with canonical strength/invert intent;
- deterministic brush operations with algorithm version, seed, raw path samples,
  pressure/tilt where available, target layer, and affected tiles. The current
  `round_path_v2` keeps those raw control samples as semantic intent and derives
  pressure/tilt-interpolated round stamps at half-radius spacing with a
  quarter-pixel floor. Derived stamps are bounded execution data, never journal
  identity; admission rejects an over-budget path before tile discovery or
  allocation. `round_stamp_v1` remains the exact isolated-stamp compatibility
  program. The Assets control surface authors exact opacity, hardness, and a
  nonempty RGBA channel mask into this same descriptor; those values affect
  deterministic raster output and remain part of journaled semantic intent;
- semantic undo/redo, checkpoints, history budgets, dependencies, diagnostics,
  and deterministic compile;
- future procedural graph outputs without changing document identity;
- CPU/reference evaluation and capability-selected accelerated execution.

Only touched tiles require edit storage, deltas, upload, residency, and cache
writes. Untouched tiles may be implicit clear, inherited, procedural, lazily
decoded, or absent.

Nondeterministic/external brush output stores changed tile payloads. Deterministic
brushes may replay from operations plus periodic checkpoints.

Layer transforms and filters are canonical document semantics, not physical
texture state. The current deterministic compile mirrors within each mip first,
then applies signed integer translation; authored offsets scale by `2^mip` using
signed integer truncation. It then applies grayscale luma, RGB inversion, and
signed brightness in that order while preserving alpha. These operations never
rewrite sparse source tiles. The Assets surface authors them through EpochGui and
records one semantic layer-properties operation per accepted change.

Texture source schema 3 extends the bounded schema-2 layer descriptor table with
layer role and generation-checked spatial-mask binding records. Mask layers own
sparse R8 coverage; absent mask tiles reveal content, painted coverage conceals
it, and strength/invert are canonical semantic properties. Schema-1 and schema-2
sources are verified against their exact legacy hashes and migrated to the
current in-memory identity without rewriting operator source; later explicit
serialization emits schema 3. Truncated, mismatched, stale-mask, future, or
integrity-broken sources fail atomically.

## Compiled Texture And Residency

Compilation produces logical artifacts such as channel/mip/tile data and content
hashes. Runtime residency chooses among:

- standalone image;
- compatibility atlas;
- bindless table;
- sparse/virtual pages;
- CPU/software storage.

The choice uses capability, project policy, texture size/format, update rate,
sampling, memory/upload budgets, and workload. Atlases remain valuable physical
caches for compatibility and batching; they are not canonical asset meaning.

The Assets projection derives aspect-preserving thumbnails from compiled
artifacts and batches up to 1,024 compact 96x96 surfaces into the existing
EpochGui runtime atlas with one upload transaction per catalog identity.
Thumbnail pixels, atlas placement, and sprite handles are disposable cache state;
the live editable preview continues to read the active authoring document even
when no compiled catalog row is selected.

Changing atlas placement, descriptor index, sparse mapping, compression variant,
or GPU backend must not change the texture document revision.

The runtime project registry supplies stable project/asset identity and the
currently accepted source revision; it does not own compiled bytes or physical
placement. A logical texture reference combines that stable asset key with a
content-derived artifact revision. Source sequence remains separately
authenticated, so undo or branch reconstruction that returns to identical
content can reuse the same compiled artifact and cache key.
The current key is deterministic from canonical logical path; persistent imported
asset IDs and semantic rename/move migration remain required before path changes
can preserve the same identity.

`project.texture_resources` is the bounded runtime consumption service for the
first linear RGBA8 base-mip lane. It validates registry revision and complete
artifact integrity before producing CPU views or disposable residency.
`project.texture_library` and `project.texture_pipeline` own serialized reads,
atomic publication, restore, and exact leases; `project.texture_admission` owns
capability/budget policy. BMP, TGA, and P6 PPM import is present. Broader
formats/mips, editable source-document serialization, and complete authoring UI
remain later work.

Required metrics include decoded/compressed bytes, resident/virtual bytes, tile
count, mip cost, atlas padding, upload cost, history cost, compilation time, and
cache pressure.

## 2D Scene And Tilemap Documents

The next production documents are:

- tileset/palette;
- tilemap with stable layer/chunk/cell/object identity;
- sprite animation;
- scene with stable objects/components and project-owned settings.

Compiled outputs contain deterministic visible chunks, sprite batches, collision
shapes, animation tables, and dependency keys. Renderer buffers, atlas regions,
physics broadphase data, and audio device state remain caches.

Editor selection, placement, painting, transforms, deletion, Focus, spawn, and
scene settings route through commands. Play, Run, and Build consume the same
saved document/artifacts.

## Shared Node Graph

Epoch eventually uses one typed graph foundation for texture, material, particle,
animation, audio, AI, procedural model, and simulation domains.

It owns stable node/pin/connection identity, typed connections, validation,
cycle rules, defaults, subgraphs/functions, parameters, migrations, semantic
history, diffing, diagnostics, and evaluation cache keys.

Each domain defines allowed nodes/types, cycle/feedback policy, side effects,
execution targets, and budgets. Nodes cannot invoke unrestricted native code.

Graphs compile through validated IR with type checking, constant folding, dead
node removal, dependency analysis, resource estimates, scheduling, and backend
selection into CPU, SIMD, GPU compute/graphics, or software/reference plans.

The 2D product does not wait for the general graph editor. Texture compilation
must leave a clean seam for it.

## Morphology And Forest Documents

`authoring.morphology` owns renderer-neutral stable node, segment, and terminal
identity, deterministic branching recipes, per-organ temporal ranges, sampled
growth, and voxel LOD planning for plant, vascular, respiratory, electrical,
coral, and generic branching domains.

`forest.factory` now wraps that spine in a stable `ForestAssetDocument`. Profile
changes are bounded semantic operations with undo/redo and deterministic content
revision. Compilation derives a validated morphology graph and time sample,
renderer-neutral preview segments/leaves, a multi-level voxel LOD plan, and
bounded voxel occupancy from the same source revision. Plant Lab owns one live
document, exposes semantic edit/undo/redo controls, and projects only its current
compiled artifact. Forest Factory remains the standard-editor placement portal,
places that same compiled revision into the active scene, and does not own or
silently fork Plant Lab documents. Package staging records matching source
revision, content hash, morphology hash, preview counts, voxel LOD levels, and
bounded occupancy.

The compiled preview is portable scene meaning, not native renderer proof. Each
context consumes shared trunk, branch, and leaf-cluster geometry through the
existing scene projection and owns only disposable physical resources. A
bounded source codec, atomic `Assets/` and `Library/` publication/reopen,
interactive typed-node editing, sparse voxel materialization, mesh/impostor
compilation, and accepted live pixels in every context remain delivery work.

## Later Authoring Domains

After the playable 2D loop:

- material documents add logical texture references, alpha/PBR/stylized schemas,
  instances, diagnostics, and CPU/GPU previews;
- model documents add stable vertex/edge/face identity, sparse attributes,
  topology operations, modifiers, sculpt chunks, collision, LOD, and meshlets;
- animation/effects/audio/simulation documents add explicit temporal and
  side-effect policies;
- unified scene authoring composes every document through shared services.

Large model/texture work remains sparse/chunked and does not store full GPU
buffers in undo history.

## Shared Authoring Services

Workspaces are configurations over shared services:

- document manager;
- selection and command routing;
- history, undo/redo, checkpoints, and branches;
- inspector and property validation;
- asset/dependency browser;
- node graph;
- viewport/canvas and direct manipulation;
- compilation and artifact registry;
- previews;
- diagnostics/metrics;
- persistence/migration;
- optional collaboration.

The first workspaces are Scene and 2D/Texture. Model, Material, Animation,
Effects, Audio, Simulation, Timeline, Profiler, and Packages follow as the shared
services mature.

## Preview Contract

Preview requests are:

- budgeted and cancellable;
- cacheable and generation-checked;
- lower priority than interaction;
- capability-selected with software fallback;
- invalidated by document/dependency revisions;
- released when workspaces close without destroying documents.

Preview targets, decoded images, shaders, GPU buffers, and atlas allocations do
not enter history unless explicitly captured as source content.

## Settings And UX

Authoring settings and controls ship with each domain:

- progressive disclosure for advanced policy;
- one consistent inspector, units, validation, reset, and history indicators;
- direct manipulation where safe;
- search-driven commands and context-aware creation;
- visible causality: what changed, why it rebuilt, dependencies, branch, and cost;
- safe defaults that do not require descriptor/atlas/barrier knowledge;
- no modal workflow traps;
- actionable diagnostics instead of fake progress or hidden failure.

Portable control state belongs in EpochGui. Engine adapters own renderer drawing,
input translation, native hosts, project state, and backend evidence. Floating
and docking hosts are optional desktop tooling.

## Source And Cache Layout

Projects separate portable source from generated output:

```text
Project/
  Assets/       canonical documents and imported source
  History/      semantic operations, branches, checkpoints
  Library/      compiled artifacts
  Cache/        previews, atlases, GPU/backend variants, temporary evaluation
```

`Assets/` and meaningful `History/` are portable. `Library/` and `Cache/` are
reproducible and may be deleted.

## Extension And Collaboration Safety

Preferred extension order:

1. declarative schemas/nodes;
2. sandboxed scripts;
3. constrained WebAssembly-like modules;
4. explicitly admitted source add-ons compiled into generated projects, or
   separately launched child tools.

Add-ons declare inputs, outputs, determinism, side effects, memory estimates,
execution targets, version, and migration. Downloaded projects never silently
compile source or launch tools, and Epoch exposes no native editor-plugin lane.

Collaborative clients submit semantic commands. A server validates commands and
records authoritative document revisions. It rebuilds its own shaders, atlas
layouts, GPU buffers, and caches rather than trusting foreign physical artifacts.
Large content uses the engine's offer, consent, hash, quarantine, and capability
grant process.

## Tests

Every domain adds build-safe tests for:

- stable handles and stale generations;
- deterministic serialization/content revision;
- command validation and atomic failure;
- operation replay, undo, redo, branch, and checkpoint reconstruction;
- dependency invalidation;
- history/storage budgets;
- compilation reproducibility;
- cache deletion/recreation;
- preview cancellation and release;
- widgets not directly mutating documents.

Texture adds sparse allocation, deterministic brush/tile replay, tile-delta undo,
large-canvas boundedness, residency-plan independence, and CPU/accelerated
agreement where a native path exists.

Tilemap/scene adds stable object identity, deterministic chunk compilation,
selection persistence, collision artifact generation, and save/reopen/Run/Build
agreement.

## Delivery Order

The authoring dependency order follows the canonical eight-week roadmap:

1. capability/project profile and Tier-0 scene;
2. texture document/history/compiler/residency;
3. Canvas2D and sprite batch;
4. tilemap and 2D scene authoring;
5. input and deterministic 2D physics;
6. audio, animation, Run, and Build;
7. integration/portability;
8. hardening.

Afterward: shared graph, material, model, procedural modeling, effects/animation,
unified scene, collaboration, and UX refinement.

## Invariants

- Documents and semantic history are canonical.
- Commands validate before operations commit.
- Widgets do not mutate internals directly.
- Public identity is stable and generation-checked.
- History is semantic, deterministic, bounded, and branchable.
- Compiled artifacts are reproducible.
- Physical caches are disposable and capability-selected.
- Every domain shares common authoring services instead of becoming a separate
  application.
- Heavy authoring/floating GUI can be compiled out of products that do not need
  it.
- Dangerous extensions are sandboxed or explicitly installed.
- Texture and 2D scene authoring deliver the first complete product loop before
  broad editor expansion.
