# Runtime Operations

This guide documents the current intended workflow for running Epoch honestly:
editor, project, scripts, systems, AI, and runtime should all travel through
the same engine-owned path.

## Startup model

- enter through the normal engine bootstrap so scripting, AI, backend setup,
  logging, capture, and project/runtime selection share one path
- desktop example wiring still lives under `Engine/examples/ConsoleApplication1/`
- multicontext behavior depends on the active runtime/config macros documented
  in `aengineconfig_flags.md`

## Project-driven runtime direction

- the editor should play the active project and scene, not a hardcoded sample
  game menu
- `Play Project` should reject non-project scene ids from the editor path so the
  live shell cannot quietly fall back to built-in sample launches
- built-in sample games should move behind project templates or script actions
- the long-term target is a Unity/Unreal-style project shell generated from
  duplicated engine source/layout
- that project shell should support both game projects and software/tool
  projects so Epoch remains a creative software platform as well as a game
  engine
- the first generated shell flow should create a real on-disk project root,
  manifest, world file, script stub, and README for both game and tool projects
- an editor/project launcher profile is valid here as a prestep for choosing
  projects, contexts, settings, and future automation flows
- `aeditor.scene.cpp` should own project profiles, script profiles, runtime
  scene ids, and seed entities
- `aeditor.cpp` should act as the live shell over that scene/project data, not
  as a second hardcoded editor universe

## Scripting and reload workflow

- script sources belong to the engine/project scripting tree used by the active
  project
- engine-owned compiled scripting means project/game logic compiles with the
  engine/project build; it is not a text-macro or string-eval layer
- the scripting/project dock should expose script lists, source paths, run/build
  actions, and compile/load diagnostics
- build diagnostics can start as honest source-path validation and loader
  reporting, then grow into fuller project/script compile diagnostics
- hot reload remains a development feature and needs smoke coverage instead of
  trust

## Systems workspace direction

- `Systems` is the future tooling surface for:
  - frame graph / render graph
  - task graph / multithreading
  - pacing / perf select
  - diagnostics
- graph views should render as engine-generated textures inside the docked UI
- graph views must support pan/zoom and remain clipped when they are wider than
  the available panel
- this surface should help unify renderer/backend behavior instead of becoming
  another debug text dump

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
- `workspace/auto_train.jsonl` and `workspace/mcp_capture.jsonl` are raw/staged
  capture paths
- checkpoints, compiled local models, and caches stay under local
  `workspace/ai/` paths
- outdated or bad training data should be deleted or replaced when the training
  direction changes

## Editor shell direction

The live editor shell should continue to organize around reusable workspaces:

- `Project`
- `Scripts`
- `Systems`
- `AI`
- `Output`

These should be backed by reusable GUI controls and the atlas-driven UI system,
not by hardcoded editor-only tab strips that cannot scale.

## Troubleshooting checklist

- verify the expected backend/config macros are enabled
- launch Windows smoke tests from `x64/Debug/` or `x64/Release/`
- prefer engine-owned capture output over ad hoc desktop grabs
- keep Windows resources under `Engine/resource/`
- keep local compiled AI artifacts out of the repo
- when investigating backend issues, prefer backend-local fixes over broad
  multiplexer edits unless the shared layer is clearly proven at fault
