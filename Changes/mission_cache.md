# Mission Cache

This file preserves durable operator intent and accepted constraints. The active
gate is `Changes/active_pass.md`; scheduling is `Changes/roadmap.md`; architecture
is `Engine/docs/engine/capability_tier_architecture.md`. Debugging chronology and
release history belong in the changelog/archive, not architecture docs.

## Immediate Product Mission

- Deliver a playable baseline 2D project within two months.
- The acceptance loop is author map, control actor, animate, collide, play audio,
  save/reopen, Play/Stop, Run, and Build.
- Keep the plan streamlined. Advanced 3D, networking, persistent AI, planetary
  work, and high-tier effects cannot displace the 2D critical path.
- Every implemented system must bring its settings, controls, diagnostics,
  persistence, and tests with it. Avoid dead controls and undocumented knobs.

## Capability-Tier Mission

- Tiers are backend-neutral capability levels, not API rankings.
- `T0-CPU` is the universal correctness, headless, software, deterministic, and
  CI floor.
- `T1-GLES` and `T1-GL` are mobile/desktop compatibility raster profiles.
- T1 adds capability-gated portable compute. T2 is explicit Vulkan/DirectX
  graphics/compute. T3 adds individually proven advanced explicit features.
  T4 is inline hardware ray query. T5 is a full RT pipeline.
- Select implementations per subsystem/request using project requirements,
  features, evidence, quality, determinism, stability, latency, memory, power,
  and measured policy.
- Equivalent Vulkan and DirectX capabilities use the same numeric tier.
- Vendor-specific features are capability packs, not fictional higher tiers.
- Extend existing `Capabilities`, `Budgets`, `perf.tier`, runtime profiles, and
  render-device evidence. Never create a parallel global tier registry.
- `platform.budgets` owns performance-tier recommendations; implementation cost
  remains unknown until measured or explicitly estimated. Never copy available
  VRAM/upload budgets into cost fields.
- Project profiles own typed headless/portable/explicit requirements plus
  experimental and software-fallback policy. Diagnostics distinguish the active
  editor backend from the independently selected project-run backend.
- Generated manifests persist `capability_profile`; a missing legacy field maps
  to portable without regeneration, while malformed, duplicate, wrong-type,
  unknown, or mismatched policy fails closed.
- `Present` requires implementation plus validation evidence. Descriptors,
  safe refusal, API version, and build success alone do not prove presentation.
- Passive provider scoring may use comparable single-context evidence only.
  Multicontext diagnostics are excluded because concurrent backends distort FPS,
  latency, memory, input, and stability measurements.

## Baseline 2D Mission

- Build Canvas2D as a real project/runtime path, not a decorative editor mode.
- `render.canvas2d` now owns renderer-neutral pixel-aware camera/viewport policy,
  offscreen/final-compose planning, sprite material/alpha/sampler declarations,
  stable ordering, bounded quad batches, tile set/layer/chunk validation,
  immutable submissions, diagnostics, and project settings.
- `render.canvas2d.cpu` now owns deterministic `T0-CPU` reference raster,
  explicit RGBA8 texture/clip bindings, fixed-point triangle coverage,
  nearest/linear sampling, alpha composition, final presentation compose,
  bounded metrics, image hashes, and staged failure diagnostics.
- `render.texture.residency` now owns bounded generation-checked physical
  records, logical-artifact reuse, priority/LRU eviction, pinning, upload
  budgets, backend epochs, recreation, metrics, and staged failure proof.
- `render.canvas2d.presentation` now connects complete CPU canvas output to that
  cache and an explicit backend-owned presentation packet; the primary OpenGL
  compositor is compiled and build-proven.
- Runtime project asset identity now authenticates compiled texture source
  revisions and binds content-derived logical references into bounded Canvas2D
  CPU resource sets plus optional residency. Immutable semantic scene
  publication now routes committed editor entities through the protected OpenGL
  scene slot. Next bind authenticated project textures there and compare live
  native pixels to the CPU oracle.
