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
- packaged updates stay binary-first: the updater shell should pull the newest
  named runtime package first, and only continue to source when the packaged
  runtime is already version-equal or newer
- the active packaged asset contract is versioned runtime archives such as
  `epoch_win10_x64_vX.Y.Z.zip` and `epoch_linux_x64_vX.Y.Z.tar.gz`
- the updater extracts the packaged version directly from the archive name
- WSL is treated as Linux for runtime package selection and should consume the
  same `epoch_linux_x64_vX.Y.Z.tar.gz` asset unless a future package layout
  proves a separate WSL asset is necessary
- source checkout installs still use the same binary-first rule; only after
  packaged parity or absence of a newer package should they rebuild from the
  GitHub source snapshot using the platform build path
- OpenGL editor composition is scene-first: draw the scene preview, drain
  queued render work, then render the latest persistent GUI batch. The
  persistent batch is required because the OpenGL render thread can run between
  editor/UI ticks; without replaying the latest GUI batch, frames can alternate
  between scene+GUI and scene-only, which presents as flicker.

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
  manifest, world file, script stub, and README for both game and tool projects
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
  projects, contexts, settings, and future automation flows
- that launcher should stay flat and direct: project entry, clean editor launch,
  contexts/settings, updates, and quit belong there; layered game/puzzle menus
  do not
- the launcher may open project demos directly or preload a project before the
  editor, but it should not drift back into multiple menu layers or become a
  fake game shell
- launcher and editor theme ownership should stay split: the launcher can keep
  its classic steel palette while the editor stays on the darker neutral tool
  palette, and any future theme selector should preserve that separation rather
  than forcing one skin across both shells
- backend ownership should stay equally explicit: switching the live editor to a
  different backend should tear down the inactive backend rather than leaving it
  rendering off-screen or parked in the background
- `editor.scene.cpp` should own project profiles, script profiles, runtime
  scene ids, and seed entities
- `editor.cpp` should act as the live shell over that scene/project data, not
  as a second hardcoded editor universe
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
- script stub creation should append `PROJECT_NOTES.md` entries; a useful
  Sandbox iteration must leave at least one of: build log output, script-host
  log output, staged packet evidence, selected project file path, or project
  notes explaining what changed
- build diagnostics should now cover the generated child-project build path too:
  entry source, generated project file, build script, build log, and expected
  output executable should all be visible from the Project workspace
- generated child projects, including the Sandbox shell, should repair stale
  Windows toolset metadata to `v143` before invoking MSBuild, and the checked-in
  engine projects they reference should stay on the same VS 2022 toolset.
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
- generated game project shells can carry the `engine_arcade` local
  runtime-mini package. The package is a project asset/script option that
  invokes engine-owned mini-runtime scenes such as Snake/Tetris/Pacman through
  the script host; it must not copy those implementations out of the kernel
  engine.
- the command-menu Package Manager is the intended modal surface for local
  runtime-mini packages first, then explicit downloadable source packages later.
  Downloadable source packages must compile through an updater-style human-gated
  path and must not auto-run servers, listeners, hidden model channels, or any
  service that bypasses operator approval.
- research prototypes such as voxel terrain, planetary rendering, procedural
  vegetation, and tool harnesses should enter Package Manager as local
  research-package candidates first. A package candidate needs provenance,
  source/hash, build/test commands, known limitations, and a proposed engine API
  boundary before mainline source promotion.
- Self-Iteration Sandbox controls always target the `sandbox` profile. They must
  not reuse the active ProjectLauncher/game/tool project when queueing engine
  self-iteration work.
- project shells should only be materialized by explicit operator action:
  File > Save Project, Project > Save Active Project, or the centered Run
  button. Merely selecting a project profile must not create files silently.
- the centered Run button now always saves and rebuilds the active generated
  project before launch. If the build fails, launch is canceled so stale
  `Projects/**/bin/...` outputs are not mistaken for the result of the current
  run.
- generated project builds are serialized inside the editor process, and emitted
  Windows `build_project.ps1` scripts also take a repo-level build lock. Until
  ProjectLauncher/Sandbox child builds have isolated engine-object/module/PDB
  output directories, every generated child build must either hold that lock or
  fail visibly instead of racing over shared `StaticLib1` outputs.
- the Project workspace should also surface simple existence checks for the
  manifest, entry source, build script, `project.paths.txt`, expected output,
  build log, and active script source so the user can tell whether the shell is
  real without leaving the editor
- hot reload remains a development feature and needs smoke coverage instead of
  trust

## Game/2D editor surface

- `Game/2D` is the same scene viewed through a dedicated Canvas2D camera, not a
  separate scene or project island.
- entering `Game/2D` creates/selects an editor-only `Canvas2D` plane and switches
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

## Systems workspace direction

- `Systems` is now the active tooling surface for:
  - frame graph / render graph
  - task graph / multithreading
  - time-system diagnostics and controls
  - pacing / perf select
  - diagnostics
