# Epoch Roadmap

## Mission

Build Epoch into one professional C++23 engine/editor where projects,
renderers, GUI, scripting, packages, updates, OS AI tooling, timing, input, and
voxel/world systems travel through one engine-owned spine instead of becoming
disconnected experiments.

## Non-Negotiables

- Source first, docs after proof. No placeholder systems, fake UI, fake AI, or
  completion claims without build/runtime evidence.
- Preserve stable renderer/editor checkpoints when the operator asks for a
  push. Do not experiment past a confirmed good state before committing it.
- GPU/runtime launches are approval-only; default validation is source review
  plus build-only checks.
- GUI/scene composition order is protected. Command menus and modals must be
  top-layer GUI work, not frame-order guesses that reintroduce flicker.
- Capability truth matters. A backend feature is `Present` only when native
  implementation exists and build verification proves it.
- OS AI means operator-selected external models and evidence gates, not
  EpochBot, hidden autonomy, learner/watcher language, or silent fallback.
- Packages, servers, model weights, and generated projects are opt-in,
  reviewable, license-aware, cache-local, and never hidden bypass channels.
- The published `v0.87.69` runtime and updater are sealed. Source missions do
  not edit updater/release code, scripts, packaging, tags, or assets unless the
  operator explicitly reopens that gate.

## Current Working Contract

The hot path is renderer-resource truth plus reusable source-library growth.
The published runtime stays at `v0.87.69`; development source may advance
independently without reopening or changing the sealed updater/release lane.

The current source shape is:

- reusable GUI module metadata: `Engine/dep/EpochGui`
- reusable GUI public headers: `Engine/include/gui`
- reusable GUI implementation: `Engine/src/epochgui`
- renderer/context implementation folders: `Engine/src/renderers/<backend>`
- internal engine headers that were moved out of public include stay under
  `Engine/src`

The sealed updater contract is retained as read-only behavior documentation:

- normal update remains binary-first and platform-build-gated
- source rebuild remains explicit or fallback-only when no compatible package
  exists
- every update path uses executable-local cache for downloaded packages,
  source snapshots, worker scripts, handoff logs, and managed tools
- source rebuild prefers a validated installed vcpkg checkout when present,
  then falls back to updater-managed vcpkg with CMake-policy overlay ports
- the editor/launcher must keep visible progress, cancel, failure, and restart
  evidence instead of closing or reporting success because a worker merely
  started
- no active mission may modify this lane until the operator explicitly reopens
  it

The current context contract is exclusive: the normal editor owns one live
backend, captures state before a switch, fully retires the old render thread and
native resources, then creates one docked replacement in the same host and
restores that state. Multicontext grids are diagnostics only. Windows source now
routes Raylib, SFML, SDL, OpenGL, Vulkan, DirectX, and Software through this
transaction; runtime acceptance still requires repeated switch proof for state,
focus, GUI/font refresh, and teardown before release work resumes.

## High-Output Source Strategy

Productive passes should land bounded source slices with build evidence, then
record only the changed contract. Use subagents only for independent sidecar
lanes that do not touch the same files: backend audits, MSVC/CMake metadata,
standalone `EpochGui` mirror checks, docs consistency, or release packaging
verification. The operator's throughput target remains roughly 9.2k useful
source/docs/test lines per day, but fake UI, placeholders, or unverified churn
do not count.

Optimize for completed missions per validation cycle, not edit count. For each
failure cluster: preserve the full transcript, identify the first causal error,
apply the smallest production fix that covers all repeated symptoms, run the
closest local production build/contract lane, and use hosted CI as confirmation.
Release/updater validation remains dormant while that baseline is sealed.

Current split lanes:

- updater/release: sealed at `v0.87.69`; documentation reference only until the
  operator explicitly reopens it
- GUI: `EpochGui` portable controllers, `engine.gui` adapter rendering/input,
  themes, modal/menu/top-layer behavior, docking/floating hosts
- context/session: single-context handoff, snapshot restore, routed pane
  ownership, backend shutdown, focus evidence
- renderer resource truth: sampled RTT, graph binding, resource devices,
  capability status/proof layers
- package/project: package activation, generated project parity, scene
  persistence, cache/package boundaries
- OS AI: selected external model control, evidence gates, no hidden autonomy

## Acceptance Gates

- `Changes/active_pass.md` names one source gate and its allowed source areas.
- `Engine/docs/engine/renderer_feature_matrix.md` uses:
  `Present`, `Partial`, `Missing`, and `Deferred` with strict definitions.
- Builds/checks use the safest command allowed by `AGENTS.md`.
- New systems update only the relevant docs/changelog after source proof exists.

## Deferred / Archive Links

- Active pass: `Changes/active_pass.md`.
- Full mission cache: `Changes/mission_cache.md`.
- Renderer status truth table: `Engine/docs/engine/renderer_feature_matrix.md`.
- Runtime/editor behavior: `Engine/docs/engine/runtime_and_editor_workflows.md`.
- GUI library direction: `Engine/docs/engine/gui_library_architecture.md`.
- OS AI policy: `Engine/docs/engine/os_ai_tooling_and_evidence_policy.md`.
- Historical release/version detail remains in `Changes/changelog.txt` and
  older roadmap sections below, not in the hot agent path.

## Release And Source Policy

- Public docs must always distinguish:
  - current development source
  - published stable runtime release
  - bootstrap updater-shell release when one exists
- Packaged runtime assets use versioned platform names:
  - `epoch_win10_x64_vX.Y.Z.zip`
  - `epoch_linux_x64_vX.Y.Z.tar.gz`
- Release packages must be production runtime layouts, not stripped CMake
  executable folders and not source-shaped repo bundles. Public packages carry
  one editor/runtime executable, root `assets/`, README/LICENSE files, and
  required app-local runtime libraries only. Generated/cache atlases,
  duplicated `x64` compatibility folders, source-shaped `Engine/` folders,
  headless smoke binaries, and `ConsoleApplication1.exe` aliases are not public
  runtime payload.
- Runtime-created data stays executable-local under `cache/`: updater work,
  temporary update probes, managed tools, and extraction state live under
  `cache/updates/`; downloaded runtime/source packages live under
  `cache/packages/`; on-demand OS model weights live under `cache/models/`;
  generated/runtime atlases live under `cache/atlases/`.
  None of these cache buckets belong in public release payloads or tracked
  source.
- The updater must resolve the active install type before replacing files:
  - packaged Windows runtime: install the newest matching `.zip` runtime asset
  - packaged Linux/WSL runtime: install the newest matching `.tar.gz` runtime asset
  - source checkout/install: prefer the newest packaged runtime first, then
    rebuild from the GitHub source snapshot only when packaged parity is already
    reached or no newer packaged runtime exists
- Bootstrap updater-shell assets use their own versioned names:
  - `epoch_updater_shell_only_win10_x64_vX.Y.Z.zip`
  - `epoch_updater_shell_only_linux_x64_vX.Y.Z.tar.gz`
- Packaged version identity travels with the tagged source and release asset
  names rather than standalone packaged version files.
- The updater remains binary-first and platform-specific: check the newest
  packaged runtime for the current platform first, then continue to source only
  when the packaged runtime is already version-equal or newer.
- Update availability is also platform-build-gated: Windows checks the
  `windows-msvc` Actions job, Linux checks `linux-clang-engine`, and
  queued/running/failing/missing build evidence withholds update UI/actions.
- Source update archives must converge from mutable branch URLs to the exact
  commit revision approved by the matching platform CI job. Until that pin is
  implemented and proven, the platform-build gate is necessary but not a full
  time-of-check/time-of-use security boundary.
- The editor auto-checks updates after startup, but the visible modal appears
  only when the current platform has newer, build-proven update evidence. The
  normal smart-update path is binary-first: it downloads/verifies the matching
  packaged runtime when one exists, then reports and uses source fallback only
  when no compatible newer package exists. `Advanced Source` remains an
  explicit source-test path, not the default update action.
- Source-fallback updates build the platform release lane, not the currently
  running debug lane. A Windows Debug editor may test the updater, but the
  worker must build `Release|x64`, disable MSBuild node reuse/parallelism, keep
  the editor open, and surface progress through `epoch_update_handoff.log`
  until a replacement executable is proven ready.
- Update cache entries use release/source asset names with version suffixes
  under `cache/packages/` instead of anonymous temp names. Cached packages are
  extraction-verified before reuse, broken packages are deleted and
  redownloaded, and source rebuilds delete stale source snapshots before a fresh
  download.
- GitHub source archives stay full source snapshots. Do not slim them down to
  imitate runtime/bootstrap packages.
- Commit titles stay descriptive and versionless. Version numbers belong in:
  - `Engine/modules/engine.version.ixx`
  - README/public version badges
  - changelog/release notes
  - release tags
  - packaged asset filenames
- After a release is cut, `main` moves forward again as the development line.

## Preserved Baseline

These are already established and must stay intact while new work lands:

- canonical example workspace under
  `Engine/examples/ConsoleApplication1/workspace`
- repo-root runtime/path discovery instead of cwd-dependent guessing
- separated atlas truth:
  - source images under the example asset tree
  - tracked prebaked atlases separated from disposable dump output
- repo-root CMake wrapper with an honest floor story
- build-only CI direction instead of GUI smoke inside hosted runners
- hosted CI split:
  required Windows and Linux hosted lanes keep the asset-light
  `epoch_ci_headless` smoke, while the Linux Clang engine lane now builds the
  real `epoch` target with runner-safe OpenGL/software/SFML build dependencies
  and still avoids launching GUI windows
- stable Windows/Linux packaged release path with explicit bootstrap/runtime
  distinction, including Windows `.zip`, Linux/WSL `.tar.gz`, and source-snapshot
  fallback rules
- launcher/editor separation and the current project-centric runtime shell
- current multicontext baseline:
  real backend panes, real detach/redock flow, and no fake demo-launch path
- normal editor docking is selected-backend multi-pane: a DirectX editor should
  create DirectX dockable scene/context panes, an OpenGL editor should create
  OpenGL panes, and so on. Mixed-backend grids remain explicit diagnostic or
  accurate-preview proof surfaces, not the default editor/project runtime shape
- floating GUI/tool windows follow the same rule: undocking clones an
  editor-owned GUI pane from the currently selected backend/context family
  unless the operator explicitly opens a mixed-backend diagnostic grid
- current project scene/world files are metadata shells; live editor preview is
  still seeded from engine-owned project profiles until scene parsing and
  serialization own runtime loading

## C++26 Future-Ready Multi-Build Roadmap

This section is a first-pass foundation. It does not make C++26 required and
does not claim renderer, editor, atlas, or source-tree migration work is done.

### Language Policy

- C++23 remains the stable shipping baseline for packaged and developer builds.
- C++26/latest is an optional validation lane for compiler readiness only.
- Experimental C++26 features must be feature-probed before use and must not
  become required by default.
- New language experiments must keep fallback paths that compile in C++23.

### Multi-Compiler Policy

- Support MSVC, clang-cl, Clang, and GCC where practical.
- Support Visual Studio, Ninja, and Unix Makefiles where practical.
- Linux/GCC 16.1 is currently a headless validation lane by default while the
  full GNU C++ module path remains experimental; full Linux editor/runtime
  builds should use Clang until GCC module support stabilizes.
- The GCC headless lane must stay module-free unless
  `EPOCH_ALLOW_GCC_MODULE_ENGINE=ON` is explicitly enabled. `epoch_ci_headless`
  uses a dedicated logger shim in that lane so hosted GCC can configure, build,
  and run the smoke contract without CMake C++ module dependency scanning.
- Linux/Clang 22.1.8 is the current full-engine Linux rendering build lane,
  paired with CMake 4.4.0 and Ninja 1.13.2. Single-context OpenGL remains the
  default WSL proof path, while SDL, SFML, Raylib, Vulkan, and software are
  build-required Linux contexts. Do not auto-fall
  back to Vulkan in WSL; Vulkan remains explicit validation work on Linux/WSL
  until it is proven locally. DirectX remains explicitly Windows-only and must
  stay disabled for Linux/WSL presets.
