# Epoch Temporal World Contract

## Role

This document defines Epoch's authoritative world-time and persistence model.
It is subordinate to the product and capability plan in
`capabilities.md` and does not set an independent delivery
schedule.

`authoring.md` owns editable asset documents. This document
owns world addresses, events, transactions, branches, observations, replay,
persistence, simulation fidelity, side effects, and server authority.

The complete temporal world is a target architecture. Current modules implement
bounded portions and must not be described as the full system.

## Sampled water and project movement — October29 source

```text
central clock → editor timeline playhead → analytic water mesh
                                       → copied project movement keys
```

The clock still advances monotonically. The editor timeline chooses a forward,
paused, sought or backward presentation sample. `water.surface` builds a bounded
indexed mesh from `water.system` at that explicit time. A normal Water scene
object participates in creation, Undo/Redo and save/reopen. Ordinary project
runtime receives the existing clock's sample through its own Context, rather
than storing mutable time on a Scene shared by several contexts.

Project movement starts from the reusable Engine template but is copied into
the project's `scripts/timeline_translation.ascript.cpp`. The separate sized
script-host V2 entry submits finite translation keys. The host copies them into
the existing temporal history representation; playback never calls an unloaded
script DLL. The track is bound to a project generation, scene and object, and
offsets the object's authored position without overwriting it. There is no
automatic loop. Removing the track restores the authored pose.

Actual Debug/Release owner checks and the combined Editor builds/contracts pass
for these source tranches. Native water/model acceptance is still open. Movement
source persists with the project; its copied live track must currently be
registered again by running the script after reopening. Standalone generated
runtime movement-host integration is still open. This is not physics reversal,
full event-history replay, rigging, or animation authoring.

### One Water system; optional FFT implementation

The operator's target is one editable Water system, from a still pool or pond
to a rough ocean. Users should not have to choose between unrelated water tools.
The FFT package supplies its spectral wave implementation and a standalone
example. Planetary uses that same implementation for oceans: choosing an ocean
must resolve the FFT package dependency through the existing package installer.
A dry planet does not need the ocean dependency. Missing packages must be shown
before generation, with the normal installation choice, not hidden manual wiring.

Water settings, transforms, project persistence and timeline control must remain
shared. Planetary supplies the curved surface, body scale and local coordinates;
the selected wave implementation supplies displacement and surface normals.
Sea of Thieves and No Man's Sky are visual references, not present capabilities.
Foam, reflection, refraction, underwater rendering and caustic lighting need their
own renderer evidence; wave displacement alone does not implement them.
Caustics must form changing light patterns from the interacting wave surface:
bright lines join, separate and concentrate as waves change. Translating a fixed
grid or texture is not acceptance. Their time sample must match the water;
seeking back to the same time must reproduce the same pattern. This remains
unimplemented, including its renderer, performance and visual checks.

The optional FFT package now supplies the planetary wave evaluator. Typed retained
water geometry records its source triangles, clipping basis and coordinate frame;
runtime resampling keeps the ground unchanged and clips the current shoreline.
The FFT converts its discrete spectral density to metres after the normalized
inverse transform. This includes the frequency-cell width and cancels the FFT's
numerical division by resolution squared. Resolution therefore refines the same
physical sea instead of shrinking its waves. The previous default was about
one centimetre peak height; the corrected CPU samples are roughly metre-scale.
That correction does not prove visible planetary animation or finished shading.
CPU Debug/Release checks reproduce time0→1.25→0 through the real provider. Existing
owned child-control pipes carry explicit Editor Play/Pause/seek/reverse/rate changes
to the existing runtime timeline. This changes presentation time; planetary physics
still advances forward. No per-frame IPC, additional clock or listener is added.
The existing scheduler coalesces water work and retains the previous valid surface
on cancellation, stale source or failure. A48kvertex cut takes about114ms to retime
on the measured target; a20Hz request interval does not promise20completed updates.
Native animation/control acceptance and the combined water/vegetation appearance
remain open. Reflection, refraction, underwater effects and merged caustics are not
implemented by this CPU FFT/time proof.

## Editor Authoring History

`editor.authoring_history` is the current bounded cross-surface coordinator. It
does not replace the authoring documents' journals. Instead, each real World,
project-GUI, Plant Lab, and open code document exposes a branch snapshot with a
stable identity, generation, revision, cursor, retained-entry count, and exact
Undo/Redo labels. The coordinator selects the visible compatible branch first
and otherwise the most recently changed compatible branch.

