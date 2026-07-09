# Runtime And Editor Workflows

This guide documents the current intended workflow for running Epoch honestly:
editor, project, scripts, systems, AI, and runtime should all travel through
the same engine-owned path.

## Startup model

- enter through the normal engine bootstrap so scripting, AI, backend setup,
  logging, capture, and project/runtime selection share one path
- desktop example wiring still lives under `Engine/examples/ConsoleApplication1/`
- multicontext behavior depends on the active runtime/config macros documented
  in `../build/build_configuration_flags.md`
- the Windows parented multicontext host should fit the active desktop work area
  by default so the full context matrix remains visible on baseline hardware
- the longer-term shell default should converge toward explicit, independently
  owned context surfaces rather than a mixed hidden/proxy shell: editor favors a
  stable OpenGL scene path today, Windows now has a first DirectX/D3D11 native
  renderer path, D3D12 remains future work, software remains safe-launch/debug
  GUI fallback, backend switching is explicit, and inactive backends must be torn
  down instead of running hidden behind the active shell
- packaged Linux releases should follow that same main-runtime rule: the normal
  packaged `epoch` entry is the product path, while updater-shell mode remains
  an explicit bootstrap build instead of the default Linux release identity
- packaged updates use the source-preferred modern lane: when main source is
  newer than the running build, Update launches the local source rebuild worker
  so progress and Cancel remain visible. Versioned packaged runtime archives are
  used only when no newer source lane is available or when an explicit
  bootstrap/package-only flow owns that choice.
- the active packaged asset contract is versioned runtime archives such as
  `epoch_win10_x64_vX.Y.Z.zip` and `epoch_linux_x64_vX.Y.Z.tar.gz`
- the updater extracts the packaged version directly from the archive name
- WSL is treated as Linux for runtime package selection and should consume the
  same `epoch_linux_x64_vX.Y.Z.tar.gz` asset unless a future package layout
  proves a separate WSL asset is necessary
- source checkout installs use the same source-preferred rule: a newer main
  source snapshot drives the update worker directly, while packaged archives
  remain a verified fallback when source is not newer.
- editor update checks must also prove the matching hosted build lane before
  surfacing an update: Windows waits for `windows-msvc`, Linux waits for
  `linux-clang-engine`, and pending/failing/missing job evidence withholds the
  update affordance
- editor update checks are automatic after the editor has loaded. The modal is
  shown only for a newer current-platform update path. The smart/default path is
  source-preferred: if main source is newer, it launches the source rebuild
  worker and keeps Cancel/progress visible; if source is not newer, it may use a
  verified packaged runtime archive. Project Source Code Download remains a
  cache-only source snapshot action and does not update or restart Epoch.
- update work must stay visible while it runs. During checking, packaged
  install, and source rebuild, the toolbar/modal/output evidence should report
  elapsed time, current lane, and completion/failure state instead of leaving a
  silent background worker.
- source rebuild workers must create `epoch_update_handoff.log` as soon as they
  start, mirror major download/dependency/build stages into it, and surface
  `[ERROR]` lines as failed update evidence instead of parking the UI at a
  progress ceiling.
- starting a source rebuild worker must clear stale cancel/source/replacement
  logs before the detached process is spawned. A previous worker's final error
  or staged replacement script is not valid evidence for the new run.
- source rebuild downloads and extraction live in disposable per-run work roots
  under `cache/updates/work/`. Cancel writes the worker cancel marker and clears
  the active run when possible; if a crash leaves a run folder behind, the next
  update sweeps stale run/source/download folders before fetching fresh source.
- source rebuild workers always build the Windows `Release|x64` runtime lane,
  even when the editor was launched from `Debug|x64`. Debug update tests should
  keep Epoch open, watch the handoff/progress evidence, and only restart after
  the Release replacement executable is proven ready.
- source rebuild workers disable MSBuild node reuse and build parallelism for
  the update lane. The worker console stays hidden by default; set
  `EPOCH_UPDATER_SHOW_WORKER_CONSOLE=1` only when deliberately debugging the
  detached update script.
- managed-vcpkg source updates stage disposable overlay ports under
  `cache/updates/` when old dependency ports need modern CMake policy options;
  do not mutate the user's vcpkg checkout or mask restore failures.
- runtime-created update/package/cache data is app-local: updater work,
  temporary probes, extraction folders, and helper tools live under
  `cache/updates/`; downloaded release/source packages live under
  `cache/packages/`; generated/runtime atlases live under `cache/atlases/`.
  These folders are disposable runtime state, not public release payload and not
  tracked source.
- downloaded update packages use the release/source asset name with its version
  suffix inside `cache/packages/`. The current always-redownload policy removes
  same-URL package caches before download so recreated release assets cannot
  reuse stale bytes; invalid package caches are still deleted and redownloaded,
  and source rebuilds use fresh per-run source snapshots.
- source snapshot extraction must prove the manifest root before running vcpkg
  or MSBuild. If a downloaded GitHub archive leaves one nested top-level folder,
  the worker may repair that shape only when the nested root contains
  `Engine/vcpkg.json`; otherwise the update fails with visible evidence instead
  of continuing into a missing-manifest toolchain shutdown.
- launcher/editor mode changes and update replacement waits should use the shared
  EpochGui loading-screen/progress primitive. During an active update the
  launcher hides unrelated Start/Quit actions and exposes only the valid
  update-stage action: Cancel while the source worker can still honor it, then
  Restart after replacement evidence is ready.
- OpenGL editor composition is queue-explicit: build the normal GUI/backend
  batch before the scene, render the scene preview once, drain follow-up work,
  then replay only the explicit GUI top-layer batch for command menus and modal
  chrome. This protected order keeps command windows scene-over without
  reviving the slow scene/menu flicker.

## Project-centric runtime direction

- the editor should play the active project and scene, not a hardcoded sample
  game menu
- when no `EPOCH_EDITOR_START_WORKSPACE` override is set, the editor should
  start on a scene-backed Perspective surface; evidence-only Project/System
  surfaces may temporarily clear the scene viewport, but they must not be the
  default first-run view
- the bottom dock `Output` tab is also a scene-workbench recovery path; a user
  should not need command-line arguments to get back to the live Perspective
  viewport after inspecting project or systems evidence
- `Play Project` should reject non-project scene ids from the editor path so the
  live shell cannot quietly fall back to built-in sample launches
- built-in sample games should move behind project templates or script actions
- the long-term target is a Unity/Unreal-style project shell generated from
  duplicated engine source/layout
- that same shell must also support the static-compile path where a project
  embeds the engine directly and consumes the active engine surface from
  `Engine/include/`, `Engine/modules/`, `Engine/src/`,
  `Engine/src/scripts/`, and `Engine/resource/`
- that project shell should support both game projects and software/tool
  projects so Epoch remains a creative software platform as well as a game
  engine
- the first generated shell flow should create a real on-disk project root,
  manifest, world file, script starter, and README for both game and tool
  projects
- generated shells should land under repo-root `Projects/` so creation and
  discovery stay stable regardless of the current working directory
- generated shells should emit `project.paths.txt` so the editor log, build
  actions, and troubleshooting flow can point at concrete files on disk
- generated non-template `Projects/**/project.epoch.json` manifests should be
  discovered back into the live editor project list so the shell generation
  path immediately feeds real project selection and play
- a newly created project shell should become the active editor project instead
  of forcing the user to restart or manually stitch a second fake load path
