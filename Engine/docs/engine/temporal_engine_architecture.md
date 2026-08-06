# Epoch Temporal World Contract

## Role

This document defines Epoch's authoritative world-time and persistence model.
It is subordinate to the product and capability plan in
`capability_tier_architecture.md` and does not set an independent delivery
schedule.

`temporal_authoring_platform.md` owns editable asset documents. This document
owns world addresses, events, transactions, branches, observations, replay,
persistence, simulation fidelity, side effects, and server authority.

The complete temporal world is a target architecture. Current modules implement
bounded portions and must not be described as the full system.

## Mission

Epoch treats time as a world coordinate rather than a frame counter. A world can
be observed, edited, reconstructed, compared, branched, and replayed at explicit
times without making renderer caches, physics integrator state, or backend
resources authoritative.

The central invariant is:

```text
meaningful causes are stored
reconstructible consequences are derived
physical execution state is disposable
```

## World Address

Persistent queries use an explicit address:

```cpp
struct WorldAddress
{
    TimelineId timeline;
    BranchId branch;
    TimePoint time;
};
```

Stable IDs include generation or equivalent stale-reference protection. Raw
pointers, vector offsets, descriptor indices, and cache locations are never
public world identity.

An immutable `WorldObservation` is resolved for an address and requested
fidelity. Renderers, physics, audio, AI, editors, capture, and networking consume
observations instead of mutating the world store directly.

## Time Domains

Epoch keeps explicit domains:

- real time;
- simulation time;
- physics time;
- presentation time;
- animation time;
- effects time;
- audio time;
- network time;
- editor time;
- replay time.

A domain declares rate, direction, pause state, quantization, parent mapping, and
allowed side effects. Forward, stopped, backward, and discontinuous jumps are
first-class. There is no universal `deltaTime` that silently drives all systems.

The immediate 2D product uses explicit fixed physics time, animation time, audio
scheduling, and editor/play state. It does not wait for the complete persistent
world.

## Implemented Request Mapping Foundation

`temporal.request` is the current bounded observation foundation. It names each
quantity instead of passing unrelated raw seconds through one generic scalar:

```cpp
GlobalTime
SampleTime
Duration
TemporalRate
TemporalAnchor
TemporalMapping
```

The mapping is explicit:

```text
sample_time =
    anchor.sample
    + (global_time - anchor.global)
    * rate.sample_seconds_per_global_second
```

Positive rates observe forward, zero freezes the sample coordinate, and
negative rates observe backward. Inverse mapping is available only for finite,
nonzero rates.

`RequestDrivenHistory<State>` owns generation-checked subjects and bounded,
time-ordered samples. Requests choose:

- exact lookup;
- nearest retained sample;
- a lower/upper bracket with interpolation weight;
- optional bounded edge clamping.

Every result carries status, requested/lower/upper sample times, exact/nearest/
clamped flags, declared truth class, and whether exact reconstruction is still
required. An exact channel sampled through nearest, bracket, or boundary clamp
does not silently become exact truth.

Retention rejects invalid, stale, older-sequence, and already-evicted samples.
Lifetime append/replacement/eviction/rejection metrics survive subject
retirement while live subject/sample/capacity metrics remain separate. Borrowed
sample spans are mutation-scoped; retained consumers request a copy.

This module does not yet provide immutable events, transactions, world pages,
branches, dependency invalidation, or authoritative reconstruction. Those
remain later layers over the request contract.

`Autodidac/VoxelRayBenchmark` is an external evidence laboratory for the
request-driven voxel/ray design. Epoch may ingest immutable result packets,
licensed algorithmic findings, and reproducible command/configuration metadata.
A repository label, bootstrap document, or one local result does not prove a
production capability. Concurrent multicontext measurements are excluded from
automatic editor-backend selection because they measure contention rather than
the normal one-backend editor path.

## Implemented Scene Foundation

The first bounded persistent scene slice is now real without claiming the full
temporal world:

- `authoring.document` owns generation-checked document handles, deterministic
  content revisions, and history policy;
- `scene.document` owns stable scene object handles, typed components, semantic
  operations, transactions, undo/redo, and deterministic snapshot projection;
