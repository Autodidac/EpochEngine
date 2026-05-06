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
5. Parented multicontext UI must expose one honest pane per backend. Nested
   backend child windows remain implementation detail.
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
  - `Engine/modules/aengine.version.ixx`
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
  OpenGL, software renderer, and SFML validated as build-time backends.
- CMake is authoritative for cross-platform builds and project-wide presets.
- Visual Studio project/filter files must not drift from filesystem and CMake
  whenever files are moved or added.
- `CMakePresets.json` is the shared project preset layer.
- `CMakeUserPresets.json` is for local developer overrides and must stay
  ignored.

### Backend Roles

- OpenGL is the stable editor/runtime GPU backend for now.
- The software renderer is the fallback and headless-validation backend.
- Vulkan remains the future explicit graphics backend until runtime support is
  fully stabilized.
- Raylib, SDL, and SFML remain context/backend compatibility and validation
  lanes, especially for docking, popout, and backend ownership checks.

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
- Promote the first AI loop visualizer into a dedicated surface with packet
  replay, scene-state diffs, and eventually direct 3D model/weight views.
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
- a real project/scripts dock with build, run, and diagnostics surfaces
- project-local script stub creation from the Scripts workspace, with new stubs
  written under the active project's `scripts/` folder and surfaced in project
  notes for build/run evidence
- a shallow active-project file/folder browser that skips generated build/bin/.vs
  output and lets project scripts be selected without leaving the editor
- an `Assets` workspace tab with first-pass file-type thumbnail cards for active
  scene, demo model, image, audio, text, and project asset paths. Full decoded
  image/model thumbnail previews are still future work.
- scene preview now receives typed primitive preview data, so seed objects and
  newly created Cube/Light/Spawn entities render as ambient-colored solid
  primitives with wire outlines while the fuller ECS/material/object runtime is
  still being built out
- a live Systems workspace with graph surfaces, backend ownership visibility,
  first time-control diagnostics, and build-confidence/feature-probe status
  already on-screen
- the Systems workspace now mirrors Phase 5 self-iteration evidence, including
  watcher/build/tool status, staged packet count/root, and the
  planner/executor/builder/verifier/gate contract beside build confidence
- `addons/` is ignored as a local/offline staging area, and the AI control
  contract now allows local game/tool/app/server artifact generation while
  forbidding bypass-capable app/server launch, listener creation, port binding,
  hidden control surfaces, or model-accessible services without a human
  enable/run action
- a temporary bottom Console Dock split into Project, Scripts, Assets, Systems,
  AI, and Output evidence tabs. This is not the final editor-window system; it
  is the current log/evidence strip until separate editor frames/views are
  promoted.
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
- an editor-native Self-Iteration Sandbox panel that summarizes evidence
  readiness, active planner/builder/verifier/gate state, continuous build
  status, and tool-harness activity without depending on runtime surface/atlas
  packing
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

## Phase Progress

- [x] Phase 1: Repository structure, research intake, workspace placement,
  initial build-path honesty, and documentation cleanup.
- [x] Phase 2: Project-centric runtime shell, generated project creation,
  project/script proof rows, launcher/editor separation, and baseline
  Project/Scripts workspace flow.
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

- keep SDL, SFML, Raylib, Vulkan, OpenGL, and software behavior converging
  instead of drifting into backend-specific hacks
- finish IDE-class docking/popout behavior so detach, input ownership, z-order,
  redock, and startup presentation remain stable
- eliminate remaining OpenGL flicker in both single-context and multicontext
  modes; menu open/close activity and software-context interaction are known
  repro amplifiers and should be investigated before cosmetic-only fixes
- keep maximize/restore in the renderer-windowing repro matrix; maximize can
  still crash the editor and must be fixed before claiming docking stability
- fix the Raylib redock crash that can still bring down the parent editor
  process during backend-window docking tests
- tighten terminology so runtime/module/doc names stop leaning on ambiguous
  legacy words like `multiplexer`

### 2. Project-Centric Runtime And Scripted Pipeline

- keep project creation, project play, script build/run, and generated project
  discovery truthful
- keep the default demo path honest:
  generated or discovered projects should build, launch, and hand any
  declared demo model through the engine-owned script host without falling
  back to confusing sample-only behavior
- keep the Mini Sponza demo owned by `ProjectLauncher`, while the
  Self-Iteration Sandbox stays the AI/engine-iteration shell
- continue replacing hardcoded built-in sample assumptions with project-owned
  runtime flow
- keep the already-landed project/scripts shell honest instead of letting it
  drift back toward placeholder tooling
- keep the launcher centered on projects, contexts, settings, and updates
- keep generated project `PROJECT_NOTES.md` visible from the Project workspace
  so scripted/project/AI actions leave a readable synopsis and usage trail
- keep generated Sandbox/ProjectLauncher child builds anchored to current VS
  2022 `v143` toolset metadata in both generated files and the checked-in
  engine projects those generated files reference.
- keep the bottom dock centered on `Project`, `Scripts`, `Assets`, `Systems`,
  `AI`, and `Output` as evidence/status tabs, not as the final
  scene/editor-window model
- replace the temporary console/chat column buttons with a real draggable
  resize-column control once the GUI input model has stable splitters
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
- keep the Inspector copy of AI repair/build/watcher controls as the primary
  operator command surface; the bottom Console Dock is status/log/visual
  feedback until dedicated AI and editor windows land
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

1. Keep GitHub/workflow reliability and local/hosted build truth aligned after
   the headless plus Linux Clang engine split.
2. Strengthen the Systems workspace with deeper pacing diagnostics and backend
   convergence guidance.
3. Carry the time spine deeper into runtime and scene ownership.
4. Tighten the AI capture, replay, review, and promotion loop until
   Phase 5 can run from staged packets with builder/verifier gates and visible
   script/file/asset evidence inside the editor.
5. Keep UI/editor maturity moving forward, especially text/input reliability,
   shell polish, drag/drop, and backend-window stability.
6. Complete the primitive/object system and keep it aligned with the project
   runtime shell.
7. Replace metadata-only `.epoch` scene shells with real project-owned
   scene loading, editing, saving, and play/runtime handoff.
8. Start Android with an honest single-context bring-up, touch/input
   integration, packaging/install path, and asset-resolution discipline.
9. Promote the first-pass file browser, script stub creator, and asset cards
   into professional bounded editor controls with decoded thumbnails and
   editable script/source panes.
10. Continue safe include/src restructuring and MSVC/CMake synchronization
   whenever touched areas can be normalized without collateral damage.

## Acceptance Gates

- The editor runs real projects/scenes instead of sample-launch illusions.
- Scene/world files load, save, and drive preview/runtime state instead of
  acting as metadata-only placeholders.
- The launcher remains project/context/update focused instead of collapsing back
  into a fake demo shell.
- The Systems workspace shows real graph/tooling surfaces plus time
  diagnostics.
- The engine owns one shared simulation clock and exposes real time controls.
- Multicontext proof stays honest:
  all six panes are real, detached shells behave like real top-level windows,
  and helper hosts do not linger incorrectly.
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
