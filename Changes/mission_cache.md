# Mission Cache
- v0.89.31 is the intentionally source-only successor to the binary-first
  v0.89.30 package. Advance both source authorities and source-build Windows
  metadata to 31 while leaving all platform packaged-version authorities at 30.
  The updater build-safe contract must prove `source 31 > packaged 30 > prior
  29`; never restore source/package equality merely to simplify the test. Site
  v43 has now published and independently verified the v0.89.30 EpochEditor
  runtime; activate v0.89.31 source only through a separate exact-commit pass.
- Linux v0.89.31 source proof uses the updater-equivalent managed Clang 22.1.8
  Release lane with source delivery enabled. All 1325 build steps pass; the
  24,731,808-byte executable has SHA-256
  `ef6e880195c7fd596685bba57642eaacd21c9bc9270ac3e79200374c9086854e`, reports
  v0.89.31, and passes the aggregate build-safe contract without renderer startup.
  All 33 no-display Linux CTests pass.
- The local v0.89.30 editor-identity baseline now names the executable project
  `EpochEditor` and the static engine project/library `EpochEngine` everywhere
  active: solution/project paths, project references, updater source-build
  target, generated-project links and migration, runtime/source discovery,
  tooling, ignores, and current documentation. The only retained
  `StaticLib1.lib` text is a deliberate one-way migration input for existing
  generated child projects.
- Template-only debris is gone: the repo-local `.codex/.gitkeep`, tracked
  `.vcxproj.user` files, the dead `fake()` library anchor, and its empty
  framework/PCH chain were removed. EpochEngine disables that unused PCH lane
  instead of keeping Visual Studio wizard scaffolding as production source.
- Exact local proof after cleanup: source naming passes all 495 first-party
  files; the stale-name audit and whitespace check pass; Debug EpochEditor is
  30,246,400 bytes (`8e78777eca9bbe6d62dd7c8e0ca5e5f92c40feb2a543292b22ca08523f8a8f2f`)
  and Debug EpochEngine.lib is 637,881,618 bytes
  (`31cc46ebc9d565357f6797ed02b3598a3c8ae51cc36f936a113c2ececd7d7dd3`);
  Release EpochEditor is 9,585,664 bytes
  (`8770aaecac4accf07cfcd9f029b0ecb2f8a0ae2effcec2b1b43a04f85f8c9f13`)
  and Release EpochEngine.lib is 255,133,070 bytes
  (`7c2dce995e87af7b4d6a896fccf9648d807aba0c03a88df8c4222b198fbdc471`).
  Both Debug and Release `--engine-contract-self-test` lanes pass with exit 0;
  no GUI or renderer was launched.
- Current v0.89.30 Release eye evidence shows the canonical World surface, World
  Outliner, Properties, Output, and AI Chat layout. The earlier bad start was
  the older v0.89.29 package, not a missing release config; standard layout
  preferences remain user-local under
  `%LOCALAPPDATA%/EpochEngine/config/standard_editor.layout`. The workspace
  strip now uses explicit full-label widths and a 34-pixel height so Assets and
  Systems do not truncate; new-build eye proof remains separate.
- EpochGui now owns a renderer-neutral responsive tab-strip layout. It preserves
  full requested widths, keeps the active route visible when space permits, and
  returns stable visible/overflow index sets. The engine GUI adapter renders the
  bounded overflow selector; the editor consumes it for the main workspace row
  and reviewed engine-source tabs. Direct EpochGui CTest plus fresh Debug and
  Release editor/aggregate contracts pass. Rebuilt editor evidence is Debug
  30,265,344 bytes / `7238606367b635289bebb404ae968fcff848500201f808b49b25bd9fcc1886a8`
  and Release 9,596,416 bytes /
  `ef50e4836ccb555ad95b233d8b6c5f052248e109c1ee51b097ffec4f46f90903`.
- The operator's professional-editor concept art is a design target, not runtime
  evidence. Close its gaps through shared ownership: EpochGui provides compact
  icon toolbars, responsive non-truncating strips/overflow, hierarchy/table
  controls, inspector sections, timeline/task/evidence/status primitives, and
  DPI/minimum-size behavior; the editor composes those controls into navigator,
  scene, inspector, timeline, task, output, performance, and AI workspaces; the
  engine supplies only real scene components, materials, tracks, simulation,
  build, and telemetry data. Never fake cinematic content or capability claims.
