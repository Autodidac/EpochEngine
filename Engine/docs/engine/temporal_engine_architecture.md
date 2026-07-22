# Epoch Temporal Engine Architecture
> Status: durable target architecture. Implement it through the phased gates in
> `Changes/roadmap.md`; capability claims require source and validation proof.
> Code sketches use conceptual `epoch::` namespaces. Production C++ must use
> Epoch's canonical `epochengine::` namespace and current C++23 module boundaries.
> Network listeners, server activation, active downloaded content, and external
> side effects remain explicitly operator-gated under the repository safety rules.


## 1. Mission

Epoch is a persistent, reversible, event-driven spacetime engine.

It must support:

* professional editing with effectively unlimited undo and redo
* arbitrary-time scene observation
* backward playback and rendering
* persistent single-player worlds
* long-running authoritative servers
* shared and downloadable timeline branches
* unscripted AI that persists outside loaded regions
* sparse temporal storage
* software, CPU, GPU, headless, editor, simulation, and server execution
* demanding first-person shooters and astronomical-scale space simulations
* safe handling of untrusted user-created worlds and branches

The defining principle is:

> Epoch stores meaningful causes, commitments, and consequences. It reconstructs continuous detail only where observation, interaction, or authority requires it.

---

# 2. Non-negotiable invariants

1. **Time is a primary coordinate.**
   Engine state is queried by timeline, branch, and time.

2. **Authoritative mutations are events.**
   Systems do not silently mutate persistent world state.

3. **The installed base world is immutable.**
   Saves, edits, mods, and alternate histories are copy-on-write overlays.

4. **A foreign branch is never trusted as server history.**
   It may seed an isolated server-owned branch after validation.

5. **Clients submit commands, not authoritative events.**
   The server validates commands and emits its own events.

6. **Nondeterminism is keyed or recorded.**
   Replays never depend on thread scheduling, unordered random calls, or rerunning an AI model.

7. **Derived state is disposable.**
   GPU buffers, particles, animation matrices, visibility lists, navigation work queues, and caches are reconstructed.

8. **Approximation is explicit and error-bounded.**
   Lossy history is never presented as exact authoritative truth.

9. **External side effects are isolated.**
   Rewinding cannot unsend packets, undo purchases, or reverse exported files.

10. **Untrusted active content never executes automatically.**
    Scripts, shaders, models, plugins, and executable logic require validation, sandboxing, policy, and often explicit user consent.

---

# 3. Fundamental world address

The canonical query is:

```cpp
namespace epoch::time
{
    struct TimePoint final
    {
        std::int64_t ticks{};
    };

    struct TimeRange final
    {
        TimePoint begin{};
        TimePoint end{};
    };
}

namespace epoch::temporal
{
    struct WorldAddress final
    {
        TimelineId timeline{};
        BranchId branch{};
        time::TimePoint time{};
    };
}
```

All major systems consume a world observation:

```cpp
const auto observation = world.observe({
    .timeline = active_timeline,
    .branch = active_branch,
    .time = presentation_time
});
```

The render frame is an observation of the world. It is not the authoritative owner of time.

---

# 4. Explicit time domains

Epoch must not use one universal `deltaTime`.

```cpp
namespace epoch::time
{
    enum class Domain : std::uint8_t
    {
        real,
        simulation,
        physics,
        presentation,
        animation,
        effects,
        audio,
        network,
        editor,
        replay
    };

    enum class Direction : std::int8_t
    {
        backward = -1,
        stopped = 0,
        forward = 1
    };

    struct DomainState final
    {
        TimePoint current{};
        double rate{1.0};
        Direction direction{Direction::forward};
        bool paused{};
    };
}
```

This supports:

* fixed-rate physics
* variable presentation
* reverse playback
* timeline scrubbing
* slow motion
* replay
* video frame rates
* network rollback
* offline rendering
* independent editor and simulation time

---

# 5. Event-sourced temporal state

Authoritative changes are immutable events.

```cpp
namespace epoch::temporal
{
    enum class EventTruth : std::uint8_t
    {
        exact,
        error_bounded,
        visual_only
    };

    struct Event final
    {
        EventId id{};
        TimelineId timeline{};
        BranchId branch{};

        time::TimePoint time{};
        ObjectId target{};
        EventType type{};

        PayloadRef payload{};
        EventTruth truth{EventTruth::exact};
        CausalRecordRef causality{};
    };
}
```

Examples:

