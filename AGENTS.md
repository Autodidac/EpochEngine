# Agent Notes

## 1. Repo Shape

- Epoch is a C++23 engine/tooling repository. Active engine work lives under
  `Engine/src/`, `Engine/modules/`, `Engine/include/`, `Engine/resource/`,
  `Engine/ai/`, and `Engine/examples/`.
- Use the documentation map instead of guessing:
  `Changes/active_pass.md` is the hot acceptance gate,
  `Changes/mission_cache.md` preserves durable cross-pass mission memory,
  `Changes/roadmap.md` is the compact planning contract,
  `Changes/changelog.txt` records version work, `Engine/docs/README.md` is the
  docs index, `Engine/docs/engine/temporal_engine_architecture.md` owns the
  persistent/reversible world target,
  `Engine/docs/engine/renderer_feature_matrix.md` owns renderer
  capability truth, `Engine/docs/engine/runtime_and_editor_workflows.md` owns
  launcher/editor behavior, `Engine/docs/engine/gui_library_architecture.md`
  owns reusable GUI primitives, and
  `Engine/docs/engine/os_ai_tooling_and_evidence_policy.md` owns OS AI policy.
- Stay inside this worktree. Do not copy README, docs, `.codex`, or source
  content from sibling worktrees or unrelated projects into Epoch.
- Do not edit generated output or local runtime artifacts unless the task is
  explicitly about those artifacts. Common generated/local paths include
  `build/`, `x64/`, `Engine/Bin/`, `Engine/build/`, `Engine/built/`, logs,
  captures, and generated `Projects/` output.
- Runtime cache buckets are executable-local and disposable:
  `cache/updates/`, `cache/packages/`, `cache/models/`, and `cache/atlases/`.
  They are not public release payload and not tracked source.
- Keep `addons/` local/offline. It is staged research material, not online repo
  content by default.
- Screenshot proof is additive. New README proof goes under `Images/readme/`
  while old screenshots stay as archive.

## 2. Hard Safety And Build Rules

- Sync before substantive work: inspect branch, dirty state, remotes, and
  upstream before editing project files.
- Diagnose from the complete failing transcript before editing. Group repeated
  dependency/compiler messages under their earliest causal error, repair that
  cause once, and rerun the closest production command instead of iterating on
  downstream symptoms.
- Prefer the fastest faithful local proof before waiting on hosted CI. Linux
  updater/release work uses `build.sh --bootstrap-current-toolchain` locally
  with the same Clang, vcpkg root, overlays, and Release configuration that the
  updater passes; GitHub Actions confirms that result and produces artifacts.
- Minimize release churn: complete source repair, local production build,
  contract checks, and package staging as one bounded pass before pushing the
  candidate. Do not advance/tag repeatedly to discover errors a local lane can
  expose.
- Preserve unrelated dirty work. Never clean, delete, revert, or stage files the
  operator did not ask you to touch.
- GPU/runtime launches are approval-only. Do not run GUI `EpochEditor.exe`
  launches, GUI runtime probes, generated project self-tests, Sandbox
  self-tests, or multicontext launches unless the operator explicitly asks for
  that exact run. The documented `--engine-contract-self-test` lane is the
  build-safe exception because it exits before project-profile builds, child
  runtimes, updater work, OS-AI gates, or renderer startup.
- Prefer source review and build-only checks until runtime proof is requested.
  If a runtime command crashes, hangs, or appears to destabilize GPU/driver
  state, stop runtime probing, preserve logs, check for leftover processes, and
  continue with build/source validation only.
- Windows runtime proof must launch from an asset-bearing output folder such as
  `x64/Debug` or `x64/Release`, not the source root.
- WSL/Linux runtime proof is single-context OpenGL by default. Do not default
  WSL to the Windows parented multicontext shell or auto-fall back to Vulkan.
