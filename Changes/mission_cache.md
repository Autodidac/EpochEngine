# Mission Cache
- Preserve the engine-wide one-dot C++ naming contract and run the naming
  validator with every source move; do not reintroduce generic bridge or flat
  root ownership.
- Complete the click-driven temporal texture workflow over the existing
  `asset.texture_artifact` import/Library/material pipeline: undo/redo,
  save/reopen interaction proof, settings/cost visibility, cache rebuild, and
  additional formats admitted through capability evidence.
- Preserve the production project contract: every generated profile must
  materialize, atomically save/reopen, build, and child-run. Finish shared
  Play/Stop/Run focus and generation-safe process evidence in the visible UI.


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
- `render.canvas2d_cpu` now owns deterministic `T0-CPU` reference raster,
  explicit RGBA8 texture/clip bindings, fixed-point triangle coverage,
  nearest/linear sampling, alpha composition, final presentation compose,
  bounded metrics, image hashes, and staged failure diagnostics.
- `render.texture.residency` now owns bounded generation-checked physical
  records, logical-artifact reuse, priority/LRU eviction, pinning, upload
  budgets, backend epochs, recreation, metrics, and staged failure proof.
- `render.canvas2d_presentation` now connects complete CPU canvas output to that
  cache and an explicit backend-owned presentation packet; the primary OpenGL
  compositor is compiled and build-proven.
- Runtime project asset identity now authenticates compiled texture source
  revisions and binds content-derived logical references into a project-scoped
  Canvas2D resource service. Capability admission requires sampled-image
  evidence and reduces project/platform/renderer limits. Immutable scene
  publication rejects incomplete or stale resource closures atomically and
  preserves old-reader lifetime. The project texture pipeline now persists,
  reopens, authenticates, and leases compiled RGBA8 artifacts into immutable
  editor Canvas2D publication. The visible Assets controller and bounded
  interaction automation cover import, selection, assignment, clear, undo/redo,
  save, and reopen over that same spine. Next prove that automation live and
  compare each native adapter against the CPU oracle.
- authoring.tilemap owns stable map meaning, sparse chunks, semantic operations,
  bounded history, undo/redo, and deterministic compilation.
- asset.tilemap_artifact, project.tilemap_library, project.tilemap_pipeline, and
  render.canvas2d_tilemap own bounded runtime bytes, exact Project Library
  persistence/restore, stable asset identity, visible-chunk culling, animation,
  deterministic sprite ordering, collision records, and map-object output.
