# Epoch Roadmap

## Mission

Ship a playable baseline 2D project within the remaining two-month window while
building the smallest coherent foundation that can later scale to mobile,
explicit GPUs, ray queries, temporal worlds, and general authoring.

The canonical architecture is
`Engine/docs/engine/capability_tier_architecture.md`. This roadmap schedules that
architecture. It does not create a second design. Historical comparison starts
at `Changes/roadmap_baseline_2026-08-10.md`; the current evidence review is
`Changes/progress_review_2026-08-16.md`.

## Planning Rules

- The playable 2D loop is the critical path.
- Capability is selected per subsystem, never inferred from API name.
- `T0-CPU` is the universal correctness/headless floor.
- `T1-GL` is the first desktop presentation target; contracts remain suitable
  for later `T1-GLES`.
- A feature and its settings, controls, diagnostics, persistence, and tests move
  together.
- Source documents and semantic history are authoritative. Compiled artifacts
  are reproducible. Physical caches are disposable.
- First-party C++ ownership follows `source_naming_architecture.md`; source moves
  preserve one-dot module/file identity and update every build surface together.
- Project Save, materialize, Build, and Run decisions flow through
  `project.lifecycle` rather than widget-local state machines.
- OpenGL proves portable techniques first without becoming engine architecture.
- Baseline 2D presentation parity across OpenGL, SDL3, SFML3, Raylib3, Vulkan,
  DirectX/D3D11, and Software is a product gate. Advanced backend features,
  mobile specialization, and effects cannot consume the playable-2D schedule.
- Hosted CI confirms faithful local proof; it is not the first place to discover
  ordinary compiler or contract failures.
- `v0.89.33` is the current development source authority and the fully
  published binary-first Windows/Linux packaged runtime on Site v55. macOS
  packaged authority remains `v0.89.30`. Preserve `v0.89.30`, `v0.89.29`,
  `v0.89.28`, `v0.89.27`, and immutable `v0.89.06` as release history, and preserve
  `multicontext-base-stable` at exact commit
  `ad6c416d930b348a61bc37ceb7d4522742be084a`. Future release changes require a
  new bounded build, contract, package, checksum, and publication pass.

## Current Foundation

Source v0.89.33 contains these completed or intentionally partial contracts:

- rollbackable August 28 checkpoints now cover canonical generated-game input
  and Project Defaults editing (`38d541b2` through `de937045`), local build
  admission receipts (`148fffa0`, `eadfea94`), durable AI queue/scheduler/
  session restore/supervision/curated context/proposal/staging (`445bc3f0`,
  `9ea6deaa`, `7711648d`, `c101507d`, `c8e1fdb8`, `2daec382`,
  `a39ea309`), and their operational editor review surfaces (`94c7357c`,
  `60ce0032`). World Outliner hierarchy, responsive tabs, and project source
  workbench are source-complete at `6deaf036`, `4cedf62b`, and `73c86889`;
  no GUI eye-test is inferred from build-safe proof;
- renderer pacing uses the centralized 120-Hz-capable policy from `1417a8e4`.
  SDL capture/loading/first-present is committed at `c937fa2f`; minimize/restore
  remains a separate pending tranche;
- the MCP supervisor adapter is implemented but deliberately unregistered,
  unbuilt, and unpublished pending approval. The source-iteration worker is
  still in audit/in-progress, is blocked on missing prerequisites, and has no
  checkpoint; the `disposable_sandbox` design remains pending;
- ParticleEngine PR-009 is a local-only, package-gated dependency candidate at
  `98d10d41c3e2d534e7023014a79333ba121b362b`; it is not an EpochEngine
  mainline or public-release claim.

- a stable renderer-neutral camera/view contract now carries logical view,
  scene, purpose, projection, orientation, clip, and revision state through
  renderer passes. The editor owns perspective, free orthographic, six locked
  axis views, independent framing, professional navigation chords, Focus, and
  Reset; splitters and GUI layers capture input before navigation. Oriented
  axis grids, authored project-camera persistence, and live all-context proof
  remain delivery;
- the Windows parent host resolves one centered client geometry from the active
  monitor work area and capability tier before launcher/editor composition.
  4K, 2K, 1080p, and compact displays receive distinct defaults; mobile/deck
  tiers scale those defaults proportionally, explicit dimensions override policy, and switching
  applications no longer changes the native host geometry;
- Standard Editor, Plant Lab, and GUI Editor now validate distinct document,
  structure, inspector, and operation-dock layouts over the shared shell. Main
  document and scene tabs remain fixed while every non-scene tool owns an exact
  routed tab that can join a compatible left, right, or bottom stack or float
  in a native context-backed host. Standard Editor provides the interactive
  project-GUI placement canvas while projectless GUI Editor owns creation,
  Preview, Component Graph, Styles, and template authoring over the same
  canonical document. Persistent layout serialization, arbitrary compatible
  stack creation, and complete app-specific tool content remain delivery;
- portal/RTT planning now owns stable portal identity, shared view descriptors,
  recursive cycle/depth/pixel budgets, clip-plane intent, logical outputs,
  disposable cache state, metrics, and render-pass bindings. Native
  stencil/oblique clipping, traversal, and live portal presentation are not
  claimed;