- C++23 output policy: engine/editor/runtime/backend/AI/capture/updater paths
  use the engine logger or visible editor evidence surfaces. Do not add
  `std::cout`, `std::cerr`, `printf`, or `fprintf` to new C++ engine code.
- Do not add ad hoc Win32 include blocks to shared code. Use existing
  platform/config wrappers and keep platform includes backend-owned.
- GLAD ownership is single-provider. CMake uses
  `EPOCH_GLAD_PROVIDER=auto|vcpkg|bundled`; never link two GLAD loaders or hide
  duplicates with `/FORCE:MULTIPLE`.
- MSVC static-vcpkg ownership must stay explicit: one private STB
  implementation, executable targets own final third-party library choices,
  SFML static/dynamic variants must not mix, and SDL3 static Windows system
  libraries belong on the executable target.
- The working GUI/scene draw model is protected. Do not alter backend frame
  order, queue-drain order, or overlay replay behavior unless the mission is
  explicitly a draw-model improvement with build proof and operator eye-test
  evidence.
- Update UI must be evidence-backed. Startup may check quietly, but a modal
  appears only for a real platform-compatible update, remains visible while
  work runs, offers Cancel for source rebuilds, and shows Restart only after
  verified handoff evidence. The editor must not close itself or claim success
  because a worker merely started.
- The published `v0.88.69` runtime release and its updater implementation are a
  sealed baseline. Do not edit updater code, updater UI, handoff/build scripts,
  packaging, release metadata, tags, or release assets unless the operator
  explicitly reopens that gate. Normal source-version advancement is allowed
  without changing packaged-version defaults or updater behavior.

## 3. Current Validation Commands

- Visual Studio / MSBuild editor target:

  ```powershell
  & "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine.sln /t:ConsoleApplication1 /p:Configuration=Debug /p:Platform=x64 /m:1
  ```

- Useful MSBuild targets in `Engine.sln`: `ConsoleApplication1`,
  `StaticLib1`, and `HeadlessCI`.
- Build-safe pure engine contract check after rebuilding the editor target:

  ```powershell
  .\x64\Debug\EpochEditor.exe --engine-contract-self-test
  ```

  Prefer this for fast agent churn when GUI/runtime validation is not explicitly
  approved. Reserve heavier validation commands such as
  `--engine-validation-self-test`, `--editor-project-self-test <id>`, and
  generated child `--project-self-test` runs for operator-approved use because
  they can materialize projects, create child processes, or touch runtime/editor
  state.
- Root CMake presets:

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

- Linux/GCC 16.1 is headless validation by default while its full module lane
  remains experimental. Use `ninja-gcc-debug` for headless
  validation unless intentionally testing the experimental full GNU module path.
- `Engine/` Linux helpers:

  ```bash
  ./build.sh [--no-vcpkg] [--updater-shell] [--bootstrap-current-toolchain] [--tool-cache-root <path>] [--check-toolchain] [gcc|clang] [Debug|Release] [-- <extra cmake args>]
  ./run.sh [gcc|clang] [Debug|Release] [-- <runtime args>]
  ./install.sh [gcc|clang] [Debug|Release]
  ./clean.sh
  ```

  For updater-equivalent local release proof, use a writable disposable tool
  cache and let the script bootstrap the pinned current toolchain:

  ```bash
  ./build.sh --bootstrap-current-toolchain --tool-cache-root /tmp/epoch-update-tools clang Release
  ```

  Add `--check-toolchain` before `clang Release` to validate the selected
  compiler, module scanner, CMake, Ninja, and vcpkg registry without configuring
  or building the engine.

- When the operator explicitly requests Linux release staging, use the tracked
  packaging command from the repo root:

  ```powershell
  pwsh -NoProfile -File .\Tools\ai\stage_epoch_linux_release.ps1 -Version <version> -Configuration Clang-Release
  ```

  This command performs the approval-only staged OpenGL smoke in addition to
  checking notices, `$ORIGIN/lib`, shared-library resolution, version and
  contract output, archive cleanliness, and the release checksum.