- `scene.interaction` resolves ray hits, drag ownership, and Focus through stable
  object identity;
- `scenesnapshot` and `sceneserializer` own canonical `epoch_snapshot 2` text;
- `scene.persistence` validates, verifies, and atomically replaces scene files,
  while old v1/editor text is accepted only as migration input;
- `scene.runtime` compiles a validated snapshot revision into a deterministic,
  renderer-neutral runtime projection.

Explicit editor Save, Play, Build, and Run paths must commit valid scene evidence
before continuing. The live editor entity collection still adapts existing UI
code and is not yet the canonical mutation owner. The next integration step is
to route those mutations through `SceneDocument` semantic commands.

## Events And Transactions

Authoritative mutations are immutable typed events. Events include stable
identity, schema version, time/branch, causal inputs, validated payload, author,
and content hash.

Events commit through atomic temporal transactions:

```text
validate every operation
-> resolve conflicts and invariants
-> write immutable events/pages
-> publish one new branch revision
```

A failed transaction publishes nothing. Persistent state cannot be partially
updated.

Events describe semantic causes such as object creation, transform change,
impulse, route change, collision consequence, property edit, and accepted
external result. They do not store arbitrary process memory.

## Branches

The installed/base world is immutable. Saves, edits, mods, predictions, replays,
and alternate histories are copy-on-write branch overlays containing:

- divergence identity;
- immutable events;
- changed content-addressed pages;
- branch-local checkpoints;
- dependency invalidation;
- explicit metadata and policy.

Branches share unchanged pages and compiled artifacts. Attach, cherry-pick,
semantic merge, and rebase remain explicit operations with validation; bytewise
cache merging is forbidden.

## Storage

The world store is content-addressed and page-based. It owns:

- schema/version metadata and migrations;
- bounded decoders and validated sizes;
- hashes and deduplication;
- immutable pages and checkpoint manifests;
- references and reachability;
- adaptive checkpoint spacing;
- hot, warm, and cold retention;
- branch-aware garbage collection;
- crash-safe journal/commit boundaries.

Hot history favors interactive undo/replay. Warm history keeps checkpoints and
compressed operations. Cold history retains portable branch meaning while
reconstructible caches may be removed.

## Observations And Reconstruction

Observation resolves only the requested objects, regions, systems, and fidelity.
It may combine a checkpoint, later events, sparse motion segments, procedural
identity, and derived artifacts.

Reconstruction is deterministic for exact channels. Bounded or visual channels
must state their error/quality policy. Disposable channels may be regenerated
or omitted.

Past edits invalidate only their causal future. Dependency records identify
objects, regions, systems, resources, and time ranges so unrelated history and
compiled artifacts remain reusable.

## Truth Classes

Every temporal channel declares one class:

- **Exact**: must replay identically from authoritative data.
- **Error-bounded**: may use a declared approximation within a tested bound.
- **Visual-only**: may differ without changing authoritative outcomes.
- **Disposable**: may be dropped and reconstructed or ignored.

Examples:

- accepted commands, ownership, inventory, and collision outcomes are exact;
- compressed trajectories may be error-bounded;
- particles and interpolation may be visual-only;
- GPU buffers, descriptor tables, visibility lists, and previews are disposable.

## Motion And Discontinuities

Continuous motion uses sparse model segments plus correction keys. Segments are
cut at authoritative discontinuities:

- teleport;
- impulse or collision;
- parent change;
- route/model change;
- activation/deactivation;
- branch edit that changes consequences.

Symmetry and compression may reduce storage only when identity, error, and
reconstruction policy remain explicit.

## Determinism And Nondeterminism

Persistent behavior cannot depend on global random-call order, thread schedule,
unordered-container iteration, current GPU, or rerunning an AI model.

Randomness is identity-keyed by stable inputs such as world address, entity,
operation, stream, and sample. Nondeterministic external/AI outcomes that become
authoritative are recorded as events with provenance.

## Persistent Simulation

Regions advance between meaningful scheduled changes rather than consuming one
global tick forever. Observer demand selects fidelity:

- exact interaction;
- physics;
- visual;
- audio;
- network;
- editor recording;
- historical query.

