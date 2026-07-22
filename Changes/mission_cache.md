# Mission Cache

This file preserves operator-confirmed missions without turning `AGENTS.md` or
`Changes/active_pass.md` into a living novel. Agents should use it as memory
after choosing the current source gate from `Changes/active_pass.md`.

## Renderer Spine

- Build a truthful renderer spine where reusable feature families prove
  themselves in OpenGL first, OpenGL-derived backends share the same contract,
  and Vulkan/DirectX keep equivalent native contracts without drift.
- Formalize handles/descriptors/resource ownership for buffers, textures,
  samplers, shaders, pipelines, materials, meshes, models, render targets,
  binding sets, command lists, render passes, and frame graph work.
- Near-term feature order after sampled render-to-texture: model import, normal
  mapping, skybox, instancing, shadows, deferred/G-buffer, and SSAO.
- Capability honesty is mandatory: `Present` means native backend implementation
  exists and build verification proves it; contracts alone are `Partial`.

## Context And Backend Parity

- OpenGL, SDL3, SFML3, and Raylib3 should share one OpenGL-derived
  renderer-resource contract while preserving backend-specific context nuances.
- DirectX and Vulkan implement equivalent native contracts in their own models.
- Software is a debug/safe-launch fallback, not production parity.
- All production contexts need shared visual palette, object colors, opacity,
  helper colors, and GUI theme inputs from the central spine.
- Floating GUI/tool panes are optional editor/tool host routes. Desktop editor
  builds can present them as dockable/undockable context windows that clone the
  selected backend family by default; games, mobile apps, console targets, and
  headless tools should be able to omit those routes while still using the
  portable `EpochGui` layout/state library. Mixed-backend grids are
  diagnostic/preview mode, not normal editor workflow.
- Context selection must become a real session/backend handoff, not UI theater.
  The editor toolbar combobox should:
  - reflect the active context each frame
  - keep only one live backend context as the editor owner during normal use;
    other duplicate editor sessions in a diagnostic grid should park back to the
    launcher/menu after handoff
  - focus and restore an already-live context of the selected backend when it
    exists
  - create a replacement single editor context when the selected backend is
    compiled but not live: capture state, retire and fully clean the old source
    backend, create one docked replacement in the same native host, then restore
    the snapshot instead of opening a side editor shell
  - fail closed with visible status when no selected backend can be created or
    restored
  - capture current editor state before handoff and restore it into the target
    context when the target enters the session loop
  - treat explicit unavailable backends as failures, never as permission to
    silently substitute the priority/default backend
  - distinguish request-posted, window/context-created, session-entered,
    snapshot-restored, and frame-present evidence
  - log snapshot capture/restore failures visibly and avoid success claims when
    state handoff did not complete
  - log unsupported/missing backend cases plainly instead of opening the wrong
    shell or pretending the switch happened
- Floating GUI and routed GUI windows are individual GUI containers. Current
  pane popouts should start from real pane title-bar drag/release gestures and
  open cloned routed context panels (`pane.outliner`, `pane.inspector`,
  `pane.console`, `pane.ai_chat`). The Window menu owns show/hide/reset only.
  Optional native routes such as `floating.gui` remain host infrastructure, not
  another full editor and not the context picker. Future routes should be
  concrete panels such as asset browser, code editor, build/output, or AI
  visualizer.
  Successful pane routes must hide the source pane in the original editor,
  restore it on Dock Back, Close, or native window close, and refresh routed
  GUI/font upload state before the first cloned-panel frame so font smearing
  after spawn fails validation.
- Redocking must grow into a professional docking guide system, not a vague
  parent-rect drop. Add visible dock glyph/chrome controls on routed popout
  windows and MSVC/Unreal-style guide zones so panes can redock into explicit
  left/right/top/bottom/center slots, with the cloned routed context closing
  cleanly after the original docked pane is restored.