- Extend the current tile descriptors with palettes, map objects, collision,
  visible-chunk culling, project persistence, and compiled runtime artifacts.
- Add animation-frame selection and sprite culling without weakening stable draw
  order or exposing physical cache identity as authoring state.
- Add configurable input actions and keyboard/controller bindings.
- Add a deterministic fixed-step 2D solver adapter behind `physics.manager`.
- Add a physical audio adapter behind `audio.manager` with buses and clean device
  failure.
- The editor must author and persist the same scene that Play, Run, and Build
  consume.
- Default project state should include useful camera, ground/map, light where the
  selected renderer needs it, spawn, and starter object rather than placeholder
  junk.
- Selection uses the shared ray/query system; Focus changes the actual camera.
- Game/mobile/headless builds can omit editor/floating GUI and unused backends.

## Texture And Resource Mission

- Every editable texture separates authoring document, semantic history,
  compiled artifact, and physical execution cache.
- Texture documents use stable identity/revision, sparse tiles, layers, semantic
  operations, deterministic brushes where applicable, undo/redo, checkpoints,
  bounded history, dependencies, diagnostics, and deterministic compilation.
  The first compiler produces owning RGBA8 mip payloads and validates their
  complete identity/content chain; unsupported conversions and compression fail
  closed rather than relabeling bytes.
- Logical texture identity never contains descriptor slots, atlas coordinates,
  sparse mappings, GPU handles, upload state, or preview targets.
- Project asset identity is supplied separately from compiled content identity.
  `project.asset.registry` now owns deterministic project/asset keys, portable
  path authentication, generation-checked lifetime, and accepted source
  revision. `project.texture.resources` validates that revision and the full
  artifact before binding CPU resource sets or requesting residency.
- Logical artifact revision is content-derived rather than copied from source
  edit sequence. Equivalent content reconstructed at a later sequence therefore
  reuses compiled/cache identity while the registry still authenticates the
  current source revision.
- `render.texture.artifact` maps the explicit linear RGBA8 mip into the shared
  standalone residency cache and preserves backend epochs as disposable state.
- Compiled artifact schema, stable hashing, and integrity validation are
  runtime-owned so game, mobile, console, server, and headless products can
  consume compiled output while excluding authoring UI and history. Serialized
  artifact reading and capability-derived admission remain the next
  consumption gate.
- Standalone, atlas, bindless, and sparse representations are physical residency
  plans chosen by capability, budget, format, update rate, and workload.
- The first physical cache contract supports sampled color resources through a
  standalone base-mip upload. Atlas, bindless, sparse, streaming, and mip-aware
  paths extend that same identity/budget boundary instead of replacing it.
- Canvas2D presentation passes complete raster identity, pixel semantics, stable
  artifact identity, and scene-surface bounds across one renderer-neutral
  boundary. Backend adapters consume the packet without exposing native handles
  to authoring state.
- Native presentation evidence is context-specific. A primary OpenGL compositor
  does not imply SDL3, SFML3, or Raylib3 share-group compatibility, and a
  no-context refusal test does not imply visible pixel proof.
- Atlases remain useful compatibility and batching caches; they are not canonical
  or universally modern/obsolete.
- Source and meaningful history are portable. Library output and cache variants
  are reproducible and disposable.
- Cost surfaces expose decoded/compressed bytes, resident/virtual bytes, tiles,
  mip cost, padding, upload cost, history cost, and cache pressure.

## Renderer Spine

- `render.device` and `render.graph` own logical resources, materials, targets,
  bindings, commands, passes, dependencies, and sampled render surfaces.
- OpenGL is the first portable technique proof, not the authoritative engine
  model. It implements explicit-style Epoch contracts.
- OpenGL-derived SDL3, SFML3, and Raylib3 share engine data/contracts while each
  retains backend-specific context, window, lifetime, and presentation proof.
- Vulkan and DirectX implement equivalent native contracts in their own models.
- Software is a first-class deterministic/headless/fallback device, not an
  emulation of Vulkan or DirectX.