- the local extension catalog records descriptor-only provenance, capability,
  platform, tier, integrity, activation, and restart requirements for the
  known optional systems without claiming unavailable payloads were installed;
- a typed temporal GUI document owns stable widget identity, validated hierarchy,
  typed image/image-button/tab/input/slider/scroll meaning, semantic content,
  layout, style and interaction operations, deterministic revisions, bounded
  history, snapshots, and metrics. Its integrity-checked bounded codec restores
  and atomically publishes canonical `<project>/Assets/Gui/main.epochgui` source;
- projectless GUI Editor opens real projects without changing application role,
  creates five built-in document templates, and reads/writes standalone
  `.epochgui` templates through the same bounded integrity codec and atomic
  publication path as project GUI source. It edits widget content, appearance,
  layout, visibility, enablement, focus, pointer policy, actions, tooltips, and
  real tab pages. Project Save, Build, and Run require verified GUI and scene
  source evidence. Save compiles and publishes immutable runtime artifacts;
  ProjectPlayScene restores them through the EpochGui adapter. Exact canonical
  project-input actions execute as one-frame deterministic impulses while
  unknown strings fail closed and focused controls suppress physical gameplay
  sampling. Arbitrary script/application command dispatch and compiled-only
  game-build proof remain delivery;
- reusable EpochGui image-box, image-button, tab-button, asset-grid,
  system-workspace, and node-graph controllers own generic layout and
  interaction. Asset tiles are bounded and virtualized; graph view state,
  movement, selection, connection, and disconnection remain portable intents.
  Asset-grid context menus own bounded per-context/window state and stable
  target/action identity;
  The editable
  Learning Graph supports stable drag/connect/disconnect/select/add/remove,
  semantic undo/redo, validation, topology, critical path, and parallel waves,
  while the Live Scheduler remains read-only evidence over real editor work;
- Project Assets is now a first-class document surface with a bounded
  project-rooted browser, compact Outliner navigator, folders, search,
  type filters, GUI source classification, create/open/import/refresh actions,
  restart catalog hydration from Assets and Library, revision-safe texture
  reimport, exact compiled image-button thumbnails, and semantic scene material
  assignment. Browser thumbnails resolve source identity canonically and hydrate
  one bounded artifact-keyed atlas when a texture catalog is presented; right-click
  Open, Show Details, and Copy Path actions resolve through the
  real project controllers. Scripts use the central full-width source editor
  while the Outliner remains a navigator. The texture workspace owns sparse
  layers, drag strokes, blend,
  opacity, sampling, independent U/V addressing, alpha cutoff, undo/redo,
  atomic source publication, and deterministic Library regeneration. Broader
  formats, canonical dependency inspection, and deep folder traversal remain
  delivery work;
- backend-neutral capability profiles, evidence, project requirements, and
  deterministic per-subsystem selection;
- renderer-neutral math, bounded light frames/reference lighting, and CPU ray
  and voxel queries;
- logical physics and audio managers with fixed scheduling/snapshot contracts,
  deterministic PCM mixing, a renderer-independent physical-device boundary,
  process-owned project playback sessions, and a digest-verified decoded Project
  Audio Library artifact that supports source-authoritative rebuilds and
  compiled-only runtime restore;
- sparse voxel storage, analytic water queries, Tier-0 scene/terrain descriptors,
  and fail-closed package evidence policy;
- sampled render-to-texture, render-device/resource descriptors, render graph,
  Engine Arcade graph proof, and a renderer-neutral Canvas2D project/submission
  planner with camera, viewport, sprite, batch, tile, compose, and diagnostics,
  plus a deterministic CPU reference raster with texture/clip bindings,
  fixed-point coverage, final composition, metrics, and image hashes, plus a
  bounded generation-checked physical texture cache, a renderer-neutral
  CPU-canvas-output/native-presentation adapter, context-guarded OpenGL native
  texture hooks, a compiled active-editor OpenGL compositor with scoped
  scene-surface and GL-state ownership, one shared scene-raster session, and
  build-proven adapters for SDL3, SFML3, Raylib3, Vulkan, D3D11, and Software;
- project-scoped texture import, deterministic artifact/Library persistence,
  decoded publication, restore-on-demand, semantic scene materials, snapshot
  format 3, and one World Outliner Assets controller over the same logical
  resource spine;
- canonical temporal tilemap documents with stable IDs, sparse chunks, semantic
  history, deterministic runtime artifacts, collision/object payloads, exact
  Project Library persistence, restore-on-demand, and visible Canvas2D
  compilation;
- a reusable EpochGui tile workspace plus the Game2D engine controller now own
  palette/layer/grid state, texture attachment, map operations, diagnostics,
  canonical Assets/Maps source, exact Library publication, revision-cached
  preview, and context-handoff restoration. Map objects and layers now use
  generation-checked editor selection with staged semantic inspectors and
  create/duplicate/delete behavior. Layer visibility, locking, collision intent,
  phase, draw order, opacity, and parallax survive source/Library/runtime
  boundaries. Fresh generated-child acceptance now proves standalone map
  loading; explicit cache removal/regeneration and approved layer interaction
  remain delivery work;