Unloaded actors retain scheduled temporal meaning instead of disappearing.
Procedural state is regenerated from stable identity and branch-local edits.

Physics adapters consume world observations and fixed boundaries, then return
validated consequences. `physics.manager` remains scheduling/snapshot ownership;
a solver is an implementation, not a second authority.

Audio adapters consume explicit-time `FrameMixPlan` data. Reverse/seek and
side-effect policy remain explicit. Device state is disposable.

AI operates as active, coarse, scheduled, or dormant simulation. Decisions that
matter are recorded; models and hidden reasoning are not authoritative state.

## Backward Presentation

Rendering at time `T` consumes an observation at `T`. Temporal presentation
history is keyed by timeline, branch, sample time, direction, and camera.
Timeline jumps, branch changes, and direction changes invalidate incompatible:

- temporal antialiasing;
- upscaling history;
- motion vectors;
- exposure;
- occlusion/visibility;
- particle and animation caches.

Backward playback reconstructs state and visual channels; it does not reverse
external side effects.

## Execution And Resource Fabric

The capability plan selects an implementation after observation. A demand-driven
execution graph may span:

- CPU and SIMD work;
- GPU graphics/compute/transfer;
- file IO and decompression;
- page loading and reconstruction;
- replay and replication.

Logical resources keep stable handles across RAM, GPU memory, compressed cache,
SSD, and approved content/reconstruction sources. Physical residency is selected
by capability and budget and can be discarded.

Software remains a first-class reference device consuming the same observations,
logical resources, geometry/material meaning, and temporal contracts. It does
not emulate a GPU API.

## Networking And Branch Safety

Clients submit validated commands, never authoritative events. A server validates
commands, commits authoritative events, journals durable revisions, and publishes
approved projections.

Shared branches are signed/content-addressed packages. Foreign branches enter as
quarantined read-only seeds. A server validates and replays them in isolation
before creating its own authoritative branch.

Joining proceeds in bounded stages:

1. safe server-approved projection and placeholders;
2. classified optional content offer;
3. explicit user/operator consent;
4. hash/license/provenance verification in quarantine;
5. bounded capability grant.

Shaders, scripts, models, assets, and native extensions have different risk
classes. Native extensions never auto-download or execute. Network listeners and
server activation remain explicit human-approved capabilities.

## Side-Effect Boundary

Timeline rewind cannot unsend network traffic, reverse purchases, undo exports,
launch or terminate processes retroactively, mutate accounts, or erase physical
world effects.

External actions cross a gateway that records request, authorization, execution,
and returned outcome. The outcome may become a new authoritative event; the
external world is not treated as reversible.

## Module Ownership

Expected ownership remains modular:

- temporal identity, clocks, events, transactions, pages, branches, observation,
  dependency/invalidation, and side-effect gateways;
- scene/project serialization over those contracts;
- physics, audio, AI, renderer, voxel, water, navigation, and package adapters;
- editor timelines and history tools consuming public temporal services.

Subsystem adapters cannot hide authority in private wall-clock loops or physical
cache state.

## First Production Slice

The full temporal campaign starts only after the 2D delivery gate. Its first
bounded proof remains:

1. `TimePoint`, stable IDs, and world address;
2. immutable event journal;
3. atomic transaction;
4. immutable page checkpoint;
5. local branch overlay;
6. reversible transform edit;
7. arbitrary-time observation;
8. sparse motion segment;
9. analytic visual effect;
10. backward presentation test;
11. branch export/reimport.

Acceptance proves exact authored undo, nonlinear redo, deterministic replay,
compact branching, arbitrary-time reconstruction, portability, crash recovery,
and bounded storage before multiplayer, unscripted AI, full persistent physics,
or advanced rendering expands.

## Invariants

- Time and branch are explicit in persistent queries.
- Events and pages are immutable; transactions publish atomically.
- The base world is immutable and branches share unchanged data.
- Meaningful causes are authoritative; caches are derived.
- Nondeterminism is keyed or recorded.
- Past edits invalidate only their causal future.
- Observers select fidelity without erasing persistent meaning.
- Backward presentation invalidates incompatible history.
- Side effects cross an irreversible gateway.
- Capability selection chooses execution but never changes canonical world
  identity.