- The compatibility floor targets GTX 1660-era hardware and equivalent APIs.
  New features scale upward through capability gates.
- Preserve scene, GUI replay, top-layer, queue-drain, and present order unless a
  bounded draw-model mission explicitly proves a replacement.
- Sampled RTT truth remains layered: descriptor, graph, hook/adapter, live
  allocation, scene-surface path, presentation, benchmark, and production
  evidence.
- Engine Arcade remains the kernel-owned sampled-RTT consumer and procedural
  cabinet fallback. Its shared content contract must feed backend-owned sampled
  scene surfaces in every compiled context without leaking one API's ownership
  model into another. Optional reviewed cabinet assets stay extension-owned.

## Backend Parity And Contexts

- Normal editor use owns one primary backend surface. Context selection is a
  state-preserving whole-editor replacement or live-context promotion, not a
  second editor or fake dropdown.
- In a parented multicontext diagnostic host, exactly one context is the baked
  primary surface and cannot undock. Secondary diagnostic contexts and routed
  pane windows may pop out/redock; multicontext is never passive benchmark data.
- Launcher context selection is prelaunch configuration for three application
  profiles: standard Editor, Plant Lab, and GUI Editor. They share one
  shell/service spine but own separate source files, scenes, surface masks,
  camera/dock defaults, panes, and authoring/run policy.
- A switch captures state, retires/cleans the source, creates the exact selected
  backend in the stable host, restores state, proves a frame, and fails closed.
- No retired renderer continues in the background wasting resources.
- Floating GUI panes are individual routed panels, not context selectors or full
  editor clones.
- Redocking ultimately needs visible professional guide zones and a dock control;
  games/mobile/headless products can compile native popout hosts out.
- Operator proof accepts SDL3, SFML3, DirectX, and Software scene-solid
  orientation. Raylib and Vulkan remain `Partial` until their current correction
  candidate receives eye proof.
- Fill adapters preserve complete triangles, report failures, and must match the
  shared clockwise-outward contract without changing GUI/present order.
- Vulkan keeps its scene-solid triangle pipeline and culling separate from line
  and GUI pipelines, with correct depth and safe preview invalidation. Retirement
  must remove registry ownership atomically, retain callback lifetime, wait for
  device idle, and destroy every pipeline before destroying the device.
- Raylib3 retains specialized owner-thread/OpenGL/context behavior and must be
  tested in single-context and diagnostic multicontext lanes.
- Whole-editor repeated-switch, font, focus, state, redock/close, and no-stale-
  background-rendering proof remains required for future release claims.

## GUI And Editor

- EpochGui is the reusable portable C++23 module/static-library layer.
- EpochGui owns backend-neutral text, font, image, input, selection, layout,
  popup, progress, rounded rectangle, panel, docking, and floating-window state.
- `engine.gui` owns engine input translation, theme/font state, clipping, batches,
  top-layer replay, renderer drawing, native hosts, and project integration.
- Games, mobile, console, generated runtime, and headless builds can retain only
  the portable controls/artifact readers they need.
- Text surfaces need selection, copy/paste, wrapping, scroll bounds, focus,
  deselection, and context menus.
- Modal/dropdown/menu hit testing must capture input; events cannot fall through
  or close lower menu items prematurely.
- Themes include system light/dark, explicit light, and explicit dark. Future
  professional styling is separate from those functional choices.
- Settings use progressive disclosure and match active capability evidence.
  Rounded GUI controls are an opt-in EpochGui-owned style policy, disabled by
  default and preserved across editor context handoff.
- The Console dock reports evidence/status; it is not a substitute for actual
  workspace controls.
- Package Manager needs real rows, action/status, transfer/build progress,
  license/source/cache evidence, and cancellation without fake progress.
- Plant Lab is the dedicated launcher application for procedural vegetation
  authoring. Forest Factory remains a standard-editor surface/tool that consumes
  Plant Lab outputs for vegetation browsing, scene/object import, placement,
  package activation, and project-visible asset use.