- project-owned input source/artifact codecs now define stable actions,
  keyboard/controller bindings, fixed-point dead zones, deterministic sampling,
  and source-first restoration. A renderer-neutral adapter consumes the
  process-owned generation-checked controller snapshot once per project frame,
  prevents repeated snapshot revisions from replaying press edges, and preserves
  held/axis continuity. Project Controls publishes keyboard, controller
  button/axis/slot, and uniform dead-zone edits through that same source/artifact
  path. A fixed-step AABB/circle solver over
  `physics.manager` and a renderer-neutral actor runtime now provide map
  collision, spawn, pause, reset, snapshots, bounded catch-up, stable contacts,
  and Canvas2D publication;
- project-owned sprite-animation source/artifact codecs now provide stable
  animation/frame/event identity, deterministic sampling, source-first
  publication, compiled-only restore, and default actor locomotion selection;
- `project.gameplay2d_runtime` now composes authenticated map and texture
  closure, deterministic input, fixed-step actor physics, sprite animation,
  audio events, GUI, and Canvas2D output. A fresh generated `twodstudio` child
  accepts all 63 artifact bits, 187 input frames and fixed steps, 188 animation
  samples, three audio triggers, a stable Canvas2D hash, 1 KiB of logical
  texture data, 65 emitted sprites in one batch, one collision surface with one
  peak contact, 224640 resident audio bytes, zero Canvas2D rejections, zero
  `T1-GLES/mobile_30` budget violations, and clean teardown.
  Live editor `ProjectPlayScene` now hosts the same runtime from scene load,
  keeps input sampling and context publication host-owned, and closes gameplay
  before retiring per-context scenes. MSVC Debug/Release and managed Clang
  Release build this path; interactive Play/Stop and native pixels remain open;
- production project validation that materializes, atomically saves/reopens,
  builds, and child-runs every registered generated profile, plus an exact
  standalone external Run proof for the generated GUI project;
- explicit request-driven temporal mapping and bounded observation history plus
  a temporal texture document/history/compiler/residency foundation;
- reusable EpochGui text, primal multi-line document editing, font, image, input,
  layout, rounded rectangle, toggle, popup, panel, docking, and floating-window
  primitives, synchronized between the embedded and standalone repositories;
- direct launcher entry into three shared-spine editor applications: standard
  Editor, Plant Lab, and GUI Editor, each with separate source, canonical scene,
  surface/camera/dock policy, and authoring/run permissions;
- a shared temporal morphology spine used by the separate Plant Lab authoring
  editor for custom trees and forest configurations. A stable forest document
  now journals bounded semantic profile edits and compiles deterministic growth,
  renderer-neutral preview geometry, voxel LOD plans, and occupancy from one
  revision. Integrity-checked project source serialization, content-addressed
  immutable Library publication, exact/latest restart reopen, and project-bound
  Publish/Reopen controls now share that identity; Forest Factory remains the
  placement portal. The common editor/runtime projection maps compiled segments
  to oriented midpoint/length solids and deterministic foliage transforms before
  the shared geometry stream fans out to OpenGL, SDL3, SFML3, Raylib3, Vulkan,
  DirectX/D3D11, and Software. Import, sparse voxel materialization,
  mesh/impostor output, and approved live all-context
  pixels/switch/resize/teardown remain delivery;
- one logical active-editor authority in the Windows parent host, independent of
  physical context creation order or grid side; every physical context may
  undock/redock, and compatible routed pane popouts remain optional;
- a validated procedural Engine Arcade cabinet and camera-facing preview
  culling, plus build-proven backend-owned sampled scene surfaces across all
  seven contexts pending visual evidence;
- guarded AI development now includes immutable proposal digests, normalized
  allowlists, separate review/operator approval, bounded lifetimes, a private
  guard-issued capability, one execution claim, trusted monotonic production
  time, exact-content source execution, verified terminal evidence, and a strict
  data-only multi-file source-proposal codec. The trusted host owns canonical
  roots, preimages, hashes, risk, approval, permit, and execution authority. It
  reads exact preimages from the live source root, materializes them beneath a
  unique writable `cache/ai/iterations/session_*` root, and never points model
  execution at live source. Raw model bytes remain separate from display text;
- a contract-proven bounded iteration state machine classifies contained,
  related-file, subsystem, cross-subsystem, architecture, lifetime, concurrency,
  and Vulkan-synchronization risk. The default development surface exposes one
  explicit Approve And Run In Sandbox action after proposal staging; detailed
  evidence and guard controls use progressive disclosure. Real host compiler
  evidence gates progress, repair attempts are bounded, and architecture/frontier
  review is selected by risk. Editor inference workers are owned and joined;
  cancellation interrupts WinHTTP, curl, and direct CLI transports during pause,
  goal replacement, pane/application shutdown, and service teardown. Scene goals
  count only canonical mutations, request a distinct milestone after a first
  already-satisfied reconcile, and pause on a repeated semantic no-op. Test,
  Debug/Release contract, HeadlessCI, and operator-approved full project-profile
  and generated-child validation adapters are connected. Additional test-suite,
  analyzer, sanitizer, visual, and frontier adapters remain delivery gates and
  cannot be bypassed;
- the exact-content executor operates only within the controller-selected
  iteration root. It exclusively creates and flushes same-directory temporaries,
  verifies approved preimages/postimages, revalidates before commit, verifies
  publication, and reports rollback evidence. A contract proves the live source
  stays byte-identical while the sandbox receives the approved postimage. Git,
  release, updater, package, network, and unrestricted process authority remain
  outside it;