- generated embedded-engine shells should emit a child project file, a build
  script, a build fragment, and script include fallback so the full engine
  surface stays real instead of roadmap-only promise text
- an editor/project launcher profile is valid here as a prestep for choosing
  projects, switching among live renderer contexts, updates, and future
  automation flows
- that launcher should stay flat and direct: project entry, clean editor launch,
  live context focus handoff, updates, and quit belong there; layered
  game/puzzle menus do not
- the launcher may open project demos directly or preload a project before the
  editor, but it should not drift back into multiple menu layers or become a
  fake game shell
- launcher and editor theme ownership should stay split: the launcher can keep
  its classic steel palette while the editor stays on the darker neutral tool
  palette, and any future theme selector should preserve that separation rather
  than forcing one skin across both shells
- backend ownership should stay equally explicit: the editor toolbar combobox
  reports the active backend, only claims a switch when a live target exists,
  and does not persist unavailable backend selections as fake state
- launcher context switching is not another editor or driver shell; it only
  focuses another registered live dock and reports when none exists
- `editor.scene.cpp` should own project profiles, script profiles, runtime
  scene ids, and seed entities
- `editor.cpp` should act as the live shell over that scene/project data, not
  as a second hardcoded editor universe
- default editor seed profiles should stay lean. Sandbox, Project Hub, and
  software/tool startup should keep only the workspace/root, camera, and light
  entities they need; starter cubes, grids, player starts, tray panels, fake
  tool panels, and Forest Factory floor props must be created only by explicit
  workspace/package actions or real scene data.
- current `.epoch` scene/world files are metadata shells only. They must exist
  and be surfaced as evidence, but the live preview/runtime object list is still
  driven by `editor.scene.cpp` seed entities until scene-file loading,
  serialization, and project-owned scene authoring are wired end-to-end
- repo-root `Projects/` is a generated local-project area. The editor can use
  project manifests, build logs, output paths, and `PROJECT_NOTES.md` there as
  evidence, but those files are not automatically promoted into tracked source

## Scripting and reload workflow

- script sources belong to the engine/project scripting tree used by the active
  project
- engine-owned compiled scripting means project/game logic compiles with the
  engine/project build; it is not a text-macro or string-eval layer
- the scripting/project phase must account for both integration modes:
  duplicated engine-source projects and embedded-engine builds that include the
  engine surface from `Engine/include/`, `Engine/modules/`, `Engine/src/`,
  `Engine/src/scripts/`, and `Engine/resource/`
- the project/assets dock should expose script lists, source paths, run/build
  actions, and compile/load diagnostics
- the `Assets` workspace now owns script visibility: it creates project-local
  `.ascript.cpp` stubs, lists project and engine script files, and exposes a
  shallow active-project file/folder browser so scripts can be selected without
  command-line digging
- the central `Assets` surface is intentionally not the Sandbox and not the
  primary script-control panel; it is the project asset browser shell while the
  bottom Assets dock temporarily exposes file/script detail until the thumbnail
  grid and bounded file tree land
- script source resolution should prefer the active project's local `scripts/`
  folder before falling back to template or engine-owned script roots, so the
  dock and editor run actions operate on the real generated project shell
- script starter creation should append `PROJECT_NOTES.md` entries; a useful
  Sandbox iteration must leave at least one of: build log output, script-host
  log output, staged packet evidence, selected project file path, or project
  notes explaining what changed
- build diagnostics should now cover the generated child-project build path too:
  entry source, generated project file, build script, build log, and expected
  output executable should all be visible from the Project workspace
- generated child projects, including the Sandbox shell, should repair stale
  Windows toolset metadata to `v143` before invoking MSBuild, and the checked-in
  engine projects they reference should stay on the same VS 2022 toolset.
- generated child projects must also repair their Windows app linker surface to
  match the editor target: `RAYLIB_DLL`, `raylib.lib`, SDL3 static Windows
  system libraries, and no mixed SFML static/dynamic library set. Backend
  selection stays runtime-driven through `--backend`, not a project rewrite.
- generated child projects expose `--project-self-test` so Sandbox and
  ProjectLauncher output can be verified without opening GUI windows.
- the checked-in engine exposes `--editor-project-self-test <id>` for the same
  route from the real engine binary. Use `sandbox` for the self-iteration shell
  and `projectlauncher` for the launcher shell before running the generated
  child `--project-self-test`. The route now materializes, builds, runs the
  generated child `--project-self-test`, stages a packet, appends tool capture,
  and writes project notes. If the pass should bind to a local helper model, set
  `EPOCH_AI_MODEL` or a compatible explicit model variable before launch;
  discovery still remains separate from activation.
- `--editor-ai-gate-self-test` runs the deterministic helper-review gate without
  launching the GUI. Use it before letting helper LLM replies influence curated
  training, eval promotion, or source-change planning.
- `--engine-contract-self-test` runs only the pure Forest Factory, package
  registry/model-gate, timeline streaming-save, input profile, and scene
  snapshot/serializer checks, then exits before project-profile builds, child
  runtimes, updater work, OS-AI gates, or renderer startup. Use this as the safe
  fast contract check when GUI/runtime validation is not explicitly approved.
- `--engine-validation-self-test` first runs the pure engine contract lane for
  Forest Factory, package registry/model-gate, timeline streaming-save, input
  profile, and scene snapshot/serializer behavior, then runs every registered
  editor project profile through the engine-side project self-test route, then
  runs the OS AI gate. The contract lane is build-safe, but the full validation
  command remains operator-gated in this worktree because project self-tests can
  create child processes and touch
  renderer/runtime state.
- generated game project shells can carry the `engine_arcade` local
  runtime-mini package. The package is a project asset/script option that
  invokes engine-owned mini-runtime scenes such as Snake/Tetris/Pacman through
  the script host; it must not copy those implementations out of the kernel
  engine. The package also records the shared `engine_arcade.screen` 512x512
  sampled render target so future arcade cabinets and in-game terminals can
  bind the same renderer-owned surface instead of relying on project-local
  ad hoc textures. Installing or reinstalling Engine Arcade activates visible
  editor state immediately: the default arcade scene is selected, the `3D Scene`
  workspace receives a render-to-texture screen/cabinet proof, and the Run target
  honors recognized arcade scene ids before falling back to `project:<id>`.
  Removing the package clears the arcade preview entities and active arcade
  runtime scene so other package workspaces can take over cleanly.
- Plant Lab is the editor-facing core vegetation workspace backed by the
  Forest Factory descriptor lane, not a loose optional dump. The top editor
  workspace row owns the `Plant Lab` surface, which
  opens the current scene-backed deterministic temporal-graph preview instead of
  hiding plant work behind an Asset command-menu action. Package Manager
  activation stages
  `assets/packages/engine_forest_factory.package.json` and
  `assets/packages/engine_forest_factory/default.forest.json` in the active
  project. Package payload/source routing points at
  `Autodidac/EpochEngineExtensions`; the Plant Lab repo remains recorded
  provenance/reference source, and generated project payloads are still emitted
  only after visible package activation or main-scene use.
- the command-menu Package Manager is the intended modal surface for local
  runtime-mini packages first, then explicit downloadable source packages later.
  Downloadable source packages must compile through an updater-style human-gated
  path and must not auto-run servers, listeners, hidden model channels, or any
  service that bypasses operator approval.