- Windows source now routes SFML, Raylib, SDL, OpenGL, Vulkan, DirectX, and
  Software through one exclusive replacement transaction. The manager keeps the
  native host alive while no renderer exists, waits until the source render
  thread and deferred native cleanup are finished, then creates the selected
  backend as one docked replacement. Native initialization must publish
  `ready` before the editor creates a session or restores GUI/font/project
  state. Failed targets retire before the source backend is recreated from the
  same snapshot. SDL, SFML, and Raylib keep their render thread keyed to the
  stable manager host, backend-owned child windows are destroyed by their
  render thread instead of cross-thread `WM_CLOSE`, manager-owned hosts are
  destroyed only after that thread finishes cleanup, and failed Linux thread
  initialization runs backend cleanup before fallback. SDL, SFML, OpenGL,
  Vulkan, DirectX, and software passed the operator's `v0.87.71` switch check.
  Raylib's `v0.87.72` source now withholds backend readiness until its owner
  thread completes a real present and never enters drawing after a failed GL
  activation. The `v0.87.73` source also keeps adopted GLFW layout on the
  Raylib owner thread so input cannot lock against UI-thread window placement.
  The `v0.87.74` post-readiness gate stalled threaded hardware replacements;
  `v0.87.75` instead makes the dropdown transaction adopt the manager's exact
  target and restore its session before normal backend activation while other
  multicontext windows keep running. Operator testing then exposed a repeated-
  switch lifetime race because the dropdown retired the source inside its
  active editor frame, plus early activation of some hardware replacements.
  `v0.87.76` defers source retirement to the next manager frame boundary and
  keeps only the exact target gated until readiness completes the transaction,
  but fresh Windows evidence recorded an `AppHangTransient` after Raylib reached
  render-ready and showed DirectX/Vulkan stopping during the same adoption
  window. `v0.87.77` removed the replacement render-thread pause and the
  pre-readiness manual restore, but the generic session loop could still adopt
  the target later in the same frame before the transaction acknowledged
  readiness. `v0.87.78` publishes readiness after the first successful backend
  frame, keeps only that exact target excluded while its renderer continues,
  and retains the transaction until normal-path editor restoration and the
  first restored render frame both succeed. Failed frames/restores remain
  recoverable, repeat requests are rejected during adoption, and routed,
  floating, closing, or non-ready contexts are not eligible as whole-editor
  switch targets. Active windows, render threads, and deferred cleanup entries
  move between ownership containers under one serialized retirement boundary;
  native hosts are destroyed only after renderer cleanup joins. Raylib resize
  preserves the parent grid's assigned position and teardown clears the deleted
  GLFW GL binding, while adopted Raylib/SDL/SFML children route keyboard and
  text events through the shared GUI input path. Fresh optimized-build evidence
  requires one further ownership rule: backend-owned HWND procedures may stop
  their renderer, but active-window retirement is marshaled to the manager UI
  thread. Backend lifecycle exceptions are logged and converted to failed
  replacement evidence instead of escaping `noexcept`. Raylib redocking is
  synchronous on its GLFW owner thread, and first-present readiness requires a
  visible, parented child with a usable client area.
  Focused all-backend state/font/focus/repeated-switch acceptance remains
  required before a release claim.
- Raylib, SFML, and SDL multicontext grids are diagnostic evidence only until
  each backend can prove clean parent/child ownership, redock/close teardown,
  context switch restore, and no stale background rendering. Do not feed those
  concurrent-grid results into passive context scoring or default-context
  recommendations.
- Context implementation work should be split by source ownership when using
  agents: one lane for session/window host code, one for editor route/UI
  integration, one for backend capability truth, one for reusable GUI library
  primitives, and one for MSVC/CMake/standalone mirror metadata. Do not ask
  multiple agents to edit the same file family at the same time.
- Daily implementation output target is roughly 9.2k lines of useful
  source/docs/test delta when the operator asks for high-output mission flesh.
  Prefer many bounded production slices with build evidence over long analysis,
  summaries, placeholders, or fake UI.