- Do not convert the concept images into shipped AI-generated art. When a later
  content pass needs production assets, admit only professional third-party
  assets whose exact origin, version, commercial-use license, attribution terms,
  archive digest, and installed project/package paths are recorded. Keep source
  downloads outside the repo until that provenance gate passes, then stage only
  the reviewed asset payload and required license material; remove temporary
  downloads and extraction debris afterward.
- Iterate through substantial bounded parity passes across every supported
  editor/renderer context. Each cycle is focused implementation, local
  headless/contract plus Debug/Release evidence, a rollbackable checkpoint,
  Site-agent ingestion, and an intermittent release after a meaningful
  known-good batch. Keep several releases visible on the Site until the
  operator finishes rollback testing; removal is a later explicit cleanup.
- GitHub is not an Epoch repository, updater, release, or publication authority.
  The Epoch Site task owns protected ingestion and release history. Compatibility
  sequencing matters: shipped v0.89.29 embeds the old source-build target, so
  publish a tested packaged runtime carrying `EpochEditor` before activating a
  renamed source checkpoint for source rebuilds. A following newer source
  checkpoint then exercises the renamed updater lane without stranding the old
  client.
- The published binary-first v0.89.30 runtime is built from exact commit
  `5d6fcf982d9d8e062d0dc919502444bb5cf3458d`. Windows is 29,736,248 bytes /
  SHA-256
  `fb222ac7ae0ed21ce4f231c30e942db82f0a6226c8f7c569016eb985af70580a`;
  Linux is 31,119,632 bytes / SHA-256
  `bc5cedb8e59614d8dc38327a1657e4fcd63f5cd0b82dccc576365f70b6e60435`.
  Both staged packages pass version and build-safe engine contracts. Windows has
  one root `EpochEditor.exe`; both archives exclude legacy target names, Git
  metadata, logs, and caches. Linux native pixels remain unclaimed because its
  renderer smoke was explicitly skipped. Site v43 at exact Site commit
  `1b7ef1fce6a9bc1fdd00dc7b7655494b99e3546b` independently re-downloaded both
  immutable assets, verified these hashes and the Ed25519 envelope, and retained
  v0.89.29/v0.89.28/v0.89.27/v0.89.06 for rollback.
- Preserve the temporal GUI authoring boundary: `authoring.gui_document` owns
  stable widget meaning/history plus layout, style, interaction, image, and tab
  state; EpochGui owns reusable image, tab, text, and graph controls; editor
  scene entities remain a compatible scene projection. The portable bounded
  GUI source codec, canonical Canvas/Runtime Preview/Component Graph/Styles
  workspace, and deterministic compiled artifact are present. Finish
  editor-free runtime focus, actions, text input, tab execution, and exact
  image-resource consumption.
- Preserve the professional camera contract across every presenting context:
  stable logical view identity; independent perspective/free-orthographic/axis
  framing; LMB selection; Alt+LMB orbit; MMB pan; Alt+RMB dolly; RMB fly with
  WASD/QE and speed modifiers; Focus and Reset. Finish oriented grids/gizmos,
  authored project-camera persistence, mobile/console input mappings, and live
  all-context interaction proof without letting splitters or GUI controls leak
  input into scene navigation.
- Continue portals on the shared camera and render-pass spine. The bounded
  recursive planner, cycle/depth/pixel refusal, clip descriptors, logical RTT
  outputs, and disposable cache state are present. Add backend-native
  oblique/stencil clipping, visible portal surfaces, traversal, audio/physics
  policy, and approved cross-context pixels before claiming runtime portals.
- Keep Standard Editor, Plant Lab, and GUI Editor as separate application
  workspaces over one shared shell. Main document and scene tabs remain fixed;
  every non-scene tool owns an independent routed tab that may join a compatible
  left, right, or bottom tool stack or float in a native context-backed host.
  Finish persistent layouts, arbitrary compatible stack placement, and
  app-specific authored content without cloning editor state.
- Reconstruct external extension payloads only from reviewed operator-owned
  source or explicit local work. The main repo may retain descriptor-only
  identity/provenance/integrity/activation contracts while remote access is
  unavailable; it must not claim missing payloads are installed.
- Continue the Asset Browser from its current real texture slice: canonical
  `.epoch_texture` source, sparse layer/brush edits, undo/redo, atomic
  save/reopen, deterministic compilation, exact previews, Library regeneration,
  and semantic material controls are present. Add persistent catalog metadata,
  thumbnail virtualization, folders, dependency inspection, and broader formats
  without moving physical handles into source documents.