- Package Manager install attempts must show visible per-package state in the
  modal using the shared GUI progress bar. Package selection should use a
  scrollable list with per-package Install/Remove/Review Gate actions instead of
  a combo-box-only selector. Selecting a package should update the selected
  package/status text immediately; pressing Install should either
  materialize a local package, stage a human-approved download/build gate, or
  display the reason the package is blocked. The modal body is a clipped shared
  GUI scroll area; package rows and progress bars must not bleed into the scene
  or into command-menu/modal chrome.
- Active project evidence repair now preserves the current editor entity list
  and writes a minimal `.epoch` entity snapshot during explicit Save/Build/Run
  paths. This is the current safety lane for editor modifications until the
  full scene parser/serializer owns runtime/editor loading.
- OS model package lanes are on-demand model assets. Qwen, Nemotron, Bonsai,
  FLUX, Wan, and TRELLIS weights are staged to executable-local `cache/models/`
  only after operator action, are not cloned for engine self-iteration, and are
  included in generated projects only by explicit package opt-in with
  license/notice review. The current gate writes a project-local
  `*.model.package.json` opt-in manifest and a cache-local `download.plan.json`
  before any future downloader is allowed to transfer weights. Intelligence now
  exposes direct model-package entry buttons for Nemotron 3 Nano 4B BF16, Qwen
  3.6 27B, and the image lanes. Bonsai Ternary 4B is the recommended local
  image default, Bonsai Binary 4B is the low-memory lane, and FLUX.2 Klein 4B is
  retained as the optional higher-memory fallback.
- Project Run is project-owned, not editor-clone-owned. The Project workspace
  must expose the target backend/context and child project launches should use
  standalone single-context flags such as
  `--standalone --window-mode standalone --backend opengl`.
  Empty or stale `auto` backend payloads are clamped to `opengl` before the
  child process is launched so generated projects do not accidentally revive the
  parented multicontext shell.
  If the expected child executable is missing after the build, the editor must
  block the run with visible evidence instead of falling back to a parent
  multicontext `Project Runtime` scene across every dock.
- Launch Single Context uses a child-build freshness gate. The editor saves and
  repairs project evidence, then launches the existing child executable when the
  output is newer than the project entry source, active script source, generated
  Windows project file, generated build script, and engine static library. When
  any of those inputs are newer or the output is missing, the normal serialized
  project build path still runs before launch. Scene and manifest writes are
  runtime inputs and should not force a relink by themselves.
- Docked editor panes use selected-backend multi-pane ownership by default.
  Switching the 3D rendering window to a different backend is an explicit
  diagnostic/accurate-preview action; normal docking should not mix unrelated
  renderer families unless the operator requested that proof grid.
- The centered Run button follows the same split: normal generated projects
  save project evidence and launch through the selected single-context child
  backend, rebuilding only when the freshness gate says the executable is stale;
  the engine self-iteration lane stays editor-shaped because it manipulates the
  checked-out engine and needs visible build/review evidence.
- workspace changes from the launcher/editor toolbar may use short loading
  feedback through the shared GUI progress primitive only after that feedback is
  proven not to change scene viewport geometry or revive menu/modal flicker.
  Console Dock is status-only and must not drive these workspace switches or
  package actions.
- network/server packages follow the same gate. Shared client/network runtime
  contracts may ship inertly in the engine, but authoritative dedicated
  headless server support is an optional package for projects that explicitly
  choose that model. Client listen/nondedicated and future competitive
  client-predicted paths remain separate opt-in packages. Software projects,
  single-player games, and minimal generated engine clones must not receive
  server/listener code by default.
- heavy optional package source should live outside the engine repository. The
  canonical package-source home is
  `https://github.com/Autodidac/EpochEngineExtensions`; EpochEngine should keep
  package descriptors, security gates, updater/cache paths, and minimal inert
  runtime hooks only. Downloaded or generated package payloads resolve under
  executable-local `cache/packages/`.
- research prototypes such as voxel terrain, planetary rendering, procedural
  vegetation, and tool harnesses should enter Package Manager as local
  research-package candidates first. A package candidate needs provenance,
  source/hash, build/test commands, known limitations, and a proposed engine API
  boundary before mainline source promotion.
- Self-Iteration Sandbox controls always target the `sandbox` profile. The
  profile is classified as an engine self-iteration lane, not a normal
  game/tool project, so it mirrors the checked-out engine/editor shape for
  manipulation, build, and testing instead of inheriting generated-project
  presentation behavior.
- project shells should only be materialized by explicit operator action:
  File > Save Project, Project > Save Active Project, or the centered Run
  button. Merely selecting a project profile must not create files silently.
- the centered Run button now saves normal generated-project evidence, checks
  child executable freshness, rebuilds only when source/build inputs are stale,
  and launches the selected single-context child backend. If the build fails,
  launch is canceled so stale `Projects/**/bin/...` outputs are not mistaken for
  the result of the current run. The engine self-iteration sandbox is
  intentionally excluded from that generated-project launch path and remains
  editor-shaped for visible engine manipulation, build evidence, and review
  gates.
- generated project builds are serialized inside the editor process, and emitted
  Windows `build_project.ps1` scripts also take a repo-level build lock. Until
  ProjectLauncher/Sandbox child builds have isolated engine-object/module/PDB
  output directories, every generated child build must either hold that lock or
  fail visibly instead of racing over shared `StaticLib1` outputs.
- the Project workspace should also surface simple existence checks for the
  manifest, entry source, build script, `project.paths.txt`, expected output,
  build log, and active script source so the user can tell whether the shell is
  real without leaving the editor
- normal generated projects expose a selectable camera style from the Project
  workspace. The first production choices are editor orbit, first-person runtime,
  and locked 2D canvas. That setting applies to Play In Editor and the project
  preview path; the engine self-iteration sandbox keeps following editor tools
  because it is an engine/editor manipulation lane.
- viewport movement starts with shared input actions: LMB pan, RMB orbit,
  wheel zoom, movement/look actions, and `Home` reset. Editor Settings and the
  Project workspace expose the first input-profile presets, and project runtime
  launches carry the selected profile. The shipped presets keep movement and
  look keys non-overlapping so a single key press does not translate and rotate
  the camera at the same time. The next promoted version needs per-action
  rebinding and project/package serialization so projects opt into bindings
  instead of inheriting every editor-only control.
- hot reload remains a development feature and needs smoke coverage instead of
  trust

## 2D Scene/UI editor surface

- `2D Scene/UI` is the same scene viewed through a dedicated Canvas2D camera, not a
  separate scene or project island.
- entering `2D Scene/UI` creates/selects an editor-only `Canvas2D` plane and switches
  the preview camera to the locked 2D Canvas rig
- the `Canvas2D` plane is an upright XY-style editor canvas viewed by a
  front-facing orthographic camera. It should not be a floor-like XZ plane; the
  2D workspace reuses the scene view from a locked 2D perspective, similar to
  Unity's 2D scene editing mode.
- `render.preview_grid` owns the Canvas2D projection helper. Editor object
  selection and the OpenGL, DirectX, Raylib, SDL, SFML, Vulkan, and
  software-preview paths should all use that helper so 2D editing behaves like
  a Unity-style scene camera locked to a 3D canvas instead of drifting per
  backend.
- future 2D work should add tile/layer/canvas tools on top of this same
  entity/project spine

## System Info workspace direction

- `System Info` is now the active tooling surface for:
  - frame graph / render graph
  - task graph / multithreading
  - time-system diagnostics that point to Video for controls
  - pacing / perf select
  - diagnostics
