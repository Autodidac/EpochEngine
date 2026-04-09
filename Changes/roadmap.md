# Epoch Roadmap

## Table of Contents

- [Mission](#mission)
- [Operating Rules](#operating-rules)
- [Reference Inputs](#reference-inputs)
- [Compatibility Baseline](#compatibility-baseline)
- [Renderer Strategy](#renderer-strategy)
- [Priority Tracks](#priority-tracks)
- [Phase 1 - Spine Foundation](#phase-1---spine-foundation)
- [Phase 2 - Project-Driven Runtime and Tool Shell](#phase-2---project-driven-runtime-and-tool-shell)
- [Phase 3 - Systems Workspace and Renderer Tooling](#phase-3---systems-workspace-and-renderer-tooling)
- [Phase 4 - Time-System Spine](#phase-4---time-system-spine)
- [Phase 5 - Two-Role AI Training Spine](#phase-5---two-role-ai-training-spine)
- [Phase 6 - UI and Editor Maturity](#phase-6---ui-and-editor-maturity)
- [Phase 7 - Procedural World and Time-Node Authoring](#phase-7---procedural-world-and-time-node-authoring)
- [Phase 8 - Automation, Capture, and Documentation Loop](#phase-8---automation-capture-and-documentation-loop)
- [Helper-First Automation Rules](#helper-first-automation-rules)
- [Current Push Priorities](#current-push-priorities)
- [Acceptance Criteria](#acceptance-criteria)

## Mission

Build Epoch into a professional engine and creative software platform with one
engine-owned runtime, one modular editor shell, one honest project-driven
workflow, custom UI powered by an automated texture-atlas system, a shared
time-system spine, and renderer/tooling surfaces that scale from modern laptops
to stronger desktops without turning the engine into a pile of disconnected
wrappers.

## Operating Rules

1. The engine owns the workflow: project creation, editing, play/run,
   scripting, tooling, AI, and runtime travel through one spine.
2. The UI stays modular. Reusable GUI controls belong in shared GUI layers, not
   in hardcoded editor-only strips.
3. Naming must keep converging toward professional consistency. Legacy orphan
   names such as older `aengine*`-style units and misleading labels like
   `multiplexer` are transitional debt until files, modules, and exported
   surfaces match one coherent engine structure.
4. Epoch is a time-based engine in the simulation sense: fixed-step ownership,
   frame pacing, pause/resume, time scaling, single-step, and future
   replay/timeline hooks.
5. AI has two in-engine runtime roles only:
   - internal EpochBot
   - local MCP/control bots that can both operate the engine and train
     EpochBot
6. External local LLMs such as LM Studio are development helpers for drafting,
   evaluation, screenshot review, bounded design help, and acceleration. They
   are not a third engine runtime role.
7. Git stores repo-safe text artifacts such as JSON, JSONL, schemas, manifests,
   evals, prompts, and docs. Weights, checkpoints, compiled models, and caches
   stay local or ship through releases only.
8. Validation must happen from asset-bearing output folders and must clean up
   windows, stray logs, and bad-folder output afterward.
9. Parented multicontext presentation must expose one clean pane per active
   backend. Nested backend child surfaces are implementation detail, not the
   user-facing dock concept.
10. No normal engine source file should grow beyond 1000 lines. Oversized files
    are temporary exceptions that should be split deliberately.

## Reference Inputs

- Current steering docs in `README.md` and `Engine/docs/`
- Deep-research PDF guidance added under `Changes/` for the six-month 2D
  priority track and solo-engine strategy
- Future integration source for procedural/time-node authoring:
  `O2L` code when it is actually present in the workspace

## Compatibility Baseline

### Default automatic target

- 6-core desktop CPU class
- GTX 1660 Ti-era GPU class
- modern Linux laptops and desktops

### Support tiers

- **Baseline**: stable editor/runtime path with broad reach
- **Standard**: fuller OpenGL/Vulkan-capable hardware and richer tooling
- **Extended**: heavier libs/features enabled explicitly per project

The engine should support broad real hardware by default and push heavier
features behind opt-in tiers instead of forcing every project into the heaviest
stack.

## Renderer Strategy

### Baseline renderer order

`visibility -> surface -> lighting -> temporal -> reconstruction -> present`

### Preferred direction

- GPU-driven baseline first
- frame/task graph guidance
- async/task-friendly work partitioning
- temporal history and reconstruction
- stronger resource binding paths where supported

### Standard or Extended only

- ray tracing
- path tracing
- mesh shaders
- virtual shadowing
- sparse-resource-heavy flows
- similarly heavy modern rendering paths

These remain support-tier or project-opt-in work, not the default baseline.

## Priority Tracks

### Six-month 2D priority track

- keep a real 2D-oriented project path visible in the launcher/project shell
- prioritize 2D gameplay, tooling, UI, scripting, and asset workflows without
  abandoning the broader engine mission
- use the 2D priority track to accelerate solo-developer usefulness while the
  full renderer and tooling stack keeps maturing

### Naming and module normalization

- normalize file names, module names, exported types, and directory structure
- phase out misleading transitional labels such as `multiplexer`
- treat old `aengine*` naming as technical debt to be retired systematically

## Phase 1 - Spine Foundation

- [x] Preserve the working runtime, logging, timing, and backend bootstrap
      behavior while normalizing ownership into canonical core services.
- [x] Keep the engine running through one shared runtime/perf/logging spine.
- [x] Normalize active module/file ownership instead of growing a second naming
      mess beside Epoch.

## Phase 2 - Project-Driven Runtime and Tool Shell

- [x] Replace fake editor launch paths with real project/scene play from the
      active project.
- [x] Turn `Run Game` into project play/testing instead of a permanent built-in
      sample-game launcher.
- [ ] Move remaining built-in sample launches behind project templates or script
      actions so the editor path stays honest.
- [ ] Finish the project shell in `Engine/src/aeditor.scene.cpp` so project
      profiles, script profiles, runtime scene ids, and seed entities live in
      the scene layer instead of scattered editor state.
- [x] Add the first project creation flow that duplicates the engine-owned
      shell into either a game project or a software/tool project.
- [x] Support both generated-project modes explicitly:
      duplicated engine source/layout and static engine compilation through the
      exported `Engine/include/` surface when a project embeds the engine.
- [x] Make the include-surface/static-engine path first-class in generated
      project and scripting flows so embedded-engine projects stay just as
      supported as duplicated-source projects.
- [x] Add a real scripting/project dock with scripts, source paths, build/run
      actions, and compile/load diagnostics.
- [x] Keep the launcher/project shell centered on projects, contexts, settings,
      updates, and future automation instead of legacy demo/game menus.
- [ ] Keep the live shell organized around `Project`, `Scripts`, `Systems`,
      `AI`, and `Output` workspaces backed by reusable GUI controls.
- [x] Keep a professional 2D project path visible and honest instead of falling
      back to stale sample-profile naming.

## Phase 3 - Systems Workspace and Renderer Tooling

- [x] Turn `Systems` into a live tooling surface instead of placeholder text by
      rendering engine-generated graph textures inside the docked UI.
- [x] Render frame graph / render graph views as generated textures with pan,
      zoom, and clipping.
- [x] Render task/thread graph views beside them and keep them readable on wide
      surfaces.
- [x] Surface compatibility tier and renderer-stage guidance inside the editor.
- [ ] Add deeper pacing/perf-select diagnostics and hardware support guidance on
      top of the current graph surfaces.
- [ ] Use this surface to converge backend behavior across OpenGL, Vulkan,
      software, SDL, SFML, and Raylib instead of letting them drift.
- [x] Keep parented multicontext behavior honest: the visible pane should not
      degrade into fake extra dock wrappers or misleading nested windows. The
      current stable rule is hosted SDL/SFML panes with real backend children
      rendering inside them, plus a bridged visible `GLFW30` Raylib pane.
- [x] Keep the shared preview marker honest by deriving the visible ground-hit
      spot from the real center camera ray before any editor-focus fallback.
- [ ] Tighten multicontext terminology so file names, modules, and docs stop
      describing the parented backend grid with ambiguous legacy terms such as
      `multiplexer` when clearer Epoch-aligned ownership names are ready.

## Phase 4 - Time-System Spine

- [x] Establish the first shared simulation clock ownership in `core.time`.
- [x] Add fixed-step accumulation, pause/resume, time scaling, single-step, and
      editor-facing stats as the initial time-spine milestone.
- [x] Surface the first time diagnostics and controls through the Systems
      workspace.
- [x] Surface frame step budget and max-steps-per-frame pacing controls through
      the Systems workspace so pacing policy is visible instead of implied.
- [ ] Continue wiring the time spine through runtime and scene ownership so play
      mode, scripts, and later timeline/replay features all consume one timing
      model.
- [ ] Add pacing controls, fixed-step policy visibility, and future
      record/replay/timeline hook points without trying to ship the whole replay
      stack at once.

## Phase 5 - Two-Role AI Training Spine

- [ ] Keep only two in-engine AI roles:
      internal EpochBot and local MCP/control bots.
- [ ] Let the MCP/control layer both operate the engine and feed the training
      loop for EpochBot.
- [ ] Treat `append_training_sample()` and MCP capture as raw/staged data, then
      promote curated records into `Engine/ai/datasets/curated/` and
      `Engine/ai/evals/`.
- [ ] Document and support deleting outdated or bad training artifacts when the
      training direction changes.
- [ ] Keep repo-safe AI assets under `Engine/ai/` and compiled/local artifacts
      under `workspace/ai/`.
- [ ] Detect local helper availability at the start of a phase, then use the
      first two `/v1/models` entries as helper drafting pools when the operator
      has approved helper-first mode.

## Phase 6 - UI and Editor Maturity

- [ ] Replace remaining ad hoc editor-only layout logic with a full modular GUI
      workspace system backed by shared controls and the atlas-driven UI
      pipeline.
- [ ] Improve project, script, AI, systems, and output surfaces so the shell
      feels closer to Unreal/Godot than a debug console.
- [ ] Support application/tool-style editor projects alongside game projects so
      Epoch remains a creative software platform as well as a game engine.
- [ ] Keep backend child-window ownership, padding, clipping, and load
      presentation clean across all active contexts.
- [ ] Keep the parented multicontext shell auto-fitted to the desktop work area
      by default so the honest context matrix stays visible on baseline
      hardware.

## Phase 7 - Procedural World and Time-Node Authoring

- [ ] Build a SpeedTree-like modular procedural world authoring path on top of
      Epoch's time/node direction.
- [ ] Support time-based and node-based authoring for vegetation, modular world
      generation, and graph-driven procedural assets.
- [ ] Add editor tooling for graph-driven procedural assets without breaking the
      main modular GUI shell.
- [ ] Absorb future O2L time/node code as an integration source when it is
      present in the workspace. O2L is a later source/input for this phase, not
      a dependency for the current implementation pass.

## Phase 8 - Automation, Capture, and Documentation Loop

- [ ] Make engine-owned smoke/capture validation the default proof path.
- [ ] Always build from the repo root and launch from asset-bearing output
      folders such as `x64/Debug/` or `x64/Release/`.
- [ ] Avoid stray logs and bad-folder runs; clean disposable output created
      only for temporary validation.
- [ ] Refresh the README multicontext screenshot automatically every 10th
      feature version, or sooner when layout/color/docking changes make the
      current proof misleading.
- [ ] Keep docs strict enough that future automated passes can follow the
      build, launch, test, capture, commit, and push loop without improvising.

## Helper-First Automation Rules

- At the start of a phase, detect local helper availability through
  `/v1/models`.
- Ask whether helper-first mode should be used if that preference has not
  already been made explicit by the operator.
- Use the first two models as helper drafting pools when available.
- Fill up to four parallel drafting lanes per helper model when there is enough
  separable work.
- Prefer the LM Studio `/v1/responses` helper path for direct drafting, with
  `input` payloads and `reasoning.effort = none` so the helper returns usable
  text instead of wasting budget on hidden chains.
- Keep the first detected model as the only runtime-parity/in-engine smoke
  model.
- If a helper returns blank `content` but useful `reasoning_content`, harvest
  the useful output instead of discarding the helper pass.
- Use helper models for roadmap wording, code-shape proposals, screenshot
  review, doc rewrites, bug triage, subsystem design, changelog drafting, and
  file-splitting plans before spending main-model effort on integration.

## Current Push Priorities

1. Finish the remaining project-shell seams and generated project/profile flow.
2. Keep the Systems workspace growing into a real renderer/runtime tooling
   surface.
3. Continue carrying the time spine deeper into runtime and scene ownership.
4. Tighten the two-role AI capture/training loop and MCP-driven curation path.
5. Keep multicontext/backend child-window ownership honest across all active
   backends.
6. Keep naming cleanup active whenever a touched area can be normalized without
   collateral damage.

## Acceptance Criteria

- The editor runs projects and scenes honestly instead of pretending through
  sample launchers.
- The Systems workspace shows real graph/tooling surfaces plus time diagnostics.
- The engine owns a shared simulation clock and exposes the first real time
  controls.
- The AI/training loop stays clearly two-role in-engine and keeps repo-safe
  text assets separate from local-only compiled artifacts.
- The multicontext proof shows all six panes honestly, with hidden helper hosts
  and no fake visible dock wrappers.
- The docs stay strong enough to steer future automated passes without needing
  to rediscover the architecture from scratch.