* entity created or destroyed
* property changed
* parent changed
* impulse applied
* collision outcome committed
* animation action started
* AI decision committed
* inventory transferred
* dialogue outcome selected
* motion representation changed
* particle effect emitted
* region ownership changed
* scheduled task completed

Do not create events for every frame, particle, bone, or steering correction.

---

# 6. Atomic temporal transactions

Related events commit together.

```cpp
auto transaction = timeline.begin_transaction(simulation_time);

transaction.emit(PropertyChanged{...});
transaction.emit(NavigationCellInvalidated{...});
transaction.emit(AgentReplanRequired{...});

const CommitResult result = transaction.commit();
```

Transactions provide:

* atomic undo
* crash recovery
* branch consistency
* server durability
* causal tracking
* idempotent replay

Persistent state must never be partially updated.

---

# 7. Canonical timeline and local branch overlays

The base timeline remains immutable:

```text
A ─ B ─ C ─ D ─ E ─ F
```

Editing the past at `C` produces an overlay:

```text
A ─ B ─ C ─ D ─ E ─ F
        │
        └── G ─ H ─ I
```

```cpp
namespace epoch::temporal
{
    enum class BranchRetention : std::uint8_t
    {
        transient,
        session,
        persistent,
        archival,
        canonical
    };

    struct Branch final
    {
        BranchId id{};
        std::optional<BranchId> parent{};

        time::TimePoint fork_time{};
        EventId parent_head_at_fork{};

        EventSegmentRef local_events{};
        CheckpointManifestRef local_checkpoints{};
        DependencyOverlayRef invalidations{};

        BranchRetention retention{BranchRetention::persistent};
    };
}
```

The branch stores only:

* divergence events
* changed pages
* local checkpoints
* invalidation records
* newly referenced content

All unchanged base history remains shared.

This is the core optimization for undo, saves, mods, alternate futures, client prediction, and downloadable branches.

---

# 8. Shared branch packages

A branch can be exported without duplicating its base world.

```cpp
namespace epoch::sharing
{
    struct BranchManifest final
    {
        BranchPackageVersion package_version{};

        WorldId world{};
        ContentHash base_world_hash{};
        ContentVersion base_world_version{};

        BranchId exported_branch{};
        time::TimePoint fork_time{};
        EventId parent_head{};

        ContentHash event_manifest_hash{};
        ContentHash checkpoint_manifest_hash{};
        ContentHash dependency_manifest_hash{};

        std::vector<ContentHash> required_content{};
        std::vector<ContentHash> optional_content{};

        AuthorIdentity author{};
        DigitalSignature signature{};
    };
}
```

A branch package contains:

* manifest
* branch-local event segments
* branch-local checkpoint manifests
* modified immutable pages
* content hashes
* optional user assets
* provenance and schema information

It does not need to contain the full base world.

Missing content is resolved through hashes from:

* the local installation
* the authoritative server
* an approved content repository
* an explicitly accepted user package

---

# 9. Foreign branch security model

The server never joins a user branch by directly trusting the user’s running process.

The server receives a branch package or manifest and processes it through quarantine.

```text
Receive manifest
→ validate format and limits
→ verify content hashes and signatures
→ check base-world compatibility
→ classify referenced content
→ quarantine unknown data
→ replay allowed events in isolation
→ validate resulting world invariants
→ create server-owned branch instance
```

The imported branch is a read-only seed.

The server creates its own branch:

```cpp
namespace epoch::server
{
    struct ImportedBranchSource final
    {
        ContentHash foreign_manifest_hash{};
        AuthorIdentity foreign_author{};
        TrustAssessment trust{};
    };

    struct AuthoritativeBranchInstance final
    {
        BranchId server_branch{};
        ImportedBranchSource source{};

        EventId authoritative_head{};
        DurableJournalRef journal{};
        CheckpointManifestRef latest_checkpoint{};
    };
}
```

After import, every connected action is recorded by the server’s own journal.

The user does not remain a source of truth.

---

# 10. Authoritative branch session flow

When a user enters a shared branch:

1. The server resolves or imports the branch seed.
2. The server creates or opens a server-owned branch instance.
3. The client receives a safe world projection.
4. The client sends commands.
5. The server validates each command.
6. The server executes authoritative simulation.
7. The server emits and durably commits events.
8. The server replicates resulting projections to clients.
9. Disconnecting does not remove the branch.
10. Later joins continue from the server-owned authoritative record.