- GUI Editor similarly authors reusable GUI documents/assets; the standard
  editor consumes those results without duplicating the dedicated designer.

## Shared Scene Systems

- `render.math` is the shared renderer-neutral float-vector/linear-color
  vocabulary. Domain-specific high-precision physics math may remain separate.
- `render.lighting` owns logical lights, bounded registries, immutable frames,
  ambient environment, metrics, and reference raster evaluation. Native shaders,
  light buffers, PBR, and shadows require separate evidence.
- `render.ray` owns validated CPU reference AABB/sphere/triangle/scene queries
  and voxel DDA. Hardware acceleration structures and ray pipelines are separate
  capabilities.
- `physics.manager` owns bodies, deterministic bounded commands, fixed-domain
  commits, snapshots/restoration, and metrics. Solvers consume this spine.
- `audio.manager` owns clips, sources, buses, listener/spatial state, temporal
  policy, mix plans, and metrics. Physical adapters consume those plans.
- `voxel.storage` owns deterministic sparse chunks/cells, budgets, atomic writes,
  hashes, snapshots, queries, negative coordinates, and metrics.
- `water.system` owns stable water bodies, explicit-time analytic queries,
  bounded voxel projection, and metrics. Projection is disposable cache data.
- `scene.tier0` and terrain foundations provide reusable default scene/ground
  descriptors; optional heavyweight terrain remains extension-owned.
- Connect systems through canonical handles/adapters instead of repeating small
  math, light, selection, state, or timing implementations. Preserve genuinely
  local systems when they own a distinct lifecycle or precision domain.

## Temporal World

- Time, timeline, and branch are primary world coordinates.
- Authoritative mutation is immutable typed events committed atomically.
- Installed/base worlds are immutable; saves, edits, mods, predictions, and
  alternate histories are copy-on-write overlays.
- World storage is content-addressed, page-based, schema-versioned, bounded,
  hashed, deduplicated, checkpointed, and garbage-collected.
- Truth classes are exact, error-bounded, visual-only, and disposable.
- Nondeterminism is identity-keyed or recorded; global random-call order and
  thread scheduling cannot define persistent truth.
- Past edits invalidate only their causal future.
- Persistent regions advance from meaningful scheduled changes and observer
  demand, not one global tick.
- Backward rendering keys history by timeline, branch, sample time, direction,
  and camera.
- External side effects cross an explicit gateway and cannot be rewound.
- The 2D objective uses only the narrow temporal value it needs now:
  semantic asset/scene history, save/reopen, explicit physics time, and
  deterministic animation. The full world campaign follows later.
- `temporal.request` is the first shared runtime foundation: `GlobalTime`,
  `SampleTime`, `Duration`, `TemporalRate`, and `TemporalAnchor` explicitly map
  `sample = anchor.sample + (global - anchor.global) * rate`. Negative, zero,
  and positive rates represent reverse, frozen, and forward observation.
- Request-driven history is generation-checked and bounded. Exact, nearest,
  bracket, and boundary-clamped observations carry truth and reconstruction
  evidence; physical observation caches are disposable and never canonical
  authoring/world state.
- Store metrics retain lifetime append/replacement/eviction/rejection evidence
  after subjects retire while separately reporting live subjects and retained
  sample capacity.
- `Autodidac/VoxelRayBenchmark` is an external evidence laboratory for
  request-driven voxel/ray techniques. Epoch may ingest immutable benchmark
  result packets and licensed algorithmic findings after publication, but a
  repository name, bootstrap README, or local run is not production evidence.
  Multi-context runtime measurements remain excluded from automatic backend
  selection because concurrent contexts distort normal editor cost.

## Temporal Authoring

- All editable domains share stable documents, semantic commands/operations,
  bounded history, dependencies, compilation, diagnostics, previews, and
  source/library/history/cache separation.