- Keep graph UI reusable and data truthful. EpochGui owns generic node/pin/edge
  viewport interaction; authoring.task_graph owns editable semantic meaning;
  the shared editor scheduler remains read-only and must report dependency edges
  and failures exactly.
- Preserve the engine-wide one-dot C++ naming contract and run the naming
  validator with every source move; do not reintroduce generic bridge or flat
  root ownership.
- Complete the remaining temporal texture UX over the existing
  `asset.texture_artifact` source/Library/material pipeline: live drag preview,
  brush hardness/opacity/channels, rename/masks/transforms/filters, dependency
  diagnostics, settings/cost visibility, approved interaction proof, and
  additional formats admitted through capability evidence.
- Preserve the production project contract: every generated profile must
  materialize, atomically save/reopen, build, and child-run. Project Save, Build,
  and external Run stay in `project.lifecycle`; selected-script compilation is a
  separate C++23 lane with source/output revalidation, verified candidate,
  atomic publication, and published-artifact verification. External Run now has
  generation-safe child ownership, duplicate focus, PID/elapsed/exit evidence,
  and graceful/forced Stop controls. Open/Switch/Close Project now admit and
  release explicit project sessions without deleting source; Plant Lab remains
  a launcher-owned application rather than a generated project. Finish
  cooperative project/script build
  cancellation, editor Play/Stop convergence, and operator-visible interaction
  proof without weakening the lifecycle gate.
- Make Systems useful without making the editor slower: collect timing only
  while its central workspace is visible, project immutable revisioned snapshots
  through EpochGui, and keep the editable learning graph isolated from the live
  scheduler. The read-only Live Scheduler now reports shared editor TaskGraph
  work while the separate Learning Graph remains non-executing.
- Let AI propose engine/project work without owning authority: exact immutable
  proposals, review, explicit operator approval, single-use permits, bounded
  execution, and verified evidence are mandatory before any registered executor
  may mutate or run.


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
  Generation-checked map objects now have canvas selection, hierarchy rows,
  staged name/type/position/size/rotation controls, and single-operation
  apply/duplicate/delete behavior. Direct dragging previews rotated-bounds
  clamped motion and commits once on release. Handoff, source reload, exact
  Library restoration, and compiled preview verify the same object output.
  Layers now use generation-checked selection and staged name/visibility/lock/
  collision/phase/order/opacity/parallax controls with semantic create,
  duplicate, apply, and guarded delete. Locked paint rejection and exact
  handoff/source/Library/runtime layer preservation are contract-proven.
  Live layer interaction and full project restart proof remain pending.
  Context replacement restores serialized unsaved map history and portable view
  state without preserving physical resources.
- Project Save and the current editor Run publication consume this document.
  `project.lifecycle` now defines a strict path binding selected project,
  committed scene, materialized shell, build inputs, active build generation,
  verified artifact, and runtime generation. Its contracts and production
  editor integration reject stale or cross-project completion; Save,
  materialize, Build, and external Run populate the same ledger, and accepted
  executable bytes are SHA-256 checked before launch. The Project workspace
  exposes its decision and generations. Standalone runtime
  preparation regenerates canonical source or restores exact Library artifacts
  and authenticated texture closure. Project input uses its own canonical
  source/artifact pair with stable actions, keyboard/controller bindings,
  fixed-point dead zones, and deterministic sampling.
- `physics.solver2d` and `project.actor2d_runtime` now provide deterministic
  fixed-step AABB/circle collision, stable contacts, solid/one-way/slope map
  collision, spawn, pause, reset, snapshots, bounded catch-up, and
  renderer-neutral Canvas2D publication. Generation-checked palette selection
  stages exact shape/bounds/filter intent as one semantic operation and preserves
  it through source, handoff, Library artifacts, and runtime preview.
- `project.gameplay2d_runtime` now composes authenticated map/texture closure,
  project input, fixed-step actor physics, sprite animation, audio events, GUI,
  and Canvas2D output. Fresh generated `twodstudio` acceptance proves all 63
  artifact bits, deterministic frame/step/sample/event counts, a stable canvas
  hash, and teardown on Windows. Live editor `ProjectPlayScene` now hosts this
  composition while retaining host-owned physical input, GUI impulses, camera,
  context publication, and protected GUI/present order. MSVC Debug/Release and
  managed Clang Release build the path; contracts cover repeated sessions and
  active-destination move-assignment teardown. Interactive Play/Stop, physical
  audio, and native pixels remain unclaimed.