```text
Foreign branch package
        ↓
Validated read-only seed
        ↓
Server-owned authoritative branch
        ↓
Server journal records all connected activity
```

This avoids poisoning the canonical server through continued dependence on a hostile or unstable user process.

---

# 11. Server command and commit model

Clients never submit raw authoritative events.

```cpp
namespace epoch::network
{
    struct ClientCommand final
    {
        ClientId client{};
        CommandSequence sequence{};
        BranchId branch{};

        time::TimePoint predicted_time{};
        CommandType type{};
        PayloadRef payload{};
    };
}
```

The server performs:

```text
authenticate client
→ validate permissions
→ validate command schema
→ validate world preconditions
→ execute command
→ append durable transaction
→ apply immutable page changes
→ publish accepted result
```

The server must not report success before durable commit.

Use:

* append-only journal segments
* checksums
* atomic checkpoint manifests
* idempotent event application
* sequence numbers
* replay protection
* per-client quotas
* event and payload size limits
* background replication
* recovery replay

---

# 12. Safe joining before optional downloads

Joining a server branch should not require blindly accepting every user asset.

Use a staged join.

## Stage 1: Safe world projection

The client receives:

* identities
* transforms
* basic geometry proxies
* server-approved materials
* gameplay state
* region summaries
* safe event streams

Unknown optional content uses placeholders.

## Stage 2: Content offer

The server presents a content manifest:

```cpp
namespace epoch::content
{
    enum class RiskClass : std::uint8_t
    {
        passive_data,
        validated_shader,
        sandboxed_script,
        model_asset,
        native_extension,
        prohibited
    };

    struct ContentOffer final
    {
        ContentHash hash{};
        std::string display_name{};
        std::uint64_t compressed_bytes{};
        std::uint64_t expanded_bytes{};

        RiskClass risk{};
        PermissionSet requested_permissions{};
        AuthorIdentity author{};
    };
}
```

## Stage 3: User consent

The client may accept or reject optional content.

Rejecting content must not block core state synchronization when a fallback representation is available.

## Stage 4: Quarantined download

Accepted content downloads into a content-addressed quarantine store.

Validate:

* hash
* signature
* archive structure
* compressed and expanded size
* file count
* schema
* dependency graph
* requested capabilities

## Stage 5: Capability grant

Passive assets may become available after validation.

Active content requires stronger rules.

---

# 13. Dangerous-content policy

## Passive data

Examples:

* textures
* meshes
* audio
* animation clips
* declarative materials
* scene metadata

These still require bounds checking and decompression limits.

## Shaders

Shaders must:

* use approved intermediate formats or source languages
* compile through Epoch’s controlled pipeline
* pass validation
* have resource and instruction policies
* never receive arbitrary host memory access
* be cached by validated hash

## Scripts

Scripts must run in a restricted environment with:

* explicit capabilities
* deterministic APIs where required
* instruction budgets
* memory budgets
* no arbitrary filesystem access
* no unrestricted networking
* no arbitrary process launch

## AI models

Model files are optional content.

They require:

* user consent
* size disclosure
* license metadata
* isolated model loading
* resource limits
* recorded outputs when used authoritatively

## Native extensions

Native binaries must never be silently downloaded and executed.

For normal public multiplayer they should be disabled or require a separately installed, trusted, explicitly approved package.

---

# 14. Immutable page-based world database

Persistent state is stored in immutable pages.

```text
Transform pages
Motion pages
Physics pages
Agent pages
Inventory pages
Relationship pages
Region pages
Material pages
Timeline indexes
```

```cpp
namespace epoch::world
{
    struct PageDescriptor final
    {
        PageId id{};
        PageType type{};
        ContentHash hash{};

        std::uint32_t schema_version{};
        std::uint32_t object_count{};
        std::uint64_t uncompressed_bytes{};
    };

    struct CheckpointManifest final
    {
        BranchId branch{};
        time::TimePoint time{};
        EventId last_event{};

        std::vector<PageDescriptor> pages{};
    };
}
```

Changing one object copies only its containing page.

Use:

* copy-on-write
* content-addressed storage
* page hashing
* deduplication
* reference counting
* compressed pages
* atomic manifests
* branch-local page overlays

---

# 15. Arbitrary-time observation

The world database must support:

```cpp
WorldObservation TemporalWorldDatabase::observe(
    temporal::WorldAddress address,
    ObservationRequirements requirements);
```

