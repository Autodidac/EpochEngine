# Epoch Roadmap

## Mission

Build Epoch into one professional, engine-owned runtime and editor shell for:

- project creation, editing, play, scripting, tooling, and updates
- renderer and systems tooling that stay honest across backends
- engine-owned GUI/text/input instead of middleware-owned editor behavior
- a staged engine AI/self-iteration loop that stays reviewable and evidence-gated
- packaging and runtime rules that hold across desktop first, then Android

## Non-Negotiable Rules

1. The engine owns the workflow. Projects, scripts, systems, AI, updates, and
   runtime all travel through one spine.
2. Commit only stable, verified changes. Do not move the branch forward with
   speculative or half-validated runtime/build states.
3. Validation must come from asset-bearing outputs and clean up after itself.
4. Packaged/runtime path logic must resolve from the executable path first, not
   the working directory.
5. Multicontext UI must converge toward first-class individual context panes,
   like an IDE/MSVC-style tool shell where each visible surface has one clear
   owner. Nested backend child windows remain backend-specific implementation
   detail, not the user-facing model.
6. Broad hardware support stays the default. Heavy features remain tiered or
   opt-in.
7. Research imports are staged first, reviewed second, and promoted only when
   they materially improve repo truth.
8. Build/tooling floors must stay honest. Preserve baseline compatibility where
   possible, and document the real split when newer CMake/module support is
   required.
9. External local LLM endpoints are explicitly selected tooling providers, not
   hidden authority and not an auto-selected default.
10. The engine AI architecture keeps three distinct internal pieces:
    - `EpochBot`, the primary engine-owned trainable LLM
    - local MCP/control/tool harnesses that operate the editor and collect proof
    - an offline/injectable OSS or tiny backup LLM path for fallback, generated
      software embedding, and EpochBot training support
11. AI may generate local game, tool, app, and server project artifacts only
    through visible, reviewable requests. It must not create or run apps/services
    that provide model bypass channels, self-accessible servers, hidden control
    surfaces, listener creation, port binding, or network-serving mode
    activation without an explicit human enable/run action.
12. Local game/tool tests through approved editor/MCP/harness controls are
    allowed when visible, evidence-captured, and not exposing a new
    model-accessible network/control surface.
13. `addons/` is local/offline by default. Treat it as staged source material
    for future review, not as online repo content.

## Release And Source Policy

- Public docs must always distinguish:
  - current development source
  - published stable runtime release
  - bootstrap updater-shell release when one exists
- Packaged runtime assets use versioned platform names:
  - `epoch_win10_x64_vX.Y.Z.zip`
  - `epoch_linux_x64_vX.Y.Z.tar.gz`
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
- Linux/GCC is currently a headless validation lane by default because GCC 14
  can ICE while writing full-engine C++ module BMIs; full Linux editor/runtime
  builds should use Clang until GCC module support stabilizes.
- Linux/Clang 18 is the current full-engine Linux rendering build lane, with
  OpenGL, software renderer, and SFML validated as build-time backends. DirectX
  remains explicitly Windows-only and must stay disabled for Linux/WSL presets.
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
  basic shader preview path, and GUI replay path, but still needs deeper
  renderer-resource parity and a real module/source split before it is treated
  as feature-complete. D3D12 stays a future explicit renderer track.
- Vulkan remains the future explicit cross-platform graphics backend until
  runtime support is fully stabilized.
- Raylib, SDL, and SFML remain context/backend compatibility and validation
  lanes, especially for docking, popout, and backend ownership checks.

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
- Generated/cache atlases must not pollute source directories.
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
  `Engine/examples/ConsoleApplication1/atlases`, generated dump output under
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
- project-local script stub creation from the Assets/project surface, with new
  stubs written under the active project's `scripts/` folder and surfaced in
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
- sandbox scene-training packets can now be staged from the editor so EpochBot
  has an explicit, watchable 3D edit/test learning lane instead of answering
  that it is "working fine" without evidence
- the scene viewport now has first-pass object interaction: visible editor
  primitives can be click-selected and left-dragged while empty scene space
  still supports camera pan/orbit/zoom