- Visible project keyboard rebinding now owns stable binding selection,
  duplicate-source refusal, revision advancement, exact source/artifact
  publication, save/reopen proof, and the existing live runtime sampler. A
  process-owned SDL3 provider publishes generation-checked controller snapshots
  once per engine frame. A renderer-neutral per-scene adapter now maps authored
  button/axis sources into project actions, consumes press edges once, and
  preserves held/axis continuity. Project Controls persists controller source,
  slot, and dead-zone edits through the canonical source/artifact path. Eye-test
  physical-device behavior and reopen, then retain the editor camera profile as
  a separate input domain.
- Add sensor/trigger dispatch and richer collision diagnostics only after the
  actor event contract owns them; sensors remain rejected by the play runtime.
- Retain the proven fresh generated-child materialize/save-reopen/Build
  acceptance path, then prove live actor Play/Stop, restart persistence,
  interactive external Run, cache-deletion regeneration, native presentation,
  and complete renderer/physics/audio teardown through approved runtime evidence.
  Build-safe tests are not visual proof.
- Keep the process-owned physical audio adapter behind `audio.manager` and
  `audio.mixer`. The canonical Project Audio profile owns content-addressed WAV
  import, buses, cue semantics, volume/mute, loop/autoplay, jump/land bindings,
  and one immutable decoded PCM artifact under `Library/Audio`. Valid source is
  authoritative and refreshes that artifact; game-only runtime preparation may
  restore the verified artifact without authoring source. Never fall back to an
  older artifact when present source is malformed. Eye-test the controls, then
  prove ambient/music and event cues through the physical device across
  repeated Play/Stop/restart.
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
- The operator observed OpenGL near 60 FPS while the other active backends
  reported near 120 FPS on 2026-08-25. Matched Windows Debug reruns on
  2026-08-26, first with an explicit 120 FPS override and then with the default
  editor policy, report swap interval 0, sustain 120-121 measured frames per
  second with approximately 0.02 ms `SwapBuffers`, display 120 FPS in the
  native title, retain the current launcher/editor GUI, and close cleanly. This
  closes the current OpenGL frame-pacing/telemetry discrepancy without claiming
  broader native pixel, resize, or repeated-switch coverage.
- Floating GUI panes are individual routed panels, not context selectors or full
  editor clones.
- World Outliner, Asset Browser, GUI Hierarchy, Script Browser, Tile Map,
  Properties, World Settings, Output, and AI Chat own exact pane routes. Project,
  Assets, AI Output, and Systems are filters within Output, not duplicate tool
  windows; the selected filter persists with context snapshots. Each real tool
  can move among the left, right, and bottom stacks or float in a native
  context-backed host; main document and scene tabs never accept tool panes.
  Floating content uses native chrome and guide/ghost feedback rather than Dock
  Back or Close Window command buttons, and route restoration never clones
  editor state.
- Complete desktop docking with arbitrary compatible stack creation, tab
  reordering, persisted layouts, and operator-approved multi-monitor interaction
  proof. Keep native floating hosts and desktop docking optional so games,
  mobile, console, and headless products can compile them out.
- Operator proof accepts SDL3, SFML3, DirectX, and Software scene-solid
  orientation. Raylib and Vulkan remain `Partial` until their current correction
  candidate receives eye proof.
- Authorized 2026-08-26 Windows SDL evidence at 150% display scale accepts the
  launcher, replay-backed loading transition, current editor GUI, Systems
  workspace, resize, World Script Browser, large source editor, wheel input,
  120 FPS title, and clean close. SDL owns separate logical and physical
  dimensions and normalizes both sampled and queued mouse coordinates through
  one mapping. This closes the SDL GUI/loading/high-DPI interaction gate, not
  sampled-RTT pixels, Canvas2D reference agreement, repeated context switching,
  or native memory soak. Preserve the protected queue-drain, GUI-replay,
  top-layer, scene, and present order.
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