- CMake is authoritative for cross-platform builds and project-wide presets.
- Visual Studio project/filter files must not drift from filesystem and CMake
  whenever files are moved or added.
- `CMakePresets.json` is the shared project preset layer.
- `CMakeUserPresets.json` is for local developer overrides and must stay
  ignored.

### Backend Roles

- OpenGL is the stable editor/runtime GPU backend for now and remains the
  comparison point while the Windows-native renderer comes online.
- The software renderer is retired from "peer desktop renderer" status for the
  normal Windows multicontext proof.
  Its target role is safe-launch, debug/error-message GUI, capture diagnostics,
  and headless validation when GPU backends are unavailable.
- DirectX/D3D11 is now the first Windows-native renderer slice in the normal
  Windows multicontext proof. It owns a real device/swapchain/render target,
  basic shader preview path, GUI replay path, and real split implementation
  units for state, device setup, preview geometry, and GUI atlas/sprite replay,
  but still needs deeper renderer-resource parity before it is treated as
  feature-complete. D3D12 stays a future explicit renderer track.
- Vulkan remains the future explicit cross-platform graphics backend until
  runtime support is fully stabilized.
- Linux/WSL does not use the Windows parented multicontext editor shell by
  default. Current WSL proof is single OpenGL context with no automatic Vulkan
  fallback; other backend/tool outputs can later launch as explicit child
  processes from editor output settings once their Linux context ownership is
  stable.
- Raylib, SDL, and SFML remain context/backend compatibility and validation
  lanes, especially for docking, popout, and backend ownership checks.
- Mixed backend grids are a diagnostic/accurate-preview feature only. Normal
  docked editor windows clone the selected backend so pane docking, input,
  resizing, and scene composition are stable before intentionally crossing
  renderer families.

### Long-Horizon World And Renderer Architecture

- Epoch's target renderer/world direction is a futuristic voxel-first engine
  spine: multi-informational voxel LOD, planetary-scale hybrid terrain,
  voxel-assisted navigation, voxel/path-traced lighting, and procedural
  generation that can resolve into classic terrain, mesh, model, and vegetation
  assets when that is the right runtime representation.
- Distant objects and vegetation should converge through integrated
  SpeedTree-like procedural generation plus voxel LOD instead of treating
  triangle virtualization as the only path.
- First-pass core contracts now have a narrow source shape: `voxel.field` owns
  multi-informational cell/chunk/LOD metadata, `voxel.trace` owns shared
  rendering/lighting/navigation/visibility/smoke/AI query intent, and
  `forest.factory` owns deterministic Forest Factory temporal-graph descriptors.
- Forest Factory is a core editor/runtime feature and needs its own 3D editor
  workspace/window. Generated projects must include it only after a human-visible
  scene-use/package activation path adds the `engine_forest_factory` package
  manifest and assets.
- NMS-like planetary terrain, multi-terrain authoring, FFT ocean, external
  voxel demos, and heavyweight renderer/game extensions stay package-managed
  under `cache/packages/` or review branches until their API boundary and
  build/test/provenance gates are clear.
- Operator voxel and procedural forest/temporal-graph prototypes are
  design/reference material for future reviewed imports. They must be audited
  and promoted through the source-shape/import gate before any prototype code
  becomes tracked Epoch engine source.
- Prototype terrain/voxel/forest work should first become a package-manager
  candidate, not a direct source dump. Package candidates must record source,
  hash, provenance, build/test commands, known limitations, and the exact
  engine-owned API boundary they propose before they are promoted.
- The local `vk_cp_cursor_nodoublefree.zip` snapshot inspected on 2026-05-21 is
  a Vulkan voxel/chunk reference with SDF terrain, chunk/LOD ownership,
  platform shells, and seam tests. Keep it staged as research/package material
  unless a later branch/repo import gate explicitly promotes a subset.
- See `Engine/docs/engine/voxel_planetary_package_track.md` for the current
  package-gated voxel/planetary direction.
- This direction does not change the current renderer acceptance gate: OpenGL,
  DirectX, Vulkan, Raylib, SDL, and SFML must keep passing their existing
  build/runtime proof while the central resource/capability spine is built in
  small verified batches.

### Multicontext Windowing Direction

- The target shape is an MSVC-style professional editor shell made from
  explicit, independently owned context surfaces: Scene/Perspective, Game/2D,
  Inspector, AI Chat, Asset Browser, Systems, Build/Output, and future popout
  hosts.
- "One visible context, one owner" is the rule. Proxy shells, hidden helpers,
  and backend-specific host children are allowed only as documented bridge
  details while a backend is being brought into the common shell.
- Command menus, modals, and diagnostics must always draw above scene contexts
  without flicker or z-order fighting.
- Multicontext validation must include normal windowed, maximize/restore,
  detach/redock, and backend-specific shutdown/recreate paths before a branch is
  called stable.
- Linux/WSL parity follows the same product contract: current Windows changes
  must be reflected in the Clang/Linux build path and release notes before new
  screenshots or runtime packages are promoted.

### Renderer Feature Matrix

- `Engine/docs/engine/renderer_feature_matrix.md` owns the imported
  OpenGL/Vulkan/Direct3D feature-family list and separates what Epoch already
  has from missing renderer work.
- Already-present or partial foundations include context/window bootstrap,
  frame clear/present, primitive preview geometry, shader setup, uniforms,
  vertex/index buffers, transforms, camera controls, texture/atlas upload,
  basic material/light placeholders, debug/logging, partial 3D picking,
  capture/runtime-surface plumbing, text/UI rendering, and platform window
  layers.
- Missing renderer work is grouped into baseline renderer completion, shadows
  and lighting, deferred/post-processing, animation/geometry/particles, and
  GPU-driven diagnostics rather than being treated as 61 isolated tasks.
- OpenGL should prove feature behavior first where practical, while the engine
  abstraction is shaped around the explicit Vulkan/D3D resource model:
  buffers, textures, samplers, pipelines, binding sets, render targets, command
  submission, synchronization, and debug/profiling hooks.
- The central renderer-resource spine is now source-owned: formal
  `render.device` handles/descriptors cover buffers, textures, samplers,
  shaders, pipelines, materials, render targets, command lists, mesh/model
  resources, and render-pass/FrameGraph-ready targets. Compiled graph passes now
  carry resolved buffer/texture/material/render-target/mesh/model bindings, so
  sampled render targets, model import, Engine Arcade RTT, and future instancing
  can flow through one backend-neutral shape. System Info must report the active
  backend capability slice, while the next gate is backend-native allocation and
  draw/binding behavior behind those handles across OpenGL, SDL3, SFML3,
  Raylib3, Vulkan, and DirectX; software remains debug/safe-launch fallback
  rather than a parity target.
- DirectX/D3D11 now has a first-pass Windows smoke lane and support claim for
  context/swapchain/preview/GUI proof. D3D12 equivalents stay in the design
  matrix until that backend is deliberately promoted.

### Future Feature Gates

- Probe contracts through feature-test macros before any syntax is used.
- Probe static reflection through feature-test macros before any syntax is
  used.
- Probe `std::execution` availability instead of assuming the standard library
  ships it.
- Keep feature-test macro coverage visible in a small compatibility layer.
- Add hardened compiler and standard-library settings per compiler instead of
  applying one global flag set.

### Asset Root Policy

- Runtime asset lookup must resolve from the executable path, not the working
  directory.
- The resolver should walk upward from the executable directory and probe
  inward for canonical repo and `Engine/assets` layouts.
- Visual Studio/MSBuild `Debug` and `Release` output directories must work
  without cwd assumptions.
- Hardcoded machine-local paths are not allowed.
- The resolved asset root should be logged once.
- Explicit override by CLI, environment, or config must win before automatic
  resolution.

### Atlas Policy

- Later cleanup must classify source atlas assets, generated atlases,
  runtime/cache atlases, and test/demo atlases.
- Source atlases belong under the canonical asset tree.
- Generated/cache atlases must be written under executable-local
  `cache/atlases/` and must not pollute source directories.
- Generated/cache atlases must be ignored.
- Editor-generated graph/runtime surfaces must use the dedicated runtime-surface
  atlas and must not be packed into the small built-in GUI skin atlas.
- Runtime behavior must not depend on random working directories.

### Deferred Heavy Work

- Verify workspace placement and move only if every reference is known.
- Harden the canonical asset resolver with `std::filesystem`, result caching,
  override support, and hard-fail diagnostics.
- Normalize the atlas pipeline and generated/cache ignore policy.
- Continue `src/` and `include/` cleanup without broad blind moves.
- Move the growing Vulkan backend files into a backend-owned source folder only
  as a coordinated filesystem/CMake/MSVC-filter/module migration, not as an
  opportunistic partial rename during renderer flicker work.
- Vulkan asset lookup must keep using the shared runtime/engine asset resolver.
  Do not reintroduce backend-local texture probes that only check `x64/Debug`
  or copied working-directory assets.
- Clarify module ownership and avoid globbing experimental modules into active
  builds by accident.
- Keep MSVC project/filter entries synchronized with filesystem and CMake when
  files are moved.
- Stabilize GUI docking/popout behavior and remaining OpenGL flicker root
  causes.
- Complete primitive/object authoring and runtime surfaces.
- Add desktop-grade editor scaling, outliner clipping/resize behavior, and
  separate in-app editor windows for project/game editing, software/tool work,
  the self-iteration sandbox, and AI visualization.
- Promote the first AI loop visualizer from the central AI Sandbox surface into a
  dedicated editor window with packet replay, scene-state diffs, and eventually
  direct 3D model/weight views.
- Expand CI/test automation without launching GUI windows on hosted runners.
- Harden packaging/install asset behavior for runtime releases.

### First-Pass Audit Notes

- Workspace placement is currently canonical at
  `Engine/examples/ConsoleApplication1/workspace`; this pass did not move it.
- Asset lookup already has an executable-root resolver and environment
  overrides in `core.path`, but scattered legacy relative asset requests such
  as `assets/games/...` and backend-specific fallback probes still need one
  resolver-only cleanup pass.
- Atlas state is mixed but classified: source/demo assets live under the
  example/canonical asset trees, tracked prebaked atlases live under
  `Engine/examples/ConsoleApplication1/atlases`, generated/runtime atlas output
  belongs under executable-local `cache/atlases/`, legacy dump output under
  `Engine/examples/ConsoleApplication1/atlas_dump` is ignored, and runtime
  copies under `x64/` are build output.
- MSVC project/filter files were only touched for newly added files in this
  pass; future file moves must update filesystem, CMake, `.vcxproj`,
  `.vcxitems`, and `.filters` together.
- Hosted CI should remain build-only for graphics targets and should not launch
  GUI windows until a deterministic runner-safe harness exists; no explicit
  Node 20 setup remains in the checked workflows.

## Established Capabilities

These are no longer "future phase" items. They are already part of the live
engine shape and should be treated as starting truth for the next passes:

- one shared runtime/perf/logging/bootstrap spine instead of backend-local
  bootstrap chaos
- staged research import and reviewed promotion instead of letting planning
  files rewrite repo truth directly
- a project-centric launcher/editor shell rather than a demo-first launch path
- generated game/tool project creation and generated project discovery
- a real project/assets dock with build, run, script, and diagnostics surfaces
- project-local script starter creation from the Assets/project surface, with new
  starters written under the active project's `scripts/` folder and surfaced in
  project notes for build/run evidence
- a shallow active-project file/folder browser that skips generated build/bin/.vs
  output and lets project scripts be selected without leaving the editor
- an `Assets` workspace tab with first-pass file-type thumbnail cards for active
  scene, demo model, image, audio, text, and project asset paths. Full decoded
  image/model thumbnail previews are still future work.
- scene preview now receives typed primitive preview data, so seed objects and
  newly created Cube/Light/Spawn entities render as ambient-colored solid
  primitives with wire outlines while the fuller ECS/material/object runtime is
  still being built out
- a live Systems workspace with central graph surfaces, backend ownership
  visibility, first time-control diagnostics, and build-confidence/feature-probe
  status already on-screen