## 4. Source Ownership Rules

- Public/external headers belong under `Engine/include/...`; internal
  implementation headers stay near owning source under `Engine/src/...` or
  module-private paths.
- Prefer owned folders over flat dumps as systems mature: `src/epochgui`,
  `src/editor`, `src/project`, `src/packages`, `src/renderers/<backend>`,
  `src/ai`, `src/platform`, and similar ownership boundaries.
- Renames and moves are source work, not cosmetics. Before moving files,
  inventory includes/imports, module ownership, MSVC projects/filters, CMake
  targets, runtime asset references, and docs links.
- `engine.gui` is an engine-internal reusable GUI library. Add reusable
  primitives there first: tabs, dropdowns, scroll areas, text inputs,
  selectable text, context menus, window chrome, modal layers, splitters,
  progress bars, docking, and future floating GUI windows.
- GUI text surfaces must behave like real controls: selection, copy/paste,
  word wrap, scroll bounds, context menus, focus, and deselection.
- Current `.epoch` scene/world files are metadata shells. Do not claim
  scene-file authoring is complete until parser/serializer owns preview/runtime
  loading.
- Built-in mini-runtime/game modules stay kernel-owned and ship with
  applications. Expose them as package/script assets such as `engine_arcade`,
  not loose generated project scripts.
- Voxel field/pathing/tracing contracts and deterministic Forest Factory
  descriptors are core primitives. Heavy planetary terrain, multi-terrain
  authoring, FFT ocean, imported prototypes, and game-specific world stacks are
  package candidates first.
- Bulky optional package source belongs in
  `https://github.com/Autodidac/EpochEngineExtensions`, not in EpochEngine
  mainline or local `addons/` dumps.
- Forest Factory is a core editor feature with its own 3D scene/window, but
  generated projects include Forest Factory assets/scripts only after visible
  package activation or main-scene use.
- Server-capable work is package-gated. Anything that can bind a port, listen,
  host, or expose a network/control surface requires explicit human approval.
- OS AI is selected external tooling, not EpochBot or an internal persona.
  Visible AI surfaces use operator-selected OS/source-available models such as
  Qwen, Nemotron, Bonsai, Wan, TRELLIS, and FLUX fallback.

## 5. What Agents Must Never Do

- Never add placeholders, fake UI, fake AI autonomy, hidden self-training,
  dead code, or duplicate wrappers as if they were production progress.
- Never claim completion for unverified GUI, AI, project, renderer, update, or
  backend behavior.
- Never broaden scope to compensate for a blocked active gate. Report the
  stop condition and preserve partial progress.
- Never add named tutorial references, mirrored snippets, or copied lesson
  structure from external renderer study material to Epoch source, docs,
  manifests, comments, or release notes.
- Never auto-create servers, listeners, hidden control surfaces, model-bypass
  channels, or network-serving modes from AI/package tooling.
- Never mark a renderer feature `Present` unless native backend implementation
  exists and build verification proves it.
- Never stage generated caches, transient capture JSON/startup probes, local
  runtime folders, or unrelated operator files.

## 6. Active Pass Pointer

- Start every churn pass by reading this file and `Changes/active_pass.md`.
  Read `Changes/roadmap.md` only for broader context or durable follow-up
  placement. Use `Changes/mission_cache.md` only after identifying the current
  gate; it is long-term memory, not permission to widen the pass.
- The active pass owns the current source gate. Implement/refactor production
  C++ first, build/test what changed, then record only new/completed systems
  and changed contracts.
- Documentation-only work is allowed when explicitly requested, when preserving
  a new/completed system contract, or when a safety/build/release gate would be
  lost without the note.
- When the operator asks for a push at a known-good point, stop risky edits and
  preserve the checkpoint first: inspect status, stage the focused batch,
  commit, push `main`, and update the stable branch only when explicitly asked.