- World Outliner, Asset Browser, GUI Hierarchy, Script Browser, and Tile Map are
  independent structure-tool routes rather than nested Outliner modes or
  top-level document destinations. Preserve exact selection/project ownership
  while completing their route-specific floating content and persisted layout.
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
- Its `SystemWorkspace` controller owns bounded transactional row replacement,
  filter/sort/category/status projection, hierarchy, selection, keyboard
  navigation, summaries, and explicit empty/error/stale states. It knows nothing
  about the engine registry, live TaskGraph, or authoring graph.
- `editor.systems_workspace` adapts engine diagnostics, the real shared editor
  scheduler, and the isolated learning document into that portable controller.
  AI evidence builds, script builds, project builds, and the approved tool
  harness use one TaskGraph shown as read-only Live Scheduler evidence. The
  separately labeled Learning Graph remains editable but has no live execution
  authority. Sampling lifetime, drawing, and evidence labels remain engine
  responsibilities. The bounded Time view and real scheduler queue/run timing
  are implemented; approved GUI responsiveness proof remains.
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
- Themes include System Light/Dark, Light, Dark, Classic Launcher, Midnight
  Blue, Ember Forge, Forest Terminal, and Aurora Steel. Theme names, palette
  contrast, modal/dropdown hover states, and text readability are EpochGui-owned
  contracts rather than editor-local decoration.
- Settings use progressive disclosure and match active capability evidence.
  Rounded GUI controls are the default EpochGui-owned style policy, may be
  disabled explicitly, and persist across editor context handoff.
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
  Canonical project source is `Assets/Gui/main.epochgui`, persisted through the
  bounded integrity-checked GUI codec. The next durable slice compiles that
  source into an editor-free runtime artifact and executes focus, actions, text
  input, images, and tab state without carrying authoring history, editor scene
  vectors, docking, floating hosts, thumbnails, or renderer cache identity.
- Project Assets and texture authoring continue as one logical resource spine.
  Restart-safe source/Library catalogs, a virtualized reusable asset grid,
  stable activation, canonical source matching, and eagerly hydrated bounded
  artifact-keyed thumbnail batches are current. Canonical
  dependency inspection, deeper folder models, broad import formats, texture
  graph evaluation, mip/compression/color conversion, and native
  sparse/bindless policy remain measured follow-up work.

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
  policy, mix plans, and metrics. `audio.mixer` owns bounded decoded PCM and
  deterministic frames; `audio.device` owns the optional physical sink;
  `audio.playback_runtime` owns one process-level device and generation-checked
  project sessions so renderer replacement cannot duplicate audio ownership.
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
  frameworks. `authoring.task_graph` is currently a narrow temporal learning
  prototype with stable handles, semantic operations, undo/redo, validation,
  topology, critical path, and parallel-wave simulation. It must inform the
  shared graph design rather than become a competing universal framework.
- The live `taskgraph.dotsystem` snapshot is scheduler evidence only. Editing the
  learning document never creates, removes, schedules, or executes live tasks.
- Graph execution compiles through validated IR into CPU, SIMD, GPU, or software
  plans. Evaluation caches are derived and shared by identical revisions.
- Texture is the first domain because it directly unlocks the 2D product.
- `authoring.morphology` is the first generalized branching-domain slice: stable
  node/segment/terminal IDs, deterministic domain recipes, per-organ temporal
  sampling, and voxel LOD plans. `ForestAssetDocument` adds stable genome
  identity, bounded semantic profile edits, undo/redo, revision/content hashing,
  deterministic compilation into morphology, preview, voxel LOD, and bounded
  occupancy, plus an integrity-checked project source and content-addressed
  immutable Library artifact. Plant Lab publishes and reopens that pair; Forest
  Factory places the same revision in the active scene, and package staging emits
  matching revision/hash/LOD evidence. The common editor/runtime projection owns
  Euler rotation and midpoint/length segments, so every active renderer consumes
  the same oriented trunk/branch/leaf solids and selected outline without a
  backend copy. Typed node editing, sparse voxel materialization,
  mesh/impostor compilation, imported compiled output, and approved native
  pixel/switch/resize/teardown evidence remain subsequent slices.
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

- `local_ai_llama_cpp_runtime` is a pinned executable-local package installed
  only by the tracked, operator-invoked Qwen3.8 installer. It verifies the
  immutable llama.cpp artifact and separately licensed community GGUF before
  writing readiness receipts, starts no server, and keeps weights out of source
  and generated builds. Generated projects select Off, shared Epoch-local
  Qwen3.8, or an operator-managed external model endpoint with Epoch MCP guards
  through a disabled-by-default project profile.