- the Systems workspace now mirrors Phase 5 self-iteration evidence, including
  watcher/build/tool status, staged packet count/root, and the
  planner/executor/builder/verifier/gate contract beside build confidence
- `addons/` is ignored as a local/offline staging area, and the AI control
  contract now allows local game/tool/app/server artifact generation while
  forbidding bypass-capable app/server launch, listener creation, port binding,
  hidden control surfaces, or model-accessible services without a human
  enable/run action
- a temporary bottom Console Dock split into Project, Assets, Systems, AI, and
  Output evidence tabs. This is not the final editor-window system; it is the
  current log/evidence strip until separate editor frames/views are promoted.
- first-pass shared GUI scroll areas for arbitrary window bodies, now used by
  the World Outliner, Inspector, and non-output Console Dock pages so tall AI
  controls, project evidence, and status panels stay reachable on normal
  single-monitor layouts.
- active GUI clipping and hit-test containment for panel content so editor
  controls stop bleeding visually or interactively into the Perspective scene
  view while the fuller dock/window system is still being built.
- AI Console Dock domains split Self-Iteration Sandbox, Tool Harness,
  Engine Assistant, ProjectLauncher evidence, Training, Viz, and Ops status so
  the Inspector can expose the real controls without mixing normal
  game/software authoring with engine self-iteration
- local AI model discovery is explicit-selection only: the editor may list
  available local OpenAI-compatible models, but chat/tooling stays disabled and
  unnamed until the operator selects one
- an editor-native Self-Iteration Sandbox surface that summarizes evidence
  readiness, active planner/builder/verifier/gate state, continuous build
  status, tool-harness activity, and AI loop graph feedback through the dedicated
  runtime-surface atlas
- an editor-owned continuous self-iteration build lane that watches active project/script
  evidence, queues one child-project build at a time, and feeds successful
  build artifacts back into staged AI packets for verifier/gate review
- generated Sandbox child builds now repair stale Windows toolset metadata to
  `v143`, and the checked-in engine projects referenced by those child builds
  also advertise `v143` so manual solution builds and Sandbox scripts do not
  require unavailable `v145` tooling.
- the Self-Iteration Sandbox can repair active project evidence from the editor
  by regenerating or verifying the selected project shell before queueing a
  builder pass
- the Self-Iteration Sandbox exposes repair, manual builder queue, watcher
  controls, and sandbox scene-training packet staging in the Inspector, while
  the AI Console Dock remains a status/visual/log surface so Phase 5 can be
  driven without command-line operation
- AI evidence lookup is now executable/repo-root aware, so launching the editor
  from Visual Studio/MSBuild output directories no longer makes project
  manifests and build artifacts appear missing only because the cwd changed
- an AI tool harness that builds/runs the selected script through the real
  editor host, captures before/after editor state, records MCP evidence, and
  stages packets from successful tool actions
- a committed self-iteration control-loop contract that records the planner, executor,
  builder, verifier, and gate handoff rules for future replay/training work
- explicit AI iteration packet staging and local-vs-committed AI artifact
  separation
- AI iteration packets now carry review-gate state, evidence readiness, and
  the current `planner -> executor -> builder -> verifier -> gate` loop stage
  so future replay/training passes can reason from staged evidence instead of
  guessing from editor state
- project actions now append `PROJECT_NOTES.md` operator notes so generated
  ProjectLauncher shells can show what changed, how to run it, and which
  self-iteration packets/builds affected the project
- sandbox scene-training packets can now be staged from the editor so OS AI
  has an explicit, watchable 3D edit/test learning lane instead of answering
  that it is "working fine" without evidence
- the scene viewport now has first-pass object interaction: visible editor
  primitives can be click-selected and left-dragged while empty scene space
  still supports camera pan/orbit/zoom
- console-dock text rendering now accepts safely visible edge glyphs while still
  rejecting non-finite or unreasonable glyph rectangles, preventing missing first
  letters without reopening the stretched vertical smear artifacts seen while
  scrolling tall AI/path rows
- GUI buttons now capture on press and fire on release, so launcher/editor
  actions happen after the pressed visual state instead of racing it
- project selection no longer silently creates or rewrites project shells; File
  > Save Project, Project > Save Active Project, and the centered Run button are
  the explicit operator actions that materialize/update generated project files
- the top scene command strip now has one centered Run action. It saves and
  rebuilds normal generated projects before launching the selected
  single-context child backend. Script assets validate through their explicit
  script build controls instead of stealing the project Run path. The engine
  self-iteration sandbox remains editor-shaped because it manipulates and tests
  the checked-out engine rather than acting like a generated game/tool child. A
  failed build cancels launch instead of falling through to stale child
  executables.
- generated Sandbox and ProjectLauncher shells expose `--project-self-test` so
  child project output can be verified without launching GUI windows
- the checked-in engine now exposes `--editor-project-self-test <id>` so Sandbox
  and ProjectLauncher shells can be materialized and built from the real engine
  before their generated child `--project-self-test` paths are run
- `v0.84.30` extends that self-test route into the AI evidence loop: the engine
  now appends MCP-style tool captures and stages review-gated iteration packets
  for Sandbox and ProjectLauncher, giving OS AI a real materialize -> build
  -> capture -> packet path to inspect before proposing the next pass
- Self-Iteration Sandbox controls now force the `sandbox` profile and rewrite
  stale generated shell identity when manifest id/script/template evidence does
  not match the selected profile. Sandbox is for manipulating/testing Epoch
  itself, not for silently creating normal scripted projects.
- GUI button press identity is geometry-stable again, while splitters now use a
  dedicated non-button draw path. This keeps resize chrome from polluting normal
  button press state and reduces the launcher pressed-state flicker regression.
  OpenGL preview now snapshots and restores key GL state around scene-preview
  rendering, so the remaining OpenGL present/resize flicker should be chased in
  present timing, resize timing, and panel-host composition instead of obvious
  preview state leakage. Real borderless linked-context popouts stay open work.
- A follow-up OpenGL flicker patch removed the editor/menu queued clear from
  OpenGL UI frames and changed preview-grid rendering so grid lines no longer
  write depth while helper/selection lines render as overlay controls. This is
  patched for the next manual eye test; do not mark the flicker issue closed
  until launcher buttons, dropdowns, Perspective, Game/2D, and graph/matrix
  scenes are manually confirmed stable.
- After the launcher fix was manually confirmed, the next patch deferred
  workbench tab/menu switches until the current GUI frame completes, restores
  core panes when entering scene/game/AI/system workbenches, and adds a small
  OpenGL scene-viewport guard band. This targets the remaining GUI/3D overlap
  flicker and missing-pane reports. MSVC Debug/Release builds and the Sandbox
  project self-test pass; the standalone OpenGL smoke launch was inconclusive
  because OpenGL context initialization failed before the bounded smoke exit, so
  it still needs manual confirmation before promotion.
- World Outliner, Inspector, Console Dock, and AI Chat are now individually
  hideable/reopenable from Window, with first-pass draggable side and bottom
  splitters. The old Console/Chat/Dock sizing button strip has been removed;
  bottom dock width/height is resize-bar driven. Borderless linked-context
  popouts are only staged as a host route and are not yet normal operation.
- top-level editor modes now route the center of the shell into separate
  Scene/Game, Project, Assets, Self-Iteration Sandbox, and Systems surfaces.
  Scene/Game keep the 3D viewport; Project/Assets/AI/Systems disable the scene
  preview and show mode-specific GUI instead of forcing all controls into the
  bottom console dock.
- Game/2D mode now creates/selects an editor-only `Canvas2D` plane and switches
  the scene preview into a locked Canvas2D camera. OpenGL and editor selection
  use an orthographic projection in that mode, matching the Unity-style
  "same scene, dedicated 2D camera" direction. Tile/layer tooling still needs to
  land on top of this.
- Systems now uses larger central graph surfaces with readable labels/data for
  render/frame flow, task/thread scheduling, support tiers, and AI loop status.
  Do not duplicate graph information; combine related scheduling/thread data
  into the task/thread graph unless a new graph answers a distinct question.
- the central Asset Browser has been separated from Sandbox/script-command
  controls. It should represent project assets, while Sandbox remains the
  engine self-iteration domain and script detail stays a project/asset workflow.
- the central editor now has a first-pass tabbed `Editor Workbench` shell for
  Perspective, Game/2D, Assets, Project, and AI Sandbox. Systems is intentionally
  opened as a direct Systems-only surface instead of showing the cross-surface
  submenu again inside Systems.
- AI Sandbox, the bottom AI dock tab, and Window > Open AI Control Surface now
  route through the same control-surface activation path so Inspector, AI Chat,
  and the Console Dock are reopened together before self-iteration controls are
  shown.
- the `aengine`/`aeditor` filename migration is now completed for the current
  source/header/module/project-file batch, including CMake and MSBuild filters.
  Keep `a2048like` as the explicit module-name exception because module names
  cannot start with digits.
- Outliner and Inspector sizing is splitter-owned; stale `Narrow`/`Wide`
  buttons have been removed, and generic scroll areas plus scroll-text panels
  now have first-pass clickable/draggable scrollbars instead of decorative-only
  thumbs.
- Native context hosts now publish a one-second `host FPS` heartbeat in window
  titles. Windows updates both child/context titles and the parent docking
  title; Linux/X11 updates context titles. Use this to compare parent/child
  loop cadence while chasing the remaining OpenGL flicker/double-present
  suspicion.
- `EPOCH_SINGLE_PARENT=0` remains a supported compile path. The Win32 context
  host has inert fallback docking/proxy helpers for non-parented builds so
  `StaticLib1` does not inherit parent-dock-only symbols when the single-parent
  host is disabled.
- `EPOCH_SINGLE_PARENT=0` must be authoritative at launch time too. The current
  fix forces the resolved launch config to standalone contexts when the compile
  flag disables the single-parent host, so CLI defaults cannot accidentally
  create the parent/dock path and reproduce stale launcher/editor resize
  behavior.
- Game/2D now treats `Canvas2D` as an upright XY-style editor canvas with a
  front-facing orthographic rig. This keeps it the same scene, but aligns the
  visual plane with the intended 2D editing view instead of laying it flat like
  a floor.
- `ProjectLauncher` remains the compatibility id/path for generated artifacts,
  but the editor-facing display label is now `Project Hub` to avoid confusing
  the normal launcher, generated project shell, and project-selection workflow.
  A full id/path migration is deferred until all generated references and docs
  can be moved safely.
- local runtime-mini packages now start with `engine_arcade`: generated game
  shells can materialize an asset package manifest and script bridge that invoke
  kernel-engine mini-runtime scenes without copying or relocating the built-in
  game modules. The command-menu Package Manager modal is the intended GUI
  surface for local packages first; future downloadable source packages must
  route through an updater-style build/approval gate.
- `v0.87.00` tightens that Engine Arcade path into a shared package contract:
  `package.registry` owns the arcade default scene, scene inventory,
  render-to-texture asset role, and renderer-resource requirements; generated
  project manifests/scripts consume that contract; and installing Engine Arcade
  from Project Hub creates a game shell instead of dead-ending on a tool profile.
- Engine Arcade install/reinstall now has a visible editor activation path:
  the default arcade scene is selected, a 3D Scene RTT screen/cabinet proof is
  staged, the Package Manager closes to reveal it, and recognized arcade scene
  ids route through the Run target before project-id fallback. Removing the
  package clears its preview/runtime selection so later Plant Lab or extension
  package activations do not inherit stale arcade state. The next package UX
  gate is still richer per-package progress/actions, not silent staging.
