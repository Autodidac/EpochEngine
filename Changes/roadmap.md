# Epoch Roadmap

## Mission

Build Epoch into one professional engine-owned runtime and editor shell with:

- honest project creation, editing, play, scripting, tooling, and update flow
- a shared time-system spine instead of backend-local timing behavior
- renderer/tooling surfaces that scale from baseline hardware upward
- engine-owned GUI/text systems rather than middleware-owned editor behavior
- a staged two-role AI control loop that stays reviewable and evidence-gated

## Working Rules

1. The engine owns the workflow. Project creation, play, tooling, scripting,
   AI, and runtime travel through one spine.
2. Epoch keeps exactly two in-engine AI runtime roles:
   - `EpochBot`
   - local MCP/control
3. External local LLMs are development helpers, not a third runtime role.
4. Validation must come from asset-bearing outputs and must clean up after
   itself.
5. Parented multicontext UI must expose one clean pane per backend. Nested
   backend child windows are implementation detail.
6. Broad hardware support stays the default. Heavy features remain tiered or
   opt-in.
7. Research imports are staged first, reviewed second, and promoted only when
   they materially improve repo truth.

## Reference Inputs

- `README.md`
- `Engine/docs/`
- staged research under `workspace/research/`
- release/changelog history under `Changes/`
- utility tools like `botface.html` only after reviewed extraction, not by
  default

## Snapshot

- **Phase 1 complete:** engine spine foundation and staged research-import path
- **Phase 2 complete:** project-centric runtime/tool shell and generated
  project flow
- **Phase 3 in progress:** systems workspace and renderer/runtime tooling
- **Phase 4 in progress:** time-system spine ownership
- **Phase 5 onward:** AI control/training loop, UI maturity, procedural
  authoring, and stronger automation

## Compatibility and Runtime Direction

### Baseline target

- 6-core desktop CPU class
- GTX 1660 Ti-era GPU class
- modern Linux laptops/desktops

### Backend direction

- editor should converge toward **single-context OpenGL**
- launcher should converge toward **single-context software**
- backend switching should be explicit
- inactive backends should be torn down, not left running invisibly

### Renderer order

`visibility -> surface -> lighting -> temporal -> reconstruction/post -> present`

### 2D priority track

- OpenGL remains the current six-month 2D golden path
- software stays useful as a correctness/capture oracle
- the 2D lane is a vertical slice through the real engine spines, not a
  separate 2D-only subsystem world

## Phase 1 - Spine Foundation

**Status:** complete

- [x] Preserve the working runtime, logging, timing, and backend bootstrap
      behavior while normalizing ownership into canonical core services.
- [x] Keep the engine running through one shared runtime/perf/logging spine.
- [x] Normalize active module/file ownership instead of growing a second naming
      mess beside Epoch.
- [x] Add a lightweight research-import path so new PDF/HTML planning material
      can be converted into staged text artifacts with provenance before it
      changes roadmap language, datasets, or automation policy.
- [x] Keep staged research generic rather than tied to `Changes/`; reviewed
      promotion is what turns staged input into repo truth.

## Phase 2 - Project-Centric Runtime and Tool Shell

**Status:** complete

- [x] Replace fake editor launch paths with real project/scene play from the
      active project.
- [x] Turn `Run Game` into project play/testing instead of a permanent
      built-in sample-game launcher.
- [x] Move remaining built-in sample launches behind project templates or script
      actions so the editor path stays honest.
- [x] Finish the project shell in `Engine/src/aeditor.scene.cpp` so project
      profiles, script profiles, runtime scene ids, and seed entities live in
      the scene layer instead of scattered editor state.
- [x] Add the first project creation flow that duplicates the engine-owned
      shell into either a game project or a software/tool project.
- [x] Discover generated non-template project manifests into the live project
      list so newly created shells become selectable/playable editor projects.
- [x] Support both generated-project modes explicitly:
      duplicated engine source/layout and embedded-engine compilation through
      headers, modules, source, scripts, and resources together.
- [x] Add a real scripting/project dock with scripts, source paths, build/run
      actions, and compile/load diagnostics.
- [x] Keep the launcher/project shell centered on projects, contexts, settings,
      updates, and future automation instead of legacy demo/game menus.
- [x] Keep the live shell organized around `Project`, `Scripts`, `Systems`,
      `AI`, and `Output`.

## Phase 3 - Systems Workspace and Renderer Tooling

**Status:** active

- [x] Turn `Systems` into a live tooling surface instead of placeholder text by
      rendering engine-generated graph textures inside the docked UI.
- [x] Render frame graph / render graph views as generated textures with pan,
      zoom, and clipping.
- [x] Render task/thread graph views beside them and keep them readable on wide
      surfaces.
- [x] Surface compatibility tier and renderer-stage guidance inside the editor.
- [x] Keep parented multicontext behavior honest: visible panes should be real
      backend child surfaces, and helper shells should stop lingering as stray
      top-level windows after redock.
- [x] Keep the shared preview marker honest by deriving the visible ground-hit
      spot from the center camera ray.
- [ ] Add deeper pacing/perf-select diagnostics and hardware support guidance on
      top of the current graph surfaces.
