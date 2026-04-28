# Epoch Roadmap

## Mission

Build Epoch into one professional, engine-owned runtime and editor shell for:

- project creation, editing, play, scripting, tooling, and updates
- renderer and systems tooling that stay honest across backends
- engine-owned GUI/text/input instead of middleware-owned editor behavior
- a staged two-role AI control loop that stays reviewable and evidence-gated
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
9. External local LLMs are development helpers, not a third in-engine runtime
   role.
10. The engine keeps exactly two in-engine AI runtime roles:
    - `EpochBot`
    - local MCP/control

## Release And Source Policy

- Public docs must always distinguish:
  - current development source
  - published stable runtime release
  - bootstrap updater-shell release when one exists
- Packaged runtime assets use versioned platform names:
  - `epoch_win10_x64_vX.Y.Z.zip`
  - `epoch_linux_x64_vX.Y.Z.tar.gz`
- Bootstrap updater-shell assets use their own versioned names:
  - `epoch_updater_shell_only_win10_x64_vX.Y.Z.zip`
  - `epoch_updater_shell_only_linux_x64_vX.Y.Z.tar.gz`
- Packaged version identity travels with the tagged source and release asset
  names rather than standalone packaged version files.
- The updater remains binary-first:
  check the newest packaged runtime first, then continue to source only when
  the packaged runtime is already version-equal or newer.
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
  required Windows and Linux hosted lanes use the asset-light
  `epoch_ci_headless` smoke until the full graphics dependency surface is
  reliable there; full vcpkg-backed app builds remain local/release validation
  work, not required hosted GUI checks
- stable Windows/Linux packaged release path with explicit bootstrap/runtime
  distinction
- launcher/editor separation and the current project-centric runtime shell
- current multicontext baseline:
  real backend panes, real detach/redock flow, and no fake demo-launch path

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
- Hosted CI should remain headless/build-only until GUI/window tests have a
  deterministic runner-safe harness; no explicit Node 20 setup remains in the
  checked workflows.

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
- a live Systems workspace with graph surfaces, backend ownership visibility,
  first time-control diagnostics, and build-confidence/feature-probe status
  already on-screen
- an AI workspace split into Dark Factory, Tooling, Engine AI, Software,
  Training, and Ops/How-To domains so rebuild/tooling/training controls are
  discoverable in the editor shell
- a visual Dark Factory surface that summarizes evidence readiness, active
  planner/builder/verifier/gate state, continuous build status, and tool-harness
  activity
- an editor-owned continuous AI build lane that watches active project/script
  evidence, queues one child-project build at a time, and feeds successful
  build artifacts back into staged AI packets for verifier/gate review
- an AI tool harness that builds/runs the selected script through the real
  editor host, captures before/after editor state, records MCP evidence, and
  stages packets from successful tool actions
- a committed AI factory-loop contract that records the planner, executor,
  builder, verifier, and gate handoff rules for future replay/training work
- explicit AI iteration packet staging and local-vs-committed AI artifact
  separation
- AI iteration packets now carry review-gate state, evidence readiness, and
  the current `planner -> executor -> builder -> verifier -> gate` loop stage
  so future replay/training passes can reason from staged evidence instead of
  guessing from editor state

## Phase Progress

- [x] Phase 1: Repository structure, research intake, workspace placement,
  initial build-path honesty, and documentation cleanup.
- [x] Phase 2: Project-centric runtime shell, generated project creation,
  project/script proof rows, launcher/editor separation, and baseline
  Project/Scripts workspace flow.
- [~] Phase 3: Systems workspace, time spine, backend ownership diagnostics,
  build-confidence surfacing, and hosted/local build reliability.
- [ ] Phase 4: GUI maturity, drag/drop, text-input smokes, editor polish, and
  OpenGL startup-flicker/root-cause cleanup.
- [~] Phase 5: Two-role AI control loop, captured task packets, review gates,
  dataset/eval promotion, and tool-schema replay.
- [ ] Phase 6: Primitive/object authoring, procedural world spine, 2D vertical
  slice, and material/cellular simulation boundaries.
- [ ] Phase 7: Android-first mobile bring-up with one renderer, one input path,
  one packaging story, and lifecycle stability.

## Active Mission Tracks

### 1. Runtime And Multicontext Ownership

- keep SDL, SFML, Raylib, Vulkan, OpenGL, and software behavior converging
  instead of drifting into backend-specific hacks
- finish IDE-class docking/popout behavior so detach, input ownership, z-order,
  redock, and startup presentation remain stable
- eliminate remaining OpenGL startup flicker and related launch cosmetics
- tighten terminology so runtime/module/doc names stop leaning on ambiguous
  legacy words like `multiplexer`

### 2. Project-Centric Runtime And Scripted Pipeline

- keep project creation, project play, script build/run, and generated project
  discovery truthful
- keep the default demo path honest:
  generated or discovered projects should build, launch, and hand any
  declared demo model through the engine-owned script host without falling
  back to confusing sample-only behavior
- keep the Mini Sponza demo owned by `ProjectLauncher`, while `Sandbox`
  stays the AI/engine-iteration shell
- continue replacing hardcoded built-in sample assumptions with project-owned
  runtime flow
- keep the already-landed project/scripts shell honest instead of letting it
  drift back toward placeholder tooling
- keep the launcher centered on projects, contexts, settings, and updates
- keep the editor centered on `Project`, `Scripts`, `Systems`, `AI`, and
  `Output`

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
- improve project, script, AI, systems, and output surfaces until the shell
  reads as a professional editor rather than a debug console
- keep AI workspace domains organized around concrete jobs: rebuild/control,
  editor tooling, normal engine guidance, software artifacts, training
  promotion, and operator instructions
- keep launcher and editor theming intentionally separate

### 6. AI Control, Training, And Review Loop

- standardize the full two-role runtime story:
  `EpochBot` plus local MCP/control
- build on the current iteration-packet/capture roots already present in the
  editor instead of inventing a second AI staging path
- make staged packets the first durable handoff between planner/executor work
  and builder/verifier review, including loop-stage and gate-state metadata
- keep continuous AI builds nonblocking and single-flight so the engine can
  produce fresh evidence while preserving explicit review gates
- train from real editor tool actions by capturing before/after state from the
  selected script harness before promoting any dataset/eval records
- treat `Engine/ai/factory/continuous_build_loop.json` as the current contract
  for the self-improvement factory until a replay runner can enforce it
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

1. Fix GitHub/workflow reliability and keep local/hosted build truth aligned.
2. Strengthen the Systems workspace with deeper pacing diagnostics and backend
   convergence guidance.
3. Carry the time spine deeper into runtime and scene ownership.
4. Tighten the two-role AI capture, replay, review, and promotion loop until
   Phase 5 can run from staged packets with builder/verifier gates.
5. Keep UI/editor maturity moving forward, especially text/input reliability,
   shell polish, drag/drop, and backend-window stability.
6. Complete the primitive/object system and keep it aligned with the project
   runtime shell.
7. Start Android with an honest single-context bring-up, touch/input
   integration, packaging/install path, and asset-resolution discipline.
8. Continue safe include/src restructuring and MSVC/CMake synchronization
   whenever touched areas can be normalized without collateral damage.

## Acceptance Gates

- The editor runs real projects/scenes instead of sample-launch illusions.
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

## Reference Inputs

- `README.md`
- `Engine/docs/`
- staged research under
  `Engine/examples/ConsoleApplication1/workspace/research/`
- release/changelog history under `Changes/`
- utility tools like `botface.html` only after reviewed extraction, not by
  default
