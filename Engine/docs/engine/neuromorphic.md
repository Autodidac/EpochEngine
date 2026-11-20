# Neuromorphic Engine Framework

## Status

The bounded CPU neuromorphic core, sampled event camera, adapters and generic
invalidation network remain implemented with CPU contracts. October30 operator
direction removes the OpenGL cached/partial renderer integration and its A/B
benchmark from source. Conventional full-frame rendering is the current policy;
combined Debug/Release builds/full pure contracts pass, while the actual 120 retry
still lagged and native responsiveness/image acceptance remains open. The proposed animation
frame cache is cancelled. See [full-frame policy](event_render.md).

The layer is an event-driven control/perception fabric. It does not replace
rasterization, the physics solver, the audio device, or the presentation clock
when fixed-step, frame-step, or batch execution is objectively better.

### Current ownership and evidence (October30, 2026)

This contract covers reusable software, editor and game consumers; it is not a
second roadmap. `Changes/roadmap.md` orders unfinished delivery and the
[active pass](../../../Changes/active_pass.md) owns the current bounded scope.

| System | Actual implementation/integration | Remaining acceptance |
| --- | --- | --- |
| Sparse graph / `simulation.neuromorphic` | CPU bounded signals, weighted activation, leak/refractory gates, metrics and temporal reset; CPU contracts | Workload benchmarks and consumer-specific replay/cost proof |
| Event camera / `render.neuromorphic_camera` | Sampled luminance-to-event API and CPU fixtures; no production perception consumer found | Native consumer, total observation/event budget, latency/memory and threshold evidence |
| Timeline/task-graph adapters | Typed conversion and atomic graph compile with CPU fixtures | Actual authored graph/runtime/replay workflow; no continuous world-wide integration claimed |
| Invalidation / `render.neuromorphic_invalidation` | Generic bounded network and CPU contracts remain; live OpenGL bridge removed in source | Any future consumer needs explicit scope, correctness, replay/reset and measured cost proof |
| AI host triage | Deterministic path/identifier scoring in the existing self-coding panel; streaming activity and bounded discovery | Useful model patches, real builds and embedded succession; not learned/spiking agent integration |

The current graph has no hidden learning/weight updates. Event-camera output is
`visual_only`; timeline causes use `exact_cause`. Raster damage and validated
host/source evidence remain authoritative. Primitive availability does not prove
higher presentation FPS, model throughput or a whole-engine speedup.

### DAVID alignment and camera/render research (October 25)