- graph views render as engine-generated textures inside the central System Info
  surface only; the bottom Console Dock keeps compact text diagnostics and does
  not duplicate the graph UI
- the central System Info surface gives the render/frame graph and task/thread graph
  full-width readable rows instead of tiny side-by-side thumbnails
- graph views support pan/zoom and remain clipped when they are wider than the
  available panel
- top-level editor mode buttons now route the central work area. `3D Scene` and
  `2D Scene/UI` keep the real scene viewport; `Assets`, `Plant Lab`, `Video`,
  `Project`, `Intelligence`, and `System Info` own their dedicated surfaces so
  those workflows do not have to be operated from the console dock.
- the central work area now has a first-pass tabbed `Editor Workbench` strip for
  `3D Scene`, `2D Scene/UI`, `Assets`, `Plant Lab`, `Video`, `Project`,
  `Intelligence`, and `System Info`.
- Intelligence activation is centralized: the toolbar, bottom AI dock tab, and
  Window > Open AI Control Surface all reopen Inspector, AI Chat, and the
  Console Dock before selecting the self-iteration sandbox.
- `EPOCH_EDITOR_START_WORKSPACE=AI`, `Systems`, or `Assets` selects the matching
  central editor surface at startup instead of only changing the bottom dock tab.
- splitter bars are dedicated GUI chrome rather than blank buttons. They should
  stay visually stable while resizing/maximizing and must not consume launcher
  or editor button press identity.
- the bottom Console Dock / AI Chat column controls are resize-only now: drag
  the vertical splitter between them or the horizontal splitter above them.
  Button rows such as `Console +`, `Chat +`, `Dock +`, `Dock -`, and `Reset
  Columns` are intentionally removed from the active workflow.
- Bottom Dock `Project`, `Assets`, `AI`, and `Systems` pages are compact
  selectable text status panels using the same visual path as `Output`. Their
  job is evidence/status only; controls for packages, script editing, model
  selection, time controls, and graph surfaces belong in central workspaces,
  Video, or the Inspector.
- Phase 5 self-iteration should have visible graph/flow feedback, not only text
  rows. The first-pass AI loop visualizer shows planner, builder, verifier,
  gate, and human-review readiness as an engine-generated surface in the central
  Intelligence. Generated graph/runtime surfaces use the dedicated runtime-surface
  atlas, not the small built-in GUI skin atlas. Future work should promote that
  into a dedicated editor window with packet replay, scene-state diffs, and
  eventually 3D model/weight visualization
- support-tier diagnostics should stay visible beside renderer stage flow and
  worker-count information so compatibility policy is visible in the editor
- docked backend hosts should present one clean pane per active context
- the target GUI shape is MSVC/IDE-like: visible context panes, ordinary
  close/resize affordances, modal/menu layers over scene views, and no duplicate
  console-only control surfaces for editor-critical actions
- `engine.gui` is the reusable engine GUI library layer. Primitive widgets
  such as tabs, dropdown/select boxes, text inputs, scroll areas, image views,
  window chrome, modal layers, and future context menus should live there before
  editor workspaces consume them. `editor.cpp` chooses the active workspace and
  feeds domain data; it should not own generic widget behavior.
- The detailed GUI library contract is tracked in
  `Engine/docs/engine/gui_library_architecture.md`; use that before adding new
  panes, tabs, dropdowns, modal windows, package-manager UI, scripting views, or
  AI sandbox controls.
- The first dropdown/select-box primitive is the AI local-model selector. It
  replaces long repeated model buttons with one reusable control so package
  manager, project settings, backend selection, and script/asset selectors can
  follow the same path instead of creating one-off UI. Open dropdowns own their
  mouse-wheel focus so parent scroll panes do not steal model-list scrolling.
  Opening a dropdown anchors the list near the selected value, and clicking
  outside the closed control or list closes it through the shared GUI primitive.
- the stable Windows top-row contract is visible real child panes:
  `GLFW30`, `SDL_app`, and `SFML_Window`
- helper `EpochChild` wrappers are implementation detail only:
  they should stay aligned to the same dock slot while parented and must stop
  lingering as floating top-level shells after a real redock
- SDL3 and SFML3 should keep obeying their elevated host-child hierarchy: the
  visible proxy host is the movable shell during undock/redock, while the real
  backend child stays nested inside that shell instead of pretending to be an
  independent top-level owner
- Raylib currently uses the direct-child dock contract with a hidden parked host
  rather than a live proxy child, so harness and runtime checks should judge it
  by that honest ownership model instead of forcing the SDL/SFML proxy-child
  expectations onto it
- Raylib dock/undock/redock commands must be executed on the Raylib owner
  thread. The Win32 dock proc may queue those commands, but it must not mutate
  the GLFW/Raylib child HWND directly from the parent host thread.
- when validating Win32 parented multicontext behavior, a live window-tree probe
  should show the real backend child classes as visible pane owners and helper
  wrappers hidden in the docked state
- if the synthetic harness disagrees with a clean human six-pane validation,
  treat the live editor behavior plus the Win32 subsystem logs as the deciding
  truth and repair the harness/probe afterward instead of mutating runtime code
  to satisfy the harness
- maximize/restore validation should keep using the pane owner that is actually
  parented into the grid slot at that moment, not a stale abstract "primary"
  HWND that may still be mid-takeover
- child-docked backends should consume the grid/`WM_SIZE` dimensions as the
  authoritative resize request; avoid feeding stale child rects back into the
  same resize path
- once a backend is docked as a child pane, do not reapply top-level backend
  window-size APIs that can silently restore window chrome semantics and break
  the parented slot geometry
- editor text-entry validation is still an active gap:
  the shell needs a repeatable typed-text smoke for AI chat and other edit
  boxes, not just click/focus proof
- the first shared arbitrary scroll-area primitive now exists and is used by
  the World Outliner, Inspector, and non-output Console Dock pages. Future
  editor windows should build on this path instead of adding new per-panel
  scrolling hacks.
- the World Outliner, Inspector, Console Dock, and AI Chat now have first-pass
  open/close layout state plus draggable side/bottom splitters. This is the
  current docked layout control layer, not yet the final IDE-class window host.
- GUI draw and hit testing should remain clipped to active panel content so
  buttons, rows, and text do not bleed over or steal input from the Perspective
  scene view.
- detached context windows are explicit optional panel hosts, not hidden
  always-running backends. Real pane title-bar drag/release gestures route
  existing panes such as World Outliner, Inspector, Console Dock, and AI Chat
  into cloned native context panels. A successful route hides the docked source
  pane until the routed pane closes, docks back, or receives native close
  cleanup. The routed context refreshes GUI/font upload state before its first
  panel frame so cloned panes do not inherit broken atlas state. The Window menu
  owns show/hide/reset and emergency source-pane restore; it is not the primary
  detach surface. Optional low-level routes such as `floating.gui` remain host
  infrastructure, not the current user-facing feature. Backend selection stays
  in the editor toolbar combobox. Additional GUI containers must use visible
  request/status paths with focus ownership, teardown, and evidence logging.
  Games, mobile apps, console targets, and headless tools may omit native host
  routes entirely while still using the portable `EpochGui` layout library.
  True drag/drop redock behavior remains a later native-host movement feature.
- editor context selection is an in-process handoff, not a process restart. The
  toolbar combobox focuses/restores an existing live backend context and parks
  duplicate editor sessions back to the launcher/menu. If no live target exists,
  it fails closed with visible status instead of opening the wrong shell,
  restarting the engine, or changing only the label. This is still a desktop
  editor/tool host workflow, not a requirement for game/mobile products.