- The renderer spine now has backend-neutral sampled render-texture and material
  texture-binding contracts: `render.device` describes the texture/sampler/
  material/render-target/pass shape, `IRenderDevice` owns default handle
  allocation/destruction helpers, `render.graph` declares and compiles sampled
  render-texture assets as one owner of the color texture, sampler, and render
  target, and `v0.87.07` adds graph material resources with named texture slots.
  Compiled passes expose resolved read/write backend-handle bindings including
  the sampler needed when a pass reads a sampled render texture or material
  texture, then request backend binding-set handles so OpenGL-derived contexts,
  Vulkan, and DirectX can adopt native binding work behind the same pass shape.
  Graph passes can now carry resolved model draw intents, so a compiled
  mini-arcade cabinet pass can submit its sampled-screen model through the
  command context in the same backend-neutral shape.
  RTT/render-target allocation remains core engine ownership; Engine Arcade is
  a separate runtime-mini/game package that consumes `engine_arcade.screen` as
  a 512x512 package dependency and proof target. The next acceptance gate is
  backend-native arcade-cabinet presentation across the production contexts
  without moving game implementations into the engine runtime spine.
- Package Manager now needs visible per-package state instead of silent buttons
  or a single cramped combo box: selection uses a shared scrollable package list,
  each row exposes its own Install/Remove/Review Gate action, Install updates a
  shared `engine.gui` progress bar/status line, downloadable packages remain
  staged behind human approval, and Console Dock mirrors compact status only
  instead of controlling package/editor workflows.
- project Run must be operator-selectable by runtime backend/context. The
  current acceptance gate is that the Project workspace selector launches built
  child projects as standalone single-context processes with explicit backend
  flags instead of opening another multicontext editor clone. If the expected
  child executable is missing after a build, Run must block with visible
  evidence and must not fall back to the parent multicontext shell.
- `v0.84.48` keeps the centered Run contract aligned with that gate: normal
  generated projects build and launch through the selected single-context child
  backend, while the engine self-iteration lane remains editor-shaped. The
  bottom Console Dock is status-only again; Project, Assets, AI, and Systems use
  compact non-selectable text panels and must not regain workflow buttons,
  package controls, graph controls, or model-selection controls.
- viewport movement now starts from a shared input profile instead of hardcoded
  editor assumptions. `v0.84.53` added named movement/look/reset/cancel/confirm
  actions, mapped `Home` to reset, and routed editor/project runtime camera
  input through that spine. `v0.84.54` fixes the first profile-regression sweep:
  Editor Default and Left-Handed no longer bind arrows as movement secondaries
  while arrows are also look keys, and Arrow Pilot becomes a movement-first
  profile that does not rotate and translate from the same key press. Editor
  Settings and the Project workspace expose named input-profile selectors, and
  Play In Editor plus single-context child launches carry the selected input
  preset. The next gate is per-action key rebinding, project/package
  serialization, and a polished Input Settings surface so projects can opt into
  the same bindings without bloating software or single-player outputs that do
  not need them.
- project child launches must remain single-context runtime launches, not a
  nested multicontext editor clone. `v0.84.54` makes the child process command
  explicit with both `--standalone` and `--window-mode standalone`, while the
  CLI default no longer initializes Software as part of the backend set unless
  explicitly requested.
- `v0.84.55` tightens the reusable GUI/script/OS-AI surface without touching the
  protected draw model: dropdown/select boxes now close on outside click and
  anchor near the selected item when opened, scrollable text panels ignore wheel
  input outside their active clip, built-in script assets use ASCII headers so
  the current source preview stops showing high-byte banner question-mark
  blocks, generated script wording is now "starter" instead of "stub", and the
  stale OS-AI placeholder scorer was removed until a real verifier-backed
  scoring lane exists.
- `v0.84.56` starts making Forest Factory real instead of leaving it as a
  package-name stub. The core `forest.factory` contract now carries presets,
  temporal/branch controls, preview modes, output categories, and deterministic
  preview-stat estimates. The Package Manager stages a visible
  `engine_forest_factory` manifest/profile into an active project only after
  explicit activation, while the Asset menu opens a Forest Factory workbench
  that shows provenance, package/profile evidence, and estimated preview stats.
  Package payload/source routing points at
  `https://github.com/Autodidac/EpochEngineExtensions`; the Plant Lab repository
  stays recorded as reference/prototype source instead of being cloned into
  every project.
- `v0.84.57` moves the Forest Factory workbench into the scene-backed editor
  path with deterministic preview primitives instead of a manifest-only text
  panel. It also fixes the Asset command-menu row-count mismatch that could
  make command menus flip between open/closed over the scene, clips the Package
  Manager modal body with the shared GUI scroll-area primitive, and saves a
  minimal `.epoch` entity snapshot during explicit project evidence repair so
  save/build/run no longer immediately discards current editor entity edits.
  Full scene parser/serializer ownership, mature Forest Factory sliders/atlas
  controls, and project payload emission remain acceptance-gated follow-ups.
- `v0.84.58` extends that saved-scene path into Play In Editor/runtime handoff:
  the editor snapshots the active scene before in-editor play, the project play
  runtime consumes the `.epoch` entity snapshot with project seed entities as
  fallback, and same-project evidence repair preserves workspace, camera,
  input-profile, selected backend, and frame-limit state instead of snapping the
  editor back to defaults.
- `v0.84.59` narrows the remaining OpenGL command-menu flicker path by making
  OpenGL GUI/scene composition order static again while keeping top-layer menu
  replay above the scene. Overlay-priority state no longer toggles whether the
  normal GUI batch drains before or after the scene, so command menus should not
  slowly flip between scene-under and scene-over composition while Play In
  Editor or a normal scene viewport is active. Acceptance: operator eye-test
  confirms File/Edit/Asset/Window/Tools dropdowns and Package Manager modal stay
  above the OpenGL scene without slow flicker.
- `v0.84.61` keeps that protected draw model untouched while tightening workflow
  presentation: non-output Console Dock tabs are compact status-only text again,
  AI Sandbox exposes direct Nemotron/Qwen model-package entry buttons that stage
  `cache/models` download plans through Package Manager, stale child `--backend
  auto` payloads clamp to OpenGL single-context launch, and Linux/WSL project-run
  choices expose only the currently proven OpenGL lane until other backends have
  runtime evidence.
- `v0.84.62` keeps the renderer/draw model untouched and narrows Project Run
  latency: Launch Single Context now uses a child-build freshness gate, launches
  an existing current executable directly, and rebuilds only when source, script,
  project build files, or the engine static library are newer than the child
  output. The pass also aligns Sandbox child artifact naming with the emitted
  `EpochEngine.exe` stem and makes generated/MSVC raylib DLL-import definitions
  conditional so static-vcpkg experiments do not request dynamic `__imp_*`
  symbols.
- `v0.84.63` keeps the renderer/draw model untouched and fixes the Package
  Manager modal containment path: package details remain clipped inside their
  scroll area, while install progress is fixed modal chrome below the details
  body. Acceptance: operator eye-test confirms the progress row no longer
  smears/bleeds across the OpenGL scene or debugger area while the modal is
  open.
- `v0.84.64` keeps the renderer/draw model untouched and moves script editing
  behavior into reusable `engine.gui` primitives: source surfaces are scrollable
  multiline editors with right-click Select All/Copy/Cut/Paste, visible
  whole-field selection feedback, and Save/Reload as evidence actions instead
  of ad hoc clipboard buttons. The same pass moves pane close buttons into
  titlebar chrome and adds Forest Factory scene-preview refresh/selection
  controls while leaving mature plant editing, sliders, and project payload
  emission acceptance-gated.
- `v0.84.65` keeps that same draw-model boundary and tightens GUI input
  behavior: command menus/select boxes now dismiss on outside left or right
  click, reusable buttons/tabs keep stable pressed-state visuals, and the
  source editor renders only visible source lines so single-character script
  edits do not walk the whole file each frame. Forest Factory now has its own
  central editor workspace button and builds a deterministic temporal-graph
  preview from `forest.factory` data instead of using the Asset menu to add a
  few placeholder blocks. Acceptance remains visual: operator eye-test must
  confirm no command-menu flicker regression, responsive script typing, and a
  real Forest Factory preview path before mature sliders/atlas/export tools are
  marked complete.
- `v0.84.66` responds to the 2026-05-29 Forest Factory/operator screenshot:
  raw temporal-graph segments and foliage were overpublished as many large
  selected cubes, causing the preview to read as a yellow spike plus floating
  green debris instead of a controlled editor plant prototype. The fix keeps
  the renderer draw model untouched, publishes only a curated trunk/branch/
  canopy preview budget, prevents Forest Factory markers from stealing normal
  scene selection, keeps their natural preview colors when selected, and latches
  reusable GUI pressed-state visuals through the release frame to reduce button
  flicker. Acceptance remains visual/operator-gated: confirm Forest Factory
  reads as an intentional preview, command menus do not reintroduce OpenGL
  flicker, and script editor typing remains responsive before marking the
  mature editor controls complete.
- `v0.84.67` keeps the same renderer boundary and promotes the shared
  `engine.gui` source editor from whole-field editing toward a normal desktop
  text surface: click-to-caret, drag ranged selection, selected-range
  copy/cut/paste, Ctrl+A/C/X/V, and Left/Right/Home/End navigation now live in
  the reusable primitive. Acceptance remains operator/runtime-gated: verify
  script editing no longer requires holding the mouse, selected text copies from
  the right-click context menu, single-character edits stay responsive, and the
  command-menu/button flicker fix remains intact in OpenGL.
- `v0.84.68` promotes the 4D/time-based engine direction from roadmap intent
  into a first central Timeline Editor surface. `saveload.system`,
  `scenesnapshot`, and `sceneserializer` now define configurable streaming-save
  modes, checkpoint labels, scene object snapshots, timeline keys, and
  deterministic text output. Acceptance remains staged: the Timeline Editor can
  show/configure checkpoint flow now, but full replay/persistence is not
  complete until `.epoch` scene parsing/serialization owns live editor/runtime
  loading and checkpoint restore.
- `v0.84.69` adds a build-safe engine contract self-test lane before the heavier
  project/AI validation path. It verifies Forest Factory profile/preview
  contracts, scene-use activation policy, streaming-save clamp/capture behavior,
  checkpoint labels, scene snapshot lookup/counting, timeline sorting, and
  deterministic text escaping without opening a renderer. Acceptance remains
  staged: this gives source/build evidence for the contracts, while full
  `--engine-validation-self-test` execution and GUI eye-test proof stay
  operator-gated because project self-tests and renderer contexts can touch live
  GPU/runtime state.
- `v0.84.70` exposes that lane directly as `--engine-contract-self-test` so
  agents and operators can run the pure Forest Factory/timeline/snapshot
  contract checks without materializing projects, starting child runtimes,
  touching updater flow, or instantiating renderer contexts. This is the default
  safe quick-check before expanding contracts into project generation or OS
  model workflows; the broader `--engine-validation-self-test` remains a
  heavier operator-gated route.
- `v0.84.71` gives the 4D Timeline Editor its first dedicated data spine:
  `timeline.system` now owns editor tracks, keyed events, playhead state,
  scrubbing helpers, event sorting, and scene-key conversion. Acceptance remains
  staged: this makes timeline UI and streaming-save gates contract-backed, but
  full replay/restore still requires scene parser/serializer ownership and
  runtime-safe persistence proof.
- `v0.84.72` deepens the configurable streaming-save contract with named
  profiles, rolling-retention descriptions, checkpoint records, and manifest
  lines. Acceptance remains staged: profiles and records are now visible and
  build-tested, but the actual writer/restore pipeline still needs explicit
  scene parser/serializer ownership and operator-approved runtime proof before
  being called complete.
- `v0.84.73` adds the first deterministic scene snapshot parser beside the
  serializer and covers it with the pure engine contract self-test. Acceptance
  remains staged: serializer/parser round-trip is now build-safe evidence for
  scene object data and timeline keys, but live `.epoch` scene restore,
  streaming disk writes, and editor/runtime replay still need runtime-safe
  proof before they are called complete.
- `v0.84.74` adds a deterministic streaming-checkpoint package contract:
  `saveload.system` now binds a checkpoint record, scene payload, manifest line,
  and payload hash before any runtime disk writer is promoted. Acceptance
  remains staged: the pure contract lane can now verify package validity and
  parser restore from the staged payload, but actual file writes, rolling
  retention cleanup, and editor/runtime replay still require runtime-safe proof.