Observation uses:

1. nearest valid checkpoint
2. shared parent pages
3. branch-local overlays
4. relevant event replay
5. analytic motion evaluation
6. cached reconstructed pages

A timeline cursor is only a convenience. The actual system is time-addressable.

---

# 16. Sparse temporal motion codec

Continuous motion is represented by models and sparse corrections.

```cpp
namespace epoch::motion
{
    enum class Model : std::uint8_t
    {
        stationary,
        constant_velocity,
        constant_acceleration,
        ballistic,
        spline,
        orbital,
        parent_attached,
        animation_driven,
        route_driven,
        rigid_body_replay,
        procedural
    };

    struct ErrorContract final
    {
        double position_world{};
        float position_screen_pixels{};
        float orientation_degrees{};
        float collision_distance{};
        float velocity_error{};
    };

    struct Segment final
    {
        time::TimeRange range{};
        Model model{};

        QuantizedMotionState initial{};
        ParameterBlock parameters{};
        ErrorContract allowed_error{};
    };
}
```

During active simulation:

1. Predict from the current segment.
2. Compare predicted and actual state.
3. Discard temporary samples while error is valid.
4. Emit a correction key when the error limit is reached.
5. Start a new segment after discontinuities.

Always cut segments at:

* collisions
* teleports
* impulses
* attachment changes
* route changes
* motion-model changes
* important physical contacts
* authoritative gameplay events

---

# 17. Symmetry-aware compression

State that cannot influence observation or simulation should not be stored.

```cpp
namespace epoch::motion
{
    enum class RotationEquivalence : std::uint8_t
    {
        none,
        spherical,
        axial,
        half_turn,
        quarter_turn,
        custom
    };

    struct Symmetry final
    {
        RotationEquivalence rotation{};
        std::uint32_t custom_order{};
    };
}
```

Examples:

* sphere: no orientation history
* cylindrical projectile: direction but no roll
* distant glow: position, radius, and intensity
* detailed spacecraft: full orientation
* repeating rotor: orientation modulo repeated symmetry

---

# 18. Temporal truth classes

Every persistent channel has a declared truth level.

```cpp
enum class TemporalTruth : std::uint8_t
{
    exact,
    error_bounded,
    visual_only,
    disposable
};
```

## Exact

* entity identity
* authored edits
* branch graph
* player commands
* ownership
* inventory
* relationships
* AI decisions
* deaths and spawns
* dialogue outcomes
* economic transactions
* meaningful collision results
* server commits

## Error-bounded

* continuous position
* velocity
* orientation
* animation phase
* health curves
* route progress
* long-running tasks

## Visual-only

* smoke
* sparks
* camera smoothing
* trail density
* debris decoration
* distant proxies

## Disposable

* GPU buffers
* bone matrices
* visibility lists
* pathfinding search queues
* broad-phase caches
* local avoidance neighborhoods
* reconstructed particle buffers

---

# 19. Particles and visual effects

Particles are functions over time.

```cpp
namespace epoch::effects
{
    struct EffectEvent final
    {
        EffectId id{};
        EffectProgramId program{};

        time::TimeRange lifetime{};
        Transform initial_transform{};

        std::uint64_t seed{};
        ParameterBlock parameters{};
    };
}
```

Particle identity:

```cpp
ParticleId id = hash(effect_id, spawn_index, seed);
```

Particle state:

```cpp
ParticleState state = evaluate(program, id, requested_time);
```

Store only:

* emitter event
* seed
* parameters
* collision outcomes when meaningful
* sparse checkpoints for genuinely chaotic effects

Trails are reconstructed from motion history. Trail geometry is disposable.

---

# 20. Animation

Store animation causes, not complete pose streams.

```cpp
namespace epoch::animation
{
    struct ActionEvent final
    {
        AgentId agent{};
        AnimationProgramId program{};

        time::TimePoint begin{};
        float playback_rate{1.0f};

        ParameterBlock parameters{};
        std::uint64_t variant_seed{};
    };
}
```

At arbitrary time, reconstruct:

* clips
* blends
* root motion
* procedural layers
* motion matching
* pose

Record sparse exact data for:

* foot contacts
* hand contacts
* physical interactions
* ragdoll transitions
* nondeterministic motion selections

Bone matrices remain disposable.

---

# 21. Physics

Physics uses several representations.