- The operator owns
  `Autodidac/Temporal_Parametric_Graph_Lindenmayer_System_Plant_Lab` and has
  explicitly authorized its Epoch integration. Source revision `40a3db7` is
  the recorded provenance point; there is no third-party license blocker.
- `authoring.morphology` generalizes the Plant Lab foundation across plant,
  vascular, respiratory, electrical, coral, and generic branching domains with
  stable IDs, per-organ time ranges, deterministic sampling, and voxel LOD
  planning. Plant Lab owns a temporal forest document and its compiled morphology,
  preview, LOD, and occupancy outputs; Forest Factory is the standard-editor
  placement portal and already consumes the live compiled revision. Portable
  project-published tree and forest artifacts remain the restart-safe handoff.
- Mainline owns stable contracts, validation, temporal/document identity, safe
  fallback, provenance policy, and project/runtime integration.
- EpochEngineExtensions owns heavy optional generators, FFT ocean, planetary or
  game-specific world stacks, immutable payload source, manifests, licenses,
  hashes, tests, and generated artifacts.
- If hosted repository access cannot be restored, recover the Bootstrap Loader
  and EpochEngineExtensions into a temporary, source-controlled `Recovery/`
  workspace inside EpochEngine. Keep both recovery projects excluded from the
  default engine build, runtime, packages, and release payload. Inventory local
  source, archives, manifests, checksums, notices, and revision evidence before
  writing canonical files; never promote cache output or an unverifiable binary
  as source. Acceptance requires clean independent builds, retained provenance
  and licenses, reproducible checksum-bearing packages, and a documented split
  into replacement remotes before `Recovery/` may be removed.
- Extension activation fails closed. Current package preflight evaluates
  verifier-produced evidence; it is not itself a downloader/verifier.
- Built-in mini-runtimes remain kernel-owned and can be exposed as package/script
  assets.
- Anything that can listen, host, bind a port, execute downloaded native code,
  or expose a control surface requires explicit human approval.

## OS AI

- Epoch does not train or silently activate an LLM. It runs either an
  operator-selected external/source-available model through an
  OpenAI-compatible endpoint that may live on another machine, or the verified
  Epoch-local `llama-cli` plus separately licensed GGUF.
- The Epoch-local Qwen3.8 provider pins llama.cpp `b10516` and a community
  Qwen3.8 27B Q4_K_M GGUF with exact revision, size, hashes, and executable-local
  receipts. It is a peer to, not a replacement for, the external provider.
- Generated project profiles default to Off and may explicitly select the
  shared Epoch-local provider or external model compute with Epoch MCP guards;
  builds copy the profile but never weights.
- Model discovery is inventory only. Selection and runtime state are explicit
  and executable-local.
- `ai.mcp` is model-provider-independent and owns bounded tool descriptors,
  calls, results, errors, capabilities, cancellation, evidence, and session
  budgets. Epoch starts no MCP server/listener.
- `ai.development_guard` owns immutable SHA-256 proposals, normalized
  allowlists, typed operations/risks/content transitions, separate review and
  operator approval, bounded lifetimes, cancellation, evidence, and audit state.
  Its private `ExecutionPermit` is issued only by the guard and claimed once
  before work; callers cannot forge it from a digest.
- `editor.ai_development_controller` uses trusted monotonic production time,
  serializes execution entry, and accepts source completion only from its
  registered transaction executor. It reads exact preimages from the live source
  root, materializes them beneath a unique writable
  `cache/ai/iterations/session_*` root, and never points model execution at live
  source. Externally driven time is contract-only and rejects backward movement.
- `ai.development_executor` applies exact engine/project source transitions only
  inside the controller-selected iteration root. It verifies canonical paths and
  content preimages/postimages, writes and flushes exclusive same-directory
  temporaries, revalidates before commit, verifies publication, and emits
  rollback evidence. Its contract proves the live file remains byte-identical
  while the iteration sandbox receives the approved postimage.
- This source executor is not an OS filesystem database. Hostile external-writer
  exclusion, directory crash journaling/durability, and complete
  ACL/xattr/alternate-stream/ownership preservation remain future hardening.