- time diagnostics should show the shared simulation clock state: pause/resume,
  scale, fixed-step cadence, accumulator, and simulated time
- time diagnostics should also show the current frame step budget and the
  max-steps-per-frame clamp so pacing policy is visible, not implied
- the shared preview marker should prefer the editor focus projected onto the
  grid plane, then fall back to the center camera ray and last valid hit, so
  editor panning and the visible look spot stay stable across contexts
- this surface should help unify renderer/backend behavior instead of becoming
  another debug text dump

### Context Implementation Contract

The editor has two separate ideas that must stay separate in code and UI:

1. **Backend/context selection**: the editor toolbar combobox chooses the
   renderer/context family for the editor session.
2. **Floating/routed GUI containers**: pane title-bar drag/release gestures
   present existing editor panes in their own cloned native/context windows.
   Optional desktop host routes such as `floating.gui` can still exercise lower
   level GUI hosting, but they are not the user-facing popout feature.

Do not merge those ideas back into one "Context Driver" button. A context
selection action may open or focus an editor context, but it must not open the
floating GUI proof panel. A floating GUI route may show its renderer as evidence,
but it must not own backend switching.

Current source ownership:

| Responsibility | Primary files/modules |
| --- | --- |
| Editor command/result payloads | `Engine/modules/editor.ixx` |
| Toolbar context combobox and visible status | `Engine/src/editor.cpp` |
| Editor state capture/restore for handoff | `Engine/src/editor.cpp` |
| Session loop, live context discovery, handoff fallback | `Engine/src/engine.cpp` |
| Native detached context/window request and `WindowData::guiRoute` | `Engine/modules/context.multiplexer.ixx`, `Engine/modules/context.window.ixx`, `Engine/src/renderers/host/engine.context.host.*.cpp` |
| Reusable GUI layout state | `Engine/include/gui`, `Engine/src/epochgui`, `Engine/dep/EpochGui` |
| Engine GUI adapter/render/input bridge | `Engine/modules/engine.gui.ixx`, `Engine/src/engine.gui.cpp` |

Context switching acceptance:

- the combobox label tracks the actual active backend, not stale desired state
- choosing the active backend logs that it was kept and does not create windows
- choosing another live backend focuses/restores that context, carries the
  editor snapshot, and parks other duplicate editor sessions back to the
  launcher/menu
- choosing an available backend with no live editor context fails closed with
  visible status; a future create-new-context path must report posted request,
  window/context creation, session entry, snapshot restore, and frame-present
  evidence separately before claiming success
- an explicit backend request must fail closed when that backend is unavailable;
  it must not silently substitute the priority/default backend
- request evidence is staged: `OpenDetachedContextWindow == true` means a native
  request was posted or accepted by the host, not that the window was created,
  entered the session loop, restored editor state, or presented a valid frame
- successful handoff claims require the later evidence state: window/context
  created, session entered, snapshot restored, and focus/present path live
- snapshot capture or restore failure must log visibly and must not be reported
  as a complete context switch
- choosing a backend that cannot be created fails closed with visible log/status
  evidence
- the launcher `Switch Context` action cycles/focuses live contexts only; it
  must not open the editor or duplicate the context-driver proof window
- no context switch persists across full engine restarts unless a future
  profile setting explicitly owns that policy
- no switch path may fake success by only changing labels

Current platform truth:

| Host | Missing-live-target behavior from combobox | Notes |
| --- | --- | --- |
| Windows desktop editor | Fail closed today | Live backend handoff is supported; missing targets report visible status instead of posting another editor/context-driver window. Future host-created contexts need staged evidence before success claims. |
| Linux/WSL | Fail closed today | Single-context OpenGL remains the default proof path. |
| Mobile/console/headless | Excluded unless a product host implements it | These targets should hide or reject native popout/context-create commands while keeping portable `EpochGui` controls available. |

Background context selection is a separate future system. It should passively
measure normal single-context editor sessions and recommend the best default
backend based on stability, frame pacing, input latency, memory pressure,
feature support, and user-visible evidence. It must not use parented
multicontext diagnostic grids as scoring data, because simultaneous panes
distort FPS, timing, memory, upload contention, and input ownership. Mixed
backend grids remain comparison/diagnostic tools, not runtime truth for choosing
the editor's default context.

Floating/routed GUI acceptance:

- route ids are explicit strings such as `floating.gui`, not overloaded window
  titles
- a routed GUI window runs `editor_run_context_panel(ctx, route)` and draws only
  the requested panel content
- `floating.gui` is a single GUI host proof with its own title, size, input
  capture, close path, and status evidence
- current editor pane popouts start from pane title-bar drag/release gestures
  and use concrete pane routes such as `pane.outliner`, `pane.inspector`,
  `pane.console`, and `pane.ai_chat`; native `floating.gui` remains optional
  host infrastructure, not required for games, mobile apps, console apps, or
  headless tools
- route windows are optional desktop editor/tool features. If a product target
  excludes native popouts, the command should be absent or report unsupported,
  not create hidden shells
- future redock/undock support must move through the native host layer and the
  reusable `EpochGui` dockable-window action model together

When this area is split across agents, keep file ownership disjoint: one agent
may work on editor UI/status, another on session/window host code, another on
`EpochGui` primitives/build metadata, and another on docs/build evidence. Do
not run two workers against `editor.cpp` or `engine.cpp` simultaneously unless
the write ranges are explicitly isolated.

## Naming and structure direction

- legacy `aengine*` naming and older catch-all labels such as `multiplexer`
  should be treated as transitional debt, not as the final public structure
- source filenames should move toward `engine.*`, `epoch.*`, or
  subsystem-specific ownership in small tested batches.
- current completed filename batch: the former `aengine*` and `aeditor*`
  headers/modules/source files have been moved to `engine*`, `editor*`, or
  subsystem-owned paths in CMake and MSBuild. Keep `a2048like` as the explicit
  module-name exception because numeric-leading modules are invalid.
- when a subsystem is touched, file names, module names, and exported surfaces
  should move toward consistent professional ownership instead of growing more
  orphan naming
- `source_shape_audit.md` is the current guard for config/header/module/backend
  cleanup. Use it before touching compatibility headers, bridge headers, Perf
  Manager integration, or backend file splits.

## Time-system spine

- Epoch is time-based in the simulation sense, not in an extra-dimensions sense
- the first shared time spine lives under `core.time`
- the first milestone is:
  - fixed-step accumulation
  - pause / resume
  - time scaling
  - single-step
  - shared stats for editor/runtime/systems visibility
  - step-budget and frame-cap pacing visibility
- scene play, scripting, pacing, timeline, and replay behavior should route
  through that shared clock ownership instead of inventing parallel timing
  systems
- `System Info` remains the diagnostics surface for renderer/backend/context
  graphs, support policy, and live system lists. It no longer owns pacing
  buttons or frame-step controls.
- `Video` is the dedicated 4D/time-based workspace. It reads the
  shared `core.time` stats, exposes manual/interval/frame/timeline-key
  checkpoint modes, owns pacing/playhead controls, and presents configurable
  streaming-save status beside the editor scene flow.
- scene-backed editor workspaces reserve a bottom Video Timeline strip when
  there is enough room. That strip shrinks the scene viewport instead of
  letting 3D/2D rendering draw behind timeline controls or timeline graph
  chrome.
