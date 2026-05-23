# Agent Notes

## Repository Shape

- Epoch is a C++23 engine/tooling repo. Active engine work is under
  `Engine/src/`, `Engine/modules/`, `Engine/include/`, `Engine/resource/`,
  `Engine/ai/`, and `Engine/examples/`.
- Start with `README.md`, `Engine/docs/README.md`, and `Changes/cpp.md` before
  making broad changes. Build and runtime details live in `Engine/docs/build/`
  and `Engine/docs/engine/`.
- Use the documentation map, not random README guesses:
  `Changes/roadmap.md` is the active planning contract, `Changes/changelog.txt`
  records current version work, `Engine/docs/README.md` is the docs index,
  `Engine/docs/engine/runtime_and_editor_workflows.md` owns launcher/editor
  behavior, `Engine/docs/engine/gui_library_architecture.md` owns the shared
  GUI library/control-surface contract,
  `Engine/docs/engine/ai_training_memory_and_dataset_policy.md` owns AI
  capture/training policy, and `Engine/ai/README.md` plus
  `Engine/ai/control/continuous_build_loop.json` own the live AI loop contract.
- Stay inside this worktree when reading or editing docs. Do not copy README or
  `.codex` content from sibling worktrees or unrelated projects into Epoch.
- Do not edit generated output folders or local runtime artifacts unless the
  task is explicitly about those artifacts. Common generated/local paths include
  `build/`, `x64/`, `Engine/Bin/`, `Engine/build/`, `Engine/built/`, and
  runtime logs/captures.
- Runtime-created cache buckets are executable-local: updater work, temporary
  probes, extraction state, and managed helper tools belong in `cache/updates/`;
  downloaded package archives belong in `cache/packages/`; generated/runtime
  atlases belong in `cache/atlases/`. These are disposable local artifacts, not
  public release payload and not tracked source.
- Keep `addons/` local/offline. It contains extra starter projects and research
  imports that may later be reviewed into core Epoch, but it must not be added
  to the online repo by default.
- Repo-root `Projects/` is a local generated-project area. Treat generated
  project manifests, build outputs, logs, and `PROJECT_NOTES.md` as runtime
  evidence unless a task explicitly promotes a template or fixture into tracked
  source.
- Current `.epoch` scene/world files are metadata shells. The live editor
  preview still comes from engine-owned project profiles and seed entities in
  `editor.scene.cpp`; do not claim scene-file authoring is complete until the
  scene parser/serializer owns preview/runtime loading.
- Built-in mini-runtime/game modules remain in the kernel engine that ships with
  applications. Expose them to projects as package/script assets such as
  `engine_arcade`, not by moving their implementations into loose generated
  project scripts.
- Voxel field/pathing/tracing contracts and deterministic Forest Factory
  descriptors are core engine primitives. Heavy planetary terrain,
  multi-terrain authoring, FFT ocean, imported prototype demos, and
  game-specific world stacks are package candidates first and should flow
  through `cache/packages/`, review branches, or separate repos before any
  source promotion.
- Forest Factory gets its own editor 3D scene/window as a core feature, but
  generated projects include Forest Factory assets/scripts only after a visible
  package activation or main-scene use gate.

## Roadmap Discipline

- Sync the repository before starting substantive work: inspect the current
  branch, dirty state, remotes, and fetched upstream before editing project
  files.
- Treat Epoch as a professional production engine at every step. "First pass"
  means narrow scope, not throwaway code: every checked-in change should have
  clear ownership, real behavior, build/test evidence, and a documented follow-up
  gate for anything intentionally incomplete.
- Treat `Changes/roadmap.md` as the active planning contract. Use its Phase
  Progress, Active Mission Tracks, Current Push Order, and Acceptance Gates to
  choose the next small batch of work.
- Keep the roadmap, changelog, version surfaces, and relevant engine docs
  updated as facts are confirmed. Do not mark roadmap work complete until the
  corresponding build/test/manual evidence exists.
- Prefer short, reviewable batches: implement a focused set of changes, build
  and test them, document what changed, then commit only after the batch is
  stable.
- Avoid speculative rewrites. If a roadmap item is too large for the current
  pass, add precise follow-up notes instead of pretending the phase is done.