- one shared editor TaskGraph now runs AI evidence builds, selected script builds,
  project builds, and the approved tool harness. Systems exposes this as a
  read-only Live Scheduler with bounded revision-cached snapshots, exact
  acceptance/queue/start/finish timestamps, queue/run totals and peaks, and a
  bounded Time view whose same-size chart surfaces update in place;
- the separately labeled editable Learning Graph retains semantic edits,
  undo/redo, validation, topology, critical-path, and parallel-wave simulation
  without live-scheduler authority;
- selected-script C++23 compilation revalidates source and output ownership,
  verifies a bounded candidate, atomically publishes it, and verifies the
  published artifact. The production editor now uses the strict
  generation-stamped project lifecycle ledger for selected project, committed
  scene, materialized shell, input fingerprint, build attempt, verified output,
  and external Run. Stale or cross-project completions fail closed, and the
  Project surface exposes the current action, reason, generations, and verified
  executable identity;
- external Project Run now uses one bounded cross-platform process supervisor
  keyed by selected project, accepted artifact generation, and SHA-256. Repeated
  Run focuses the exact active artifact; cross-project overlap fails closed; PID,
  elapsed time, exit code, Focus, graceful Stop, and Force Stop are visible.
  An isolated Windows MSVC/Linux Clang module contract proves process
  ownership without pulling unrelated renderer packages. The production Linux
  full-engine lane remains Clang 22 plus the shared vcpkg manifest; GUI
  interaction remains pending operator eye evidence.

These are source and contract claims, not blanket runtime or GUI claims. The
v0.89.27 candidate still requires explicit runtime or eye evidence wherever the
capability matrix remains Partial.

## Product Definition

The acceptance project is a small 2D game with:

- one tile-based map;
- a controllable animated actor;
- camera and deterministic draw ordering;
- collision against the map and scene bodies;
- at least one sound effect and one music/ambient bus;
- save, close, reopen, and deterministic scene restoration;
- Play/Stop in the editor;
- Run and Build producing a project-owned executable/runtime;
- cache deletion followed by successful artifact regeneration;
- visible capability, performance, memory, and failure diagnostics.

A polished demo is useful, but the core acceptance is the complete project loop.

## Eight-Week Critical Path

### Week 1: Capability And Tier-0 Scene

Completed foundation:

- one typed project capability policy now reaches project profiles, generated
  manifests, active-editor and project-run admission, System Info, Settings,
  recommended budgets, and build-safe contracts without adding a tier registry.

Deliver:

- add a project profile for `T0-CPU` plus `T1-GL` and a headless test profile;
- finish ray-based editor selection and working Focus;
- make default ground, light, camera, spawn, and starter object use one saved
  project scene in editor and runtime;
- ensure Run/Build consumes saved state rather than a separate preview shell;
- expose only proven backend/profile choices in settings.

Exit gate:

- deterministic capability selection and no-overclaim tests pass;
- default scene saves, reopens, focuses, and runs;
- Debug/Release build-safe proof passes.

### Week 2: Texture Document And Residency

Foundation status: the canonical texture document, sparse tiles, layers,
semantic operations, undo/redo, checkpoints, deterministic RGBA8/mip compile,
source/Library separation, exact reopen, cache regeneration, project-scoped
identity, capability admission, physical-plan independence, editor preview,
scene-material assignment, and Assets workspace are build-proven. Interactive
painting now uses the versioned `round_path_v2` program: raw path samples remain
semantic history while deterministic pressure/tilt interpolation produces
bounded execution stamps at half-radius spacing with a quarter-pixel floor.
Legacy `round_stamp_v1` behavior remains exact. EpochGui opacity/hardness
sliders and nonempty RGBA channel toggles author the same stroke descriptor.
MSVC Debug/Release and managed Clang 22 plus all 32 CTests prove gap fill,
deterministic output, raw-intent and exact-property retention, soft/hard edge
coverage, selective channel writes, and no-mutation budget/invalid-mask refusal.
Canonical layer translation/mirroring and deterministic grayscale, invert, and
signed-brightness filters now compile non-destructively through EpochGui-authored
properties. Schema 3 adds sparse R8 spatial-mask roles and generation-checked
content bindings with strength/invert semantics; schema 1/2 are integrity-checked
and migrated only in memory. Contracts prove source pixels remain unchanged,
transformed and masked output plus undo/redo artifacts are exact, schema-3 reopen
is lossless, and malformed/integrity failures are atomic. Editable preview no
longer depends on catalog selection, and compact project thumbnails batch into
one disposable EpochGui runtime atlas per catalog identity. Native pointer feel,
compressed/color-conversion execution, and operator save/reopen/assignment proof
remain delivery.

Deliver:

- stable texture document identity, revisions, sparse tiles, layers, semantic
  operations, undo/redo, checkpoints, bounded history, and deterministic compile;
- logical texture artifacts independent of placement;
- standalone, atlas, bindless, and sparse physical plans selected by capability
  and budget;
- source/library/history/cache separation and cache recreation tests;
- texture settings for history budget, sampling intent, compilation, residency,
  and cost visibility.

Exit gate:

- equivalent document revisions compile identically;
- undo/redo and sparse storage remain bounded;
- changing physical residency does not change authoring identity.

### Week 3: Canvas2D And Sprite Batch

Foundation status: renderer-neutral project/submission planning, deterministic
batch compilation, tile validation, editor policy, and build-safe contracts are
present. Deterministic T0-CPU raster, sampling/blending, final composition,
metrics, and image hashes are build-proven on MSVC and Clang. Bounded physical
texture residency, deterministic RGBA8 artifact serialization, runtime project
asset authentication, content-derived logical revisions, project-scoped CPU
resource binding, optional residency, evidence-backed sampled-image admission,
strict/experimental policy, cross-budget limit reduction, exact immutable scene
closure, raster-to-residency presentation staging, OpenGL texture hooks, the
primary compositor, and semantic editor scene-slot routing are build-proven.
Project Library persistence, bounded BMP/TGA/P6 import, semantic scene
materials, snapshot save/reopen, runtime restoration, and Assets-workspace
controls are build-proven. Approved native capture proves exact OpenGL/T0-CPU
Canvas2D parity across 1,178,872 pixels, and the generated GUI executable
passes the editor-equivalent external Run path. One shared raster session and
native adapters for all seven baseline contexts pass MSVC Debug/Release builds
and contracts. The shared runtime now proves resize invalidation followed by
stable reuse. The presenter owns one current output lease, rolls back failed
new dispatch, and proves exact packet metadata, malformed-surface refusal, 64
same-epoch revisions bounded to two transactional slots/one live texture, 64
backend-epoch replacements, and balanced final fake-device retirement. Approved
live interaction, non-OpenGL pixel/switch evidence, native minimized/restore and
memory stability, secondary GL share groups, and sRGB/compressed/mip execution
remain work. One renderer-neutral limit mapper now converts the selected
capability profile into fail-closed compile, CPU-raster, upload, and residency
limits for all seven baseline paths. Presenter-backed lanes receive a bounded
two-slot replacement transaction, Vulkan uses the same mapped upload/canvas
ceiling instead of a desktop constant, and raster-session reuse is invalidated
when limits or policy change. MSVC Debug/Release and managed Clang 22 plus all
32 CTest contracts prove this source boundary; live native allocation and
visual stability remain evidence work.

Deliver:

- Canvas2D offscreen target and final compose graph;
- pixel-aware camera, resize, letterbox, integer-scale, and viewport policy;
- sprite material, batch, transform, UV, tint, alpha, sampler, layer, and stable
  draw-order contracts;
- nearest/linear sampling and blend/cutout modes;
- diagnostic default textures from license-verified CC0 assets or generated
  engine-owned data;
- OpenGL compatibility presentation with CPU/reference contract checks.

Exit gate:

- deterministic batch order;
- correct scaling and alpha in build-safe tests plus operator visual proof;
- cache loss recreates textures and render targets.

### Week 4: Tilemap And Scene Authoring

Foundation status: stable temporal map identity, sparse chunks, semantic
operations, deterministic artifacts, bounded serialization/culling, animated
Canvas2D submission, collision/object output, exact Project Library restore,
canonical Assets/Maps source, and the reusable EpochGui/Game2D authoring
workspace are build-proven. Save and editor publication consume the same
document; whole-editor context replacement restores unsaved history and
portable workspace state. The standalone project runtime now regenerates the
canonical source into Library/TileMaps when authoring is present and restores
the exact map plus authenticated texture closure when authoring is compiled out.
Fresh generated-child acceptance exercises that path over a starter map;
malformed source cannot silently fall back to a stale compiled artifact.

Remaining delivery:

- eye-test generation-checked object selection, direct drag with one-operation
  release, staged transform/property editing, hierarchy rows, and semantic
  apply/duplicate/delete now present in the temporal tile-map workspace;
- retain the proven fresh generated-child save/reopen and Build path while
  proving live Play/Stop, interactive external Run, explicit cache regeneration,
  authoring interaction, and cost diagnostics over one authored map;
- capture approved visual evidence without weakening headless/game compile-out.

Exit gate:

- one map can be authored without editing source files;
- authored map survives restart, runs externally, and regenerates its disposable
  compiled artifact from source.
### Week 5: Input And 2D Physics

Foundation status: project input source/artifact persistence, stable action and
binding identity, keyboard/controller schemas, dead-zone policy, deterministic
evaluation, fixed-step AABB/circle solving, body/filter/contact contracts, actor
movement, authored solid/one-way/slope map collision, spawn, reset, pause,
snapshots, and Canvas2D publication are build-safe proven. Palette collision
properties preserve exact bounds and filters through source, Library, preview,
and actor-runtime preparation. The Project workspace exposes stable keyboard,
controller button/axis/slot, and dead-zone selectors. Semantic edits and reset
use one matched source/artifact publication boundary that stages and verifies
both members, rechecks the canonical source preimage, commits authored source
before the disposable artifact, and reports source-ahead regeneration evidence.
Corrupt or conflicting derived artifacts cannot outrank valid authored source.
Fresh-store contracts prove persisted keyboard evaluation, controller-axis
dead-zone behavior, and exact default restoration without crossing into
editor-camera controls. A monotonic renderer-neutral adapter maps the
process-owned generation-checked controller snapshot into project actions,
preserves held/axis continuity, and prevents repeated revisions from replaying
press edges. The composed generated-child runtime deterministically accepts
187 sampled input frames and fixed simulation steps with clean teardown.