- `v0.84.75` expands the universal input profile contract inside `engine.input`:
  camera center reset, frame selection, clipboard, context-menu, play-in-editor,
  timeline, and package-install actions now have named bindings, modifier
  support, mouse bindings, validation, summaries, and contract self-test
  coverage. Acceptance remains staged: this is the data spine for selectable
  text, right-click menus, project single-context launch controls, and editor
  input settings, but live event routing and GUI editing of bindings still need
  runtime-safe eye-test proof.
- `v0.84.76` validates the package/model surface as a first-class engine
  contract instead of loose Package Manager text. `package.registry` now exposes
  package kind labels, activation labels, Bonsai-as-default local image model
  selection, core-without-project-payload checks, network-sensitive gates, and a
  deterministic registry validator. The pure engine contract self-test now
  proves Forest Factory activation, Qwen/Nemotron/Bonsai model lanes, and
  server/listener approval gates without launching renderer contexts. Acceptance
  remains staged: Package Manager layout, actual download/build progress, and
  project opt-in payload writes still require GUI/runtime eye-test proof.
- `v0.84.77` adds the next build-safe Timeline Editor data shape: timeline view
  metrics now describe visible time ranges, seconds-per-pixel, playhead X,
  visible event counts, and per-track event summaries. The Timeline Editor can
  display a view summary from shared data instead of hardcoded status text, and
  `--engine-contract-self-test` covers the metrics. Acceptance remains staged:
  real timeline lanes, editable keys, drag/scrub UI, persistence writing, and
  replay restore still require the shared GUI library and runtime-safe proof.
- `v0.84.78` makes streaming-save profiles descriptor-backed instead of
  switch-only. `saveload.system` now owns stable profile IDs, labels, summaries,
  defaults, retention caps, and included-data flags for manual review,
  15-second editor streams, 120-frame editor streams, and timeline-keyed replay
  gates. The Timeline Editor displays the active descriptor metadata and the
  pure engine contract self-test validates profile lookup/application. Acceptance
  remains staged: selectable profile dropdowns, disk writer promotion, replay
  restore, and real timeline lane editing still need GUI/runtime proof.
- `v0.84.79` adds the first timeline lane/marker layout contract. Tracks now
  produce lane rectangles, keyed events produce marker positions inside the
  visible time range, and the Timeline Editor reports the shared lane/marker
  summary before drawing a full visual lane renderer. Acceptance remains staged:
  the next GUI pass must turn these metrics into selectable lanes, draggable
  keys, visible scrub handles, and persisted layout without disturbing the
  protected scene/GUI draw order.
- `v0.84.80` adds the next streaming-save writer contract without writing files
  yet. `saveload.system` now creates checkpoint write plans that bind the staged
  snapshot path, serialized scene payload path, manifest path, manifest line,
  and validity message. The Timeline Editor surfaces those paths so the future
  human-approved writer gate can be reviewed before disk persistence, rolling
  cleanup, or replay restore are claimed complete.
- `v0.84.81` adds streaming-save cadence planning. `saveload.system` can now
  report whether a checkpoint is due now or scheduled by seconds/frames from the
  shared `core.time` stats, and the Timeline Editor exposes that next-capture
  summary beside the profile/write-plan rows. Acceptance remains staged:
  editable profile dropdowns, visible lane rendering, disk writes, and replay
  restore still require GUI/runtime proof.
- `v0.84.82` adds the matching non-reading restore/replay plan for staged
  checkpoints. Restore plans mirror the snapshot path, serialized scene payload
  path, manifest path, and checkpoint label from the write-plan contract so the
  future replay gate can be reviewed before disk reads or live scene restore are
  enabled.
- `v0.84.83` adds the first guarded checkpoint writer implementation. It can
  create parent directories, write the serialized scene payload, write snapshot
  metadata, and append the manifest line, but only when passed an explicit human
  approval object. The pure engine contract self-test verifies the blocked gate
  and snapshot payload shape without writing files. Acceptance remains staged:
  no editor auto-write, rolling cleanup, or live replay restore is enabled until
  the writer gate has runtime-safe UI proof.
- `v0.84.84` adds the matching non-destructive checkpoint retention plan. The
  save/load contract can now report which staged checkpoint records would be
  retained or pruned under the active rolling-retention cap, and the Timeline
  Editor exposes that cleanup summary without deleting files. Acceptance remains
  staged: retention execution still requires a human-approved disk cleanup gate
  plus runtime-safe UI proof.
- `v0.84.85` adds streaming-save profile-change plans so future Timeline Editor
  dropdowns can preview a profile transition before mutating live save settings.
  The contract reports source/target profile IDs, mode, enabled/manual state,
  cadence, retention cap, and included-data flags, and the pure self-test covers
  the interval-to-keyed transition. Acceptance remains staged: clickable profile
  selection still needs GUI/runtime proof and must not revive command-menu or
  button flicker.
- `v0.84.86` repairs the hosted Linux Clang full-engine CMake path after Forest
  Factory editor integration exposed a strict module-visibility gap. `editor.cpp`
  now imports `voxel.field` directly before using `epoch::voxel::Float3`; this is
  a build-only repair and intentionally leaves renderer ordering, GUI draw-model,
  Package Manager layout, and Timeline/Forest Factory runtime behavior untouched
  for the next acceptance-gated pass.
- `v0.84.87` repairs OS AI chat/editor input regressions without touching the
  renderer draw model: shared AI chat input fields now have independent widget
  focus keys, selected OS models initialize immediately, leaked
  reasoning/debug-only helper text is rejected from visible chat, and model
  package staging no longer looks like a frozen 35% download.
- `v0.87.07` is the feature-line refresh checkpoint. It preserves the OS
  AI/model-package repairs, Linux Clang full-engine optimizer fix, cleaner
  default editor seed set, Package Manager list/action/detail direction,
  Forest Factory workspace direction, Systems graph refinement, binary-first
  updater handoff, modal progress/cancel evidence, and GUI text wrapping work
  without changing renderer order or the protected GUI draw model.
- System Info now distinguishes declared renderer graph descriptors from
  backend-native allocation. Mesh/model and sampled render-texture descriptors
  can compile through the graph today, and the visible status rows now separate
  backend-native sampled-RTT allocation from graph-declared/native-pending
  lanes. Raylib, OpenGL, SDL3, and SFML3 now have concrete sampled-RTT device
  paths at different proof levels; Vulkan/DirectX remain resource-model
  implementation gates. Backend-native mesh/material/model presentation remains
  the next renderer-spine acceptance gate.
- Raylib is now the first backend-native model-handle and sampled-RTT bridge:
  model descriptors map through `render.device_raylib` into the existing Raylib
  loaded-model registry with per-handle draw/unload calls, and compiled
  render-texture assets map to real Raylib render targets with paired color
  texture/sampler/render-target handles. The next renderer-spine gate is
  OpenGL-derived parity for sampled render-target presentation, material
  bindings, and model/mesh allocation without changing the protected
  GUI-over-scene draw order.
- The engine contract self-test now compiles the Engine Arcade
  `engine_arcade.screen` sampled render texture through the shared render graph
  with the null device. This is the build-only acceptance gate for the
  mini-runtime RTT spine before backend-native arcade-cabinet presentation.
- OpenGL-derived renderer work now has a shared first-pass RTT contract:
  `render.device_opengl_family` compiles the same `engine_arcade.screen` graph
  through OpenGL, SDL3-over-GL, and SFML-over-GL logical devices with shared
  sampled texture, sampler, render-target, binding-set, render-pass, material,
  mesh, and model records. The current build-only gate proves the mini-arcade
  cabinet shape: RTT screen output feeds a material texture slot and a logical
  cabinet screen mesh/model pass. The shared render-texture plan now also
  records backend requirements for color/depth attachments, sampler, offscreen
  target, and presentable-surface behavior, and OpenGL-family records keep that
  native work order without claiming allocation is complete. The build-only
  gate also checks that graph execution binds the cabinet material/model,
  sampled texture, sampler, render target, RTT dimensions, and model draw
  submission through the OpenGL-family command context. Native FBO/texture
  allocation and live presentation remain the next context-owned acceptance
  gate.
- OpenGL-family native RTT promotion now has an injection seam instead of a
  runtime shortcut. `render.device_opengl_family` can accept backend-owned
  allocate/begin/end/destroy hooks for sampled render textures, and the contract
  harness proves that path with a fake native allocator for OpenGL, SDL3-over-GL,
  and SFML-over-GL before any real FBO code touches the live editor frame path.
- SDL3 and SFML3 now have backend-native sampled RTT device modules beside the
  shared OpenGL-family logical contract. `render.device_sdl` owns SDL
  target-texture allocation/bind/clear/restore/teardown, while
  `render.device_sfml` owns SFML render-texture allocation/bind/clear/display
  and window reactivation. Both devices also own graph-native buffer/material/
  render-target slots plus mesh/model records for the mini-arcade cabinet graph,
  so build-only contracts prove model binding, `draw_model` submission, and
  release without launching renderer windows. Live arcade-cabinet presentation
  and editor preview hookup are still separate runtime gates.
- Engine Arcade RTT construction is now source-owned by `render.arcade`.
  The screen sampled target, render-surface material binding, screen mesh/model
  descriptors, cabinet pass, and model draw intent are reusable graph builders
  instead of inline self-test scaffolding. The current proof is MSVC Debug x64
  `ConsoleApplication1`; Raylib's no-runtime-safe RTT contract, backend-native
  OpenGL-family FBO allocation, and live cabinet presentation remain the next
  renderer-spine gates.
- The visible editor workspace contract is now `3D Scene`, `2D Scene/UI`,
  `Assets`, `Plant Lab`, `Video`, `Project`, `Intelligence`, and
  `System Info`. Old labels such as Perspective, Game/2D, Forest Factory,
  Video Editor, AI Sandbox, and Systems should remain internal/provenance terms
  only when needed, not the primary user-facing navigation.
- Package Manager list rows should behave like selectable text/link rows with
  separate Install/Remove/Review Gate actions. Package names, package summaries,
  and status evidence are not fake buttons; the shared GUI library now owns a
  `text_link` primitive as the first step toward reusable automatic list/content
  containers.
- Automatic content containers remain an active GUI-library mission: scroll
  areas, link rows, action rows, progress bars, edit boxes, source editors, and
  future virtualized lists should resize from available content/viewport rules
  instead of each editor workspace hardcoding row geometry.
- Current Forest Factory acceptance remains open: the buildable preview may act
  as a node/low-LOD visualization, but the mature target is still a Plant
  Lab-grade Forest Factory scene with temporal graph controls, atlas/export
  tools, package activation, and project payload emission backed by runtime
  proof.
- The latest accepted source gate moves Forest Factory beyond cube placeholders:
  deterministic trunk, branch, and foliage entities now resolve through shared
  preview primitives, while `forest.factory` estimates voxel occupancy for the
  future LOD/hit-detection/navigation/lighting/path-trace spine. This is still a
  preview and evidence contract, not the final Plant Lab asset-grade renderer.
- workspace launches and toolbar surface switches should eventually use the
  shared progress primitive for short transition feedback. The acceptance gate is
  that loading feedback appears without moving the scene viewport or reviving
  command-menu/scene z-order flicker; inline toolbar loading bars are deferred
  until they meet that gate.
- built-in mini-runtimes remain part of the core engine that ships with
  applications. They should be script-invokable and usable as future
  render-to-texture/game-arcade assets, not migrated into loose project script
  source.
- current GUI/render observations from manual runs: launcher flicker is reported
  resolved. The `v0.84.29` pass keeps OpenGL scene-first composition and
  replays the latest persistent GUI batch after the scene pass so the continuous
  OpenGL render thread cannot alternate scene-only frames between UI ticks.
  `v0.84.31` is the stable checkpoint to preserve: command menus/modals request
  overlay-priority drawing, workbench surface changes apply before the center
  panel draws, visible scene-viewport blanking during surface settling is
  removed, and Systems graph buttons have a small input cooldown so repeated
  presses do not churn graph surfaces every frame. AI Chat, Inspector,
  Perspective pane visibility/title chrome, and the transparent scene-backed
  workbench still need continued eye-test confirmation. Graph surfaces also need
  stronger data density, design polish, and performance.
