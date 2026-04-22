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

## Scripting and reload workflow

- script sources belong to the engine/project scripting tree used by the active
  project
- engine-owned compiled scripting means project/game logic compiles with the
  engine/project build; it is not a text-macro or string-eval layer
- the scripting/project phase must account for both integration modes:
  duplicated engine-source projects and embedded-engine builds that include the
  engine surface from `Engine/include/`, `Engine/modules/`, `Engine/src/`,
  `Engine/src/scripts/`, and `Engine/resource/`
- the scripting/project dock should expose script lists, source paths, run/build
  actions, and compile/load diagnostics
- script source resolution should prefer the active project's local `scripts/`
  folder before falling back to template or engine-owned script roots, so the
  dock and editor run actions operate on the real generated project shell
- build diagnostics should now cover the generated child-project build path too:
  entry source, generated project file, build script, build log, and expected
  output executable should all be visible from the Project workspace
- the Project workspace should also surface simple existence checks for the
  manifest, entry source, build script, `project.paths.txt`, expected output,
  build log, and active script source so the user can tell whether the shell is
  real without leaving the editor
- hot reload remains a development feature and needs smoke coverage instead of
  trust

## Systems workspace direction

- `Systems` is now the active tooling surface for:
  - frame graph / render graph
  - task graph / multithreading
  - time-system diagnostics and controls
  - pacing / perf select
  - diagnostics
- graph views render as engine-generated textures inside the docked UI
- graph views support pan/zoom and remain clipped when they are wider than the
  available panel
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

Epoch documents two engine AI runtime roles:

- internal EpochBot
- local MCP/control bots that can operate and train EpochBot

External local LLMs such as LM Studio are development helpers. They can help
with testing, evals, dataset cleanup, and faster iteration, but they are not a
third runtime AI role inside the engine.

Data rules:

- curated repo-safe assets belong in `Engine/ai/`
- `Engine/examples/ConsoleApplication1/workspace/auto_train.jsonl` and `Engine/examples/ConsoleApplication1/workspace/mcp_capture.jsonl` are raw/staged
  capture paths
- checkpoints, compiled local models, and caches stay under local
`Engine/examples/ConsoleApplication1/workspace/ai/` paths
- outdated or bad training data should be deleted or replaced when the training
  direction changes
- helper-first passes should check `/v1/models` at the start of a phase, use
  the first two models as drafting pools when available, and keep the first
  detected model as the only runtime-parity/in-engine smoke model
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

## Editor shell direction

The live editor shell should continue to organize around reusable workspaces:

- `Project`
- `Scripts`
- `Systems`
- `AI`
- `Output`

These should be backed by reusable GUI controls and custom UI powered by an
automated texture-atlas system, not by hardcoded editor-only tab strips that
cannot scale.

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