```text
Immediate interaction region
    full numerical rigid-body simulation

Nearby region
    simplified rigid bodies

Distant moving objects
    sparse trajectories

Orbital scale
    analytic propagation

Dormant objects
    static or scheduled state
```

General backward integration is not authoritative.

Backward observation uses:

1. checkpoint before the requested time
2. recorded inputs and physical events
3. deterministic or controlled forward replay
4. stop at target time

Gameplay-significant contacts remain exact.

Decorative debris may use error-bounded or visual reconstruction.

---

# 22. Persistent AI

Every meaningful agent persists globally.

Agents use four simulation levels.

## Active

* full perception
* planning
* detailed navigation
* combat
* physics
* animation
* frequent event recording

## Coarse

* reduced planning frequency
* regional routes
* simplified encounters
* approximate resource usage
* no bone evaluation or local avoidance

## Scheduled

* future semantic transitions
* arrivals
* work completion
* trade execution
* repairs
* faction activity

## Dormant

* compact state
* no regular ticking
* evaluated only when queried or scheduled

```cpp
namespace epoch::agents
{
    struct PersistentState final
    {
        AgentId id{};
        time::TimePoint evaluated_through{};

        RegionId region{};
        SpatialAnchor location{};

        GoalId goal{};
        ActionId action{};

        ConditionState condition{};
        InventoryState inventory{};
        RelationshipDigest relationships{};

        motion::Segment movement{};
        ScheduledEventId next_event{};
        RandomState randomness{};
    };
}
```

---

# 23. AI decisions and nondeterminism

Store decisions, not planner implementation internals.

```cpp
namespace epoch::agents
{
    struct DecisionEvent final
    {
        AgentId agent{};
        time::TimePoint time{};

        GoalId goal{};
        ActionId action{};

        ObservationDigest observations{};
        DecisionReason reason{};
        RandomSampleKey random_sample{};
    };
}
```

Do not persist:

* planner heap internals
* behavior-tree pointers
* temporary perception arrays
* neural activations
* pathfinding open lists

For model inference:

```cpp
struct ModelDecisionEvent final
{
    ModelId model{};
    ModelVersion version{};

    InputDigest input{};
    PayloadRef recorded_output{};
};
```

Replay uses the recorded output. It does not rerun the model.

---

# 24. Order-independent randomness

Global random generators are forbidden in persistent simulation.

```cpp
namespace epoch::random
{
    struct SampleKey final
    {
        WorldId world{};
        ObjectId object{};
        SystemId system{};
        EventId event{};
        std::uint32_t sample_index{};
    };
}
```

A sample is derived from identity.

This prevents results from changing because:

* thread scheduling changed
* systems ran in another order
* unrelated random calls were added
* CPU and GPU scheduling differed

True external nondeterminism is recorded as an event.

---

# 25. Persistent regions and tickless worlds

Unloaded regions do not disappear and do not require continuous frame ticks.

```cpp
namespace epoch::world
{
    struct PersistentRegion final
    {
        RegionId id{};
        time::TimePoint evaluated_through{};

        PopulationSummary population{};
        EconomySummary economy{};
        ConflictSummary conflicts{};
        InfrastructureSummary infrastructure{};

        PageManifestRef entities{};
        ScheduledQueueRef scheduled_events{};
    };
}
```

A region advances when:

* a scheduled event becomes due
* a player approaches
* another region interacts with it
* a global simulation requests it
* the server advances a maintenance horizon

The region jumps between meaningful transitions.

---

# 26. Observer-driven fidelity

Use overlapping demand regions:

```text
Exact interaction demand
Physics demand
Visual demand
Audio demand
Network-interest demand
Editor-recording demand
Historical-query demand
```

An object receives the highest fidelity required by any active observer.

This supports:

* dense FPS combat
* enormous space environments
* editor camera views
* server interest filtering
* replay cameras
* remote simulation

---

# 27. Causal dependency graph

A past edit must invalidate only its causal future.

```cpp
namespace epoch::temporal
{
    struct CausalRecord final
    {
        EventId cause{};

        ObjectSet affected_objects{};
        RegionSet affected_regions{};
        SystemMask affected_systems{};
        time::TimeRange affected_time{};
    };
}
```

Moving one chair may invalidate:

* the chair transform
* its navigation cell
* routes crossing that cell
* dependent agent decisions

It must not recompute unrelated planets.

Branches reuse causally unaffected events, pages, and checkpoints.

---

# 28. Backward rendering

Rendering requests a world observation at time `T`.