- console-dock text rendering now avoids submitting partially clipped glyph
  atlas quads through the current sprite path, preventing the stretched vertical
  smear artifacts seen while scrolling tall AI/path rows
- GUI buttons now capture on press and fire on release, so launcher/editor
  actions happen after the pressed visual state instead of racing it
- project selection no longer silently creates or rewrites project shells; File
  > Save Project, Project > Save Active Project, and the centered Run button are
  the explicit operator actions that materialize/update generated project files
- the top scene command strip now has one centered Run action. It runs the
  selected script when a script asset is active, otherwise it saves and rebuilds
  the active project before launching the generated output. A failed build now
  cancels launch instead of falling through to stale child executables.
- generated Sandbox and ProjectLauncher shells expose `--project-self-test` so
  child project output can be verified without launching GUI windows
- the checked-in engine now exposes `--editor-project-self-test <id>` so Sandbox
  and ProjectLauncher shells can be materialized and built from the real engine
  before their generated child `--project-self-test` paths are run
- `v0.84.30` extends that self-test route into the AI evidence loop: the engine
  now appends MCP-style tool captures and stages review-gated iteration packets
  for Sandbox and ProjectLauncher, giving EpochBot a real materialize -> build
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
  primitive survives projection. Remaining DirectX work is renderer-resource
  parity, real depth/resource ownership, the module/source split, and any
  operator-observed GUI flicker in the D3D11 pane, not basic context creation.
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
- split the DirectX implementation into the same kind of owned module/source
  surfaces as the mature backends after the first-pass D3D11 behavior is stable;
  do not add empty placeholder DirectX modules just to make the tree look even
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
- keep engine-owned runtime-mini packages first-class: local package metadata
  belongs in project assets, script bridges call into engine-owned scenes, and
  downloadable packages must use explicit updater-style source build/approval
  instead of hidden auto-execution.
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
- replace file-type asset cards with decoded image/model thumbnails and make the
  project browser grow into a real bounded file/folder panel with rename/move,
  text editing, filtering, and safer script authoring controls
- design borderless linked-context popouts as explicit operator-controlled
  editor windows for GUI containers, not hidden always-on backends. Each popout
  must own focus, z-order, teardown, redock, and evidence logging before it can
  become part of normal AI/editor operation.

### 3. Systems Workspace And Time Spine

- deepen pacing diagnostics, perf-select guidance, and hardware guidance in the
  Systems workspace
- surface the renderer feature matrix in Systems as present/partial/missing
  backend capability status before claiming new renderer features complete
- keep compiler/language/CI validation status visible in Systems so build
  confidence stays tied to the live editor surface
- use that build-confidence baseline to feed the AI workspace with current
  build logs/output before task packets are promoted toward Phase 5 replay
- build on the graph/time surfaces that already exist instead of replacing them
  with another temporary debug-only panel
- continue carrying the shared time-system spine deeper into runtime and scene
  ownership
- add replay/timeline hook points without pretending the full replay stack is
  already shipped
- keep backend ownership explicit inside live tooling surfaces

### 4. Asset, Build, And Packaging Discipline

- keep one canonical asset resolver rooted from the executable path with:
  - explicit override
  - resolved root discovery
  - hard fail with diagnostics
- keep Visual Studio, repo-root CMake, and packaged runtime path behavior
  aligned
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
  currently makes the issue easier to trigger and should stay documented until
  root cause is fixed
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

- standardize the full engine AI architecture:
  `EpochBot`, local MCP/control/tool harnesses, and the offline/injectable
  backup LLM path, with the self-iteration sandbox separated from normal
  ProjectLauncher/editor scene authoring
- build on the current iteration-packet/capture roots already present in the
  editor instead of inventing a second AI staging path
- make staged packets the first durable handoff between planner/executor work
  and builder/verifier review, including loop-stage and gate-state metadata
- keep continuous AI builds nonblocking and single-flight so the engine can
  produce fresh evidence while preserving explicit review gates
- keep project evidence repair available in the Self-Iteration Sandbox domain
  so Phase 5 work can recover from missing generated shells without leaving the
  editor