Remaining delivery:

- eye-test Project Controls interaction and physical-device input;
- complete approved Play/Stop and repeated runtime proof without leaking or
  duplicating bodies.

Exit gate:

- repeated input replay produces the same accepted simulation result;
- save/reopen and Play/Stop do not leak or duplicate bodies.

### Week 6: Audio, Animation, Run, And Build

Foundation status: deterministic PCM mixing, optional process-owned SDL3 output,
device/session failure contracts, sprite-animation source/artifact persistence,
compiled-only restore, deterministic idle/run/rise/fall frame selection, actor
Canvas2D publication, content-addressed decoded project WAV import, canonical
bus/cue persistence, volume/mute/loop/autoplay controls, jump/landing cue
routing, immutable decoded Library publication, corruption refusal, and
artifact-only runtime restore are build-safe proven. Project and Assets expose
one Project Audio workspace over that canonical profile and its visible
Current/Stale/Missing artifact state. The shared gameplay runtime
composes those inputs, physics, animations, cues, GUI, and Canvas2D output in
both fresh generated children and live `ProjectPlayScene`. Build-safe contracts
prove deterministic composition and a 64-session open/advance/close soak.
Selected `.ascript.cpp` sources also have a real C++23 shared-library compiler
path and single-flight asynchronous editor build status; this does not replace
the project lifecycle or prove external Run.

Remaining delivery:

- eye-test the shared `project.gameplay2d_runtime` composition through repeated
  live editor Play/Stop while retaining proven generated standalone execution;
- native Project Audio interaction proof, looping ambient/music ear proof, and
  jump/landing cue proof;
- approved physical-device output and repeated Play/Stop resource proof;
- editor Play/Stop, external Run, and Build use the same scene and selected
  capability profile;
- project output excludes unused editor/floating GUI/backends where configured.

Exit gate:

- actor animates, collides, and plays sound in editor and built runtime;
- stopping/restarting releases audio, physics, and renderer resources cleanly.

### Week 7: Integration And Portability

Foundation status: generated manifests now declare
`project_format: epoch-project-v1` and
`build_profile: epoch-runtime-static`. Known legacy shells remain readable and
migrate atomically on Save Project, Build, or Run; duplicate, malformed,
future, and conflicting metadata is rejected with actionable diagnostics. The
focused `twodstudio` child proves manifest admission, production save/reopen,
static-runtime Build, external child validation, and deterministic cache
regeneration without changing project sources. A request-driven gameplay cost
snapshot now composes logical Canvas2D, texture, physics, and audio costs; the
Project Runtime Preview samples it on a bounded cadence without rasterizing.
`platform.budgets` now owns bounded Canvas2D limits for mobile, deck, desktop,
and editor tiers. Capability profiles inherit the matching tier limits, and a
synthetic pressure contract proves every violation domain fails closed.

Deliver:

- complete acceptance game loop and project template;
- native resource budgets, atlas pressure, GPU residency, and frame timing in
  diagnostics; logical batch, texture, physics, and audio costs are present;
- expose measured native allocation, upload, residency, atlas pressure, and
  frame timing behind the now-enforced tier-derived admission limits;
- generated/project builds, MSVC, CMake/Clang, and headless contract lanes aligned.

Exit gate:

- new project to built game succeeds from documented commands;
- caches can be removed and rebuilt;
- unsupported settings fail closed.

### Week 8: Hardening And Release Readiness

Deliver:

- fix only acceptance blockers and regressions;
- Debug/Release builds and contract tests on supported compiler lanes;
- repeated editor/runtime restart, save/reopen, Play/Stop, Run, and Build tests;
- bounded soak for memory, handle generations, queues, and cache growth;
- operator screenshot/eye proof for the acceptance project;
- concise release notes only after proof, if the operator opens a release gate.

Exit gate:

- the playable project meets every product-definition item;
- renderer/capability matrix contains no unsupported `Present` claims;
- source checkpoint is clean, reproducible, and documented.

## Parallel Work That May Proceed

Parallel work is allowed only when it does not collide with the critical path:

- Raylib/Vulkan scene-solid orientation eye proof;
- Vulkan repeated-replacement retirement proof;
- EpochGui portable primitive integration and tests;
- strict single-call active-project model tool dispatch is source/build
  complete through existing host-owned inspect/save/build/run/test/diagnostics
  authority; live model/GUI approval and approved child execution remain
  operator evidence;
- renderer-neutral material/view/post-process descriptors;
- documentation and build metadata kept synchronized with proven source;
- extension manifests or assets in their owning repositories after direct audit.

Parallel work may not change frame/queue/GUI replay order casually and may not
claim runtime success without visual evidence.

## Backend Truth And Repair Queue

- OpenGL remains the first T1 desktop presentation lane. Its Canvas2D presenter
  storage is per `core::Context` while native texture/presentation hooks still
  activate process-global GL backend state; context-bound hook ownership and
  cleanup-current proof remain a native lifecycle repair gate.
- SDL3, SFML3, DirectX, and Software have operator-accepted current solid
  orientation.
- Vulkan has a dedicated scene-solid pipeline; its corrected front face and
  retirement ownership now need repeated-switch eye proof.