- `v0.84.33` extends that safety guard without disturbing the confirmed
  OpenGL frame order: module SDL/SFML previews now consume the shared preview
  marker line data instead of staying grid-only, and Systems graph controls have
  a longer repeat guard. MSVC `ConsoleApplication1` Debug builds cleanly and
  standalone OpenGL editor smoke exits cleanly. Parented multicontext editor
  smoke still times out, and parented `--smoke --capture` writes captures but
  does not exit before timeout, so multicontext shutdown remains an open
  acceptance gate.
- `v0.84.34` narrows the parented maximize/resize fix: proxy-host SDL/SFML
  window moves remain asynchronous, while direct child renderers such as Raylib,
  OpenGL, Vulkan, and software get immediate parent-grid sizing again. MSVC
  `ConsoleApplication1` Debug builds cleanly, and a parented multicontext
  maximize/restore smoke completed without crashing. Raylib resize convergence
  still needs operator eye-test confirmation before the branch is called done.
- `v0.84.35` promotes the first DirectX/D3D11 backend slice into the Windows
  multicontext proof and refreshes the README screenshots: the normal six-pane
  proof is Raylib, SDL, SFML, Vulkan, OpenGL, and DirectX. Software remains a
  safe-launch/debug/headless fallback instead of a normal Windows product pane.
  MSBuild Debug/Release, HeadlessCI, CMake/MSVC configure/build/ctest, and WSL
  Clang configure/build/ctest passed for this checkpoint. DirectX scene drawing
  now honors the editor scene-preview gate so launcher-only GUI surfaces are not
  repainted by D3D11 grid/object geometry. The angle-dependent DirectX floating
  diagonal artifact was traced to partial primitive clipping and fixed by
  appending D3D11 preview lines/triangles only when every vertex in that
  primitive survives projection. The first DirectX source split now keeps the
  public `directx.context` interface while moving behavior into
  `directx.state.cpp`, `directx.device.cpp`, `directx.preview.cpp`, and
  `directx.gui.cpp`. Remaining DirectX work is renderer-resource parity, real
  depth/resource ownership, and any operator-observed GUI flicker in the D3D11
  pane, not basic context creation.
- `v0.84.38` is the cache/update and command-menu stability release line:
  OpenGL top-layer menu sprites are replay-only, toolbar dropdowns close on
  outside click, updater work/tools/temp extraction stay under executable-local
  `cache/updates/`, downloaded packages stay under `cache/packages/`, and
  generated atlases are reserved for `cache/atlases/`. Windows Release and WSL
  Clang release build/CTest lanes are required proof before publishing the new
  runtime packages.
- `v0.84.40` is the static-vcpkg/MSVC linker repair line for the current
  source tree: STB image symbols are owned by one private source file instead
  of a backend context object, SFML is no longer linked as both static and
  dynamic in the app target, raylib static/dynamic import macros match the
  static vcpkg build, SDL3 static Windows system libs are explicit on the final
  editor executable, CMake selects exactly one GLAD provider through
  `EPOCH_GLAD_PROVIDER`, and the editor status strip reports thread capacity
  without repeating launcher/editor labels.
- `v0.84.41` adds the first visible script-source editing controls in the
  Assets workspace: shared GUI clipboard helpers, Copy Source, Paste Clipboard,
  Save, Reload, and UTF-8 continuation-byte collapse so unsupported source
  banners do not smear into repeated question marks. The next acceptance gate is
  a real code editor viewport with internal scrolling, ranged selection, caret
  navigation, syntax-aware display, and clean copy/paste behavior across
  Console Dock, AI Chat, and source editing surfaces.
- `v0.84.42` adds the first hard Phase 5 OS AI promotion-safety guard:
  hidden-reasoning-only local model replies, no-model/no-decode failures, API
  errors, and obvious leaked reasoning drafts stay visible as operator feedback
  but are blocked before local training capture and MCP chat evidence.
  `--editor-ai-gate-self-test` now proves those non-promotable replies are
  blocked while a valid evidence-backed final answer remains allowed. The next
  acceptance gate is AI Chat/evidence-panel copy/paste/selection smoke proof and
  the first watchable executor action in the closed-loop sandbox.
- `v0.84.50` targets the newest minor OpenGL/GUI button-state flicker report
  without touching the protected draw model: `engine.gui` now exposes an
  explicit selected-button primitive, and the top menu/workspace toolbar rows
  use it for active/open state instead of relying only on transient hover or
  press state. Acceptance remains operator eye-test proof that half-selected
  menu/workspace buttons no longer flip-flop during normal OpenGL use.
- `v0.84.51` separates live engine thread accounting from detected CPU thread
  capacity. Engine-owned scheduler workers, task-graph workers, context render
  threads, AI chat calls, self-iteration builds, and project build async tasks
  now enter a shared RAII thread counter; the toolbar and Systems workspace show
  live spawned threads alongside detected CPU/hardware threads. DirectX preview
  present no longer uses a backend sync interval, keeping frame pacing owned by
  the core limiter and the editor/project frame-limit controls. Acceptance:
  operator eye-test confirms live thread counts rise/fall with active engine
  work and FPS caps only follow explicit core limiter settings.
- `v0.84.52` targets the OpenGL-only project-run/menu flip-flop: overlay-priority
  frames now drain normal GUI after the scene instead of letting command menus,
  modals, or project runtime panels fight the scene viewport for z-order.
  Project runtime preview chrome is promoted into the explicit GUI top layer and
  OpenGL project-runtime frames avoid a redundant pre-scene clear. Acceptance:
  operator eye-test confirms Project Run no longer alternates GUI behind/in-front
  of the scene and command/menu input remains usable while the scene is active.
- `v0.84.53` started the universal input system. `engine.input` now exposes a
  shared action/profile layer over raw keys, expands Win32 key mapping beyond
  letters/numbers, and carries selected project camera style into Play In Editor
  plus single-context child runs. Editor Settings and the Project workspace now
  expose input profile selectors for Editor Default, Runtime WASD, Arrow Pilot,
  and Left-Handed IJKL, and project runtime payloads carry the chosen profile.
  `v0.84.54` de-conflicts those presets so WASD/QE or IJKL/UO move, arrows look
  where enabled, Arrow Pilot moves with arrows without also turning the camera,
  and `Home` remains reset. Future Input Settings work changes bindings through
  profile data rather than per-view code.
- `v0.84.54` also hardens the project-run/runtime split. Built child projects
  are launched with explicit standalone window mode and the CLI backend defaults
  no longer pre-enable the Software fallback backend. Acceptance: Project Run
  starts one selected backend process, while Software appears only when
  requested as a debug/safe fallback.
- Canvas2D projection ownership has moved into `render.preview_grid` via one
  shared projection helper. Editor picking, OpenGL, DirectX, Raylib, SDL, SFML,
  Vulkan, and the software preview fallback now consume the same Canvas2D
  orthographic framing instead of duplicating backend-local perspective/2D
  branches. The Game/2D acceptance gate is that all normal multicontext panes
  show the 2D canvas head-on instead of leaving Raylib/SDL/SFML/Vulkan in
  tilted editor perspective; this pass is build-validated and still needs the
  operator multicontext eye-test.
- The Run button now serializes generated project builds inside the editor
  process and routes normal project launches through the selected single-context
  child backend. A duplicate Run/build request returns a visible failure instead
  of launching a second ProjectLauncher/Sandbox build that can collide over
  shared `StaticLib1` clean logs, module IFC/BMI state, or PDB outputs.
- Generated Windows child build scripts now carry a repo-level lock as well, so
  manual ProjectLauncher/Sandbox builds launched outside the editor wait for the
  shared MSVC engine build lane instead of corrupting shared logs/libs/PDBs.
- ProjectLauncher and Sandbox generated outputs both build and pass their
  generated `--project-self-test` when run serially. A deliberate parallel
  ProjectLauncher/Sandbox build reproduced the prior shared-output collision,
  confirming that generated child builds either need the shared lock or fully
  isolated engine-object directories.
- Raylib redock crash work has moved from direct cross-thread Win32 mutation to
  owner-thread dock command routing through the Raylib render command queue.
  The patch builds cleanly; the acceptance gate is still live multicontext
  drag-out/redock confirmation with no parent crash.
- SDL/SFML top-row redock crash evidence points at parent-grid repositioning of
  proxy host/child windows. The current fix routes grid placement through the
  proxy host redock command and avoids redock-triggered layout recursion; the
  acceptance gate is a live six-context redock eye-test focused on SDL and SFML
  returning to top-row slots without crashing, hiding, or desynchronizing input.
- WSL/Linux status for `v0.84.35`: repo-root `ninja-clang-debug` builds,
  `epoch_ci_headless` passes with DirectX disabled, and Ubuntu WSL2/WSLg now
  produces a real non-black single OpenGL editor capture when launched with
  `epoch --renderer opengl --standalone --editor --smoke --capture`. A Linux
  runtime package was staged at
  `C:\tmp\epoch_release\epoch_linux_x64_v0.84.35.tar.gz`; the staged package
  reports `Epoch v0.84.35` and passes `epoch_ci_headless .` after including
  source-shaped `Engine/assets`, `Engine/resource`, and `Engine/ai/control`
  runtime support paths. Plain runtime smoke without `--editor` can still write
  a black capture and should not be treated as failed OpenGL dependency proof.
  Keep workstation-specific capture failure notes in local/pass context rather
  than public release copy.
- A local Windows runtime package was staged at
  `C:\tmp\epoch_release\epoch_win10_x64_v0.84.35.zip` with app-local backend
  DLLs, assets, and VC143 CRT DLLs, and the staged executable reported
  `Epoch v0.84.35`. GitHub CLI authentication was restored from the existing git
  credential path for publishing the `v0.84.35` release assets.
- Current visual evidence is now preserved at
  `Engine/docs/engine/diagnostics/2026-05-17-gui-regression/README.md`.
  Acceptance gates from that set: Perspective title stays visible with World
  Outliner open; Inspector and AI Chat draw normally; Console Dock stays compact
  and does not duplicate central AI/Systems controls; scrollbar extents do not
  smear at min/max scroll during resize.
- Console Dock is only a temporary evidence/log strip. Output should remain a
  plain scrollable log, and Systems graph UI belongs in the central Systems
  workspace rather than duplicated inside the dock.
- the editor still has too many duplicate paths to equivalent controls across
  central surfaces, docks, menus, and inspectors. Keep reducing duplicate command
  surfaces while preserving one discoverable path and one quick-access path.
- current GUI shape targets from operator review: shared tab controls should draw
  as real connected tabs instead of generic buttons; close buttons belong as
  top-right window X affordances; launcher/editor settings should open useful
  modal surfaces; Project Hub should use a game/software launcher mockup instead
  of mirroring the editor; scripting needs a real code/text editor view; World
  Outliner rows need human names and grouping; OS AI model-analysis
  belongs in a separate graph/terrain-style AI visualizer that samples summaries
  rather than trying to draw billions of parameters.
- latest editor workflow gates from operator review: Systems should replace the
  generic Inspector side panel with live `[time]`/pacing stats when Systems is
  active; AI model selection should live in the renamed OS AI inspector
  rather than duplicating selection controls in the central AI surface; Run
  Scene In Editor and Run External Project must become distinct actions;
  Project Run needs an explicit single-context/backend selector; 2D Editor,
  GUI Factory, Forest Factory, and Script Editor each need their own clear
  workspace entry and should not be hidden in Console Dock output.
- current script-editing evidence is not good enough: the asset/script panel is
  still mostly a preview, broken high-byte banner text renders as question-mark
  blocks, and copy/paste/select-all are not proven across AI Chat, script
  source, and evidence panels. Do not call scripting UI complete until the
  engine GUI text primitive supports editable code text, clipboard operations,
  stable focus, visible caret/selection, save/reload evidence, and build/run
  feedback without holding mouse focus.