Temporal caches must be keyed correctly:

```cpp
namespace epoch::rendering
{
    struct HistoryKey final
    {
        TimelineId timeline{};
        BranchId branch{};

        time::TimePoint sample_time{};
        time::Direction direction{};

        CameraId camera{};
    };
}
```

Backward rendering must handle:

* reversed motion vectors
* animation time
* particle age
* trail direction
* motion blur
* exposure
* temporal antialiasing
* temporal upscaling
* occlusion history
* disocclusion

Timeline jumps, forks, and direction changes invalidate incompatible histories.

---

# 29. Demand-driven execution architecture

Epoch’s render graph should become a general execution graph.

```text
CPU tasks
SIMD kernels
GPU graphics
GPU compute
transfer operations
file IO
decompression
sparse memory mapping
checkpoint loading
event replay
network replication
```

Tasks describe time requirements:

```text
Observe region at T
Replay physics from T0 to T1
Evaluate animation at T
Predict visibility over T1–T2
Load pages before T2
Render camera at T1
```

The compiler selects:

* CPU, SIMD, GPU, or software execution
* queue assignment
* resource residency
* fidelity
* synchronization
* caching
* work elimination

---

# 30. Unified virtual resource fabric

Textures, geometry, audio, temporal pages, AI data, and simulation segments use one virtual resource model.

```cpp
namespace epoch::resource
{
    struct ResidencyRequest final
    {
        ResourceId resource{};
        SpatialRegion region{};

        time::TimeRange useful_interval{};
        time::TimePoint deadline{};

        QualityRequirement quality{};
        float prediction_confidence{};
    };
}
```

Memory hierarchy:

```text
GPU-local memory
→ shared/system memory
→ compressed RAM cache
→ local SSD
→ optional approved content source
→ procedural reconstruction
```

The backend may use:

* committed resources
* streamed mip levels
* sparse texture pages
* sparse geometry
* ordinary CPU pages
* software representations

Stable logical resource handles must survive physical residency changes.

---

# 31. Geometry and rendering

Geometry becomes a hierarchical cluster database.

```text
Model
└── instances
    └── cluster hierarchy
        ├── bounds
        ├── geometric error
        ├── compressed vertices
        ├── material references
        ├── child clusters
        └── collision and ray representations
```

The same source geometry supports:

* GPU rasterization
* GPU mesh processing
* ray queries
* software rasterization
* CPU ray tracing
* collision
* editor selection
* lower-detail proxies

Selection is based on:

* projected error
* visibility
* collision demand
* memory budget
* temporal relevance
* available execution device

---

# 32. Software as a first-class device

Software rendering must not emulate Vulkan or exist as a toy fallback.

It should consume the same:

* world observations
* cluster geometry
* materials
* virtual resources
* execution graph
* temporal history

Software roles:

* correctness reference
* deterministic CI
* headless server rendering
* remote rendering
* low-resource fallback
* offline capture
* CPU path tracing
* debugging

Suggested architecture:

```text
Hierarchical culling
→ cluster binning
→ tile scheduling
→ SIMD vertex processing
→ tile-local depth
→ visibility buffer
→ SIMD material evaluation
→ output
```

---

# 33. Persistent single-player

Single-player supports:

* free timeline branching
* unlimited logical undo
* nonlinear redo
* save branches
* alternate futures
* replay editing
* local modifications
* branch export and import

A save is primarily a branch reference and local overlay head.

```cpp
struct SaveReference final
{
    WorldId world{};
    ContentHash base_world_hash{};

    BranchId branch{};
    EventId head{};
    CheckpointId nearest_checkpoint{};
};
```

---

# 34. Persistent multiplayer servers

The live server uses one authoritative branch per world instance.

```text
Canonical server history
├── short network rollback window
├── durable checkpoints
├── replay branches
├── imported-world instances
├── administrative forks
└── archival history
```

Players cannot rewrite committed server history.

Servers may create isolated forks for:

* debugging
* incident review
* moderation
* simulation testing
* rollback recovery
* imported shared worlds

---

# 35. Client prediction branches

Each client may maintain a temporary local branch:

```text
Server-confirmed history
        │
        └── predicted client events
```

When server results arrive:

* matching predictions are accepted
* mismatches are discarded
* the client reforks from the corrected server head
* presentation reconciles smoothly

Prediction branches store only:

* local commands
* predicted event results
* short-lived checkpoints
* presentation corrections