- Raylib remains a specialized OpenGL-derived context with its own ownership and
  presentation evidence. Normal cleanup makes its context current, but the
  deferred-delete error path needs proof that pending native IDs survive until
  an owning context can flush them.
- DirectX currently means the active D3D11 lane; D3D12 is a planned capability
  family and must not be inferred from that implementation.
- Software remains the deterministic/headless and safe fallback.

Raylib and Vulkan solid orientation remains `Partial` until the correction has
eye proof. Vulkan retirement also needs repeated switch-away proof, especially
to Software. This parity work is valuable, but the 2D acceptance project does
not require every editor backend to become a production renderer in the same
eight weeks.

## Settings And Editor Controls

Each subsystem pass includes:

1. typed settings and defaults;
2. project/user/session persistence ownership;
3. EpochGui control state and layout;
4. engine adapter input/drawing;
5. capability-aware availability;
6. diagnostics and error evidence;
7. tests for invalid, unavailable, and stale states.

Near-term controls include:

- project profile and fallback policy;
- backend selection and evidence display;
- Canvas2D resolution, scaling, sampling, blend, and batch budgets;
- texture history, compile, residency, and cache budgets;
- tilemap grid/chunk/collision controls, with the current layer properties and
  hierarchy actions kept synchronized as the runtime contract expands;
- input actions and bindings;
- physics fixed-step, gravity, layers, and diagnostics;
- audio buses, device state, and volume;
- Run/Build profile and included systems.

World Outliner, Asset Browser, GUI Hierarchy, Script Browser, Tile Map,
Properties, World Settings, Output, and AI Chat own independent tool-tab routes.
Project, Assets, AI Output, and Systems evidence are filters in Output, so
status views do not duplicate the names or movement of authoring tools.
Compatible tools may tab into any left, right, or bottom tool stack or float
through native chrome; they never enter the main document strip. The script
lane uses the shared EpochGui text-control contract; syntax, diagnostics,
document tabs, and large-file virtualization extend that one controller. Focus,
input capture, selection, guide/ghost feedback, and redock are acceptance
behavior for every pane, not optional polish. Miscellaneous Tools actions
migrate to their owning surface.

Systems uses the reusable EpochGui `SystemWorkspace` controller as a cached
projection, not as a second registry. Registry timing is enabled only while the
central Systems view is visible. The read-only Live Scheduler reports the shared
editor TaskGraph used by AI evidence, script, project, and approved harness work;
the editable Learning Graph remains a detached temporal simulation document.
The Time view, real scheduler timing, and bounded in-place chart updates are
source/contract complete. Operator-approved GUI responsiveness proof remains.

Direct OS AI supports an OpenAI-compatible API lane and an offline `llama-cli`
lane. The normal-editor authoring lane accepts only strict bounded
`EPOCH_AUTHORING_PLAN_V1` scene-clear, scene-create, exact-count scene-reconcile,
stable-ID scene-transform, and GUI-create calls. Reconcile owns counts; transform
owns explicit position, rotation, and scale changes. Plans stage visibly and use
existing semantic command gateways only after operator approval. Every response
contains exactly one visible semantic command. Scene replacement is a sequence of
separately approved clear and rebuild milestones, never an opaque batch.
Discovery is inventory-only and the operator confirms the selected model once
per Epoch session before any prompt is sent.

Goals are persistent editor tasks with Play/Pause, Edit, and Delete controls.
The next milestone queues automatically only after the approved command changes
the canonical scene revision; failed and no-op plans pause instead of claiming
completion. A bare `/goal` resumes and `/goal <objective>` starts or replaces
the objective. Goal progress remains operator-reversible and scene-aware.
Broad goals are capped at 24 milestones, exactly one semantic command per
milestone, and eight bounded object consequences for that command. Requests publish through detached timeout-bounded
state so pane destruction cannot block on a local-model transport. Epoch never
trains or mutates the selected model; reviewed fixtures live under
`Engine/ai/evals/fixtures/`, and personal AI development remains out of scope.

