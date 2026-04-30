# Current Engine Architecture

## Snapshot

Epoch is now documented as a module-first engine with the active runtime living
under `Engine/modules/` and `Engine/src/`, while older compatibility/archive
surfaces have been retired and mapped into active replacements.

Current source version: `v0.84.04`

## Architecture highlights

- **Multi-context runtime**: the shared context layer and multiplexer coordinate
  backend-owned windows, command queues, and render-thread work.
- **Backend stack**: OpenGL, SDL, Raylib, SFML, software, and noop/headless are
  all represented in the active engine tree; Vulkan remains experimental.
- **Custom UI on automated texture/atlas plumbing**: GUI layout, atlas upload,
  sprite submission, and font/text rendering are engine-owned systems shared
  across the active render paths.
- **Launcher/editor split**: project and game entry now live in the launcher,
  while the editor uses a more traditional desktop-style menu flow.
- **Task graph + scripting**: reload and background work are funneled through
  task scheduling rather than ad hoc threaded entry points, and editor-triggered
  compiled scripts now run through an explicit host API instead of a loose
  filewatch-first loop.
- **Executable-root runtime resolution**: fonts, scripts, shaders, captures,
  workspace paths, and updater scratch roots are being normalized around one
  executable-root/runtime-root resolver instead of cwd guesses.
- **Two-role engine AI path**: the intended runtime roles are internal
  EpochBot and the local MCP/control layer, while external local LLMs remain
  development helpers for testing, curation, and acceleration rather than a
  third engine runtime role.
- **Systems workspace direction**: the editor shell is moving toward real
  frame/task graph surfaces rendered as engine-generated textures inside the
  dock UI instead of placeholder text.
- **Migration map**: formerly archived compatibility surfaces are now either
  preserved in active modules or called out explicitly in
  `Engine/docs/engine/legacy_feature_map.md`.

## Current strengths

- Strong module-first organization for the active engine.
- Safer runtime path ownership now that more subsystems resolve assets and
  support files from the executable/runtime root instead of the working
  directory.
- Broad backend coverage for desktop experimentation and tooling.
- Clearer support target discipline around 6-core / 1660 Ti-era desktops and
  modern Linux laptops as the default automatic compatibility baseline.
- Good separation between active code and archived compatibility material after
  moving the legacy tree under `Engine/`.
- Startup and shutdown behavior are getting more disciplined as backend-local
  diagnostics and hot-path logging are trimmed back out of the render loop.
- SDL and Software now share the same editor preview-grid geometry/palette path
  as the GPU editor previews, which tightens backend parity for scene-view
  smoke tests.
- Parent-window shutdown now behaves more like a real engine host lifecycle:
  docked children are marked for close and the session exits instead of leaving
  a dead console/process behind.
- The parented docking layout now stays intact during shutdown instead of
  undocking backend panes as the host window closes.
- SDL3 and SFML3 are back on the real promoted-proxy path under the six-context
  Win32 parent: the visible shell is what detaches, the backend child stays
  nested inside that shell, and the detached window can escape the parent as a
  true top-level host instead of freezing under the parent bounds.
- Live pane management is back in a better place too: undocked windows keep the
  information they need to redock cleanly, and only still-docked panes are
  considered part of the parent grid layout.
- Manual pane management is now usable again without modifier keys: a dedicated
  drag strip provides left-drag docking while leaving the rest of the pane free
  for normal backend/editor interaction.
- Parent-close ownership now behaves like a real multi-window host lifecycle:
  undocked promoted backend windows can outlive the parent host, but once they
  are redocked they return to normal parent-owned shutdown behavior.
- The last surviving undocked pane now shuts the process down cleanly when it
  closes, even if that backend window is owned by a render thread.
- The editor shell now maps more cleanly to production-tool expectations:
  launcher responsibilities are separated from editor responsibilities, and the
  updater is confirmation-gated before it can run from the UI.
- The source-update path is back to behaving like an engine updater instead of
  a passive download tool: it now restores manifest dependencies with `vcpkg`,
  rebuilds from the downloaded snapshot, and replaces the running runtime from
  the rebuilt output.
- Update/install policy is platform-aware: Windows uses `.zip` runtime assets,
  Linux/WSL uses `.tar.gz` runtime assets, and source installs fall back to a
  source snapshot rebuild only after packaged-runtime parity is reached or no
  newer packaged asset is available.
- Backend presentation is more visually coherent now that the active renderer
  base colors are being pulled toward the same darker Vulkan-style baseline,
  and SFML’s shared preview path is clipped back to the intended scene view.

## Current cautions

- Vulkan is present but should still be treated as a migration/integration path,
  not the default renderer, even though the editor palette, editor grid preview,
  and GUI presentation have been brought closer to the OpenGL baseline.
- A few minor archival and compatibility surfaces still exist and should be
  changed carefully.
- Backend fixes are usually safest when applied locally to the affected backend
  instead of globally in the multiplexer.
- Raylib parent/docking work should continue to respect GLFW/raylib ownership of
  the native GL context instead of swapping in fresh Win32 DC handles after
  initialization.

## Recommended priorities

1. Keep renderer/backend smoke coverage improving, especially around startup,
   resize, docking, and shutdown behavior.
2. Continue reducing duplicated compatibility naming in public surfaces while
   preserving compile compatibility where needed.
3. Promote only active, tested functionality into the main engine surface and
   keep migration notes honest when a retired system still needs a modern home.