- `ai.iteration_loop` preserves the build-safe policy/state-machine contract
  for a bounded model-development campaign: architecture inspection, stated
  invariants, guarded implementation, typed validation, bounded repair, and
  risk-selected review. Engine Development now connects a contained live slice:
  Qwen3.8-class related-files admission, source-specific 64K context/32K output
  budgets, raw prompt bytes, exact-copy buildable workspaces, generation-owned
  TaskGraph materialization, strict proposal/digest approval, sandbox-first
  transactions, hidden direct MSBuild Debug and Release compiler passes, and
  separate build-safe engine-contract children, followed by a separately
  scheduled HeadlessCI Debug build and asset-light run. A further visible
  operator approval runs the Release editor's full engine validation across
  registered project profiles, generated-child self-tests, and the AI gate.
  Failure in any actor may
  start at most three fresh-generation repair proposals with bounded
  diagnostics; every changed proposal needs a new digest and exact operator
  approval. Only after all seven Debug, Release, HeadlessCI, and full-validation
  completions pass for the current generation may a distinct
  `Stage Live Promotion` action verify exact live preimages and sandbox
  postimages, reparses the retained proposal through a new live-root controller,
  requires identical operations, and presents a new digest without writing.
  `Approve Live Promotion` rechecks that evidence and uses a fresh single-use
  permit to atomically apply only the reviewed existing-file source changes.
  Staleness, tampering, path changes, commit failure, cancellation, and replay
  fail closed; success consumes the candidate and creates no autonomous follow-
  on, shell, Git, release, updater, package, network, or approval authority.
  Analyzer, sanitizer, architecture, visual, and frontier adapters remain
  blocked rather than becoming model self-attestation.
- The strict `EPOCH_TOOL_PLAN_V1` lane gives AI Chat `/tool` one bounded
  non-source active-project proposal. The only argument-free calls are
  `project.inspect`, `project.save`, `project.build`, `project.run`,
  `project.test`, and `diagnostics.read`; paths, native commands, permissions,
  and multiple calls are invalid protocol. The exact parsed call stays attached
  to visible approval, is revalidated by the host-owned MCP registry, and then
  reaches only existing project lifecycle owners. Approved test can request the
  canonical prerequisite build before running a cancellable hidden
  `--project-self-test` from accepted artifact evidence; stale-project
  completion is discarded. Run remains a distinct visible approval. The model
  receives no permit, shell, Git, network, release, updater, or self-approval
  authority. Debug/Release editor builds and build-safe contracts prove the
  source path, and the HeadlessCI Debug build/no-graphics run proves its distinct
  actor; model/GUI use and approved child execution remain operator evidence.
- The project tool registry covers inspect, create, save, document/script edit,
  build, run, test, capture, and diagnostics. Ordinary AI Authoring accepts one
  reviewed `scene.clear`, `scene.create`, exact-count `scene.reconcile`, stable-ID
  `scene.transform`, or `gui.create` command per approval through fixed allowlists
  and existing semantic gateways. Reconcile owns counts; transform owns exact
  finite transforms and support placement; GUI commands switch visibly to GUI
  Canvas and never become 3D scene markers. `/plan` owns one proposal. `/goal`
  owns a persistent Play/Pause/Edit/Delete task whose next milestone queues only
  after an approved canonical revision. Repeated or no-op semantics pause. The
  source proposal lane is host-curated and context-first. Exact canonical paths in
  the objective outrank vocabulary ranking; the first primary candidate may use
  the 184 KiB evidence ceiling while related candidates remain under the 128 KiB
  aggregate budget. Review Paths reads and sends nothing. Share Curated Context
  revalidates the objective, selected endpoint, canonical source root, and exact
  candidates, materializes an exact-copy disposable workspace, and opens the
  reviewed files in the large source editor. Complete content is supplied only
  through 48 KiB; larger files use one UTF-8-safe 16 KiB objective-centered
  excerpt.
  The production prompt accepts only
  `EPOCH_SOURCE_PATCH_PROPOSAL_V1` exact-block operations or exactly
  `EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1`. Framed and raw direct-CLI replies
  pass through the same line-boundary extractor; prose, absent or ambiguous
  search blocks, no-op replacements, whole-file regeneration from excerpts,
  generic logger/singleton/entry rewrites, placeholders, stubs, duplicate
  wrappers, unrelated cleanup, invented architecture, textual `.ixx` includes,
  destructive shrinkage, removed ownership metadata, and build/test claims fail
  before staging. The trusted controller reconstructs full postimages from exact
  live preimages and stages only immutable hashes and complete postimages.
  Direct-CLI normalization prefers a complete structured packet and extracts it
  only from an exact line-framed header through its matching terminator;
  transcript prefix/suffix bytes remain outside the strict codec. A completed
  generation is consumed exactly once, and pre-staging failures receive at most
  two automatic host-diagnosed retries over the same evidence. The packet format
  example is grounded in the first reviewed path and, for an explicit unique
  quoted replacement, the exact reviewed search/replacement bytes. This
  scaffolds the local model without weakening the decoder or approval boundary;
  ambiguous text still fails closed. The live Qwen3.8 acceptance run reached a
  staged sandbox proposal while live source remained read-only.

  Direct local source inference uses below-normal process priority, half the
  available logical CPUs for both generation and batch work, and CPU-only model
  layers so the editor GPU remains responsive; ordinary AI chat keeps its
  configured GPU path. AI Controls owns objective/evidence inspection;
  response-specific Review/Approve/Cancel remains in AI Chat and a changed
  objective invalidates earlier evidence. Save/Build/Run/Test iteration adapters
  for the source-development campaign remain unfinished. Model-facing tools must
  not expose approval, permit issuance, unrestricted native invocation, commit,
  push, release, updater mutation, listener/server start, or downloaded native
  code.