- Widgets never mutate document internals directly.
- Build one typed node graph for texture, material, particles, animation, audio,
  AI, procedural model, and simulation domains; do not create incompatible graph
  frameworks.
- Graph execution compiles through validated IR into CPU, SIMD, GPU, or software
  plans. Evaluation caches are derived and shared by identical revisions.
- Texture is the first domain because it directly unlocks the 2D product.
- Material, model, effects, animation, general scene, collaboration, and broad UX
  phases follow in dependency order.
- Previews are budgeted, cancellable, cacheable, generation-checked, lower
  priority than interaction, and software-fallback capable.
- Downloaded projects never silently execute native editor extensions.

## OpenGL Technique Laboratory

- `Autodidac/tiered_gfx_OpenGL_modular_context_demo` is a reference/ingestion
  source, not an authority or donor engine.
- Translate capability/quality controls into Epoch's existing profiles/budgets.
- Translate material, view, post-process, RTT, and effect behavior into
  renderer-neutral descriptors and render-graph dependencies.
- OpenGL implementation belongs under the OpenGL backend and cannot leak raw GL
  objects into documents or shared resources.
- Do not copy its alternate resource spine, platform bootstrap, vendored
  EpochGui, hard-coded scenes, direct uniforms, or unsupported status labels.
- Preserve MIT notices for derived code. Only manifest/hash/license-proven assets
  may enter packages. Curated assets without equivalent provenance stay
  quarantined.
- Immediate useful subset: RTT/final compose, texture policy, instancing ideas,
  and small CC0 diagnostics. PBR, sky, shadows, terrain, water, foliage,
  particles, cloth, and other effects follow the playable 2D loop.

## Extensions And Packages

- Mainline owns stable contracts, validation, safe fallback, provenance policy,
  and project/runtime integration.
- EpochEngineExtensions owns heavy optional generators, FFT ocean, planetary or
  game-specific world stacks, immutable payload source, manifests, licenses,
  hashes, tests, and generated artifacts.
- Extension activation fails closed. Current package preflight evaluates
  verifier-produced evidence; it is not itself a downloader/verifier.
- Built-in mini-runtimes remain kernel-owned and can be exposed as package/script
  assets.
- Anything that can listen, host, bind a port, execute native code, or expose a
  control surface requires explicit human approval.

## OS AI

- OS AI means operator-selected external/source-available models, not an
  internal persona.
- Model selection initializes exactly the selected model and persists only in
  executable-local cache state.
- AI/tool decisions require visible evidence and never become hidden training or
  authority.
- Network/server/model activity remains capability- and approval-gated.

## Build, Source, And Release Boundaries

- The `v0.88.69` packaged runtime/updater is sealed until explicitly reopened.
- Linux and Windows normal builds use vcpkg according to their documented lanes;
  headless diagnostics may intentionally differ.
- Linux/WSL runtime proof defaults to single-context OpenGL. Vulkan is explicit
  validation; DirectX is disabled; software is fallback.
- C++23 is the current baseline. C++26 adoption waits for compiler/module/vendor
  readiness and does not fork core logic.
- Public headers live under `Engine/include`; internal headers stay with owners;
  renderer sources live under `src/renderers/<backend>`.
- CMake, MSVC items/filters, presets, scripts, and generated project metadata
  must remain aligned as source moves.
- Never stage generated builds, caches, logs, captures, local projects, or
  unrelated operator files.

## Delivery Discipline

- Use multiple bounded agents when source ownership is disjoint, then integrate
  their actual results. Never ask agents to collide in the same file family.
- Favor implementation, tests, and faithful local proof over long speculative
  analysis or placeholder code.
- High output means many completed vertical slices, not inflated line counts.
- Diagnose the earliest causal error from complete logs and rerun the closest
  production command.
- Preserve unrelated dirty work and avoid temporary branches unless genuinely
  required or explicitly requested.
- Before a requested push: inspect status, build/test the focused batch, stage
  only intended files, commit deliberately, and push the existing branch.