- Add background context scoring for normal editor runtime selection. The engine
  should passively measure available single-context editor backends over time
  with comparable workloads, record stability/performance/user-visible evidence,
  and recommend the best default editor context without forcing restarts during
  normal work. Multicontext diagnostic grids must be excluded from this runtime
  scoring because concurrent panes distort FPS, timing, memory, input latency,
  and backend contention; they remain diagnostics only, not data for automatic
  default-context choice.
- `Engine/src/passive_context_scoring.hpp` owns the portable
  passive scoring model. Samples must be explicitly single-context before they
  can score; multicontext, diagnostic-grid, and runtime-probe evidence is
  rejected at the API boundary so automated default-context advice cannot learn
  from artificial contention or launched probes.
- The editor feeds that scorer only from passive timing snapshots. System Info
  and the Systems dock may display the current score/recommendation, but the
  scorer does not switch contexts, open windows, or launch probes by itself.

## GUI And Editor Workflow

- Build backend-neutral reusable GUI state in `EpochGui` first, then render and
  route it through `engine.gui`: selectable/editable text, right-click context
  menus, copy/paste, word wrap, scroll bounds, modal focus, progress bars, list
  rows, tabs, docking chrome, theme tables, and floating windows.
- `EpochGui` is the portable C++23 module/static-library layer. It owns OOP
  layout/state controllers and backend-neutral data. It must not require native
  popout windows, editor project state, renderer contexts, or OS-specific host
  code.
- Completed portable text slice: `TextControlController` owns UTF-8-safe byte
  boundaries, caret/anchor selection, document/line/word/multiline movement,
  insertion and deletion, copy/cut/paste intent, read-only policy, byte limits,
  newline/tab filtering, and metric-driven scroll visibility. Platform clipboard
  calls, font measurement, event translation, drawing, wrapping, and context-menu
  presentation remain adapter work.
- Completed portable scene-mode control slice: `SelectionControlController`
  owns C++23 module/static-library segmented-control bounds, item placement, gap
  handling, and hit testing. The editor's 3D/2D mode switch uses that EpochGui
  geometry through `engine.gui`; renderer drawing and input remain adapter work.
- `engine.gui` is the engine adapter. It owns input translation, theme/font
  state, clipping, deferred GUI batches, top-layer replay, and renderer-facing
  widget drawing.
- Desktop editor/tool products may include routed native floating hosts and
  docking. Games, mobile apps, console targets, generated software outputs, and
  headless tools should be able to exclude those host routes while keeping the
  reusable controls they need.
- Preserve the proven draw model. Command menus and modals stay above the scene,
  close/deselect predictably, and must not reintroduce OpenGL flicker.
- Modal, dropdown, progress, and timeline surfaces share the same GUI containment
  contract: top-layer popups render above their owner, hit testing respects modal
  capture, and progress/timeline bars must fit their content rect without bleed.
- Package Manager should use a real list with per-row status/action controls,
  visible transfer/build progress, license evidence, source gates, cache
  validation, cancel behavior, and no fake-progress claims.
- Console Dock is evidence/status only. Editor controls belong in workspaces and
  reusable GUI surfaces, not duplicated in output panes.

## Runtime, Projects, And Packages

- Render-to-texture belongs in core engine. Arcade/game packages consume RTT
  later for in-game cabinets and runtime-minis.
- Engine Arcade must resolve to a real upright arcade cabinet asset from
  `Autodidac/EpochEngineExtensions` or executable-local `cache/packages/`,
  including screen, marquee, control deck, coin/door panels, and cabinet shell.
  Procedural boxes are only the fallback when no reviewed package model exists.
- Built-in mini-runtimes remain kernel-owned and exposed to projects as
  package/script assets.
- Play In Editor runs the saved scene inside the editor. External project launch
  is separate, chooses one backend, does not reset scene edits, and does not
  spawn multicontext clones unless requested.
- Scene persistence must remove unwanted starter junk, keep camera/light/default
  essentials, and make parser/serializer own preview/runtime loading.
- Package payloads belong under executable-local `cache/packages/` or the
  separate EpochEngineExtensions repo, not in mainline dumps.

## Forest Factory And Voxel