- Epoch's 4D direction means timing is not a side panel: timeline authoring,
  streaming-save cadence, checkpoint retention, replay keys, package preview
  playback, and future deterministic simulation review all route through the
  shared time spine before they become generated-project behavior.
- `timeline.system` is the first explicit timeline data model. It owns editor
  tracks, keyed events, playhead state, scrub helpers, recording-gate state, and
  conversion into scene timeline keys so the UI can grow around engine data
  instead of ad hoc status buttons.
- Timeline view metrics now live in `timeline.system` too: visible range,
  seconds-per-pixel, playhead X, visible key count, and per-track event summaries
  are data outputs that the editor can render as lanes once the GUI library has
  proper timeline controls.
- Timeline lane layout is also engine data. Tracks produce lane rectangles and
  keyed events produce marker positions inside the visible range, so the editor
  can grow selectable/draggable 4D timeline lanes from validated data instead of
  hardcoded drawing.
- `saveload.system`, `scenesnapshot`, and `sceneserializer` are the current
  contract layer for timeline checkpoints: they define streaming-save config,
  checkpoint labels, scene object snapshots, timeline keys, and deterministic
  text serialization plus parser round-trip. The next acceptance gate is wiring
  those contracts into real `.epoch` scene persistence, disk writing, and replay
  restore.
- Streaming-save profiles are descriptor-backed engine data, not loose UI
  switches. `saveload.system` owns stable profile IDs, labels, summaries,
  activation defaults, retention caps, and included-data flags for manual
  review, 15-second editor streams, 120-frame editor streams, and timeline-keyed
  streams. Checkpoint records carry retention, output path, included-data flags,
  scene payload byte counts, and timeline-key counts so build-safe tests can
  verify evidence before any runtime writer/restore path is promoted.
- Streaming-checkpoint packages now bind the checkpoint record, deterministic
  scene payload, manifest line, and payload hash. This gives the writer/restore
  path an auditable package shape before real disk writes, rolling cleanup, or
  editor/runtime replay are claimed complete.
- Checkpoint write plans are now explicit but non-writing. They stage the
  snapshot path, serialized scene payload path, manifest path, and manifest line
  under the configured streaming-save root so Video can expose the
  future write layout before a human-approved disk writer/restore gate lands.
- Streaming-save cadence plans make the next checkpoint decision visible: due
  now, waiting for a manual/timeline-key action, or scheduled by frame/seconds
  from shared `core.time` stats. Video displays that next-capture
  summary while disk writing remains gated.
- Checkpoint restore plans mirror the staged write layout for the future replay
  gate. They name the checkpoint label, snapshot path, scene payload path, and
  manifest path without reading disk or mutating the live scene.
- The checkpoint writer path exists but remains approval-gated. It writes scene
  payload, snapshot metadata, and manifest entries only when an explicit human
  approval object is supplied; the build-safe contract lane verifies the blocked
  gate and metadata payload without touching disk.
- Checkpoint retention plans are non-destructive. They evaluate staged
  checkpoint records against the active rolling-retention cap and report retained
  versus prune-candidate checkpoints for Video, but no cleanup or
  delete operation is enabled until a separate human-approved disk gate exists.
- Streaming-save profile-change plans are review-first. They let the Video
  Editor show what an interval/frame/manual/keyed profile transition would
  change before any dropdown mutates live save configuration.
- `engine.input` is the shared input profile spine. The default editor profile
  now names camera reset-to-center, frame selection, clipboard copy/paste,
  right-click context menu, play-in-editor, timeline play/step, and package
  install actions with modifier-aware key bindings and mouse bindings. GUI
  surfaces should consume those named actions instead of hardcoding per-window
  shortcuts, and project/package export should treat input profiles as explicit
  opt-in data.
- The non-GUI engine contract self-test now exercises the Forest Factory,
  package registry, streaming-save package, input profile, and scene snapshot
  serializer/parser contracts before the heavier project-profile and OS-AI
  validation gates. New timeline, package, model, or input contracts should join
  that lane before being exposed as generated-project behavior.

## Hardware support strategy

- default automatic compatibility target:
  - 6-core desktop CPU class
  - GTX 1660 Ti-era GPU class
  - modern Linux laptops/desktops
- baseline tier:
  broad editor/runtime reach with stable dependencies
- standard tier:
  stronger GPU/backends and fuller renderer/tooling paths
- extended tier:
  heavier libs/features enabled per project by the game developer

Epoch should prefer broad automatic support with explicit opt-in for heavier
features over forcing every integration on every machine.

## Renderer direction

- prioritize the GPU-driven baseline first:
  visibility -> surface -> lighting -> temporal -> reconstruction -> present
- use frame/task graph guidance and temporal history/reconstruction as the main
  planning surface
- keep heavier paths such as ray tracing, path tracing, mesh shaders, virtual
  shadowing, sparse-resource-heavy flows, and similar techniques behind
  Standard/Extended tiers or explicit project opt-in
- keep backend convergence visible in the System Info workspace so OpenGL, Vulkan,
  DirectX, the software fallback, SDL, SFML, Raylib, and future D3D12 do not drift
  without tooling feedback
- OpenGL launcher/editor flicker has been manually reported resolved for the
  current pass, but GUI/scene composition remains guarded because z-order bugs
  can make command windows, AI Chat, Inspector, or viewport titles appear hidden
  behind the 3D/2D preview.
- Native context hosts publish a `host FPS` counter in their window titles
  during local runs. Use the parent and child/context title rates as quick
  evidence when checking whether flicker is coming from the dock host, backend
  process loop, or a duplicated present/composition path.
- `v0.84.23` removes the extra queued OpenGL clear from editor/menu UI frames
  and makes preview-grid lines non-depth-writing reference geometry. Manual
  confirmation should test launcher press transitions, dropdown menus,
  Perspective orbit angles, Game/2D canvas angles, and graph/matrix scenes
  before closing this issue.
- The same pass now defers workbench tab/menu switches until the GUI frame is
  complete, restores the relevant docked panes when entering scene/game/AI
  workbenches, and keeps the OpenGL scene viewport one pixel inside its GUI
  chrome. These changes are specifically for the remaining GUI/3D overlap
  flicker and missing-pane reports. MSVC Debug/Release builds and the Sandbox
  project self-test pass; manual eye-test confirmation is still required because
  the standalone OpenGL smoke launch hit `PlatformGL::make_current(final) failed`
  before it could complete.
- `v0.84.24` tested GUI-first OpenGL composition after the flicker fix. Manual
  follow-up showed the launcher flicker was resolved, but GUI-first composition
  put command/dropdown windows and pane chrome behind the scissored scene view.
- `v0.84.25` restores scene-first / GUI-over composition for OpenGL: the
  scissored Perspective/Game preview renders first, then queued GUI commands
  draw AI Chat, Inspector, menu dropdowns, and scene viewport titles on top.
  System Info graph surfaces now belong only to the central System Info workspace; the
  bottom Console Dock stays a compact evidence/log strip.
- `v0.84.26` keeps that composition order but makes the central workbench
  background transparent when a scene-backed surface is active, so the retained
  GUI batch can draw pane chrome without hiding the 3D/2D preview underneath.
- `v0.84.29` issue evidence is archived in
  `diagnostics/2026-05-17-gui-regression/README.md`. The key clue is that
  closing World Outliner exposes the Perspective title while Inspector and AI
  Chat remain blank, so the remaining work should stay focused on GUI
  layout/composition and pane draw order instead of speculative renderer
  rewrites.
