# Current Engine Architecture

## Snapshot

Epoch is a C++23 module-first engine/editor. The published Windows/Linux
runtime baseline is `v0.88.69`; active development source is `v0.88.72`.
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
- **Exclusive editor switching**: normal editor operation owns one live backend,
  captures session state, retires the old renderer/native resources, creates one
  replacement in the same dock host, and restores state. Multicontext is an
  explicit diagnostic mode rather than the normal editor model.
- **Raylib custom frame control**: Epoch flushes Raylib drawing, swaps the native
  buffer, and polls input events when the vcpkg build enables custom frame
  control. Renderer retirement remains owner-thread and teardown ordered.
- **Reusable GUI boundary**: public primitives live under `Engine/include/gui`,
  implementation under `Engine/src/epochgui`, and module/build metadata under
  `Engine/dep/EpochGui`. Editor surfaces compose the library through the engine
  adapter; engine/render contexts do not own GUI feature logic.
- **Project/editor split**: launcher, editor, project generation, package review,
  scene preview, build output, and OS AI evidence are distinct workflows. Generated
  child builds have non-GUI validation paths and serialized build ownership.
- **Renderer resource spine**: shared resource handles, graph binding, sampled
  render-target descriptors, capability truth, and backend-native allocation
  are growing from OpenGL-family proof into explicit backend contracts.
- **Path and cache ownership**: runtime assets and disposable cache resolve from
  executable-local roots. Updates, packages, models, atlases, and logs retain
  separate cache/storage boundaries.
- **Release/update baseline**: the `v0.88.69` Windows/Linux release and updater
  are sealed. Source work may advance without editing updater behavior, packaging,
  tags, or release assets unless the operator explicitly reopens that gate.
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

The first implementation path is deliberately smaller:

`TimePoint -> event journal -> transaction -> page checkpoint -> branch overlay
-> reversible transform -> arbitrary-time observation -> sparse motion ->
analytic effect -> backward-rendering test -> branch export/reimport`

Multiplayer, unscripted AI, full physics, and advanced temporal rendering follow
only after exact undo/redo, deterministic replay, portable branches, and bounded
storage growth are proven.

## Current Gaps

- `.epoch` scene/world files are still metadata shells; parser/serializer does
  not yet own complete authoring, preview restore, and runtime handoff.
- Existing timeline, streaming-save, snapshot, and Video controls are precursor
  contracts, not the immutable event/page/branch temporal database.
- Renderer context coverage is broader than renderer feature parity. Vulkan and
  DirectX remain partial feature paths, and Software parity is a continuing goal.
- Professional docking, text controls, decoded asset previews, project browser
  operations, and independently routed GUI windows remain incomplete.
- Package installation, active downloaded content, servers/listeners, and native
  extensions remain explicit human-gated capabilities.
- OS AI remains operator-selected external tooling with evidence and promotion
  gates; it is not authoritative simulation and cannot silently activate itself.

## Current Priorities

1. Finish the active sampled render-to-texture/resource-truth gate without
   regressing the accepted context, GUI composition, or release baseline.
2. Begin the temporal production slice at stable time/identity primitives,
   canonical immutable events, atomic transactions, and exact replay tests.
3. Connect immutable pages and branch overlays to real project-owned scene
   loading, saving, reversible edits, and arbitrary-time observation.
4. Let Video/timeline GUI consume temporal state only after the data spine owns
   branch, checkpoint, direction, scrub, undo, and redo semantics.
5. Continue EpochGui, renderer capability truth, Software reference parity,
   package boundaries, generated-project parity, and Android bring-up as scoped
   source missions with explicit validation.

## Canonical References

- `temporal_engine_architecture.md`
- `renderer_feature_matrix.md`
- `runtime_and_editor_workflows.md`
- `gui_library_architecture.md`
- `../README.md`
- `../../../Changes/active_pass.md`
- `../../../Changes/mission_cache.md`
- `../../../Changes/roadmap.md`