- Do not introduce placeholders, fake UI, fake AI autonomy, or dead-end
  scaffolding as if it were production progress. Temporary compatibility code is
  allowed only when it preserves a working path and is documented with an owner,
  reason, and removal/promotion condition.
- Treat operator-reported runtime issues as roadmap evidence. When the user
  reports flicker, missing panes, broken project output, confusing controls,
  AI behavior gaps, or workflow regressions, update `Changes/roadmap.md` and
  the relevant engine/AI docs with the observation, current evidence, and next
  acceptance gate before the detail is lost.
- Current churn prompt for each pass: read `Changes/roadmap.md`, pick the next
  smallest acceptance-gated batch, document every confirmed user observation,
  keep source/version/changelog/docs aligned, build/test what changed, and do
  not claim completion for unverified GUI, AI, project, or renderer behavior.
- EpochBot and helper models may generate local games, tools, apps, or server
  project code only as reviewable artifacts. They must not create or run any
  app/service that gives the model a bypass channel, self-accessible server,
  hidden control surface, listener, or port bind without an explicit human
  approval/run action.
- Local game and tool execution through approved editor/MCP/harness controls is
  allowed when it is visible, evidence-captured, and does not expose a new
  model-accessible network/control surface.
- The Package Manager direction starts with local runtime-mini packages. Future
  downloadable repo/source packages must compile through an updater-style,
  human-approved build/run gate and must not auto-create servers, listeners,
  hidden control surfaces, or model-bypass channels.
- Server-capable work is package-gated. Authoritative dedicated headless server
  support is optional, not the default networking model; client
  listen/nondedicated and future client-predicted competitive paths remain
  separate opt-in packages. Normal software projects, single-player games, and
  minimal generated engine clones must not include server/listener code by
  default. Any package that can bind a port, listen, host, or expose a
  network/control surface requires an explicit human approval/run action.
- Voxel terrain, planetary renderer, procedural vegetation/Forest Factory, AI, and tooling
  prototypes are package candidates first, not direct mainline imports. Stage
  them with provenance, source/hash, build/test commands, limitations, and a
  clear engine API boundary before promoting any subset into active source.

## Build Commands

- Visual Studio / MSBuild solution entry point:

  ```powershell
  & "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine.sln /t:ConsoleApplication1 /p:Configuration=Debug /p:Platform=x64 /m:1
  ```

- Useful MSBuild targets in `Engine.sln`: `ConsoleApplication1`, `StaticLib1`,
  and `HeadlessCI`. CI builds `Engine\examples\HeadlessCI\HeadlessCI.vcxproj`
  with `EpochExtraDefines=EPOCH_CI_HEADLESS_BUILD=1`.
- Root CMake presets are the shared local/CI path:

  ```powershell
  cmake --preset windows-msvc-debug
  cmake --build --preset windows-msvc-debug
  ctest --preset windows-msvc-debug
  ```

- Linux full-engine CMake validation currently uses Clang:

  ```bash
  cmake --preset ninja-clang-debug
  cmake --build --preset ninja-clang-debug
  ctest --preset ninja-clang-debug
  ```

- Linux/GCC presets are headless validation by default because GCC 14 can ICE
  while writing full-engine C++ module BMIs. Use `ninja-gcc-debug` for headless
  validation unless intentionally testing the experimental full GNU module path.
- WSL runtime proof is single-context OpenGL. Do not default WSL to the Windows
  parented multicontext shell or auto-fall back to Vulkan; Vulkan on WSL is an
  explicit validation task until local proof says otherwise.
- The module-aware CMake path requires CMake 3.28 or newer. If the available
  CMake is older, prefer the checked-in Visual Studio/MSBuild solution.

## Running And Validation

- Run Windows local builds from asset-bearing output folders so runtime assets,
  fonts, shaders, scripts, captures, and logs resolve correctly:

  ```powershell
  Set-Location x64/Debug
  .\ConsoleApplication1.exe
  ```

  ```powershell
  Set-Location x64/Release
  .\ConsoleApplication1.exe
  ```

- Do not treat a source-root GUI launch as runtime proof unless the task is
  specifically testing broken-path behavior.