- `v0.84.31` is the stable multicontext checkpoint to preserve before the next
  risky pass. It keeps OpenGL scene-first / GUI-over frame order, adds an
  overlay-priority path for command menus and modals, removes visible
  scene-viewport blanking during workbench switches, and adds a short cooldown
  around Systems graph controls. Raylib redock crash and multicontext
  maximize/restore remain open blockers for the stable branch.
- `v0.84.32` records the next architecture direction before risky code churn:
  keep the current multicontext baseline stable, move the editor shell toward
  first-class individual context panes, retire software to safe-launch/debug
  GUI fallback, and begin the Windows Direct3D/D3D12 renderer track as the
  eventual native replacement for software-as-product-renderer. Do not claim D3D
  runtime support until a real device/context/swapchain/shader/resource slice is
  built and screenshot-validated.
- `v0.84.33` keeps that baseline conservative: module SDL/SFML previews now
  use the shared preview marker data instead of grid-only rendering, and Systems
  graph controls have a longer repeat guard. MSVC `ConsoleApplication1`
  `Debug|x64` builds cleanly and standalone OpenGL editor smoke exits cleanly.
  Parented multicontext editor smoke still times out, and parented
  `--smoke --capture` writes proof captures but does not shut down before
  timeout, so smoke shutdown is still an open multicontext acceptance gate.
- `v0.84.34` keeps the proxy-host crash guard but restores immediate parent-grid
  sizing for direct child renderers. SDL/SFML proxy hosts still move
  asynchronously; Raylib, OpenGL, Vulkan, and software panes should resize with
  the parent more directly. A parented multicontext maximize/restore smoke
  completed without crashing, but Raylib resize speed still needs operator
  confirmation.
- `v0.84.35` promotes DirectX/D3D11 into the Windows multicontext proof set.
  DirectX owns a D3D11 device/swapchain/render target, renders preview markers,
  replays GUI, and replaces Software as the normal sixth Windows README proof
  pane. Software stays available for safe-launch/debug GUI, capture diagnostics,
  and headless validation.
- The same `v0.84.35` line now serializes editor Run builds, validates
  ProjectLauncher and Sandbox child `--project-self-test` paths when built
  serially, and queues Raylib redock operations through the backend owner
  thread. Linux Clang build/headless CTest is green with DirectX disabled, and
  Ubuntu WSL2/WSLg has a non-black single OpenGL editor proof when launched as
  `epoch --renderer opengl --standalone --editor --smoke --capture`. Do not use
  plain runtime smoke captures as editor proof; they can be black without
  indicating an OpenGL dependency failure.
- When `EPOCH_SINGLE_PARENT=0`, the launch config must force standalone
  top-level contexts even if CLI defaults still prefer parented mode. This mode
  is used to isolate resize/flicker from the single-parent dock host, so any
  parent-window creation in that build is a regression.

## Logging

- the engine logger is the preferred way to tag subsystem output
- keep backend-specific noise behind subsystem names so multi-context runs stay
  readable
- repeated identical startup/error lines should collapse instead of flooding the
  console
- one tagged line per repeated condition is the goal, with subsystem/source
  context still preserved

## Editor viewport controls

- right-drag orbits the editor preview camera
- left-drag pans the preview focus across the grid plane
- mouse wheel zooms the preview camera
- the preview should show a visible look-hit marker where the center camera ray
  intersects the grid

## AI runtime direction

Epoch documents three AI/control pieces:

- OS AI, the engine-owned open-source model harness for memory, retrieval,
  planning, tool use, verification, evidence metrics, and dataset/eval gates
- local tool/MCP control harnesses that operate the editor and collect proof
- operator-selected Qwen/Nemotron local model lanes for coding, review,
  fallback, and future generated-software embedding where licensing allows, with
  Bonsai/Wan/TRELLIS tracked as package-managed creative model lanes and
  FLUX.2 Klein kept as a higher-memory image fallback

External local OpenAI-compatible LLMs such as LM Studio or Ollama are selected
runtime/helper providers. They can help with testing, evals, dataset cleanup,
and faster iteration, but they are not hidden authority and are not a substitute
for visible build/test evidence.
The editor scans `/v1/models` and sends selected-model chat to
`/v1/chat/completions`; tool evidence capture files, including the legacy
`mcp_capture.jsonl` path, are evidence logs for the harness, not a hidden second
chat runtime.

Data rules:

- curated repo-safe assets belong in `Engine/ai/`
- `Engine/examples/ConsoleApplication1/workspace/auto_train.jsonl` and `Engine/examples/ConsoleApplication1/workspace/mcp_capture.jsonl` are raw/staged
  capture paths
- checkpoints, compiled local models, and caches stay under local
`Engine/examples/ConsoleApplication1/workspace/ai/` paths
- outdated or bad training data should be deleted or replaced when the training
  direction changes
- helper-first passes may probe `/v1/models` at the start of a phase, but the
  engine must not auto-name or activate a model from discovery. Only the
  operator-selected model is the active helper model for chat/planning.
- CLI/self-iteration passes can use `EPOCH_AI_MODEL`, `EPOCH_OPENAI_MODEL`,
  `LM_STUDIO_MODEL`, or `OPENAI_MODEL` as an explicit operator-selected helper
  identity. This exists so evidence packets can record the reviewer model during
  non-GUI runs without reverting to first-model auto-selection.
- additional loaded helpers may be used only as explicitly allowed drafting or
  review lanes, and their output remains proposal material until build/runtime
  evidence and human review promote it
- for direct helper drafting, use LM Studio `/v1/responses` or
  `/v1/chat/completions` with bounded output, and retry without any reasoning
  field when the selected model rejects explicit reasoning configuration
- if a selected helper returns blank visible content with only hidden reasoning,
  the editor must reject the response as a model/API configuration issue rather
  than showing the reasoning text in AI Chat or promoting it as training data

## Procedural/time-node direction

- the later procedural authoring phase should cover SpeedTree-like modular
  vegetation/world generation plus time-node authoring
- treat O2L as a later integration source for that phase when the code is in
  the workspace
- do not block the current time-system or Systems milestones on O2L being
  present now

## Editor Shell Direction

The bottom `Console Dock` is a temporary evidence/status strip, not the final
editor-window system. It currently hosts reusable tabbed panes for:

- `Project`
- `Assets`
- `Systems`
- `AI`
- `Output`

These tabs should keep logs, build evidence, status, model inventory, and visual
feedback available without becoming the main command surface. Action controls
that start AI repair/build/training passes belong in the Inspector until the
dedicated editor windows below exist.

The real editor shell target is a set of independently focusable/dockable
editor frames and views:

- Scene/Game editor view for project objects, play state, and runtime scene
  editing
- Software/Tool editor view for generated apps/tools and code-oriented project
  work
- Self-Iteration Sandbox view for dark-factory coding passes, staged packets,
  builder/verifier gates, and human approvals
- AI Visualizer view for model state, packet replay, scene-state diffs, and
  future 3D weight/model views
- Project Hub view for project launch/update/context selection. The
  compatibility id/path may still be `projectlauncher` /
  `Projects/ProjectLauncher` until a safe generated-artifact migration lands.
- Build/Output view for logs, diagnostics, and release/build evidence
- Asset Browser view for decoded image/model thumbnails, active project assets,
  and import/organization actions. The current `Assets` tab is only the first
  file-type-card version of that view.

