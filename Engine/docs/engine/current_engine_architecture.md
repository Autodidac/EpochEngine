# Current Engine Architecture

## Snapshot

Epoch is a C++23 module-first engine/editor. The published Windows/Linux runtime
remains sealed at `v0.89.06`; active source development is `v0.89.15`.
Runtime/editor code lives under `Engine/modules/`, `Engine/src/`, and
`Engine/include/`, with reusable GUI ownership mirrored into EpochGui and bulky
optional package implementations kept in EpochEngineExtensions.

The next foundational target is a persistent, reversible, event-driven
spacetime engine. That target is specified in
`temporal_engine_architecture.md`; it is a phased mission, not a statement that
event sourcing, immutable world pages, or arbitrary-time reconstruction already
ship.

## Established Baseline

- **Renderer contexts**: DirectX/D3D11, OpenGL, Vulkan, SDL, SFML, Raylib, and
  Software have engine-owned context paths. Backend feature depth still follows
  `renderer_feature_matrix.md`; context availability does not imply feature parity.
- **Primary editor surface**: the Windows parent host elects exactly one baked
  renderer surface. The first context owns it; launcher preselection or a live
  context switch transfers it only after state restoration, while missing
  targets use serialized replacement in the same slot. The primary cannot
  undock; secondary diagnostic contexts and routed panes retain popout/redock.
- **Vulkan retirement ownership**: the application registry lends shared
  lifetime to callbacks and atomically transfers the final owner to retirement.
  Device idle precedes GUI, pipeline, swapchain, and device destruction; every
  scene pipeline is reset before the logical device.
- **Raylib custom frame control**: Epoch flushes Raylib drawing, swaps the native
  buffer, and polls input events when the vcpkg build enables custom frame
  control. Renderer retirement remains owner-thread and teardown ordered.
- **Reusable GUI boundary**: public primitives live under `Engine/include/gui`,
  implementation under `Engine/src/epochgui`, and module/build metadata under
  `Engine/dep/EpochGui`. EpochGui now owns rounded style and toggle layout; the
  engine adapter renders it and Editor Settings keeps rounded controls opt-in.
  Engine/render contexts do not own GUI feature logic.
- **Application/editor split**: the launcher directly opens standard Editor,
  Plant Lab, or GUI Editor after launch-context selection.
  `editor.application` centralizes shared profile/scene validation while
  `editor.standard.cpp`, `editor.plant_lab.cpp`, and `editor.gui.cpp` own
  separate canonical scenes and policy. The shared shell enforces each
  application's surfaces, camera/dock defaults, panes, floating routes,
  authoring, and Run/Build availability. Project generation, package review,
  scene preview, build output, and OS AI evidence remain distinct workflows.
- **Plant ownership split**: Plant Lab is the dedicated plant-authoring
  application. Forest Factory remains in the standard editor as the
  vegetation-import and placement workflow that activates and consumes Plant
  Lab/package outputs in project scenes.
- **Renderer resource spine**: shared resource handles, graph binding, sampled
  render-target descriptors, capability truth, validated base-mip texture
  uploads, and backend-native allocation are growing from OpenGL-family proof
  into explicit backend contracts. `render.texture.residency` owns bounded,
  generation-checked logical-artifact reuse, priority/LRU eviction, pinning,
  upload budgets, backend epochs, recreation, and metrics without making
  physical handles canonical state.
- **Project capability admission**: `capability.profile` adapts existing
  render-device evidence once. `platform.budgets` owns recommendations; runtime
  cost stays unknown until measured. Project profiles and generated manifests
  own headless/portable/explicit policy with explicit experimental and software
  fallback rules. Project, System Info, Settings, and status diagnostics report
  active-editor and selected project-run admission separately. Missing legacy
  manifest policy defaults to portable without regeneration; invalid policy
  fails closed.
- **Temporal texture compilation boundary**: `authoring.texture` compiles sparse
  layer documents into deterministic owning RGBA8 mip artifacts and validates
  identity, dimensions, per-mip content, aggregate bytes, and payload digest.
  `asset.texture_artifact` owns the always-built artifact schema and
  validator; `render.texture.artifact` maps sealed linear RGBA8 mip 0 into the
  shared standalone residency cache. `project.asset_registry` and
  `project.texture_resources` authenticate in-memory project/source/artifact
  identity, own bounded CPU bindings, and optionally acquire residency without
  authoring UI. Serialized artifact reading and capability-derived admission remain. sRGB-native storage, compression, whole mip-chain upload, atlas,
  bindless, sparse, and streaming execution remain fail-closed or planned.
- **Canvas2D planning spine**: `render.canvas2d` validates project policy,
  pixel-aware camera/viewport mapping, stable sprite identity, logical texture
  materials, deterministic batching, tile descriptors, immutable submissions,
  offscreen targets, final composition, and bounded diagnostics.
  `render.canvas2d_cpu` provides the deterministic `T0-CPU` reference path with
  RGBA8 resources, clip bindings, fixed-point coverage, nearest/linear sampling,
  alpha modes, presentation composition, metrics, hashes, and staged contract
  diagnostics. OpenGL now supplies context-guarded native texture allocate,
  upload, readiness, and destruction hooks. `render.canvas2d_presentation`
  verifies complete raster/frame identity, full byte-derived content identity,
  residency acquisition, explicit image semantics, and native surface bounds.
  `opengl.canvas2d` provides a compiled primary-context final compositor with
  context-owned texture validation, viewport-confined clear/draw work,
  top-left-to-GL coordinate conversion, and scoped GL state restoration.
  Build-safe family, staged-presentation, no-context refusal, immutable scene
  publication, semantic editor projection, and protected scene-slot routing
  contracts pass. Project-scoped texture identity, sampled-image admission,
  exact immutable resource closure, Project Library persistence, temporal
  tilemap authoring/runtime compilation, deterministic input artifacts, fixed-
  step 2D solving, and actor publication are build-proven. Operator-visible
  native pixels, secondary GL share groups, live controller polling, physical
  audio, sprite animation, advanced collision semantics, and the approved
  generated-child gameplay proof remain active gates.