- For runtime, editor, backend, AI, or capture changes, the documented local
  Windows validation path is to rebuild `ConsoleApplication1` in both
  `Debug|x64` and `Release|x64`, launch from the matching `x64/...` folder,
  verify the intended panes/contexts render and respond, then close live
  windows before finishing.
- Hosted GitHub workflows should stay build-only/headless. Do not add GUI
  launches, desktop focus assumptions, or screenshot capture to CI without a
  runtime-safe automation path.

## Active GUI And AI Churn

- Track these as open work until build and manual runtime evidence says
  otherwise: operator-confirmed flicker fixes must stay regression-tested,
  GUI/scene composition z-order can still hide command windows or pane chrome,
  AI Chat/Inspector/Perspective visibility drift needs eye-test confirmation,
  graph surfaces need better information/design/performance, redundant controls
  across panes must keep shrinking, and tabbed/dockable editor windows remain
  incomplete.
- The target editor shape is professional docked GUI: tabbed windows, resizable
  panes, scrollable/selectable text views, context menus, modals, optional
  popouts, and separate editor workspaces for scene/game, assets, projects,
  systems, and AI sandbox operations.
- Treat `engine.gui` as an engine-internal GUI library. Reusable primitives
  such as tabs, dropdown/select boxes, scroll areas, text inputs, window chrome,
  modal layers, splitters, and future context menus belong there first; editor
  workspaces should compose them instead of reimplementing controls or stuffing
  workflow UI into Console Dock output.
- Do not toy with the working draw model. OpenGL editor stability depends on the
  established order: drain normal GUI/backend work, render the scene once, drain
  follow-up work, replay only the explicit GUI top-layer batch for command menus
  and modal chrome, then capture/present. Command-menu z-order, modal focus, and
  pane chrome fixes must be made as explicit GUI/draw-model improvements with
  build proof and operator eye-test evidence, not by skipping drains, moving the
  whole GUI into a deferred batch, or changing backend frame order as a shortcut.
- EpochBot's target is a closed-loop agentic cognition system, not only a chat
  prompt. Keep the architecture documented around working memory, long-term
  memory, retrieval, goals, planner, executor, verifier, scoring, self-state,
  attention, and a real-time observe/act/verify/learn loop.
- EpochBot chat must never surface hidden model reasoning. If a local
  OpenAI-compatible model returns blank assistant `content` with only
  `reasoning_content`, reject the pass as a model/API configuration issue and do
  not promote that reasoning into curated training data.

## Linux Helper Scripts

- From `Engine/`, helper scripts provide a repeatable local path:

  ```bash
  ./build.sh [--no-vcpkg] [--updater-shell] [gcc|clang] [Debug|Release] [-- <extra cmake args>]
  ./run.sh [gcc|clang] [Debug|Release] [-- <runtime args>]
  ./install.sh [gcc|clang] [Debug|Release]
  ./clean.sh
  ```

- `build.sh` writes outputs under `Engine/Bin/` and tries to discover
  `VCPKG_ROOT` unless `--no-vcpkg` is used.

## Change Discipline

- Keep unrelated dirty work intact. This repo often has local generated output
  or staged experiments; do not clean, delete, or revert them unless asked.
- If a change touches runtime/editor/backend/capture behavior, check the
  relevant docs before updating screenshots or README proof images:
  `Engine/docs/engine/smoke_capture_and_screenshot_workflow.md` and
  `Engine/docs/engine/runtime_and_editor_workflows.md`.
- Prefer adding a short TODO when a workflow is uncertain instead of inventing
  a command or support claim.
- Do not add `std::cout`, `std::cerr`, `printf`, or `fprintf` to new C++ code.
  Engine/editor/runtime/backend/AI paths use the engine logger or visible editor
  evidence surfaces. Epoch-branded smoke/validation tools use `core_log_write`
  or `core.log`; direct C++23 `<print>` is reserved for non-engine helper
  utilities that are intentionally outside Epoch runtime/tooling ownership.
- Do not add ad hoc Win32 include blocks to shared engine/editor code. Use the
  existing platform/config wrapper path, and keep backend-specific platform
  includes inside backend-owned translation units.