These views should be backed by reusable GUI controls and custom UI powered by
an automated texture-atlas system, not by hardcoded editor-only tab strips that
cannot scale.

All editor-shell work should be production-minded. A feature can be incomplete
or acceptance-gated, but the code that lands must still be owned, buildable,
usable, and honest about its limits. Do not add fake controls, duplicate command
paths, placeholder windows, or temporary UI experiments unless they preserve a
working path and are documented with the next promotion/removal condition.

Current editor-shell gaps:

- the World Outliner needs stronger grouping, clipping, and resizable columns;
  the current compact button rows are a first cleanup pass, not the final
  desktop-grade control
- the engine GUI now has reusable `tab_bar`, `scroll_text_panel`, and modal
  focus overlay paths; selectable text is currently row-level and must grow into
  true text-range selection/copy support
- `tab_bar` is a real tab primitive now, not a segmented button alias. Future
  work should keep tabs visually connected to their content pane and reserve
  ordinary buttons for actions.
- the current file browser and asset cards are intentionally first-pass
  controls. They still need bounded columns, filtering, real decoded thumbnails,
  rename/move/import actions, and a code/text editor surface for scripts.
- launcher/editor settings buttons should open modal windows with concrete
  backend, display, package, project, and AI safety controls. Modal close
  affordances should be normal top-right X controls with overlay-priority z-order.
- Project Hub should present a basic project/game/software launcher mockup, not
  a miniature duplicate of the editor shell.
- World Outliner rows should show human project/entity names, type, and useful
  grouping instead of implementation-ish labels.
- the AI Visualizer should eventually expose packet graphs, scene-state diffs,
  and sampled weight/memory terrain views; it must not attempt to draw billions
  of raw parameters directly.
- the GUI still needs context menus, popouts, dockable editor windows,
  persisted layout profiles, resize cursors, resize handles, and column controls
  for desktop editor/tool builds; product targets that do not support native
  floating hosts should exclude those routes instead of carrying hidden shells
- editor theme selection is intentionally simple and user-facing: `System
  Light/Dark` follows the platform app-theme preference when available, while
  `Light` and `Dark` are manual choices. More branded/professional Epoch themes
  should be added later as optional palettes, not as replacements for those three
  basic choices.
- global UI scaling should behave like normal desktop software, with explicit
  user scale/font controls instead of one hardcoded pixel density
- separate editor windows/domains are still needed inside the application:
  project/game editor, software/tool editor, self-iteration sandbox, and AI
  visualizer should be independently launchable/dockable surfaces
- the software/tool editor name does not mean the software renderer is a normal
  production backend. The software renderer's target role is safe launch,
  debug/error messages, capture diagnostics, and headless validation; Windows
  native rendering is now moving through the first DirectX/D3D11 slice, with
  D3D12 still future work.
- linked-context panel hosts should remain explicit GUI containers such as
  Inspector, Asset Browser, Code Editor, AI Visualizer, and Build/Output. They
  must be operator-opened, visible, closable, and logged; future redock support
  should extend the same host contract instead of becoming hidden model/control
  channels.
- pane popouts are editor pane routes, not a duplicate editor shell and not a
  generic test window. Dragging a real pane title bar may request a cloned routed
  native context for that pane after release. Window menu entries manage pane
  visibility and layout reset only. The detached pane must own input and GUI
  overlay priority, and command menus must not let lower toolbar/content
  controls consume clicks behind them.
- self-iteration needs visual state, not only console rows. The first visible
  surface is the AI loop card visualizer; later passes should add packet replay,
  scene-state diff views, and a 3D model/weight visualization surface

## Multicontext proxy-shell behavior

- SDL3 and SFML3 use a visible `EpochChild` proxy shell inside the six-context
  parent rather than exposing the nested backend child directly as the grabbed
  dock target.
- The visible proxy shell is the thing that should undock: once dragged outside
  the parent, it must become a real top-level window, keep mouse/input control,
  and continue following the drag instead of freezing in place under the parent.
- SDL/SFML proxy shells must be moved back into dock slots by their posted
  proxy-host redock command. The parent grid may decide the slot, but it must
  not directly reparent or resize the nested SDL/SFML host/child pair while the
  backend-owned shell is redocking.
- A proxy redock command must not request another parent layout from inside the
  redock handler. Drag release or the parent layout pass owns the next layout
  request; re-requesting from the proxy handler can loop placement and recreate
  redock flicker/crash behavior.
- Focused six-pane parent validation is the current honest runtime gate for this
  path. The all-backends sequential harness still needs extra sequencing cleanup
  after the Raylib pass before it should outrank focused SDL/SFML evidence.

## Linux / WSL Runtime Shape

Linux and WSL do not use the Windows parented multicontext editor shell by
default. The current WSL-proven runtime path is a single OpenGL editor context.
Do not automatically fall back to Vulkan in WSL; Vulkan remains explicit
validation work on that lane until it is locally proven, and DirectX is
Windows-only.

Project-run backend selection on Linux/WSL currently exposes only the proven
OpenGL single-context choice in the editor. Other Linux backends can still be
compiled or launched as explicit validation work, but they should not be
presented as normal project-run choices until runtime proof exists.

Other renderer/tool outputs can still exist on Linux as project output choices,
but they should launch as explicit child processes from visible editor controls
instead of hidden parented contexts. Software rendering remains a safe-launch,
debug/error-message, capture-diagnostic, and headless fallback path; it is not a
normal peer renderer for the Linux editor shell.

## Generated project shell self-tests

Run these from the repository root after building the editor runtime:

```powershell
.\x64\Debug\EpochEditor.exe --editor-project-self-test sandbox
.\Projects\Sandbox\bin\windows\Debug\x64\EpochEngine.exe --project-self-test
.\x64\Debug\EpochEditor.exe --editor-project-self-test projectlauncher
.\Projects\ProjectLauncher\bin\windows\Debug\x64\ProjectLauncher.exe --project-self-test
```

The first command materializes and builds the selected shell from the real
engine binary. The second command proves the generated child output is runnable
without opening GUI windows. The Sandbox route must report the
engine-self-iteration sandbox identity; ProjectLauncher must report its launcher
tool identity.

`--engine-validation-self-test` chains the registered project profile self-tests
and the AI evidence gate in one non-GUI pass. It is intentionally broader than a
single profile smoke, but it still does not replace manual eye testing for GUI
composition, command-menu z-order, renderer flicker, or dock behavior.

As of `v0.84.30`, the engine-side command also appends an MCP-style tool
capture and stages an AI iteration packet under
`Engine/examples/ConsoleApplication1/workspace/ai/iterations/`. That packet is
the reviewable bridge for OS AI: it records project paths, build logs,
outputs, capture logs, selected model metadata, and a human-gated verifier
state before any follow-up coding pass is allowed to promote changes.

## Troubleshooting checklist

- verify the expected backend/config macros are enabled
- launch Windows smoke tests from `x64/Debug/` or `x64/Release/`
- prefer engine-owned capture output over ad hoc desktop grabs
- treat black or invalid software captures as failed proof that needs
  investigation, not as a successful screenshot
- treat DirectX screenshots as valid Windows product-renderer proof only when
  the pane shows real editor preview content, Inspector, AI Chat, and normal GUI
  chrome from an asset-bearing output directory.
- keep Windows resources under `Engine/resource/`
- keep local compiled AI artifacts out of the repo
- when investigating backend issues, prefer backend-local fixes over broad
  multiplexer edits unless the shared layer is clearly proven at fault