- graph views render as engine-generated textures inside the central Systems
  surface only; the bottom Console Dock keeps compact text diagnostics and does
  not duplicate the graph UI
- the central Systems surface gives the render/frame graph and task/thread graph
  full-width readable rows instead of tiny side-by-side thumbnails
- graph views support pan/zoom and remain clipped when they are wider than the
  available panel
- top-level editor mode buttons now route the central work area. Scene and
  Game/2D keep the real 3D viewport; Project, Assets, AI Sandbox, and Systems
  switch to GUI surfaces and clear the scene viewport so those workflows do not
  have to be operated from the console dock.
- the central work area now has a first-pass tabbed `Editor Workbench` strip for
  Perspective, Game/2D, Assets, Project, and AI Sandbox. Systems opens as a
  direct Systems-only surface so it does not show a redundant Perspective/Game
  submenu inside the Systems view.
- AI Sandbox activation is centralized: the toolbar, bottom AI dock tab, and
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
- Phase 5 self-iteration should have visible graph/flow feedback, not only text
  rows. The first-pass AI loop visualizer shows planner, builder, verifier,
  gate, and human-review readiness as an engine-generated surface in the central
  AI Sandbox. Generated graph/runtime surfaces use the dedicated runtime-surface
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
- borderless linked-context popouts are a future editor-shell feature, not a
  hidden always-running backend. Each popped GUI container needs explicit
  operator action, focus ownership, teardown, redock behavior, and evidence
  logging before it becomes part of the normal workflow.
- time diagnostics should show the shared simulation clock state: pause/resume,
  scale, fixed-step cadence, accumulator, and simulated time
- time diagnostics should also show the current frame step budget and the
  max-steps-per-frame clamp so pacing policy is visible, not implied
- the shared preview marker should prefer the editor focus projected onto the
  grid plane, then fall back to the center camera ray and last valid hit, so
  editor panning and the visible look spot stay stable across contexts
- this surface should help unify renderer/backend behavior instead of becoming
  another debug text dump

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
- future work should route scene play, scripting, pacing, and later
  timeline/replay behavior through that shared clock ownership instead of
  inventing parallel timing systems
- Systems is the first live editor home for this information before fuller
  timeline/replay tooling exists

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
- keep backend convergence visible in the Systems workspace so OpenGL, Vulkan,
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
  Systems graph surfaces now belong only to the central Systems workspace; the
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

Epoch documents three internal AI/control pieces:

- EpochBot, the primary engine-owned trainable LLM/runtime path
- local tool/MCP control harnesses that operate the editor and collect proof
- an offline/injectable OSS or tiny backup LLM path for fallback, generated
  software embedding, and EpochBot training support

External local OpenAI-compatible LLMs such as LM Studio or Ollama are
development helpers. They can help with testing, evals, dataset cleanup, and
faster iteration, but they are selected teacher/reviewer providers rather than
hidden authority.
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
- borderless linked-context popouts should be built as explicit panel hosts for
  GUI containers such as Inspector, Asset Browser, Code Editor, AI Visualizer,
  and Build/Output. They must be operator-opened, visible, redockable, and
  logged; they must not become hidden always-running model/control channels.
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
- Focused six-pane parent validation is the current honest runtime gate for this
  path. The all-backends sequential harness still needs extra sequencing cleanup
  after the Raylib pass before it should outrank focused SDL/SFML evidence.

## Linux / WSL Runtime Shape

Linux and WSL do not use the Windows parented multicontext editor shell by
default. The current WSL-proven runtime path is a single OpenGL editor context.
Do not automatically fall back to Vulkan in WSL; Vulkan remains explicit
validation work on that lane until it is locally proven, and DirectX is
Windows-only.

Other renderer/tool outputs can still exist on Linux as project output choices,
but they should launch as explicit child processes from visible editor controls
instead of hidden parented contexts. Software rendering remains a safe-launch,
debug/error-message, capture-diagnostic, and headless fallback path; it is not a
normal peer renderer for the Linux editor shell.

## Generated project shell self-tests

Run these from the repository root after building `ConsoleApplication1`:

```powershell
.\x64\Debug\ConsoleApplication1.exe --editor-project-self-test sandbox
.\Projects\Sandbox\bin\windows\Debug\x64\EpochEngine.exe --project-self-test
.\x64\Debug\ConsoleApplication1.exe --editor-project-self-test projectlauncher
.\Projects\ProjectLauncher\bin\windows\Debug\x64\ProjectLauncher.exe --project-self-test
```

The first command materializes and builds the selected shell from the real
engine binary. The second command proves the generated child output is runnable
without opening GUI windows. The Sandbox route must report the
engine-self-iteration sandbox identity; ProjectLauncher must report its launcher
tool identity.

As of `v0.84.30`, the engine-side command also appends an MCP-style tool
capture and stages an AI iteration packet under
`Engine/examples/ConsoleApplication1/workspace/ai/iterations/`. That packet is
the reviewable bridge for EpochBot: it records project paths, build logs,
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