Normal authoring cannot save, build, run, edit source, invoke native commands,
use Git, alter the updater, or grant approval. `ai.voice_session` owns mandatory
transcript review and explicit per-session microphone/TTS state; cross-platform
capture and local STT/TTS adapters remain the next voice gate and
Dictate/Conversation stay disabled until those capabilities are real.
Exact-content source changes begin with a strict context request for every
affected path. Review Paths displays the reason, selected endpoint, and at most
four paths without reading or sending source. Share Context or Reject Request
stays attached to the originating AI Chat response. Share Context revalidates
the unchanged objective and canonical root, reads only the listed UTF-8 files
within 184 KiB, frames exact `FILE_CONTENT_SIZE`/`FILE_CONTENT_BEGIN` or
`FILE_ABSENT` evidence, and sends that request-local context only to the
displayed endpoint. No directory scan, automatic handoff, persistence, or
live-source write is permitted. Objective changes invalidate reviewed evidence.
Grounded proposals must preserve exact source ownership, reject no-op/generic/
destructive rewrites, share objective-specific vocabulary, and name a reviewed
existing symbol or block in every C++ operation summary. Accepted source changes
then require the reviewed digest, private guard-issued permit, one execution
claim, verified preimages/postimages, and executor evidence inside
a disposable iteration sandbox. The bounded iteration loop sequences
inspection, invariants, planning, approval, implementation, compiler/tests/
analyzers/sanitizers, local self-review, and risk-selected architecture/visual/
frontier review. Qwen-class local models may write bounded candidates; model text
never certifies its own evidence. After Debug and Release compiler/contract
evidence plus a separate HeadlessCI Debug build/run pass, a visible operator
approval runs full engine validation across project profiles, generated-child
self-tests, and the AI gate for that same generation. Only then does a
two-action live-source gate verify exact live preimages, tested sandbox
postimages, and a freshly reparsed identical operation set. Staging shows
a new digest without writing; the second approval rechecks and atomically applies
only that reviewed existing-file source with a fresh one-shot permit. Success
consumes the candidate and grants no automatic next request or broader authority.
The optional Extensions package stages a human-approved build plan and never
starts a server; GGUF model acquisition and licensing remain separate.
For non-source active-project work, `EPOCH_TOOL_PLAN_V1` accepts exactly one
argument-free inspect, save, build, run, test, or diagnostics proposal. The
selected model cannot choose paths, permissions, native commands, or a call
chain. The editor displays the parsed packet, requires attached operator
approval, revalidates it through the host registry, and invokes only canonical
project owners. Approved test may follow a host-owned prerequisite build with a
cancellable generated-child self-test from accepted artifact evidence. This
does not grant the model source permits, shell, Git, network, release, updater,
or self-approval authority.
Professional docking, floating panes, and advanced 3D controls continue in
EpochGui/desktop editor work but remain optional to game/mobile/headless builds.

## Temporal And Authoring Scope

The 2D campaign uses the temporal architecture only where it creates immediate
product value:

- texture documents and sparse tile history;
- tilemap and scene semantic operations;
- save/reopen, undo/redo, and deterministic runtime compilation;
- explicit physics time and animation frame addressing;
- temporal forest documents whose semantic profile history compiles one
  morphology sample, renderer-neutral preview, voxel LOD plan, and occupancy;
  Plant Lab edits and previews the live document while Forest Factory places
  the same compiled revision. The project source codec and immutable Library
  artifact now reopen exact/latest revisions across process restarts.

The complete event-sourced world, persistent AI, collaboration, networking,
planetary simulation, and shared multi-domain graph authoring remain future
phases. The current task-graph document is a deliberately isolated learning
prototype and contract test bed, not the final cross-domain node framework.
These invariants remain preserved without making them prerequisites for the
first game.

## OpenGL Technique Lab Intake

`Autodidac/tiered_gfx_OpenGL_modular_context_demo` supplies reference techniques,
not architecture.

Near-term intake order:

1. quality budgets and feature settings translated into existing capability
   types;
2. material/view/post-process descriptors;
3. RTT, HDR target, and final composition behind `render.device`/`render.graph`;
4. small manifest-backed CC0 diagnostic textures;
5. later OpenGL PBR, sky, shadows, terrain, water, foliage, particles, and other
   effects only after the playable 2D loop.

Do not copy its resource spine, platform/context scaffolding, vendored EpochGui,
hard-coded scene construction, raw GL ownership outside the OpenGL backend, or
unsupported capability labels. Preserve MIT notices for derived code and
quarantine assets without provenance.

## Repository Boundaries

- EpochEngine mainline owns stable contracts, project/runtime integration,
  capability truth, validation, safe fallback, and core default behavior.
- EpochGui owns reusable portable C++23 GUI controls/state and its own tests.
- EpochEngineExtensions owns bulky optional source/content such as advanced
  terrain, FFT ocean, game-specific stacks, and reviewed package payloads.
- If the original remotes remain unavailable, a temporary top-level `Recovery/`
  campaign will reconstruct Bootstrap Loader and EpochEngineExtensions only from
  locally provable first-party or license-audited material. Recovery targets stay
  out of default builds and releases until independent build, provenance,
  checksum, and replacement-remote migration gates pass.
- The OpenGL demo remains a separate technique laboratory.
- Games and generated projects must converge on only selected systems and
  dependencies. The current `epoch-runtime-static` profile excludes native
  extensions and tests while retaining broader runtime source; deeper
  authoring/backend compile-out remains open.

## Source Acceptance

Every source checkpoint must:

- preserve unrelated operator work;
- build the closest faithful Debug and Release targets;
- pass build-safe contract tests;
- keep CMake/MSVC/module metadata exact;
- update capability evidence and settings with source behavior;
- avoid generated caches and runtime artifacts;
- keep release/updater files untouched unless explicitly reopened;
- record durable follow-up in `Changes/mission_cache.md` instead of expanding
  architecture documents with debugging chronology.

## After The 2D Objective

The next dependency order is:

1. T1 GLES/OpenGL compute and accelerated 2D/procedural work.
2. Shared typed node graph and broader material/model/effects authoring.
3. T2 Vulkan/DirectX renderer-resource parity.
4. T3 bindless, sparse, async, subgroup/wave, and GPU-driven execution.
5. T4 hardware ray-query providers.
6. T5 full RT pipelines.
7. Persistent regions, collaboration, networking, and planetary/astronomical
   packages.

Lower tiers remain complete and selectable as higher tiers arrive.