- **Arcade scene-surface proof**: one shared attract-pattern contract feeds
  backend-owned sampled surfaces in OpenGL, SDL3, SFML3, Raylib3, Vulkan,
  DirectX, and Software. Build contracts prove ownership/routing; visual and
  repeated-switch evidence remains `Partial` until operator validation.
- **Path and cache ownership**: runtime assets and disposable cache resolve from
  executable-local roots. Updates, packages, models, atlases, and logs retain
  separate cache/storage boundaries.
- **Release/update baseline**: published `v0.89.06` updater, packaging, tag, and
  runtime assets are sealed. Source advancement does not reopen that lane.
- **Build lanes**: Visual Studio/MSBuild and root CMake remain aligned; Linux
  full-engine production proof uses current Clang, module-aware CMake/Ninja,
  vcpkg, package staging, contract checks, and bounded OpenGL smoke.

## Temporal Direction

The renderer is an observer of a world addressed by timeline, branch, and time.
It must not become the authoritative owner of simulation time. The target model
requires:

- explicit real, simulation, physics, presentation, animation, effects, audio,
  network, editor, and replay time domains
- immutable typed events committed in atomic transactions
- immutable base worlds with copy-on-write branch overlays
- content-addressed immutable pages and checkpoint manifests
- deterministic replay with identity-keyed or recorded nondeterminism
- declared exact, error-bounded, visual-only, and disposable truth classes
- sparse motion models and correction keys around meaningful discontinuities
- causal invalidation of only the affected future
- tickless persistent regions and observer-driven fidelity
- timeline/branch/direction-keyed rendering histories
- signed branch packages, quarantined foreign content, and server-owned authority
- a side-effect gateway for actions that timeline rewind cannot reverse

The first implementation path is deliberately smaller. `temporal.request`
already provides explicit global/sample mapping, rates, anchors,
forward/reverse/frozen direction, generation-checked subjects, bounded
exact/nearest/bracket observation, reconstruction evidence, and metrics. It is
a request/retention foundation, not the immutable event/page/branch world.

The larger implementation path remains:

`TimePoint -> event journal -> transaction -> page checkpoint -> branch overlay
-> reversible transform -> arbitrary-time observation -> sparse motion ->
analytic effect -> backward-rendering test -> branch export/reimport`

Multiplayer, unscripted AI, full physics, and advanced temporal rendering follow
only after exact undo/redo, deterministic replay, portable branches, and bounded
storage growth are proven.

## Current Gaps

- Canonical snapshot persistence now owns validated scene save and runtime
  projection, but project documents do not yet cover complete tilemap, texture,
  animation, physics, and audio authoring/runtime handoff.
- Existing timeline, streaming-save, snapshot, and Video controls are precursor
  contracts, not the immutable event/page/branch temporal database.
- Renderer context coverage is broader than renderer feature parity. Vulkan and
  DirectX remain partial feature paths, and Software parity is a continuing goal.
- Professional docking guide zones, text controls, decoded asset previews, project browser
  operations, and independently routed GUI windows remain incomplete.
- Runtime project asset keys are currently deterministic from canonical logical
  paths. Persistent imported asset IDs and explicit rename/move migration are still
  required before project-browser moves can preserve logical identity.
- The always-built compiled texture schema is still named
  `asset.texture_artifact` even though it has no authoring UI/history
  dependency. Runtime-oriented module/namespace ownership remains cleanup before
  the product API is frozen.
- Package installation, active downloaded content, servers/listeners, and native
  extensions remain explicit human-gated capabilities.
- OS AI remains operator-selected external tooling with evidence and promotion
  gates; it is not authoritative simulation and cannot silently activate itself.

## Current Priorities

1. Complete the playable baseline 2D path: Canvas2D compose, sprite/runtime
   artifacts, tilemap authoring, input, deterministic 2D physics, physical audio,
   save/reopen, Play/Stop, Run, and Build.
2. Keep capability selection, settings, diagnostics, and project requirements
   aligned with proven T0-CPU and T1-GL behavior before broadening claims.
3. Extend the proven protected Canvas2D scene-slot route through authenticated
   project textures, serialized artifact reading, and capability-derived
   admission, then continue through sRGB, compression, mip-chain, atlas,
   bindless, sparse, and streaming policies only as evidence permits.
4. Continue EpochGui controls and desktop docking without making floating hosts
   mandatory for game, mobile, console, or headless products.
5. Preserve the sealed runtime/updater baseline while Debug, Release, Clang, and
   build-safe contracts prove each source checkpoint.

## Canonical References

- `temporal_engine_architecture.md`
- `renderer_feature_matrix.md`
- `runtime_and_editor_workflows.md`
- `gui_library_architecture.md`
- `../README.md`
- `../../../Changes/active_pass.md`
- `../../../Changes/mission_cache.md`
- `../../../Changes/roadmap.md`