- Forest Factory is a core editor workspace with its own 3D scene/window,
  temporal graph growth, repo-derived asset lineage, voxel-node LOD and
  hit-detection descriptors, and timeline integration.
- Heavy planetary terrain, multi-terrain authoring, FFT ocean, imported
  prototypes, and game-specific world stacks remain package candidates.
- Epoch is time-based. The Video workspace owns visible timeline graph,
  playhead, cadence, streaming-save hooks, and project/video timing controls
  near the scene instead of hiding them in System Info.
- Add universal configurable input profiles, freecam/FPS-style 3D controls,
  center/reset hotkeys, package-exportable input contracts, and project camera
  style selection.

## Temporal World And Reversible Simulation

- `Engine/docs/engine/temporal_engine_architecture.md` is the canonical target
  architecture for persistent, reversible, event-driven spacetime. It governs
  implementation ordering; it is not a claim that the complete system exists.
- Time is a primary world coordinate. Persistent queries use timeline, branch,
  and `TimePoint`; the renderer consumes a world observation and never owns
  authoritative time.
- Keep explicit real, simulation, physics, presentation, animation, effects,
  audio, network, editor, and replay domains. Direction supports forward,
  stopped, and backward execution without one universal `deltaTime`.
- Authoritative mutations are immutable typed events committed through atomic
  temporal transactions. Persistent state cannot be partially updated, and
  exact replay tests must cover serialization, idempotence, and crash recovery.
- The installed base world is immutable. Edits, saves, mods, predictions, and
  alternate histories are copy-on-write branch overlays containing divergence
  events, changed immutable pages, local checkpoints, and invalidation records.
- The world store is content-addressed and page-based. Checkpoint manifests,
  schema versions, bounded decoding, hashing, deduplication, reference tracking,
  adaptive checkpoint spacing, and branch garbage collection are core storage
  contracts rather than project-specific save hacks.
- Temporal channels declare truth as exact, error-bounded, visual-only, or
  disposable. Derived GPU buffers, particles, animation matrices, visibility
  lists, navigation work queues, and caches must be reconstructible.
- Nondeterminism is identity-keyed or recorded. Persistent simulation cannot
  depend on global random-call order, thread scheduling, unordered containers,
  or rerunning an AI model to rediscover an authoritative decision.
- Continuous motion uses sparse model segments and consequence-aware correction
  keys. Segment cuts are mandatory at teleports, impulses, collisions, parent
  changes, route/model changes, and other authoritative discontinuities.
- Past edits invalidate only their causal future. Dependency records identify
  affected objects, regions, systems, and time ranges so unrelated history,
  pages, and checkpoints remain reusable.
- Persistent regions advance ticklessly between meaningful scheduled changes.
  Observer demand selects exact interaction, physics, visual, audio, network,
  editor-recording, and historical-query fidelity without erasing unloaded AI.
- Backward rendering keys temporal history by timeline, branch, sample time,
  direction, and camera. Timeline jumps, forks, and direction changes invalidate
  incompatible TAA, upscaling, motion-vector, exposure, and occlusion history.
- Grow the render graph into a demand-driven execution graph spanning CPU,
  SIMD, GPU graphics/compute, transfer, file IO, decompression, page loading,
  replay, and replication. A unified virtual resource fabric carries stable
  logical handles across GPU memory, RAM, compressed cache, SSD, and approved
  reconstruction/content sources.
- Software is a first-class execution/render device consuming the same world
  observations, resource handles, geometry clusters, materials, and temporal
  contracts. It remains the deterministic reference, CI/headless, fallback,
  remote, and offline path rather than emulating a GPU API.
- Shared branches are signed, content-addressed packages. Foreign branches are
  quarantined read-only seeds; a server validates and replays them in isolation
  before creating its own authoritative branch and durable journal.
- Clients submit validated commands, never authoritative events. Safe joins
  begin with server-approved projections and placeholders; optional content is
  disclosed, classified, consented to, hash-verified, quarantined, and granted
  bounded capabilities. Native extensions never auto-download or execute.