- [ ] Use this surface to converge backend behavior across OpenGL, Vulkan,
      software, SDL, SFML, and Raylib instead of letting them drift.
- [ ] Make backend ownership explicit in the live tooling surface so the active
      backend is obvious and switching is deliberate.
- [ ] Tighten multicontext terminology so file/module/doc names stop leaning on
      ambiguous legacy terms like `multiplexer`.

## Phase 4 - Time-System Spine

**Status:** active

- [x] Establish the first shared simulation clock ownership in `core.time`.
- [x] Add fixed-step accumulation, pause/resume, time scaling, single-step, and
      editor-facing stats as the initial milestone.
- [x] Surface the first time diagnostics and controls through the Systems
      workspace.
- [x] Surface frame-step budget and max-steps-per-frame pacing controls through
      the Systems workspace.
- [ ] Continue wiring the time spine through runtime and scene ownership so
      play mode, scripts, and later replay/timeline features all consume one
      timing model.
- [ ] Add future replay/timeline hook points without pretending the whole
      replay stack ships at once.

## Phase 5 - Two-Role AI Training Spine

**Status:** active foundation, broader loop incomplete

- [x] Stage bounded AI work into explicit iteration packets under
      `workspace/ai/iterations/`.
- [x] Surface the current AI iteration/capture/provenance roots directly inside
      the editor AI workspace.
- [ ] Standardize the full two-role runtime story:
      `EpochBot` plus local MCP/control.
- [ ] Use MCP tool schemas as the canonical tool-bus contract and replay shape.
- [ ] Separate raw observation capture from curated dataset/eval promotion.
- [ ] Require build/runtime/log evidence before AI-assisted promotion.
- [ ] Grow toward a real
      `planner -> executor -> builder -> verifier -> gate`
      loop without drifting into blind autonomy claims.
- [ ] Keep committed AI assets in `Engine/ai/` and local/generated artifacts in
      `workspace/ai/`.

## Phase 6 - UI and Editor Maturity

**Status:** active

- [ ] Add repeatable typed-text editor smokes so input regressions stop hiding
      behind screenshots and click-only probes.
- [ ] Replace remaining ad hoc editor-only layout logic with stronger shared GUI
      ownership.
- [ ] Improve project, script, AI, systems, and output surfaces so the shell
      feels closer to a professional editor than a debug console.
- [ ] Keep backend child-window ownership, padding, clipping, startup load
      presentation, and redock behavior clean across active contexts.
- [ ] Keep SDL3/SFML3 proxy panes behaving like honest top-level promoted
      windows when detached, then real docked panes again on intentional
      redock.
- [ ] Keep launcher and editor theming intentionally separate.
- [ ] Add a settings-level theme selector only after scoped theme ownership is
      stable.

## Phase 7 - Procedural World and Time-Node Authoring

**Status:** future-facing

- [ ] Build a SpeedTree-like modular procedural world authoring path on top of
      Epoch's time/node direction.
- [ ] Build the six-month 2D material/pixel world as a dedicated chunked
      simulation subsystem rather than an ECS-per-cell model.
- [ ] Keep ECS/entity ownership for macro gameplay actors while the dense
      cellular/material world remains specialized.
- [ ] Absorb future O2L time/node code when it is actually present in the
      workspace.

## Phase 8 - Automation, Capture, and Documentation Loop

**Status:** active

- [ ] Make engine-owned smoke/capture validation the default proof path.
- [ ] Always build from repo root and launch from asset-bearing output folders.
- [ ] Keep disposable validation output and failed AI iteration debris easy to
      remove without harming promoted evidence.
- [ ] Refresh README proof images whenever layout/color/docking changes make the
      current proof misleading.
- [ ] Keep docs strong enough that future automated passes can follow the
      build, launch, test, capture, commit, and push loop without rediscovering
      architecture from scratch.

## Current Push Priorities

1. Strengthen the Systems workspace with backend ownership, pacing, and
   hardware guidance.
2. Carry the time spine deeper into runtime and scene ownership.
3. Tighten the two-role AI capture, review, and promotion loop.
4. Keep UI/editor maturity moving forward, especially text/input reliability,
   shell polish, and backend-window stability.
5. Keep packaged Windows and Linux releases aligned with the intended main
   runtime identity while bootstrap updater shells stay explicit and lean.
6. Keep naming cleanup active whenever touched areas can be normalized without
   collateral damage.

## Acceptance Criteria

- The editor runs real projects/scenes instead of sample-launch illusions.
- The Systems workspace shows real graph/tooling surfaces plus time diagnostics.
- The engine owns one shared simulation clock and exposes the first real time
  controls.
- The AI/training loop stays two-role in-engine and keeps committed vs. local
  artifacts separate.
- Multicontext proof stays honest: all six panes are real, SDL/SFML detach to
  real top-level shells, and helper hosts do not linger incorrectly.
- Packaged Windows and Linux releases boot the intended runtime identity by
  default and stay version-aligned with the tagged source snapshot.
- Docs remain strong enough to steer future automated passes without having to
  rediscover the architecture each time.
