# Neuromorphic Engine Framework

## Status

Epoch v0.90.24 mainlines the CPU-only neuromorphic core from the v0.90.16
experimental branch and connects it to the v0.90.23 event-driven OpenGL scene
path. The old Neuro Lab/editor benchmark is intentionally not carried into
main. The runtime is production-wired, bounded, deterministic where required,
and covered by a CPU-only contract target.

The layer is an event-driven control/perception fabric. It does not replace
rasterization, the physics solver, the audio device, or the presentation clock
when fixed-step, frame-step, or batch execution is objectively better.

### Current ownership and evidence (October 7, 2026)

This contract covers reusable software, editor and game consumers; it is not a
second roadmap. `Changes/roadmap.md` orders unfinished delivery and the active
pass keeps model-built sandbox succession first.

| System | Actual implementation/integration | Remaining acceptance |
| --- | --- | --- |
| Sparse graph / `simulation.neuromorphic` | CPU bounded signals, weighted activation, leak/refractory gates, metrics and temporal reset; CPU contracts | Workload benchmarks and consumer-specific replay/cost proof |
| Event camera / `render.neuromorphic_camera` | Sampled luminance-to-event API and CPU fixtures; no production perception consumer found | Native consumer, total observation/event budget, latency/memory and threshold evidence |
| Timeline/task-graph adapters | Typed conversion and atomic graph compile with CPU fixtures | Actual authored graph/runtime/replay workflow; no continuous world-wide integration claimed |
| Invalidation / `render.neuromorphic_invalidation` | Persistent network in `opengl.state`, evaluated by `opengl.preview`; only partial-to-full promotion | Native image/cost proof, event storms/reset/resize; other backends' partial caches remain unproved |
| AI host triage | Deterministic path/identifier scoring in the existing self-coding panel; streaming activity and bounded discovery | Useful model patches, real builds and embedded succession; not learned/spiking agent integration |

The current graph has no hidden learning/weight updates. Event-camera output is
`visual_only`; timeline causes use `exact_cause`. Raster damage and validated
host/source evidence remain authoritative. High cached-render throughput is
not high presentation FPS, model throughput or proof of a whole-engine speedup.

### Measured next design steps

1. Keep correctness conservative: previous/current object and light damage,
   camera/global revisions, depth/occlusion reconstruction and full fallback.
   Unknown/nonlocal effects cannot be suppressed by activation policy.
2. Use the existing alternating same-cache benchmark for sparse-to-dense
   crossover. Account for graph activation, projection, redraw, presentation,
   queue drops and synchronized GPU timing separately. Benchmark mode itself
   changes pacing and is not normal-runtime throughput.
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

The v0.90.23 cached/partial OpenGL scene path now owns a persistent weighted
neuromorphic invalidation network in each OpenGL state. Dirty rectangles remain
the source of truth for correctness. The network receives sparse visual-change
signals and may conservatively promote a partial redraw to a full redraw when
activity becomes dense, its activation budget is exhausted, or work is dropped.
It is never allowed to suppress a real dirty region.

This is the first direct bridge between the neuromorphic runtime and the real
3D selective renderer. It intentionally does not cull authoritative NPC or
animation state yet; simulation LOD must use semantic/causal relevance signals
rather than infer gameplay truth from pixels.

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


## Late-September mainline integration boundary

From v0.90.24 onward the neuromorphic renderer bridge is a **policy layer over authoritative damage**, not a replacement for scene invalidation. Object previous/current bounds, bounded lighting damage, camera/global invalidation, dirty-region/coverage ceilings, and cache revisions decide what may be stale. The weighted graph may only promote work to a more conservative path.

A/B benchmarking introduced in v0.90.28 deliberately compares the selective policy against a forced 100% reconstruction through the same persistent cache and presentation path. The benchmark does not train or mutate the neuromorphic network; it measures the cost consequences of the current policy.

Renderer statistics introduced later distinguish presentation cadence from scene-render throughput. This matters especially for cached frames: a cached scene can require only a small amount of render work even while the editor remains intentionally capped at a much lower presentation FPS.

Lighting invalidation introduced in v0.90.29 remains renderer/scene evidence rather than neural inference. Point/spot influence volumes can seed bounded dirty work directly; ambient/directional changes remain global. Neuromorphic activity may then promote that work, never narrow it.