An Undo/Redo request names the document, branch, revision and history position
it expects to change. The document performs the change; the coordinator then
checks that the result was one expected step. A rejected check is detected,
but the current Editor refreshes the coordinator after the document has already
changed. It does **not** roll that document change back. This must not be
described as atomic cross-document Undo or rollback on every failure.

The first grouped repair covers GUI layout controls. A canvas move or inspector
layout change that moves a World proxy records one GUI entry and one World
transaction, linked by the existing coordinator. Undo/Redo prepares copies of
both journals and their scene projection. It checks the document identities,
content, entry IDs and exact history positions before publishing them together
with nonthrowing swaps. If either side has a later edit, the request stops with
a visible reason. It cannot silently undo half the pair. A full group budget
rejects the new layout change before either document changes; retained group
records contain compact endpoints, not copies of complete journals.

Actual Debug/Release owner tests cover repeated paired replay, redo invalidation,
foreign identities/content, stale acceptance and failed projection preparation.
The existing GUI synchronization also avoids importing a float round trip as a
new edit when the World proxy already exactly matches its GUI source. Matching
Engine and native acceptance remain pending. Proof:
`build/validation/gui-world-history-repair-oct27/source-freeze.json`.
Create/delete, AI multi-tool edits and selection-changing groups are not covered
by this slice. Extend these same owners; do not build a second history manager
or use notifications as an authoritative journal.
GUI source files currently save document state, not the Undo/Redo journal;
history across save/reopen remains a separate persistence requirement.

The existing command surface keeps each document's journal. Texture, tilemap, timeline,
audio, project-settings, and workspace-layout journals must join through the
same typed snapshot/executor boundary as their concrete journal support matures;
their names alone are not completion evidence.

### Event notifications

The existing `events.bus` typed subscription now has an actual standalone
Debug/Release consumer. Callback storage survives self-removal, nested dispatch,
clear and registration during a callback. Removing a subscription prevents later
delivery; callers must remove borrowed user data before destroying it. Ordinary
dispatch, nested dispatch, removal and clear pass tests with allocations denied.
Registering a callback may allocate. This bus is used by one owner thread and is
not a transaction journal, a cross-thread queue or a completed temporal world.
Integrated3 Release/full Engine contracts pass; broader consumers remain
unverified. Proof:
`build/validation/events-bus-repair-oct27/result.json`. The original subscription
compile failure remains in `build/validation/events-subscribe-oct27/debug-build.log`.

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
- `authoring.tilemap` applies the same separation to maps: stable source meaning
  and semantic history compile into `asset.tilemap_artifact`; project Library
  bytes and renderer submissions are reproducible derivatives, while texture
  residency and backend handles remain disposable;
- `scene.document` owns stable scene object handles, typed components, semantic
  operations, transactions, undo/redo, and deterministic snapshot projection;
- `scene.interaction` resolves ray hits, drag ownership, and Focus through stable
  object identity;
- `scenesnapshot` and `sceneserializer` own canonical `epoch_snapshot 3` text,
  including semantic Canvas2D texture materials and migration-only v1/v2 reads;
- `scene.persistence` validates, verifies, and atomically replaces scene files;
- `scene.runtime` compiles the validated revision into a deterministic,
  renderer-neutral runtime projection.

Explicit editor Save, Play, Build, and Run paths commit valid scene evidence
before continuing. Live create, duplicate, delete, transform, material, helper,
Canvas2D, Arcade, Plant Lab, and Forest Factory mutations enter `SceneDocument`
semantic operations. The editor entity collection is a disposable UI/render
projection rebuilt from that canonical document.

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

The [connected voxel/SDF world](voxels.md) uses these time/revision contracts for
durable edits, derived representations and stale-job rejection. World streaming
and AI model lifecycle share admission budgets while retaining separate owners.
Unload/reload cannot lose edits or reinterpret unavailable regions as empty.
These rules apply to universal software consumers as well as games; an empty
application or camera-only consumer need not instantiate the world or any bot.

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

Shaders, scripts, models, assets, generated-project add-ons, and explicit child
tools have different risk classes. Add-ons never auto-download, compile, or
execute. Epoch has no native engine plugin loader; network listeners, child-tool
launch, and server activation remain separate human-approved capabilities.

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