They are garbage-collected aggressively.

---

# 36. Branch merging

Branches should not be merged through blind event concatenation.

Supported operations:

## Attach

Keep the branch as an independent alternate timeline.

## Cherry-pick

Apply selected transactions to another branch.

## Semantic merge

Replay compatible domain events against a target branch.

## Rebase

Re-evaluate a local branch against a newer compatible base.

Conflicts require domain-specific resolution:

* transform conflict
* deleted-object conflict
* inventory ownership conflict
* navigation revision conflict
* script or asset version conflict
* AI causal conflict
* server authority conflict

Server history always wins over conflicting client predictions.

---

# 37. Storage architecture

```text
WorldStore/
├── base/
│   ├── manifests/
│   └── immutable pages/
├── branches/
│   ├── local/
│   ├── server/
│   ├── imported/
│   └── archival/
├── journals/
├── checkpoints/
├── content/
│   └── objects-by-hash/
├── quarantine/
├── indexes/
└── temporary-cache/
```

Use:

* canonical binary serialization
* schema versions
* bounded decoding
* content hashes
* digital signatures
* compression dictionaries
* delta encoding
* residual encoding
* quaternion compression
* variable-length integers
* page deduplication
* adaptive checkpoint spacing
* branch garbage collection

---

# 38. Hot, warm, and cold history

## Hot history

* current immutable pages
* recent exact events
* active replay caches
* dense temporary simulation samples
* nearby checkpoints

## Warm history

* sparse motion segments
* correction keys
* active branch events
* compressed checkpoints
* recent server journals

## Cold history

* sealed journal segments
* deduplicated pages
* sparse checkpoint indexes
* archived branches
* replay packages

Derived caches can disappear under pressure without losing authoritative history.

---

# 39. Branch retention and garbage collection

```cpp
enum class BranchRetention : std::uint8_t
{
    transient,
    session,
    persistent,
    archival,
    canonical
};
```

Garbage collection removes content only when:

* no branch references it
* no checkpoint references it
* no save references it
* no server retention policy protects it
* no active export depends on it

Transient prediction branches may vanish immediately.

Persistent user branches require explicit deletion or configured archival.

---

# 40. Side-effect boundary

Timeline evaluation cannot directly perform irreversible actions.

```text
World proposes side effect
→ side-effect gateway records intent
→ policy validates intent
→ external system commits action
→ result becomes a new authoritative event
```

Examples:

* file export
* network publication
* payment
* account mutation
* process launch
* device command

Rewinding changes the world’s representation of the event, not external reality.

---

# 41. Module ownership

```text
epoch::time
├── TimePoint
├── TimeRange
├── Domain
├── DomainState
└── Direction

epoch::temporal
├── Timeline
├── BranchGraph
├── EventJournal
├── Transaction
├── ReplayScheduler
├── CausalDependencyGraph
└── HistoryCompactor

epoch::world
├── TemporalWorldDatabase
├── WorldObservation
├── ImmutablePageStore
├── CheckpointManifest
├── SpacetimeIndex
└── RegionDatabase

epoch::motion
├── Segment
├── ModelRegistry
├── CorrectionKey
├── ErrorContract
└── RepresentationBridge

epoch::simulation
├── TierManager
├── AnalyticSystem
├── ReplayableSystem
├── CheckpointedSystem
└── ScheduledEventSystem

epoch::physics
├── PhysicsWorld
├── ContactJournal
├── PhysicsCheckpoint
└── RepresentationTransition

epoch::agents
├── PersistentState
├── DecisionJournal
├── StrategicSimulator
├── Materializer
└── Dematerializer

epoch::navigation
├── GlobalRouteGraph
├── LocalNavigationWorld
├── TemporalRoute
└── NavigationRevision

epoch::animation
├── TemporalAnimation
├── ActionEvent
└── ContactCheckpoint

epoch::effects
├── EffectProgram
├── ParticleEvaluator
└── TrailEvaluator

epoch::resource
├── VirtualResourceRegistry
├── ResidencyManager
├── PageCache
└── ContentStore

epoch::execution
├── ExecutionGraph
├── KernelRegistry
├── DevicePlanner
└── BudgetScheduler

epoch::rendering
├── WorldRenderer
├── VisibilitySystem
├── TemporalHistory
├── GeometryClusterDatabase
└── SoftwareRenderer

epoch::persistence
├── DurableJournal
├── RecoveryManager
├── SaveReference
├── BranchPackage
└── ArchiveStore

epoch::network
├── AuthoritativeWorldHost
├── CommandValidator
├── ClientPredictionBranch
├── InterestProjection
└── ReplicationJournal

epoch::security
├── BranchValidator
├── ContentQuarantine
├── CapabilityPolicy
├── SignatureVerifier
└── ResourceLimitPolicy
```