- the GUI layer is now documented as an engine-internal library surface:
  primitive widgets, layout/docking, theme/rendering, and editor composition are
  separate responsibilities. New widgets such as tabs and dropdown/select boxes
  should land in `engine.gui` first, then be consumed by editor workspaces. The
  active contract is `Engine/docs/engine/gui_library_architecture.md`.
- the next GUI ownership gate is a real library target split: keep the public
  module as `engine.gui`, but move reusable primitives/layout/text/progress/
  modal/chrome implementation into a separately linked static/shared GUI object
  target only when CMake, MSVC projects, filters, and backend replay users are
  updated together and MSVC/CMake validation passes.
- first dropdown/select-box primitive is active, but acceptance remains visual:
  wheel focus must stay with the open list, text must stay readable, and parent
  scroll panes must not smear or steal input during resize.
- source-shape cleanup is now tracked in
  `Engine/docs/engine/source_shape_audit.md`. Use that guard before renaming
  headers, backend bridges, config surfaces, module names, or Perf Manager
  integration points.
- OS AI training/eval work now needs measurable evidence gates, not vibes. A
  self-iteration pass is incomplete until it has a staged packet, build log,
  output artifact, generated child self-test/verifier result, visible UI state,
  and a human-review gate state with no unresolved `[missing]` evidence markers.
  Track false accepts, false rejects, evidence coverage, tool-trace coverage,
  build/test pass rate, and self-iteration completion rate before promoting
  helper output into curated data.
- Internal bundled-model work is retired. OS AI work now focuses on a
  production harness around selected Nemotron 3 Nano/Qwen models and the
  approved Bonsai/Wan/TRELLIS creative package lanes. Bonsai Ternary 4B is the
  preferred local image lane, Bonsai Binary 4B is the low-memory option, and
  FLUX.2 Klein 4B stays a higher-memory fallback. Curated traces, prompts,
  adapters, evidence gates, notices, and package/runtime integration are the
  mutable artifacts.
- Epoch is a 4D/time-based engine. The time spine now routes through the Video
  Editor mission: keyed events, replay scrubbing, scene-time inspection,
  timeline graphing, controlled timed projects, video editing, and configurable
  streaming save/checkpoint controls are built from existing `core.time`
  ownership instead of a parallel timing system. Systems must not reclaim those
  controls now that Video Editor owns them.

## Phase Progress

- [x] Phase 1: Repository structure, research intake, workspace placement,
  initial build-path honesty, and documentation cleanup.
- [x] Phase 2: Project-centric runtime shell, generated project creation,
  project/script proof rows, launcher/editor separation, and baseline
  project/assets script flow.
- [~] Phase 3: Systems workspace, time spine, backend ownership diagnostics,
  build-confidence surfacing, and hosted/local build reliability.
- [~] Phase 4: GUI maturity, drag/drop, text-input smokes, editor polish, and
  OpenGL startup-flicker/root-cause cleanup.
- [~] Phase 5: Two-role self-iteration loop, captured task packets, review gates,
  dataset/eval promotion, and tool-schema replay.
- [~] Phase 6: Primitive/object authoring, procedural world spine, 2D vertical
  slice, and material/cellular simulation boundaries.
- [ ] Phase 7: Android-first mobile bring-up with one renderer, one input path,
  one packaging story, and lifecycle stability.

## Active Mission Tracks

### 1. Runtime And Multicontext Ownership

- keep SDL, SFML, Raylib, Vulkan, OpenGL, DirectX, and the software fallback behavior
  converging instead of drifting into undocumented backend-specific hacks
- continue the Windows DirectX renderer path from the validated D3D11
  device/swapchain/shader slice toward a real engine-facing resource API, while
  keeping D3D12 as future explicit renderer work
- keep the DirectX implementation split behavior-owned instead of returning to a
  monolith: public context bridge, state/lifetime, device/shader setup, preview,
  and GUI replay each have a real source file now; do not add empty placeholder
  DirectX modules just to make the tree look even
- finish IDE-class docking/popout behavior so detach, input ownership, z-order,
  redock, and startup presentation remain stable
- eliminate remaining OpenGL flicker in both single-context and multicontext
  modes; menu open/close activity and software-context interaction are known
  repro amplifiers and should be investigated before cosmetic-only fixes
- keep maximize/restore in the renderer-windowing repro matrix; the current
  parented smoke no longer crashes, but each backend still needs operator
  eye-test confirmation that resize convergence is fast and correctly clipped
- fix the Raylib redock crash that can still bring down the parent editor
  process during backend-window docking tests
- finish the owner-thread Raylib redock validation by live-testing detach,
  redock, maximize/restore, and shutdown after the queued command patch
- fix SDL and software multicontext scene visibility so those panes either
  render honest scene content or explicitly report fallback/debug-only mode
- tighten terminology so runtime/module/doc names stop leaning on ambiguous
  legacy words like `multiplexer`

### 2. Project-Centric Runtime And Scripted Pipeline

- keep project creation, project play, script build/run, and generated project
  discovery truthful
- keep the default demo path honest:
  generated or discovered projects should build, launch, and hand any
  declared demo model through the engine-owned script host without falling
  back to confusing sample-only behavior
- keep the Mini Sponza demo owned by the compatibility `projectlauncher`
  artifact path while presenting it to operators as `Project Hub`; the
  Self-Iteration Sandbox stays the AI/engine-iteration shell
- continue replacing hardcoded built-in sample assumptions with project-owned
  runtime flow
- keep the already-landed project/script asset shell honest instead of letting it
  drift back toward placeholder tooling
- keep the launcher centered on projects, contexts, settings, and updates
- keep generated project `PROJECT_NOTES.md` visible from the Project workspace
  so scripted/project/AI actions leave a readable synopsis and usage trail
- keep generated Sandbox/ProjectLauncher child builds anchored to current VS
  2022 `v143` toolset metadata in both generated files and the checked-in
  engine projects those generated files reference.
- keep checked-in MSVC solution projects self-contained enough to build from a
  normal VS 2022/MSBuild invocation, including explicit vcpkg triplet defaults
  when machine-global vcpkg integration leaves `$(VcpkgTriplet)` empty.
- keep the checked-in MSVC x64 editor target on one dependency model at a time.
  The current multicontext app lane is dynamic-vcpkg; static-vcpkg all-backend
  experiments remain blocked until third-party static STB/GLAD/math ownership is
  isolated without `/FORCE:MULTIPLE`.
- keep the Self-Iteration Sandbox classified separately from game/tool projects.
  It mirrors the engine/editor manipulation lane, while normal generated
  projects expose their own selectable camera style for editor orbit,
  first-person runtime, or locked 2D canvas previews.
- keep engine-owned runtime-mini packages first-class: local package metadata
  belongs in project assets, script bridges call into engine-owned scenes, and
  downloadable packages must use explicit updater-style source build/approval
  instead of hidden auto-execution.
- add a research-package lane for voxel terrain, procedural vegetation, and
  renderer/tool prototypes. Prototype packages should live in a review branch,
  separate repo, or local package cache until their API boundary, provenance,
  tests, and promotion gate are clear.
- add the next source-shape package registry slice: one descriptor module for
  local runtime-mini packages such as `engine_arcade` and core opt-in packages
  such as `engine_forest_factory`, then replace duplicated package strings in
  editor/project/runtime code only after the descriptor module builds in MSVC and
  CMake.
- continue the AI safety slice: hidden-reasoning-only responses are now rejected
  before raw response snippet logging, blocked from local training/MCP capture,
  and covered by `--editor-ai-gate-self-test`; the remaining gate is
  copy/paste/selection smoke proof for AI Chat and scrollable evidence panels.
- keep the bottom dock centered on `Project`, `Assets`, `Systems`, `AI`, and
  `Output` as evidence/status tabs, not as the final scene/editor-window model
- promote the new first-pass draggable splitters into reusable dock/window GUI
  primitives with persisted layouts, keyboard accessibility, and true resize
  cursors
- build proper editor frames/windows/views next: Scene/Game, Software/Tool,
  Self-Iteration Sandbox, AI Visualizer, ProjectLauncher, and Build/Output
  should be independently focusable/dockable surfaces using shared GUI controls
- continue modular GUI foundation work: tab bars, scrollable/selectable context
  panels, modal/focus overlays, context menus, popouts, dockable/editor windows,
  draggable splitters, resize handles, and column controls must be common engine
  GUI primitives rather than per-pane hacks
- enforce the GUI library boundary: new tabs, dropdowns, window chrome,
  package-manager controls, scripting views, Intelligence surfaces, and System Info
  panels land in `engine.gui` primitives/layout first, with editor workspaces
  composing them afterward
- replace file-type asset cards with decoded image/model thumbnails and make the
  project browser grow into a real bounded file/folder panel with rename/move,
  text editing, filtering, and safer script authoring controls
- design borderless linked-context popouts as explicit operator-controlled
  editor windows for GUI containers, not hidden always-on backends. Each popout
  must own focus, z-order, teardown, redock, and evidence logging before it can
  become part of normal AI/editor operation.

### 3. System Info And Time Spine

- deepen pacing diagnostics, perf-select guidance, and hardware guidance in the
  System Info workspace
- surface the renderer feature matrix in System Info as present/partial/missing
  backend capability status before claiming new renderer features complete
- keep compiler/language/CI validation status visible in System Info so build
  confidence stays tied to the live editor surface
- use that build-confidence baseline to feed the AI workspace with current
  build logs/output before task packets are promoted toward Phase 5 replay
- build on the graph surfaces that already exist instead of replacing them with
  another temporary debug-only panel
- continue carrying the shared time-system spine deeper into runtime and scene
  ownership
- keep Video wired to `core.time`, streaming-save config, scene
  snapshot/serializer contracts, and the bottom scene timeline strip without
  pretending full replay restore or video editing is already shipped
- keep the engine contract self-test expanded with every new timeline,
  Forest Factory, package, input, and scene-persistence contract before those
  contracts are promoted into project-generation or OS-model workflows
- prefer `--engine-contract-self-test` for build-safe agent churn when runtime
  launches are not explicitly approved; reserve `--engine-validation-self-test`
  for the heavier materialize/build/child-runtime/AI gate proof path
- keep backend ownership explicit inside live tooling surfaces

### 4. Asset, Build, And Packaging Discipline

- keep one canonical asset resolver rooted from the executable path with:
  - explicit override
  - resolved root discovery
  - hard fail with diagnostics
- keep Visual Studio, repo-root CMake, and packaged runtime path behavior
  aligned
- keep generated Windows child project files in linker parity with the editor
  app target. The current project backend switch path is runtime-driven through
  `--backend`, so child `.vcxproj` files must carry `RAYLIB_DLL`, `raylib.lib`,
  SDL3 static Windows system libs, and no SFML static/dynamic mixture before
  Raylib, Vulkan, or other single-context project launches can be trusted.
- keep the project-script compiler honest across normal Windows developer
  environments instead of assuming one lucky `clang++` path is always present
- finish the low-risk include/src cleanup and module-aware source grouping
- keep `Engine/resource/` as the canonical Win32 build-resource root until a
  broader resource restructure is landed safely; do not strand `icon.ico`
  behind ad hoc relative paths while the baseline build stays active
- continue syncing filesystem, CMake, `.vcxproj`, `.vcxitems`, and `.filters`
  so disk truth and IDE truth stay aligned
- harden install/package expectations so runtime assets, shaders, scripts, and
  logs resolve correctly outside the repo too

### 5. UI, Drag/Drop, And Editor Maturity

- add repeatable typed-text editor smokes so input regressions stop hiding
  behind screenshots and click-only probes
- replace remaining ad hoc editor-only layout logic with stronger shared GUI
  ownership
- finish drag/drop and docking/popup behavior as first-class editor systems,
  not per-backend patches
- keep GUI menu rendering in the OpenGL flicker repro matrix; menu interaction
  remains an acceptance test even after the OpenGL top-layer replay-only fix
  because dropdowns, modals, and command windows must stay scene-over without
  slow flip/flicker regressions. Windows Release operator eye-test confirmed
  the first replay-only command-menu fix as stable.