- External side effects cross an explicit gateway. Timeline rewind cannot
  unsend packets, undo purchases, reverse exports, launch processes, or mutate
  accounts; committed outcomes return as new authoritative events.
- First production slice, in order: `TimePoint` and stable IDs; immutable event
  journal; atomic transaction; immutable page checkpoint; local branch overlay;
  reversible transform edit; arbitrary-time observation; sparse motion segment;
  analytic particle effect; backward-rendering test; branch export/reimport.
- Do not begin this campaign with multiplayer, unscripted AI, full physics, or
  advanced rendering. The first gate must prove exact authored undo, nonlinear
  redo, compact branching, arbitrary-time reconstruction, branch portability,
  deterministic replay, and bounded storage growth.
- After that vertical slice, advance through physics replay, persistent regions,
  persistent AI, secure branch sharing/import, authoritative networking, optional
  content negotiation, execution/resource fabric, advanced rendering, then full
  timeline/editor/administration tools. Each phase stays capability-gated.

## OS AI And Models

- OS AI is not EpochBot and not an internal persona. Use operator-selected
  external/source-available models only.
- Qwen and Nemotron handle coding/review. Bonsai, Wan, TRELLIS, and FLUX
  fallback are package-managed creative lanes with license/notice gates.
- Model scan is inventory-only. Choosing a model must initialize exactly that
  model, persist the selection under executable-local cache state, and never
  silently fall back to stale defaults.
- Hidden model reasoning must not surface in chat or become training data.

## Sealed Updater, Release, Linux, And Source Shape

- The published `v0.88.69` runtime and updater are completed, accepted, and
  untouchable unless the operator explicitly reopens the gate. The remaining
  bullets preserve the proven contract and historical source-shape rules; they
  are not permission to schedule updater, packaging, handoff-script, tag, or
  release-asset changes.

- Pin source-update archives to the exact commit revision approved by the
  matching platform CI job instead of downloading mutable branch archives
  after the build-status check. Keep archive identity and selected commit in
  updater logs and reject mismatches before executing CMake/build entrypoints.
- Move Linux source-update build orchestration toward an updater-owned helper
  under the proper tools boundary. Until then, execute `Engine/build.sh` only
  from the validated disposable source snapshot with explicit compiler,
  vcpkg-root, overlay, cache, cancellation, and Release arguments.
- Updater is binary-first, platform-build-gated, and source-fallback only when
  no compatible package exists. It must show visible modal/progress/cancel/
  restart evidence, use executable-local cache, and never self-close before
  verified handoff.
- Linux and WSL source-updater rebuilds use the same vcpkg-backed `build.sh`
  lane as normal Linux release builds. Do not convert Linux to `--no-vcpkg`
  unless the operator explicitly asks for a diagnostic/system-package pass.
- Linux Clang full-engine builds need `clang-scan-deps` from the matching
  clang-tools package, a module-aware generator such as Ninja, and a vcpkg clone
  that contains the manifest builtin baseline. Build scripts should fail early
  with actionable evidence when those are missing.
- Linux static third-party ownership stays explicit. Current vcpkg Raylib 6 is
  built with external GLFW and de-vendored cgltf/STB, allowing Epoch's one GLAD
  provider to remain authoritative; any future port change must re-prove that
  ownership before release.
- Linux normal and updater builds require OpenGL, SDL, SFML, Raylib, Vulkan,
  and software dependencies through vcpkg. SDL is intentionally limited to
  X11 and Vulkan features so editor input/window support does not pull the
  unrelated D-Bus/IBus/systemd build chain into source updates.
- Linux/WSL defaults to single-context OpenGL proof. DirectX is disabled, Vulkan
  is explicit validation only, and software remains a debug fallback.
- Continue small build-proven file moves into owned folders. Public headers move
  to `include`, internal headers stay near owners, and backend files move into
  backend folders only with MSVC/CMake/module proof.
- Documentation comes after source proof and should record only changed
  contracts, completed systems, safety gates, and durable operator observations.