---

# 42. Implementation campaign

## Phase 1: Temporal primitives

* `TimePoint`
* `TimeRange`
* time domains
* forward, stopped, and backward direction
* stable timeline and branch identifiers

## Phase 2: Event and transaction spine

* immutable event journal
* canonical serialization
* atomic transactions
* event schemas
* exact replay tests

## Phase 3: Immutable world storage

* page store
* checkpoint manifests
* copy-on-write pages
* content hashing
* deduplication
* arbitrary-time observation

## Phase 4: Local branch overlays

* fork references
* branch-local journals
* local checkpoints
* nonlinear undo and redo
* branch retention
* garbage collection

## Phase 5: Sparse temporal motion

* motion models
* adaptive correction keys
* residual encoding
* symmetry metadata
* consequence-aware error contracts

## Phase 6: Reversible visuals

* time-evaluable particles
* trails from trajectories
* animation events
* reverse rendering
* timeline-aware temporal-cache invalidation

## Phase 7: Physics

* deterministic input journal
* physical checkpoints
* contact events
* replay to arbitrary time
* active and analytic representation transitions

## Phase 8: Persistent world simulation

* persistent regions
* scheduled events
* tickless advancement
* observer-demand tiers
* materialization and dematerialization

## Phase 9: Persistent AI

* named random streams
* persistent agent state
* durable plans
* decision events
* route segments
* strategic simulation
* recorded nondeterministic model outputs

## Phase 10: Branch sharing

* branch manifests
* branch package export
* content-addressed dependencies
* signature and provenance support
* import and rebase tooling

## Phase 11: Secure server branch import

* quarantine
* schema validation
* size and resource limits
* isolated replay
* base-world verification
* server-owned authoritative branch creation
* provenance records

## Phase 12: Authoritative networking

* client commands
* durable server journal
* replication
* interest projections
* client prediction branches
* rollback and reconciliation

## Phase 13: Optional content negotiation

* safe initial join
* content offers
* risk classification
* user consent
* sandboxed downloads
* fallback representations
* capability grants

## Phase 14: Execution and resource fabric

* general execution graph
* CPU, SIMD, GPU, software, IO, and replay tasks
* virtual resource registry
* time-aware residency
* predictive prefetch
* memory budgeting

## Phase 15: Advanced rendering

* bindless resources
* GPU-driven scene data
* indirect execution
* cluster geometry
* sparse textures and geometry
* software parity
* temporal rendering history
* virtualized shadows and lighting

## Phase 16: Editor and administration

* full timeline interface
* branch browser
* causal invalidation inspector
* branch comparison
* semantic merge
* content-risk prompts
* server branch audit
* replay and moderation tools

---

# 43. First production slice

The first credible implementation should prove only this vertical path:

```text
Explicit TimePoint
→ immutable event journal
→ atomic transaction
→ page-based checkpoint
→ local branch overlay
→ reversible transform edit
→ arbitrary-time observation
→ sparse motion segment
→ analytic particle effect
→ backward rendering test
→ branch export and reimport
```

Do not begin with unscripted AI, multiplayer, or full physics.

The temporal spine must first prove:

* exact authored undo
* nonlinear redo
* compact local branching
* arbitrary-time state reconstruction
* branch portability
* bounded storage growth

---

# 44. Final operating model

```text
Meaningful event
    → stored exactly

Continuous predictable state
    → sparse temporal model

Unpredictable authoritative outcome
    → recorded event or checkpoint

Visual detail
    → regenerated on demand

Unloaded entity
    → compact persistent state and scheduled transitions

Past alteration
    → local copy-on-write branch overlay

Shared branch
    → signed content-addressed package

Foreign branch on server
    → quarantined read-only seed

Connected user activity
    → server-owned authoritative journal

Optional dangerous content
    → disclosed, consented, validated, and sandboxed
```

Epoch therefore becomes:

> A persistent, reversible, shareable spacetime engine where worlds are immutable histories plus sparse branch overlays; servers independently own all connected activity; and untrusted content can enrich a world without becoming an invisible authority or execution path.