- improve project, script, AI, systems, and output surfaces until the shell
  reads as a professional editor rather than a debug console
- keep the new script/file/asset workspaces usable as visible AI iteration
  evidence surfaces: scripts should be addable/editable, assets should be
  browseable with thumbnails, and project notes/logs should explain what changed
- continue replacing full-width placeholder button rows with proper bounded
  widgets: scroll views, resize handles, hover/click states, clipping, and
  selectable text must work before the AI workspace can be considered usable.
- keep the Self-Iteration Sandbox editor-native until runtime surfaces are
  stable enough to be optional decoration, not the only way to operate the loop
- keep AI workspace domains organized around concrete jobs:
  self-iteration sandbox, editor tool harness, normal engine assistant,
  ProjectLauncher artifacts, training promotion, and operator instructions
- keep model discovery and model activation separated in the GUI: discovered
  names can appear only as selectable options, never as the active model until
  the operator chooses one
- keep launcher and editor theming intentionally separate

### 6. Self-Iteration, Training, And Review Loop

- standardize the full OS AI architecture:
  an engine-owned OS-model harness, local MCP/control/tool harnesses, and
  selected Nemotron 3 Nano/Qwen model lanes, with the self-iteration
  sandbox separated from normal ProjectLauncher/editor scene authoring
- build on the current iteration-packet/capture roots already present in the
  editor instead of inventing a second AI staging path
- make staged packets the first durable handoff between planner/executor work
  and builder/verifier review, including loop-stage and gate-state metadata
- keep continuous AI builds nonblocking and single-flight so the engine can
  produce fresh evidence while preserving explicit review gates
- keep project evidence repair available in the Self-Iteration Sandbox domain
  so Phase 5 work can recover from missing generated shells without leaving the
  editor
- keep the central Intelligence surface and Inspector actions synchronized. The
  bottom Console Dock remains status/log/visual feedback, while the Inspector is
  a quick-command/details pane and the central Intelligence workspace is the discoverable
  operator surface until dedicated AI editor windows land.
- promote only staged packets that include root-resolved project/build/output
  evidence; cwd-dependent evidence is considered invalid
- keep the self-iteration lane branded as engine upgrade work, not as a normal
  generated game/tool project. The compatibility id may remain `sandbox`, but
  generated child artifacts should present as `EpochEngine` and should surface
  update/build evidence in the editor.
- expose OS AI control/status in normal editor chrome as well as the central
  AI workspace. The World Outliner `OS AI` tab owns compact chat,
  prompt, plan, and status controls; future tabs should add memory, tool, and
  evaluation views without hiding them in Console Dock output.
- reject local-model replies that contain only hidden reasoning. The OS AI path must
  show final `content` or fail visibly as a model/API configuration issue; hidden
  `reasoning_content` is not an assistant answer and must not be promoted into
  MCP capture, raw local training capture, or curated training data.
- train from real editor tool actions by capturing before/after state from the
  selected script harness before promoting any dataset/eval records
- reject generic OS AI self-status answers unless they cite tool/build/scene
  evidence paths or visible state changes
- grow the sandbox scene-training lane into a watchable 3D edit/test runner
  where OS AI can learn from object edits, scene-state diffs, and verifier
  output without mutating normal game/editor projects by accident
- keep local model activation operator-gated; no first-detected model fallback,
  no hidden helper identity, and no chat/tool execution before selection
- allow CLI/self-iteration runs to record an explicit operator-selected helper
  model through environment configuration, while keeping model discovery and
  activation separate in the GUI and rejecting first-detected-model fallback
- keep bypass-capable runtime activation operator-gated: local game/tool tests
  can run through visible editor/MCP/harness controls, but apps or servers that
  expose model-accessible control surfaces, listeners, ports, or serving modes
  must require an explicit human enable/run action
- implement OS AI as a closed-loop agentic cognition system, not a stateless
  chatbot. Minimum architecture: base model, working memory, persistent
  semantic/episodic/procedural memory, retrieval/ranking, goal stack, planner,
  tool executor, verifier, scoring/reward, self-state tracker, attention
  controller, and real-time observe/update/retrieve/plan/act/verify/commit loop.
- treat compiler errors, runtime logs, screenshots, file state, user
  corrections, tool results, and evals as reality pressure. No evidence means no
  belief, no training promotion, and no "working fine" status claim.
- reject helper-model self-iteration drafts that do not cite packet/build/output
  evidence, verifier results, or eval gates; weak local model output can guide a
  supervised pass but cannot promote itself into training data or source changes.
- keep `--editor-ai-gate-self-test` green as the executable version of that
  helper-review rule. It must reject status-only "working fine" replies,
  missing-verifier evidence, bypass/server requests, and any review that tries
  to promote without human approval.
- keep `--engine-validation-self-test` green as the broader non-GUI harness for
  registered project profiles plus the OS AI evidence gate. It is the minimum
  command-line proof before claiming project-run, generated-shell, or AI-gate
  stability for a source push.
- treat `Engine/ai/control/continuous_build_loop.json` as the current contract
  for the engine self-iteration control loop until a replay runner can enforce it
- use MCP tool schemas as the canonical tool-bus contract and replay shape
- separate raw observation capture from curated dataset/eval promotion
- require build/runtime/log evidence before AI-assisted promotion
- grow toward a real
  `planner -> executor -> builder -> verifier -> gate`
  loop without drifting into blind autonomy claims
- keep committed AI assets in `Engine/ai/` and local/generated artifacts in
  `Engine/examples/ConsoleApplication1/workspace/ai/`

### 7. Procedural World, Primitives, And Object Systems

- complete the primitive/object system as a real engine-owned authoring/runtime
  path
- align primitive/material/light growth with the renderer feature matrix:
  formal materials, multiple lights, render targets, model import, normal maps,
  cubemaps, shadows, and instancing should land as engine-facing feature
  families instead of one-off preview hacks
- build from the current ambient-solid primitive preview baseline toward true
  ECS-owned cube/light/material components instead of falling back to abstract
  helper glyphs
- turn the new scene click/drag path into a real transform gizmo and persist
  transform edits through the project/ECS scene-authoring source of truth
- keep the live project shell honest by surfacing current seed-object,
  archetype, and category proof directly in-editor while the fuller object
  runtime is still being built out
- build a modular procedural world path on top of the time/node direction
- keep ECS/entity ownership for macro gameplay actors while dense cellular or
  material simulation remains specialized
- keep the six-month 2D lane as a vertical slice through the real engine spines
  rather than a separate subsystem island

### 8. Android-First Mobile Bring-Up

- treat Android as the first mobile platform
- do not spend roadmap energy pretending macOS is the next platform priority
- start from one honest single-context runtime path:
  one backend, one window, one input path, one packaging/install story
- keep mobile packaging on the same executable-root asset discipline rather
  than introducing cwd-dependent mobile exceptions
- prioritize touch/input, lifecycle stability, packaging/install, and one
  stable mobile renderer path over broad backend count
- document exactly what works, what is partial, and what is still missing

## Current Push Order

1. Preserve the `v0.87.09` editor/resource checkpoint: Raylib, SDL, SFML,
   Vulkan, OpenGL, and DirectX must keep real panes, visible scene previews,
   Inspector, AI Chat, stable GUI-over-scene composition, curated command menus,
   and modal top-layer behavior. Any remaining mismatched clear/color/depth,
   graph, or context-opacity behavior must be captured and fixed or explicitly
   deferred with proof.
2. Keep GitHub/workflow reliability and local/hosted build truth aligned after
   the headless plus Linux Clang engine split.
3. Move Phase 5 to the front: implement the smallest real OS AI closed-loop
   control slice using the current sandbox/evidence paths. Required parts are
   working memory, staged goal packet, visible executor action, verifier
   evidence, score/gate result, notes update, and no hidden autonomy.
4. Strengthen the System Info workspace with deeper pacing diagnostics and
   backend convergence guidance, including present/partial/missing renderer
   feature status from the feature matrix, backend-native mesh/model allocation
   proof, sampled-RTT descriptor contract, build graph proof, hook readiness,
   live native allocation readiness, no-runtime refusal guards, presentation
   proof, and mini-arcade graph parity.
5. Carry the time spine deeper into runtime and scene ownership.
6. Keep UI/editor maturity moving forward, especially text/input reliability,
   shell polish, drag/drop, and backend-window stability.
7. Complete the primitive/object system and keep it aligned with the project
   runtime shell.
8. Replace metadata-only `.epoch` scene shells with real project-owned
   scene loading, editing, saving, and play/runtime handoff.
9. Start Android with an honest single-context bring-up, touch/input
   integration, packaging/install path, and asset-resolution discipline.
10. Promote the first-pass file browser, script starter creator, and asset cards
   into professional bounded editor controls with decoded thumbnails and
   editable script/source panes.
11. Promote the Package Manager modal from local `engine_arcade` runtime-minis
    into a reviewable package workflow for local and downloadable source
    packages, with explicit human approval before build/run and no auto-created
    servers or hidden model-accessible channels.
    The active UI direction is a reusable list/action/detail surface, not a
    combo-box-only modal: packages should show status, provenance, install or
    remove actions, review-gate state, and bounded progress without bleeding into
    the scene or Console Dock.
    Network/server packages must keep the same boundary: shared network runtime
    contracts may be inert engine capabilities, optional authoritative
    dedicated headless server support must be a deliberate project choice, and
    client listen/nondedicated or future client-predicted competitive paths must
    stay separate opt-in packages so software and single-player outputs do not
    inherit unnecessary bloat or attack surface.
    Bulky package source belongs in
    `https://github.com/Autodidac/EpochEngineExtensions`; EpochEngine mainline
    keeps descriptors, security gates, updater/cache paths, and stable API
    contracts while downloaded/generated payloads land under
    executable-local `cache/packages/`.
12. Continue safe include/src restructuring and MSVC/CMake synchronization
   whenever touched areas can be normalized without collateral damage.

## Acceptance Gates

- The editor runs real projects/scenes instead of sample-launch illusions.
- Scene/world files load, save, and drive preview/runtime state instead of
  acting as metadata-only placeholders.
- The launcher remains project/context/update focused instead of collapsing back
  into a fake demo shell.
- The System Info workspace shows real render/backend/context graph and tooling
  diagnostics. Time controls, pacing diagnostics, playhead state, and
  streaming-save cadence belong to Video.
- Renderer feature support is tracked through the feature matrix and only
  marked complete after backend-specific validation or an explicit deferral
  note.
- The engine owns one shared simulation clock and exposes real time controls
  through Video and the bottom scene timeline strip.
- Multicontext proof stays honest:
  all six panes are real, detached shells behave like real top-level windows,
  and helper hosts do not linger incorrectly.
- Software fallback proof stays honest: safe-launch/error/debug UI can run when
  GPU paths fail, but software is not advertised as the long-term Windows
  production renderer once Direct3D is promoted.
- DirectX/D3D11 is advertised only as a first-pass active Windows backend until
  it grows the full renderer resource/material/render-target API. D3D12 remains
  unpromoted until a separate Windows build proves device/context/swapchain,
  clear/present, shaders/resources, and editor screenshot gates.
- Asset, shader, script, log, and workspace resolution work from executable
  path instead of working-directory luck.
- Visual Studio, repo-root CMake, and CI stay aligned closely enough that file
  moves do not create phantom build truth.
- Packaged Windows and Linux releases boot the intended runtime identity by
  default and remain version-aligned with tagged source.
- Android bring-up starts from one honest runtime path instead of a speculative
  feature matrix.
- Docs stay strong enough that future automated passes can follow the build,
  launch, test, capture, commit, and push loop without rediscovering the
  architecture from scratch.
- Self-iteration has visible status surfaces and clear operator controls before
  any automated promotion path is trusted.

## Reference Inputs

- `README.md`
- `Engine/docs/`
- staged research under
  `Engine/examples/ConsoleApplication1/workspace/research/`
- release/changelog history under `Changes/`
- external utility projects such as Botface live in their own repos now; import
  only reviewed extracts into Epoch, never entire sibling-tool worktrees by
  default