- Chat is not captured automatically. Explicit harness/trace work may write
  `model_exchange.jsonl`, `tool_trace.jsonl`, and reviewed eval fixtures as
  evidence, never hidden training, automatic source application, or authority.
- Retained review material lives under `Engine/ai/evals/fixtures/`; Epoch has no
  dataset-promotion module, runtime training path, weight mutation, or access to
  unrelated personal AI development.
- Local API and direct CLI work publish through generation-checked shared state
  with bounded transport timeout. Pause and goal edit/delete invalidate the active
  generation without synchronously destroying WinHTTP from a GUI callback;
  shutdown owns bounded hard cancellation. The sandbox compiler future has one
  completion owner, so Bottom Dock diagnostics cannot race it or auto-stage an
  obsolete packet. Goals are capped at 24 milestones, exactly one semantic
  command, and eight bounded object consequences per milestone.
- `ai.voice_session` owns provider-neutral per-session microphone consent,
  bounded listening/transcription/review, conversation response state, separate
  TTS admission, interruption, and cancellation. It stores no raw audio and
  transcript review cannot be disabled.
- Implement cross-platform capture plus operator-selected local STT/TTS provider
  adapters before enabling Dictate or Conversation. Playback output alone is
  not voice support. No adapter may start a server, bind a port, retain raw
  audio, or listen outside a visible approved session.

## Build, Source, And Release Boundaries

- Require release-candidate builds and tests on admitted local Windows MSVC and
  managed Linux Clang systems. Each runner reports one exact committed tree,
  platform, repository-owned command set, result, log digest, contract/test
  evidence, and artifact hashes to the Epoch Site over authenticated HTTPS.
  The Site does not compile Epoch; it stores and exposes immutable accepted
  reports through its Actions-shaped `runs`/`jobs` API and owner CMS, and binds
  the exact commit, required platform matrix, evidence digest, and admission
  state (`pending`, `passed`, or `failed`) into the signed version record as
  another versioning field. Keep the legacy plain source-version sentinel for
  shipped-client compatibility, but newer updaters must require `passed`
  admission matching the candidate version and commit. Source/runtime discovery
  remains on the prior accepted revision until every required local-platform
  report is complete and successful; missing, stale, mismatched, or failed
  evidence blocks activation. The reporting path must remain automatable
  without public EpochEngine Git/source, a long-lived inbound listener, or a
  static updater credential.
- Development/private source discovery is `v0.89.30`; packaged runtime/latest
  is now the signed `v0.89.30` authority from Site v43. Public update clients
  receive runtime archives, build evidence,
  and checksums only; approved EpochEngine source access is server-authorized
  and never depends on a shipped static secret. Preserve `v0.89.29`, `v0.89.28`,
  `v0.89.27`, and immutable `v0.89.06` as release history, and keep
  `multicontext-base-stable` fixed at
  `ad6c416d930b348a61bc37ceb7d4522742be084a` inside restricted development
  history. Standalone EpochGui remains public and must exactly match the bundled
  `Engine/dep/EpochGui` tree. Every later release mutation requires a fresh
  bounded validation, package, checksum, signed-admission, and Site publication
  pass; retain interim test releases until explicit operator cleanup.
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
