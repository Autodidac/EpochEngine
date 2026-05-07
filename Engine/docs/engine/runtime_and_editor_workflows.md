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
- the longer-term shell default should converge toward one active backend at a
  time: editor favors a single-context OpenGL path, launcher favors a
  single-context software path, backend switching is explicit, and inactive
  backends must be torn down instead of running hidden behind the active shell
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

## Project-centric runtime direction

- the editor should play the active project and scene, not a hardcoded sample
  game menu
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
- `aeditor.scene.cpp` should own project profiles, script profiles, runtime
  scene ids, and seed entities
- `aeditor.cpp` should act as the live shell over that scene/project data, not
  as a second hardcoded editor universe
- current `.epoch` scene/world files are metadata shells only. They must exist
  and be surfaced as evidence, but the live preview/runtime object list is still
  driven by `aeditor.scene.cpp` seed entities until scene-file loading,
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
  child `--project-self-test`.
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
- the Project workspace should also surface simple existence checks for the
  manifest, entry source, build script, `project.paths.txt`, expected output,
  build log, and active script source so the user can tell whether the shell is
  real without leaving the editor
- hot reload remains a development feature and needs smoke coverage instead of
  trust

## Game/2D editor surface

- `Game/2D` remains a 3D-backed editor viewport for now, but entering it creates
  and selects a first-pass `Canvas2D` plane so the workflow has a concrete 2D
  edit target instead of an empty perspective scene.
- Future 2D work should lock a camera/view to that canvas, then add tile/layer
  tooling on top of the same entity/project spine instead of creating a
  separate editor island.

## Systems workspace direction

- `Systems` is now the active tooling surface for:
  - frame graph / render graph
  - task graph / multithreading
  - time-system diagnostics and controls
  - pacing / perf select
  - diagnostics
- graph views render as engine-generated textures inside the central Systems
  surface and the docked UI mirror
- graph views support pan/zoom and remain clipped when they are wider than the
  available panel
- top-level editor mode buttons now route the central work area. Scene and
  Game/2D keep the real 3D viewport; Project, Assets, AI Sandbox, and Systems
  switch to GUI surfaces and clear the scene viewport so those workflows do not
  have to be operated from the console dock.
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
- when a subsystem is touched, file names, module names, and exported surfaces
  should move toward consistent professional ownership instead of growing more
  orphan naming

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
  software, SDL, SFML, and Raylib do not drift without tooling feedback
- OpenGL launcher/editor flicker is still an active runtime defect. It appears
  tied to GUI/menu frame changes and must stay tracked as an OpenGL/frame
  synchronization issue until a local manual run proves otherwise.

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
- additional loaded helpers may be used only as explicitly allowed drafting or
  review lanes, and their output remains proposal material until build/runtime
  evidence and human review promote it
- for direct helper drafting, use LM Studio `/v1/responses` or
  `/v1/chat/completions` with bounded output, and retry without any reasoning
  field when the selected model rejects explicit reasoning configuration

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
- ProjectLauncher view for project launch/update/context selection
- Build/Output view for logs, diagnostics, and release/build evidence
- Asset Browser view for decoded image/model thumbnails, active project assets,
  and import/organization actions. The current `Assets` tab is only the first
  file-type-card version of that view.

These views should be backed by reusable GUI controls and custom UI powered by
an automated texture-atlas system, not by hardcoded editor-only tab strips that
cannot scale.

Current editor-shell gaps:

- the World Outliner needs stronger grouping, clipping, and resizable columns;
  the current compact button rows are a first cleanup pass, not the final
  desktop-grade control
- the engine GUI now has reusable `tab_bar`, `scroll_text_panel`, and modal
  focus overlay paths; selectable text is currently row-level and must grow into
  true text-range selection/copy support
- the current file browser and asset cards are intentionally first-pass
  controls. They still need bounded columns, filtering, real decoded thumbnails,
  rename/move/import actions, and a code/text editor surface for scripts.
- the GUI still needs context menus, popouts, dockable editor windows,
  persisted layout profiles, resize cursors, resize handles, and column controls
- global UI scaling should behave like normal desktop software, with explicit
  user scale/font controls instead of one hardcoded pixel density
- separate editor windows/domains are still needed inside the application:
  project/game editor, software/tool editor, self-iteration sandbox, and AI
  visualizer should be independently launchable/dockable surfaces
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

## Generated project shell self-tests

Run these from the repository root after building `ConsoleApplication1`:

```powershell
.\x64\Debug\ConsoleApplication1.exe --editor-project-self-test sandbox
.\Projects\Sandbox\bin\windows\Debug\x64\Sandbox.exe --project-self-test
.\x64\Debug\ConsoleApplication1.exe --editor-project-self-test projectlauncher
.\Projects\ProjectLauncher\bin\windows\Debug\x64\ProjectLauncher.exe --project-self-test
```

The first command materializes and builds the selected shell from the real
engine binary. The second command proves the generated child output is runnable
without opening GUI windows. The Sandbox route must report the
engine-self-iteration sandbox identity; ProjectLauncher must report its launcher
tool identity.

## Troubleshooting checklist

- verify the expected backend/config macros are enabled
- launch Windows smoke tests from `x64/Debug/` or `x64/Release/`
- prefer engine-owned capture output over ad hoc desktop grabs
- treat black or invalid software captures as failed proof that needs
  investigation, not as a successful screenshot
- keep Windows resources under `Engine/resource/`
- keep local compiled AI artifacts out of the repo
- when investigating backend issues, prefer backend-local fixes over broad
  multiplexer edits unless the shared layer is clearly proven at fault