- project.tilemap_source owns canonical bounded Assets/Maps/*.epochmap source,
  exact reload, verified temporary writes, and atomic replacement.
- EpochGui owns reusable renderer-neutral tile workspace state and layout. The
  Game2D adapter binds palette/layer/grid tools, texture attachment, semantic
  edit operations, diagnostics, exact publication, and revision-cached preview.
  Context replacement restores serialized unsaved map history and portable view
  state without preserving physical resources.
- Project Save and the current editor Run publication consume this document.
  Standalone runtime preparation now regenerates canonical source or restores
  exact Library artifacts and authenticated texture closure. Project input uses
  its own canonical source/artifact pair with stable actions, keyboard/controller
  bindings, fixed-point dead zones, and deterministic sampling.
- `physics.solver2d` and `project.actor2d_runtime` now provide deterministic
  fixed-step AABB/circle collision, stable contacts, map collision, spawn, pause,
  reset, snapshots, bounded catch-up, and renderer-neutral Canvas2D publication.
- Finish live controller polling and visible project rebinding. Keep the editor
  camera profile separate from project input.
- Add authored one-way platform and slope semantics; current map collision treats
  accepted non-sensor records as full AABBs and must not be advertised otherwise.
- Prove actor Play/Stop, restart persistence, generated-child external Run,
  cache-deletion regeneration, and native presentation through approved runtime
  evidence. Build-safe tests are not visual proof.
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
  `project.asset_registry` now owns deterministic project/asset keys, portable
  path authentication, generation-checked lifetime, and accepted source
  revision. `project.texture_resources` validates that revision and the full
  artifact before binding CPU resource sets or requesting residency.
- Logical artifact revision is content-derived rather than copied from source
  edit sequence. Equivalent content reconstructed at a later sequence therefore
  reuses compiled/cache identity while the registry still authenticates the
  current source revision.
- `render.texture.artifact` maps the explicit linear RGBA8 mip into the shared
  standalone residency cache and preserves backend epochs as disposable state.
- Compiled artifact schema, stable hashing, and integrity validation are
  runtime-owned so game, mobile, console, server, and headless products can
  consume compiled output while excluding authoring UI and history. Bounded
  serialized artifact reading, Project Library persistence, runtime
  authentication, and capability-derived admission are build-proven; visible
  interaction evidence and additional execution formats remain delivery work.
- Standalone, atlas, bindless, and sparse representations are physical residency
  plans chosen by capability, budget, format, update rate, and workload.
- The first physical cache contract supports sampled color resources through a
  standalone base-mip upload. Atlas, bindless, sparse, streaming, and mip-aware
  paths extend that same identity/budget boundary instead of replacing it.
- Canvas2D presentation passes complete raster identity, pixel semantics, stable
  artifact identity, and scene-surface bounds across one renderer-neutral
  boundary. Backend adapters consume the packet without exposing native handles
  to authoring state.
- One shared scene-raster session now feeds OpenGL, SDL3, SFML3, Raylib3,
  Vulkan, DirectX/D3D11, and Software presentation adapters. Build success proves
  their source/module integration, not visible output, switch-cycle stability,
  or performance.
- Native presentation evidence is context-specific. Exact primary OpenGL pixel
  evidence does not imply compatibility or parity in another context, share
  group, API, or software surface, and safe refusal does not prove visible output.
- `render.canvas2d_evidence` owns origin/stride-aware bounded comparison against
  the T0-CPU presentation. OpenGL has approved exact live capture evidence;
  other adapters remain `Partial` until equivalent bounded evidence is accepted.
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
- OpenGL state guards must use core-profile-valid selectors; polygon mode
  restores both faces through `GL_FRONT_AND_BACK`. Backend sprite/resource
  work uses the live platform context and render-thread context identity first,
  with logical multicontext selection only as a no-current-context fallback.
  Never hide stale GL errors by blaming the next draw call.
- Engine Arcade remains the kernel-owned sampled-RTT consumer and procedural
  cabinet fallback. Its shared content contract must feed backend-owned sampled
  scene surfaces in every compiled context without leaking one API's ownership
  model into another. Optional reviewed cabinet assets stay extension-owned.
- When `EpochEngineExtensions` is reachable again, repair the explicit Epoch
  Arcade presentation: render one correctly oriented and aligned cabinet/screen
  pair, eliminate duplicated or leaked yellow/blue HUD and debug geometry, and
  prove both the normal scene path and sampled-RTT path in every supported
  context. This is separate from default generated-project package admission.

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

- World Outliner owns dockable `World`, `Assets`, and `Scripting` tool tabs;
  Assets/Scripting are not top-level workspace destinations. Preserve drag/drop,
  pane popout/redock, selection, and project ownership as those tabs mature.
- EpochGui now owns a primal `TextEditorController` over its text-control state:
  multi-line indexing, caret/selection, clipboard command routing, scroll,
  revision/dirty state, find, replacement, and save acknowledgement. The engine
  adapter must finish migrating script/asset text surfaces onto that controller
  and add syntax, diagnostics, tabs, search UI, and large-document virtualization
  incrementally rather than creating another editor implementation.
- Focus must target every renderer context presenting the selected scene object;
  the GUI root context is not sufficient in the parented multicontext shell.
- Retire the miscellaneous Tools dumping ground. Each action belongs with its
  owning World/Assets/Scripting/Project/AI/Timeline/Package surface, and every
  visible pane needs focus, selection, overflow, resize, empty, error, and
  dock/popout behavior reviewed as part of the subsystem that owns it.
- EpochGui is the reusable portable C++23 module/static-library layer.
- EpochGui owns backend-neutral text, font, image, input, selection, layout,
  popup, progress, rounded rectangle, panel, docking, and floating-window state.
- `Engine/dep/EpochGui` is the engine's local integration copy. Reusable controls
  land there first when required by an accepted slice; standalone-repository
  synchronization is a deliberate later repository operation, never mixed Git
  history or an unreviewed source copy.
- `gui.engine` owns engine input translation, theme/font state, clipping, batches,
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
- Plant Lab is a separate launcher editor for authoring custom trees, reusable
  tree assets, and forest configurations. Forest Factory is the placement
  portal inside the standard Epoch editor: it browses Plant Lab outputs and
  places/configures them in real project scenes. Shared morphology contracts do
  not merge those editors or make Forest Factory the generator.
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
- `authoring.morphology` is the first generalized branching-domain slice: stable
  node/segment/terminal IDs, deterministic domain recipes, per-organ temporal
  sampling, and voxel LOD plans. Editable typed nodes, compiled morphology
  artifacts, and sparse voxel rasterization remain subsequent slices.
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

- `local_ai_llama_cpp_runtime` is a plan/validation package, not a downloader
  or installer. It describes an offline `llama-cli` build, requires a reviewed
  immutable llama.cpp revision before any future fetch, starts no server, and
  keeps GGUF weights and licenses separate.
- The operator owns
  `Autodidac/Temporal_Parametric_Graph_Lindenmayer_System_Plant_Lab` and has
  explicitly authorized its Epoch integration. Source revision `40a3db7` is
  the recorded provenance point; there is no third-party license blocker.
- `authoring.morphology` generalizes the Plant Lab foundation across plant,
  vascular, respiratory, electrical, coral, and generic branching domains with
  stable IDs, per-organ time ranges, deterministic sampling, and voxel LOD
  planning. Plant Lab owns generation; Forest Factory is the standard-editor placement portal that consumes authored tree and forest-configuration outputs.
- Mainline owns stable contracts, validation, temporal/document identity, safe
  fallback, provenance policy, and project/runtime integration.
- EpochEngineExtensions owns heavy optional generators, FFT ocean, planetary or
  game-specific world stacks, immutable payload source, manifests, licenses,
  hashes, tests, and generated artifacts.
- Extension activation fails closed. Current package preflight evaluates
  verifier-produced evidence; it is not itself a downloader/verifier.
- Built-in mini-runtimes remain kernel-owned and can be exposed as package/script
  assets.
- Anything that can listen, host, bind a port, execute downloaded native code,
  or expose a control surface requires explicit human approval.
## OS AI

- Epoch does not own or train an internal LLM. It runs an operator-selected
  external/source-available model through a local OpenAI-compatible endpoint or
  a directly selected `llama-cli` plus separately licensed GGUF.
- Model discovery is inventory only. Selection and runtime state are explicit
  and executable-local.
- `ai.mcp` is model-provider-independent and owns bounded tool descriptors,
  calls, results, errors, capabilities, approval gates, cancellation, evidence,
  and session budgets. Epoch starts no MCP server/listener.
- The project tool registry covers inspect, create, save, document/script edit,
  build, run, test, capture, and diagnostics. Current execution is
  operator-invoked through real editor/project harness paths; model tool-call
  parsing and multi-step dispatch remain unfinished.
- Chat is not captured automatically. Explicit harness/trace work writes
  `model_exchange.jsonl`, `tool_trace.jsonl`, and bounded session packets as
  evidence, never hidden training or authority.
- Engine-source tools require a separate developer capability, allowlisted
  roots, patch preview, explicit approval, and build/test evidence. They never
  commit, push, release, or grant themselves permission.
- Network/server/model/package activity remains capability-, integrity-, and
  approval-gated.
## Build, Source, And Release Boundaries

- The operator explicitly reopened release work for the `v0.89.x` line. Preserve updater behavior while producing and validating the new Windows/Linux baseline; reseal the accepted release afterward.
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
- OpenGL frame capture is implemented by `opengl.capture` on Linux. The
  traditional bridge is Windows/MSVC-only because importing the capture module
  into `opengl.context` exhausts the MSVC module heap; compiling that bridge on
  Linux creates a duplicate global/module declaration.
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