- keep the central AI Control Surface and Inspector actions synchronized. The
  bottom Console Dock remains status/log/visual feedback, while the Inspector is
  a quick-command/details pane and the central AI Sandbox is the discoverable
  operator surface until dedicated AI editor windows land.
- promote only staged packets that include root-resolved project/build/output
  evidence; cwd-dependent evidence is considered invalid
- train from real editor tool actions by capturing before/after state from the
  selected script harness before promoting any dataset/eval records
- reject generic EpochBot self-status answers unless they cite tool/build/scene
  evidence paths or visible state changes
- grow the sandbox scene-training lane into a watchable 3D edit/test runner
  where EpochBot can learn from object edits, scene-state diffs, and verifier
  output without mutating normal game/editor projects by accident
- keep local model activation operator-gated; no first-detected model fallback,
  no hidden helper identity, and no chat/tool execution before selection
- keep bypass-capable runtime activation operator-gated: local game/tool tests
  can run through visible editor/MCP/harness controls, but apps or servers that
  expose model-accessible control surfaces, listeners, ports, or serving modes
  must require an explicit human enable/run action
- implement EpochBot as a closed-loop agentic cognition system, not a stateless
  chatbot. Minimum architecture: base model, working memory, persistent
  semantic/episodic/procedural memory, retrieval/ranking, goal stack, planner,
  tool executor, verifier, scoring/reward, self-state tracker, attention
  controller, and real-time observe/update/retrieve/plan/act/verify/commit loop.
- treat compiler errors, runtime logs, screenshots, file state, user
  corrections, tool results, and evals as reality pressure. No evidence means no
  belief, no training promotion, and no "working fine" status claim.
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

1. Preserve the `v0.84.35` multicontext checkpoint: Raylib, SDL, SFML, Vulkan,
   OpenGL, and DirectX must keep real panes, visible scene previews, Inspector,
   AI Chat, and stable GUI-over-scene composition. DirectX launcher bleed-through
   is guarded by the scene-preview gate, and angle-dependent DirectX primitive
   pairing artifacts are guarded by whole-primitive clipping. Any remaining
   mismatched clear/color/depth behavior must be captured and fixed or
   explicitly deferred with proof.
2. Keep GitHub/workflow reliability and local/hosted build truth aligned after
   the headless plus Linux Clang engine split.
3. Move Phase 5 to the front: implement the smallest real EpochBot closed-loop
   control slice using the current sandbox/evidence paths. Required parts are
   working memory, staged goal packet, visible executor action, verifier
   evidence, score/gate result, notes update, and no hidden autonomy.
4. Strengthen the Systems workspace with deeper pacing diagnostics and backend
   convergence guidance, including present/partial/missing renderer feature
   status from the feature matrix.
5. Carry the time spine deeper into runtime and scene ownership.
6. Keep UI/editor maturity moving forward, especially text/input reliability,
   shell polish, drag/drop, and backend-window stability.
7. Complete the primitive/object system and keep it aligned with the project
   runtime shell.
7. Replace metadata-only `.epoch` scene shells with real project-owned
   scene loading, editing, saving, and play/runtime handoff.
8. Start Android with an honest single-context bring-up, touch/input
   integration, packaging/install path, and asset-resolution discipline.
9. Promote the first-pass file browser, script stub creator, and asset cards
   into professional bounded editor controls with decoded thumbnails and
   editable script/source panes.
10. Promote the Package Manager modal from local `engine_arcade` runtime-minis
    into a reviewable package workflow for local and downloadable source
    packages, with explicit human approval before build/run and no auto-created
    servers or hidden model-accessible channels.
11. Continue safe include/src restructuring and MSVC/CMake synchronization
   whenever touched areas can be normalized without collateral damage.

## Acceptance Gates

- The editor runs real projects/scenes instead of sample-launch illusions.
- Scene/world files load, save, and drive preview/runtime state instead of
  acting as metadata-only placeholders.
- The launcher remains project/context/update focused instead of collapsing back
  into a fake demo shell.
- The Systems workspace shows real graph/tooling surfaces plus time
  diagnostics.
- Renderer feature support is tracked through the feature matrix and only
  marked complete after backend-specific validation or an explicit deferral
  note.
- The engine owns one shared simulation clock and exposes real time controls.
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