This is a planned continuation, not a new learned implementation. Naming and
micromodel responsibilities live in [AI architecture](ai_plan.md#david-roles-and-shippable-micromodels--planned):
DAVID is the framework, ARCHITECT 1 routes work, ELA is a non-LLM predictive
learner, ADAM owns neuromorphic cognition research, NEO/engcoder is the planned
English/coding micromodel and optional acting consumer, and KEYMAKER is supervised
training/evaluation. “JEV-like” has no resolved model/paper identity yet; no external
architecture or weights are assumed. NEO speech I/O requires separate ASR/TTS proof.

Keep three distinct camera/render stages:

1. **Existing sampled event camera:** `Engine/modules/render/render.neuromorphic_camera.ixx`
   converts supplied luminance observations into bounded events. This is not proof
   of an event-native sensor, free framebuffer capture or a perception model.
2. **Planned perception/prediction:** a bounded project consumer may use event
   windows with explicit timestamp/coordinate/units, truth class, queue/drop limits,
   reset/replay policy and licensed data. Compare against nonlearned baselines;
   measure observation/readback cost, accuracy, latency, RAM/VRAM and event storms.
   Do not continuously read back a framebuffer while claiming cost-free event input.
3. **Generic invalidation primitive; future consumer only:**
   `Engine/modules/render/render.neuromorphic_invalidation.ixx` retains its conservative
   promotion contract, but no longer controls the production OpenGL scene path.
   A future model cannot suppress authoritative changes or replace presentation.
   Any new renderer policy needs separate operator scope and measured correctness
   and total cost against conventional rendering; the old A/B lane is removed.

Use [shared loading/admission](ai_plan.md#shared-model-loading-and-unloading-plan)
for learned model components; no second scheduler or assumed always-resident brain.
An ordinary event graph need not load a neural model. Active coding/generation,
renderer resources and optional inference compete under the same measured budget.
Training needs explicit consent, licensed data/provenance, held-out evaluation and
rollback; neither rendering nor inference grants automatic learning authority.

Prerequisites remain current reliability/software-base acceptance and a real
consumer specification. First research acceptance is one bounded replayable
consumer with measured value over the existing deterministic baseline, not a
whole-engine neuromorphic rewrite. Preserve playable 2D, renderer parity and
[Dark Iterations](dark_iterations.md) in their roadmap order.

### Measured next design steps

1. Preserve authoritative world/lighting changes, camera/global revisions,
   temporal reset and conventional full-frame rendering. Unknown/nonlocal effects
   cannot be suppressed by activation policy.
2. Establish consumer-specific cost measurements against the conventional path.
   Account for observation, graph activation, rendering and presentation separately;
   the removed same-cache benchmark is not an available comparison lane. No
   performance improvement follows automatically from neuromorphic integration.
3. Add one explicit bounded perception/simulation consumer only after current
   P0/software-base gates. Preserve consumer-owned clocks, temporal reset and
   event/output limits; do not poll a full image while claiming event-native input.
4. Evaluate event-based AI wake-up as a cost hypothesis: cheap signals can
   recommend work, but cannot start listeners, bypass session approval or alter
   source/model authority. Comparison pauses model work; cancellation and
   resource ownership still use the existing scheduler.
5. Port policy contracts before optimizations. CLI/software correctness and
   full-frame backend paths remain valid; native partial/foveated/hardware paths
   require individual capability and cost evidence.

These steps neither authorize a draw-order rewrite nor replace the full playable
2D, portable software, GUI/Extensions and private SDK goals. No measured win,
autonomous learning or hardware-accelerator support is claimed by this plan.

```text
meaningful world cause
        |
        v
canonical temporal event ---------------------> replay / branch / rewind
        |
        v
neuromorphic signal
        |
        v
sparse weighted activation graph <----------- authored node graph
        |
        +----------> simulation / AI / attention
        |
        +----------> event-camera perception
        |
        +----------> conservative render invalidation pressure
```

## Implemented Mainline Runtime

### `simulation.neuromorphic`

A bounded sparse integrate-and-fire runtime graph:

- typed signals with timestamp, channel, semantic ID, source, magnitude,
  salience, polarity and temporal truth class;
- threshold accumulation;
- linear potential leakage;
- refractory gates;
- excitatory and inhibitory input;
- weighted graph propagation;
- bounded pending activation queue and activation budgets;
- no polling of inactive nodes during signal processing;
- deterministic reset for temporal discontinuity/replay;
- metrics for emitted/rejected signals, routed/processed activations, fires,
  refractory suppression and dropped work.

This is neuromorphic-inspired engine scheduling. It does not pretend to model a
biological neuron accurately.

### `render.neuromorphic_camera`

A Dynamic Vision Sensor-style software camera front end:

- receives a luminance plane;
- stores logarithmic per-pixel reference intensity;
- emits positive or negative events only when local brightness changes exceed a
  configurable threshold;
- supports per-pixel refractory time and a bounded number of events per
  observation;
- reports event density and an activity centroid suitable for attention or
  foveation;
- converts pixel events directly into `simulation.neuromorphic::Signal`.

The current source is still a sampled luminance plane, so it emulates an event
camera from renderer output. A later native path can generate scene-change
signals before or during rendering and avoid producing full intermediate frames
for perception-only consumers.

### `simulation.neuromorphic_adapters`

The integration layer proves two important paths:

1. `timeline.system::TimelineEvent` -> exact-cause neuromorphic signal.
2. `authoring.task_graph::GraphSnapshot` -> executable neuromorphic graph.

Compilation of an authored graph is atomic: topology is built in a candidate
runtime graph and becomes active only after the complete graph validates.
EpochGui's node-graph workspace remains UI/projection state; canonical authoring
state remains the source of runtime topology.

### `render.neuromorphic_invalidation`

This generic bounded weighted network can classify sparse/dense change signals
and conservatively request full work when activity or budget pressure requires it.
Its CPU contracts remain. The October30 source removal retires the per-OpenGL-state
network and dirty-rectangle consumer; there is no live selective-renderer bridge.

Future consumers must preserve authoritative scene/gameplay state and require
separate admission, replay/reset and measured value. Simulation LOD cannot infer
gameplay truth from pixels, and the primitive does not authorize partial rendering.

### Contract coverage

`epoch_neuromorphic_contract` validates the sparse weighted graph, refractory
and inhibitory behavior, deterministic reset, timeline/task-graph adapters, the
software event camera, and sparse-versus-dense render invalidation decisions.
The target is CPU-only and does not launch a window or GPU runtime.

## Temporal Rules

The neuromorphic layer does not make every spike authoritative.

Use the signal truth class deliberately:

- `exact_cause`: commands or semantic world causes that must reproduce exactly;
- `error_bounded`: derived simulation/perception where a tested bound exists;
- `visual_only`: camera/render attention evidence that may differ without
  changing authoritative outcomes;
- `transient`: scheduling hints or disposable activations.

A camera may generate millions of transient pixel events while the timeline
stores only the semantic causes necessary to reconstruct the world. Replaying
those causes can regenerate the visual event stream when needed.

## Capabilities This Architecture Could Grant

### Sparse world simulation

Inactive objects and systems need not be polled merely because a frame occurred.
A door, sensor, dormant NPC, remote machine, unloaded logic graph or environmental
system can remain silent until an input changes something relevant.

### Event-native camera and eye model

An eye-like game camera can separate perception from conventional image capture:

- motion/change detection without requiring a complete perception frame;
- extremely fine simulated event timestamps;
- attention driven by event density;
- foveated rendering centered on activity rather than screen center;
- saccade-like camera/attention movement;
- independent peripheral and foveal thresholds;
- temporal adaptation and refractory behavior;
- visual persistence assembled from recent events rather than one instantaneous
  frame;
- reduced perception work in static regions.

A conventional color/depth renderer can still run alongside this path. The two
are complementary.

### Event-driven AI

Agents can wake reasoning modules only when salient signals arrive. Cheap local
nodes can absorb ordinary stimuli; expensive planning or language models can be
scheduled only when thresholds are crossed.

### Native gameplay signal graphs

Author-authored node graphs can become live signal circuits: sensors, gates,
accumulators, timers, inhibitors, attention nodes and actions. Cycles can model
stateful feedback while activation budgets and refractory gates bound runaway
work.

### Temporal replay and alternate histories

Because authoritative causes remain on the timeline, Epoch can reset the sparse
runtime graph at a temporal discontinuity and replay causes through it. This is a
natural match for rewind, branching, kill-cam, prediction and "what-if" worlds.

### Adaptive rendering

Signals can control rendering work rather than only gameplay work. Candidate
uses include foveated quality, local shadow/reflection refresh, animation update
frequency, particle activation, visibility work and dynamic resolution regions.
The visual result must remain classified as visual/error-bounded rather than
silently influencing exact gameplay.

### Physics and audio wake-up

Broad physics stepping remains solver-owned, but sleeping islands, contacts,
triggers and expensive secondary effects can wake downstream work through
signals. Audio emitters and acoustic simulation can likewise remain quiet until
sources/listeners/environmental state change.

### Networking

Semantic signals map naturally to replication deltas and interest management:
only changed or causally relevant state needs immediate work. This does not
replace snapshots, reconciliation or authoritative server rules.

### Future neuromorphic hardware

The graph exposes event-driven sparse work in a form that could later be mapped
to a specialized backend such as a spiking-neural accelerator. Hardware support
must remain optional; Epoch's correctness cannot depend on research hardware.

## What This Does Not Automatically Solve

Neuromorphic architecture is not universally faster. Dense workloads can be
worse when converted into tiny events. Raster rendering, broad-phase physics,
large matrix operations and other batch-friendly work often belong on ordinary
CPU/GPU paths.

It also introduces real engineering risks:

- event storms;
- feedback loops;
- nondeterminism from parallel asynchronous processing;
- difficult debugging if causal lineage is not retained;
- scheduling overhead when activity is dense;
- visual/gameplay divergence if truth classes are mixed;
- memory growth if transient sensory data is incorrectly persisted.

Epoch therefore keeps bounded queues, activation budgets, explicit timestamps,
explicit truth classes and deterministic reset as non-optional parts of the
prototype.

## Viability Measurements

Do not judge the prototype by novelty. Compare it against existing paths using:

- idle CPU cost with large mostly-static worlds;
- activations processed per meaningful world change;
- event-storm behavior and bounded degradation;
- replay determinism for exact-cause channels;
- authoring graph compile/activation correctness;
- event-camera event count versus equivalent frame-polling work;
- camera-to-action latency;
- memory bandwidth and temporary allocation rate;
- worker/task wakeups avoided;
- dense-scene crossover point where ordinary frame/batch processing wins.

If sparse scenarios do not produce a meaningful win or cleaner temporal
composition, keep the useful event-camera/signal pieces and do not force the
rest of the engine through this execution model.


## Current renderer integration boundary

The former cached/partial OpenGL bridge, damage queues and SELECTIVE/FULL benchmark
are removed in source by October30 operator direction. Historical receipts do not
accept the replacement source. Combined Debug/Release integration and native
full-frame image/responsiveness checks remain pending.

Ordinary asset, font and texture residency, immutable GUI draw snapshots, geometry
and Canvas2D derived raster preparation remain. Central events, time, jobs and
temporal ownership remain independent of this removal. Generic lighting and
neuromorphic primitives support future scoped work, not an automatic performance fix.
